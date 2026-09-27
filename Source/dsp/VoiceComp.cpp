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

    // RBJ 二阶高通，写进 Coefficients 的 5 元素 raw 数组 [b0,b1,b2,a1,a2]（a0 归一化省略）
    void writeHighPass(float* c, double fs, float freq, float q)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi
                        * jlimit(10.0, fs * 0.45, (double) freq) / fs;
        const double cs = std::cos(w0), sn = std::sin(w0);
        const double alpha = sn / (2.0 * (double) q);
        const double a0 = 1.0 + alpha;
        c[0] = (float) (((1.0 + cs) * 0.5) / a0);
        c[1] = (float) (-(1.0 + cs) / a0);
        c[2] = c[0];
        c[3] = (float) ((-2.0 * cs) / a0);
        c[4] = (float) ((1.0 - alpha) / a0);
    }

    // 侧链高通截止：去掉低频触发（人声压缩不抽气的关键）。
    // 只作用于检测路径，不改变音频本身；120Hz 以下的人声基频不再触发压缩。
    constexpr float kScHpfFreq = 120.0f;
}

VoiceComp::VoiceComp(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate)
    : modeParam   (apvts.getRawParameterValue(ParamID::compMode)),
      amountParam (apvts.getRawParameterValue(ParamID::compAmount)),
      makeupParam (apvts.getRawParameterValue(ParamID::compMakeup)),
      bypassParam (apvts.getRawParameterValue(ParamID::compBypass)),
      dspRate(&osSampleRate)
{
}

void VoiceComp::Stage::prepare(double sr)
{
    // 用平滑后的参数算系数（模式切换无 click）
    envAttack = timeToCoeff(attackMsS, sr);
    envRelease = timeToCoeff(releaseMsS, sr);
    gainAttack = timeToCoeff(attackMsS * 1.5f, sr);
    gainRelease = timeToCoeff(releaseMsS, sr);
}

void VoiceComp::Stage::reset()
{
    env = 0.0;
    smoothDb = 0.0f;
}

void VoiceComp::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (double) dspRate->load(); // OS 采样率（模块在超采样链内运行）
    mode = -1; // 强制下一块重算模式

    // 侧链高通：系数预分配，每块按当前 dspRate 重写 raw 数组（倍率热切换不失效）
    scCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, kScHpfFreq, 0.707f);
    for (auto& f : scFilter)
    {
        f.prepare(spec);
        f.coefficients = scCoeffs;
        f.reset();
    }

    updateSmartParams((int) spec.maximumBlockSize);
    fet.prepare(sampleRate);
    opto.prepare(sampleRate);
    para.prepare(sampleRate);
    reset();
}

void VoiceComp::reset()
{
    fet.reset();
    opto.reset();
    para.reset();
    peakEma = rmsEma = fastPeakEma = 0.0f;
    crestDb = 3.0f;
    transientIndex = 0.0f;
    for (auto& f : scFilter)
        f.reset();
    gainReduction.store(0.0f);
}

