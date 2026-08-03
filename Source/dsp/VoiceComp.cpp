#include "VoiceComp.h"
#include "../ParamIDs.h"

namespace
{
    // 一阶平滑系数（时间常数 ms → per-sample k）
    float timeToCoeff(float ms, double sampleRate)
    {
        return 1.0f - std::exp(-1.0f / (ms * 0.001 * sampleRate));
    }

    // 峰值因子 → 归一化（2dB 平滑 → 0，10dB 瞬态 → 1）
    float crestNorm(float crestDb)
    {
        return jlimit(0.0f, 1.0f, (crestDb - 2.0f) / 8.0f);
    }
}

VoiceComp::VoiceComp(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate)
    : modeParam   (apvts.getRawParameterValue(ParamID::compMode)),
      amountParam (apvts.getRawParameterValue(ParamID::compAmount)),
      makeupParam (apvts.getRawParameterValue(ParamID::compMakeup))
{
}

void VoiceComp::Layer::prepare(double sr)
{
    // 用平滑后的参数算系数（模式切换无 click）
    envAttack = timeToCoeff(attackMsS, sr);
    envRelease = timeToCoeff(releaseMsS, sr);
    gainAttack = timeToCoeff(attackMsS * 1.5f, sr);
    gainRelease = timeToCoeff(releaseMsS, sr);
}

void VoiceComp::Layer::reset()
{
    env = 0.0;
    smoothDb = 0.0f;
}

void VoiceComp::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    mode = -1; // 强制下一块重算模式
    updateSmartParams((int) spec.maximumBlockSize);
    fastLayer.prepare(sampleRate);
    smoothLayer.prepare(sampleRate);
    reset();
}

void VoiceComp::reset()
{
    fastLayer.reset();
    smoothLayer.reset();
    peakEma = rmsEma = 0.0f;
    gainReduction.store(0.0f);
}

// 智能参数：由峰值因子驱动（块级更新，crest 本身 ~1s 平滑 → 无跳变）
void VoiceComp::updateSmartParams(int numSamples)
{
    const int newMode = (int) modeParam->load();
    if (newMode != mode)
    {
        mode = newMode;
        modeRatioBoost = (mode == 1) ? 1.5f : 0.0f; // Rap 更狠
    }

    const float t = crestNorm(crestDb);

    // 参数目标
    fastLayer.attackMs = 3.0f + (0.15f - 3.0f) * t;
    fastLayer.releaseMs = 180.0f + (70.0f - 180.0f) * t;
    fastLayer.ratio = 3.5f + (6.0f - 3.5f) * t + modeRatioBoost;
    fastLayer.softKnee = false;

    smoothLayer.attackMs = 18.0f + (5.0f - 18.0f) * t;
    smoothLayer.releaseMs = 400.0f + (150.0f - 400.0f) * t;
    smoothLayer.ratio = 2.0f;
    smoothLayer.softKnee = true;

    // 平滑过渡（~30ms，模式切换/参数变化无 click）
    constexpr float k = 0.25f; // 块级平滑（块 ~10ms → 收敛 ~4 块）
    auto smoothTo = [k](float& s, float target) { s += k * (target - s); };
    smoothTo(fastLayer.attackMsS,  fastLayer.attackMs);
    smoothTo(fastLayer.releaseMsS, fastLayer.releaseMs);
    smoothTo(fastLayer.ratioS,     fastLayer.ratio);
    smoothTo(smoothLayer.attackMsS,  smoothLayer.attackMs);
    smoothTo(smoothLayer.releaseMsS, smoothLayer.releaseMs);
    smoothTo(smoothLayer.ratioS,     smoothLayer.ratio);

    fastLayer.prepare(sampleRate);
    smoothLayer.prepare(sampleRate);

    // UI/测试显示
    crestDbDisplay.store(crestDb);
    fastAttackMsDisplay.store(fastLayer.attackMs);
    fastRatioDisplay.store(fastLayer.ratio);
}

void VoiceComp::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& outputBlock = context.getOutputBlock();


    if (outputBlock.getNumSamples() == 0)
        return;

    const auto numChannels = outputBlock.getNumChannels();
    const auto numSamples = (int) outputBlock.getNumSamples();

    // 智能特征检测：块级峰值/RMS → 慢速 EMA
    float peak = 0.0f;
    double rms = 0.0;
    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        const auto* d = outputBlock.getChannelPointer(ch);
        for (int n = 0; n < numSamples; ++n)
        {
            peak = jmax(peak, std::abs(d[n]));
            rms += (double) d[n] * d[n];
        }
    }
    rms = std::sqrt(rms / (double) (numChannels * numSamples));
    peakEma += crestAlpha * (peak - peakEma);
    rmsEma += crestAlpha * ((float) rms - rmsEma);
    crestDb = 20.0f * std::log10(peakEma / (rmsEma + 1.0e-8f));
    crestDb = jlimit(1.0f, 14.0f, crestDb);

    updateSmartParams(numSamples);

    // 分层阈值：Fast 层浅（-30dB，只抓瞬态峰值）；Smooth 层深（-40dB，管整体节目电平）
    const float amount = jlimit(0.0f, 100.0f, amountParam->load()) / 100.0f;
    const float fastThresh = -30.0f * amount;
    const float smoothThresh = -40.0f * amount;
    const float makeupGain = juce::Decibels::decibelsToGain(jlimit(0.0f, 12.0f, makeupParam->load()));

    float* data[2] { nullptr, nullptr };
    const size_t chs = (size_t) jmin((int) numChannels, 2);
    for (size_t ch = 0; ch < chs; ++ch)
        data[ch] = outputBlock.getChannelPointer(ch);

    Layer* layers[2] { &fastLayer, &smoothLayer };

    for (size_t n = 0; n < (size_t) numSamples; ++n)
    {
        // 共享检测：所有通道峰值（stereo linked）
        float peak = 0.0f;
        for (size_t ch = 0; ch < chs; ++ch)
            peak = jmax(peak, std::abs(data[ch][n]));

        // 两层串联（Fast → Smooth），每层独立包络/增益
        for (Layer* layer : layers)
        {
            // 包络跟随（上升 attack，下降 release）
            const float k = peak > layer->env ? layer->envAttack : layer->envRelease;
            layer->env += k * ((double) peak - layer->env);

            const float envDb = 20.0f * std::log10((float) layer->env + 1.0e-6f);
            const float over = envDb - (layer == &fastLayer ? fastThresh : smoothThresh);

            // 增益曲线（Smooth 层软拐点 knee=6dB）
            float targetDb;
            if (over > 0.0f)
            {
                float effectiveOver = over;
                if (layer->softKnee)
                {
                    constexpr float knee = 6.0f;
                    effectiveOver = over > knee ? over - knee * 0.5f
                                                : over * over / (2.0f * knee);
                }
                targetDb = -effectiveOver * (1.0f - 1.0f / layer->ratioS);
            }
            else
            {
                targetDb = 0.0f;
            }

            // 增益平滑（压下去快，恢复慢）
            const float gk = targetDb < layer->smoothDb ? layer->gainAttack : layer->gainRelease;
            layer->smoothDb += gk * (targetDb - layer->smoothDb);
        }

        // 应用两层增益 + Makeup
        const float g = std::pow(10.0f, (fastLayer.smoothDb + smoothLayer.smoothDb) * 0.05f) * makeupGain;
        for (size_t ch = 0; ch < chs; ++ch)
            data[ch][n] *= g;
    }

    gainReduction.store(fastLayer.smoothDb + smoothLayer.smoothDb);
}
