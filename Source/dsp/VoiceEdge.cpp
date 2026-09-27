#include "VoiceEdge.h"
#include "../ParamIDs.h"

namespace
{
    // 最大增益变化（amount ±100 → 起音增益 0.1x ~ 3.2x）
    constexpr float kMaxBoost = 2.2f;
    constexpr float kMinGain = 0.1f;
    // 瞬态死区：稳态信号下快包络跟峰值、慢包络跟均值，天然有 ~0.09 的差值
    // （正弦实测），不加死区会持续调制音量。只有超出死区的部分才算真起音。
    constexpr float kTransientDeadZone = 0.15f;

    float timeToCoeff(float ms, double sampleRate)
    {
        return 1.0f - std::exp(-1.0f / (ms * 0.001 * sampleRate));
    }
}

VoiceEdge::VoiceEdge(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate)
    : amountParam(apvts.getRawParameterValue(ParamID::edgeAmount)),
      bypassParam(apvts.getRawParameterValue(ParamID::edgeBypass)),
      dspRate(&osSampleRate)
{
}

void VoiceEdge::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = dspRate->load(); // OS 采样率（模块在超采样链内运行）
    fastAttack  = timeToCoeff(0.3f, sampleRate);   // 快包络：跟得上字头
    fastRelease = timeToCoeff(15.0f, sampleRate);
    slowAttack  = timeToCoeff(25.0f, sampleRate);  // 慢包络：跟节目电平
    slowRelease = timeToCoeff(250.0f, sampleRate);
    gainSmoothCoeff = timeToCoeff(1.5f, sampleRate); // 限制变化率降调制失真
    reset();
}

void VoiceEdge::reset()
{
    fastEnv[0] = fastEnv[1] = slowEnv[0] = slowEnv[1] = 0.0f;
    gainSmooth[0] = gainSmooth[1] = 1.0f;
}

void VoiceEdge::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& outputBlock = context.getOutputBlock();
    if (outputBlock.getNumSamples() == 0)
        return;

    // 旁通：强度平滑归零（amount=0 → 增益恒 1.0，完全透明）
    {
        const float bypTarget = bypassParam->load() > 0.5f ? 0.0f : 1.0f;
        const double blockDur = (double) outputBlock.getNumSamples() / jmax(1.0, sampleRate);
        const float bypAlpha = 1.0f - (float) std::exp(-blockDur / 0.01);
        bypassMix += bypAlpha * (bypTarget - bypassMix);
    }

    const float amount = jlimit(-100.0f, 100.0f, amountParam->load()) / 100.0f * bypassMix;

    const auto numChannels = outputBlock.getNumChannels();
    const auto numSamples = outputBlock.getNumSamples();

    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        auto* data = outputBlock.getChannelPointer(ch);
        const size_t si = ch < 2 ? ch : 1;
        float& fe = fastEnv[si];
        float& se = slowEnv[si];
        float& gs = gainSmooth[si];

        for (size_t n = 0; n < numSamples; ++n)
        {
            const float x = data[n];
            const float a = std::abs(x);

            // 双包络跟随：快的抓字头，慢的跟节目电平
            fe += (a > fe ? fastAttack : fastRelease) * (a - fe);
            se += (a > se ? slowAttack : slowRelease) * (a - se);

            // 瞬态强度 = 快包络超出慢包络的比例（起音期间大，稳态趋零）
            const float raw = (fe - se) / (se + 1.0e-6f);
            const float transient = jlimit(0.0f, 1.0f,
                                           (raw - kTransientDeadZone) / (1.0f - kTransientDeadZone));

            // 正 amount 抬起起音（棱角），负 amount 压扁起音（圆滑）
            const float target = jmax(kMinGain, 1.0f + amount * kMaxBoost * transient);
            gs += gainSmoothCoeff * (target - gs);
            data[n] = x * gs;
        }
    }
}