// 智能参数：crest 峰值因子 + 短时瞬态 驱动（块级更新 → 无跳变）
void VoiceComp::updateSmartParams(int numSamples)
{
    const int newMode = (int) modeParam->load();
    if (newMode != mode)
        mode = newMode;

    const float t  = crestNorm(crestDb);          // 0 = 平滑，1 = 瞬态丰富
    const float ti = transientIndex;              // 短时瞬态（0~1）
    const float forge = (mode == 1) ? 1.0f : 0.0f;

    // Stage 1 FET：只抓峰值。attack 由 crest + 短时瞬态共同驱动，瞬态来了更快
    fet.attackMs    = (3.0f + (0.3f - 3.0f) * t) * (1.0f - 0.6f * ti);
    fet.releaseMs   = 120.0f + (60.0f - 120.0f) * t;
    fet.ratio       = 4.0f + (8.0f - 4.0f) * t + forge * 1.5f;
    fet.thresholdDb = -26.0f - 6.0f * forge;
    fet.softKnee    = false;

    // Stage 2 光电：做胶水（releaseMs 由 process 按压缩深度程序依赖地更新）
    opto.attackMs    = 30.0f + (15.0f - 30.0f) * t;
    opto.ratio       = 2.0f;
    opto.thresholdDb = -34.0f - 4.0f * forge;
    opto.softKnee    = true;

    // 程序依赖释放：压得越深，释放越慢（用上一块的压缩深度，约一块延迟）
    {
        const float depthNorm = jlimit(0.0f, 1.0f, -opto.smoothDb / 6.0f);
        opto.releaseMs = 120.0f + (1500.0f - 120.0f) * depthNorm;
    }

    // Stage 3 并行：重压支链（New York 风），与主信号混合增密度
    para.attackMs    = 0.5f;
    para.releaseMs   = 100.0f;
    para.ratio       = 8.0f;
    para.thresholdDb = -40.0f;
    para.softKnee    = false;

    // 平滑过渡（块级 ~4 块收敛，模式切换/参数变化无 click）
    constexpr float k = 0.25f;
    auto smoothTo = [k](float& s, float target) { s += k * (target - s); };
    smoothTo(fet.attackMsS,  fet.attackMs);
    smoothTo(fet.releaseMsS, fet.releaseMs);
    smoothTo(fet.ratioS,     fet.ratio);
    smoothTo(opto.attackMsS,  opto.attackMs);
    smoothTo(opto.releaseMsS, opto.releaseMs);
    smoothTo(opto.ratioS,     opto.ratio);
    smoothTo(para.attackMsS,  para.attackMs);
    smoothTo(para.releaseMsS, para.releaseMs);
    smoothTo(para.ratioS,     para.ratio);

    fet.prepare(sampleRate);
    opto.prepare(sampleRate);
    para.prepare(sampleRate);

    // UI/测试显示
    crestDbDisplay.store(crestDb);
    fastAttackMsDisplay.store(fet.attackMs);
    fastRatioDisplay.store(fet.ratio);
    optoReleaseMsDisplay.store(opto.releaseMs);
}

void VoiceComp::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& outputBlock = context.getOutputBlock();
    if (outputBlock.getNumSamples() == 0)
        return;

    sampleRate = (double) dspRate->load();
    const auto numChannels = outputBlock.getNumChannels();
    const auto numSamples = (int) outputBlock.getNumSamples();

    // 侧链系数每块按当前 OS 率重写（OS 倍率热切换后系数会错位）
    writeHighPass(scCoeffs->getRawCoefficients(), sampleRate, kScHpfFreq, 0.707f);

    // 智能特征：长时 crest（300ms EMA）+ 短时瞬态（5ms EMA）
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
    rms = std::sqrt(rms / (double) jmax(1, (int) (numChannels * (size_t) numSamples)));

    const double blockDur = (double) numSamples / jmax(1.0, sampleRate);
    const float slowAlpha = 1.0f - (float) std::exp(-blockDur / 0.30);
    const float fastAlpha = 1.0f - (float) std::exp(-blockDur / 0.005);
    peakEma     += slowAlpha * (peak - peakEma);
    fastPeakEma += fastAlpha * (peak - fastPeakEma);
    rmsEma      += slowAlpha * ((float) rms - rmsEma);
    crestDb = jlimit(1.0f, 14.0f, 20.0f * std::log10(peakEma / (rmsEma + 1.0e-8f)));
    transientIndex = jlimit(0.0f, 1.0f, (fastPeakEma - peakEma) / (peakEma + 1.0e-6f));

    updateSmartParams(numSamples);

    // 旁通：压缩量与补偿增益同步平滑归零
    {
        const float bypTarget = bypassParam->load() > 0.5f ? 0.0f : 1.0f;
        const float bypAlpha = 1.0f - (float) std::exp(-blockDur / 0.01);
        bypassMix += bypAlpha * (bypTarget - bypassMix);
    }

    const float amount = jlimit(0.0f, 100.0f, amountParam->load()) / 100.0f * bypassMix;
    const float makeupGain = juce::Decibels::decibelsToGain(
        jlimit(0.0f, 12.0f, makeupParam->load()) * bypassMix);

    // 阈值随 amount 展开（0% = 不压）。用幂曲线而非线性：
    // 线性缩放下 amount<1 时阈值快速变浅，旋钮拧一半几乎没有压缩，手感很差。
    const float amountCurve = std::pow(amount, 0.4f);
    const float fetThresh  = fet.thresholdDb  * amountCurve;
    const float optoThresh = opto.thresholdDb * amountCurve;
    const float paraThresh = para.thresholdDb * amountCurve;
    const float paraMix    = ((mode == 1) ? 0.35f : 0.15f) * amount;

    float* data[2] { nullptr, nullptr };
    const size_t chs = (size_t) jmin((int) numChannels, 2);
    for (size_t ch = 0; ch < chs; ++ch)
        data[ch] = outputBlock.getChannelPointer(ch);

    float grMax = 0.0f; // 三级 GR 之和的最负值
    constexpr float kneeOpto = 8.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        // 侧链检测信号（高通后，仅用于检测，不改音频路径）；stereo linked
        float scPeak = 0.0f;
        for (size_t ch = 0; ch < chs; ++ch)
            scPeak = jmax(scPeak, std::abs(scFilter[jmin(ch, (size_t) 1)].processSample(data[ch][n])));

        // Stage 1 FET：抓字头过冲
        {
            const float k = scPeak > fet.env ? fet.envAttack : fet.envRelease;
            fet.env += k * ((double) scPeak - fet.env);
            const float envDb = 20.0f * std::log10((float) fet.env + 1.0e-6f);
            const float over = envDb - fetThresh;
            float targetDb = 0.0f;
            if (over > 0.0f)
                targetDb = -over * (1.0f - 1.0f / jmax(1.0f, fet.ratioS));
            const float gk = targetDb < fet.smoothDb ? fet.gainAttack : fet.gainRelease;
            fet.smoothDb += gk * (targetDb - fet.smoothDb);
        }

        // Stage 2 光电：软拐点 + 程序依赖释放，做胶水
        {
            const float k = scPeak > opto.env ? opto.envAttack : opto.envRelease;
            opto.env += k * ((double) scPeak - opto.env);
            const float envDb = 20.0f * std::log10((float) opto.env + 1.0e-6f);
            const float over = envDb - optoThresh;
            float targetDb = 0.0f;
            if (over > 0.0f)
            {
                const float eff = over > kneeOpto ? over - kneeOpto * 0.5f
                                                  : over * over / (2.0f * kneeOpto);
                targetDb = -eff * (1.0f - 1.0f / jmax(1.0f, opto.ratioS));
            }
            const float gk = targetDb < opto.smoothDb ? opto.gainAttack : opto.gainRelease;
            opto.smoothDb += gk * (targetDb - opto.smoothDb);
        }

        const float mainGain = std::pow(10.0f, (fet.smoothDb + opto.smoothDb) * 0.05f);

        // Stage 3 并行：重压支链（从同一输入取信号）
        {
            const float k = scPeak > para.env ? para.envAttack : para.envRelease;
            para.env += k * ((double) scPeak - para.env);
            const float envDb = 20.0f * std::log10((float) para.env + 1.0e-6f);
            const float over = envDb - paraThresh;
            float targetDb = 0.0f;
            if (over > 0.0f)
                targetDb = -over * (1.0f - 1.0f / jmax(1.0f, para.ratioS));
            const float gk = targetDb < para.smoothDb ? para.gainAttack : para.gainRelease;
            para.smoothDb += gk * (targetDb - para.smoothDb);
        }
        const float paraGain = std::pow(10.0f, para.smoothDb * 0.05f);

        grMax = jmin(grMax, fet.smoothDb + opto.smoothDb);

        // 主路径 + 并行支链混合（amount=0 时 paraMix=0、两级增益均为 1 → 严格直通）
        for (size_t ch = 0; ch < chs; ++ch)
        {
            const float in = data[ch][n];
            data[ch][n] = (in * mainGain * (1.0f - paraMix) + in * paraGain * paraMix) * makeupGain;
        }
    }

    gainReduction.store(grMax);
}
