#include "VoiceEq.h"
#include "../ParamIDs.h"

namespace
{
    constexpr float kSmoothTime = 0.05f;     // 增益平滑时间（秒）
    constexpr float kFreqSmoothTime = 0.5f; // 智能频点平滑时间（秒）：0.25→0.5 减缓频点漂移

    // 1/4 倍频程对数间隔候选：200~800Hz（盒声/共振/鼻音区）、2k~8k（刺耳/齿音区）
    constexpr float deboxCandidates[13]   { 200.0f, 224.0f, 250.0f, 280.0f, 315.0f, 355.0f, 400.0f,
                                            450.0f, 500.0f, 560.0f, 630.0f, 710.0f, 800.0f };
    constexpr float clarityCandidates[13] { 2000.0f, 2240.0f, 2500.0f, 2800.0f, 3150.0f, 3550.0f, 4000.0f,
                                            4500.0f, 5000.0f, 5600.0f, 6300.0f, 7100.0f, 8000.0f };
    // 基频区候选（男声 ~100Hz，女声 ~200Hz，1/3 倍频程）
    constexpr float thickCandidates[7] { 80.0f, 100.0f, 125.0f, 160.0f, 200.0f, 250.0f, 315.0f };
    // 共振对比度阈值：高于它才算脏点；干净时用默认中心宽 Q 温和处理
    constexpr float kResonanceThreshold = 2.0f;
    // 对比度 → Q 映射（2.0 → 0.9 宽，5.0+ → 3.5 窄）
    constexpr float kQMin = 0.9f, kQMax = 3.5f;
    // 最多同时处理的峰数
    constexpr int kMaxPeaks = 3;
    // 峰间最小间距（带数，避免双峰过近）
    constexpr int kPeakSpacing = 3;

    // 与 juce::dsp::IIR::ArrayCoefficients 同公式（避免每块生成新 Ptr 的堆分配）
    double shelfAmp(double gainDb)
    {
        return std::sqrt(juce::Decibels::gainWithLowerBound(juce::Decibels::decibelsToGain((float) gainDb), -60.0f));
    }
}

VoiceEq::VoiceEq(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate)
    : boostParam    (apvts.getRawParameterValue(ParamID::eqLowBoost)),
      deboxParam    (apvts.getRawParameterValue(ParamID::eqDeboxCut)),
      clarityParam  (apvts.getRawParameterValue(ParamID::eqClarityBoost)),
      airParam      (apvts.getRawParameterValue(ParamID::eqAirBoost)),
      airFreqParam  (apvts.getRawParameterValue(ParamID::eqAirFreq)),
      dspRate(&osSampleRate)
{
}

void VoiceEq::SmartBand::prepare(const juce::dsp::ProcessSpec& spec, const float* cands,
                                 float fs, float defaultFreqValue)
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    defaultFreq = defaultFreqValue;
    candidates = cands;

    // 检测带通：Q=8 窄带（分辨率高才能分辨共振峰）；系数预分配，每块按当前 OS 率重写
    for (int i = 0; i < 13; ++i)
    {
        detectCoeffs[i] = Coeffs::makeBandPass(fs, cands[i], 8.0f);
        detectors[i].prepare(spec);
        detectors[i].coefficients = detectCoeffs[i];
    }
    std::fill(std::begin(ema), std::end(ema), 0.0f);
    // 清峰锁定状态与活跃标记：prepare 每次 transport/OS 热切换重启都执行
    // （reset() 在本插件运行路径上无调用方，清理必须落在这里才生效）
    lockedIndex = -1;
    lockedBaseline = 0.0f;
    unlockTimer = 0;
    std::fill(std::begin(targetContrast), std::end(targetContrast), 0.0f);
    std::fill(std::begin(active), std::end(active), false);
    // 检测器能量平均：时间常数 ~120ms，按块更新（blockDur 秒/块，用 OS 采样率）
    alpha = 1.0f - (float) std::exp(-((double) spec.maximumBlockSize / fs) / 0.12);

    // 3 路并行 peaking：预分配系数 + 每通道 filter 实例
    for (int p = 0; p < kMaxPeaks; ++p)
    {
        coeffs[p] = Coeffs::makePeakFilter(fs, defaultFreq, 0.9f, 1.0f);
        freqSmooth[p].reset(fs, kFreqSmoothTime);
        freqSmooth[p].setCurrentAndTargetValue(defaultFreq);
        qSmooth[p].reset(fs, kFreqSmoothTime);
        qSmooth[p].setCurrentAndTargetValue(0.9f);

        filters[p].clear();
        for (size_t ch = 0; ch < spec.numChannels; ++ch)
        {
            filters[p].emplace_back(coeffs[p]);
            filters[p].back().prepare(spec);
        }
    }
}

void VoiceEq::SmartBand::reset()
{
    for (auto& f : detectors)
        f.reset();
    for (int p = 0; p < kMaxPeaks; ++p)
        for (auto& f : filters[p])
            f.reset();
    std::fill(std::begin(ema), std::end(ema), 0.0f);
    // 清峰锁定状态与活跃标记，避免 prepare/transport 重启后残留上一段锁
    lockedIndex = -1;
    lockedBaseline = 0.0f;
    unlockTimer = 0;
    std::fill(std::begin(targetContrast), std::end(targetContrast), 0.0f);
    std::fill(std::begin(active), std::end(active), false);
}

void VoiceEq::SmartBand::runDetectors(const juce::dsp::AudioBlock<const float>& monoBlock,
                                      juce::AudioBuffer<float>& detectBuffer, float fs)
{
    auto* det = detectBuffer.getWritePointer(0);
    const auto numSamples = (int) monoBlock.getNumSamples();

    // alpha 按当前块实际时长实时算（OS 倍率热切换后仍保持 ~120ms 收敛）
    const float alpha = 1.0f - std::exp(-((double) numSamples / fs) / 0.12);

    // 检测带通系数每块按当前 OS 率重写（热切换后频率仍正确）
    for (int i = 0; i < 13; ++i)
        VoiceEq::writeBandPass(detectCoeffs[i]->getRawCoefficients(), fs, candidates[i], 8.0f);

    for (int i = 0; i < 13; ++i)
    {
        juce::dsp::AudioBlock<float> outBlock(detectBuffer);
        detectors[i].process(juce::dsp::ProcessContextNonReplacing<float>(monoBlock, outBlock));

        float energy = 0.0f;
        for (int n = 0; n < numSamples; ++n)
            energy += det[n] * det[n];
        ema[i] += alpha * (energy / (float) numSamples - ema[i]);
    }
}

void VoiceEq::SmartBand::detect(const float* cands)
{
    // 底噪检查：无信号 → 全直通 + 解锁
    float maxEma = 0.0f;
    for (int i = 2; i <= 10; ++i)
        maxEma = jmax(maxEma, ema[i]);
    if (maxEma < 1.0e-8f)
    {
        for (int p = 0; p < kMaxPeaks; ++p)
            active[p] = false;
        lockedIndex = -1;
        unlockTimer = 0;
        return;
    }

    // 候选峰：局部最大 + 对比度超阈值
    struct Peak { int index; float contrast; };
    Peak peaks[13];
    int numPeaks = 0;
    for (int i = 2; i <= 10; ++i)
    {
        if (ema[i] <= ema[i - 1] || ema[i] <= ema[i + 1])
            continue;
        const float neigh = (ema[i - 2] + ema[i - 1] + ema[i + 1] + ema[i + 2]) * 0.25f;
        const float contrast = ema[i] / (neigh + 1.0e-12f);
        if (contrast > kResonanceThreshold)
            peaks[numPeaks++] = { i, contrast };
    }

    // 按对比度排序（插入排序，量小）
    for (int a = 1; a < numPeaks; ++a)
    {
        const Peak key = peaks[a];
        int b = a - 1;
        while (b >= 0 && peaks[b].contrast < key.contrast)
        {
            peaks[b + 1] = peaks[b];
            --b;
        }
        peaks[b + 1] = key;
    }

    // ---- 峰锁定：锁定峰（若仍强）强制占 active[0]，仅邻域滑动 ----
    bool lockHeld = false;
    int pickedIdx[kMaxPeaks] { -1, -1, -1 };
    if (lockedIndex >= 0)
    {
        int li = lockedIndex;
        float le = ema[li];
        for (int i = jmax(2, lockedIndex - 1); i <= jmin(10, lockedIndex + 1); ++i)
            if (ema[i] > le) { le = ema[i]; li = i; }

        if (le >= lockedBaseline * 0.5f)   // 未衰减 >6dB → 保持锁定
        {
            unlockTimer = 0;
            lockedIndex = li;              // 允许 ±1 带内滑动
            lockedBaseline = le;           // 滚动基线
            // 邻域抛物线插值（不超出 ±1 带，杜绝跳变）
            const float l = std::log(ema[li - 1]), c = std::log(ema[li]), r = std::log(ema[li + 1]);
            const float denom = l - 2.0f * c + r;
            float d = (denom > 1.0e-9f) ? 0.5f * (l - r) / denom : 0.0f;
            d = jlimit(-0.5f, 0.5f, d);
            targetFreq[0] = cands[li] * std::exp(d * std::log(cands[li + 1] / cands[li]));
            targetQ[0] = 0.9f;                                   // 锁定期 Q 温和固定
            targetContrast[0] = le / ((ema[li - 1] + ema[li + 1]) * 0.5f + 1.0e-12f);
            active[0] = true;
            pickedIdx[0] = li;
            lockHeld = true;
        }
        else if (++unlockTimer < 8)
        {
            return;                        // 确认期内保持上一帧输出，频点不跳
        }
        else
        {
            lockedIndex = -1;              // 衰减确认 → 解锁，重新全局检测
            unlockTimer = 0;
        }
    }

    // ---- 填充其余路：候选峰按对比度选，间距去重（含锁定峰） ----
    int picked = lockHeld ? 1 : 0;
    for (int a = 0; a < numPeaks && picked < kMaxPeaks; ++a)
    {
        const int i = peaks[a].index;
        bool tooClose = false;
        for (int p = 0; p < picked; ++p)
            if (std::abs(i - pickedIdx[p]) < kPeakSpacing)
            {
                tooClose = true;
                break;
            }
        if (tooClose)
            continue;

        // 对数域抛物线插值精确定位
        const float l = std::log(ema[i - 1]), c = std::log(ema[i]), r = std::log(ema[i + 1]);
        const float denom = l - 2.0f * c + r;
        float d = (denom > 1.0e-9f) ? 0.5f * (l - r) / denom : 0.0f;
        d = jlimit(-0.5f, 0.5f, d);
        targetFreq[picked] = cands[i] * std::exp(d * std::log(cands[i + 1] / cands[i]));
        targetQ[picked] = juce::jmap(peaks[a].contrast, kResonanceThreshold, 5.0f, kQMin, kQMax);
        targetContrast[picked] = peaks[a].contrast;
        active[picked] = true;
        pickedIdx[picked] = i;
        ++picked;
    }

    // 未锁定时：把对比度最高的候选峰设为新锁定峰
    if (! lockHeld && picked > 0)
    {
        lockedIndex = pickedIdx[0];
        lockedBaseline = ema[lockedIndex];
        unlockTimer = 0;
    }

    // 未使用的路 → 直通
    for (int p = picked; p < kMaxPeaks; ++p)
        active[p] = false;
}

void VoiceEq::SmartBand::updateCoeffs(int numSamples, float gainDb, double fs)
{
    for (int p = 0; p < kMaxPeaks; ++p)
    {
        if (! active[p])
        {
            // 直通系数（0dB peak）：[1,0,0,0,0]
            auto* c = coeffs[p]->getRawCoefficients();
            c[0] = 1.0f; c[1] = 0.0f; c[2] = 0.0f; c[3] = 0.0f; c[4] = 0.0f;
            continue;
        }
        const float freq = freqSmooth[p].skip(numSamples);
        const float q = qSmooth[p].skip(numSamples);
        // 限增益：刺耳峰少提升；削减（负增益）不受限（共振越扎眼越该削）
        const float cap = 1.0f - 0.5f * jlimit(0.0f, 1.0f, (targetContrast[p] - kResonanceThreshold) / 3.0f);
        const float g = gainDb > 0.0f ? gainDb * cap : gainDb;
        VoiceEq::writePeak(coeffs[p]->getRawCoefficients(), fs, freq, q, g);
    }
}

void VoiceEq::prepare(const juce::dsp::ProcessSpec& spec)
{
    const double fs = dspRate->load();

    // 一次性创建系数对象（makeXXX 自带系数数组，之后只重写 raw 数组内容，零分配）
    lowShelfCoeffs  = Coeffs::makeLowShelf (fs, 120.0, 0.71, 1.0f);
    airCoeffs       = Coeffs::makeHighShelf (fs, 13000.0, 0.71, 1.0f);

    // 每通道建一个 filter 实例，共享同一份系数（prepare 内分配，RT 安全）
    auto buildFilters = [&spec](std::vector<Filter>& v, const Coeffs::Ptr& c)
    {
        v.clear();
        for (size_t ch = 0; ch < spec.numChannels; ++ch)
        {
            v.emplace_back(c);
            v.back().prepare(spec);
        }
    };
    buildFilters(lowShelfFilters, lowShelfCoeffs);
    buildFilters(airFilters,      airCoeffs);

    // 多峰智能段
    deboxBand.prepare(spec, deboxCandidates, (float) fs, 400.0f);
    clarityBand.prepare(spec, clarityCandidates, (float) fs, 4000.0f);

    // Thick 基频检测器（Q=4 宽带，覆盖基频带宽）；系数预分配，每块按当前 OS 率重写
    for (int i = 0; i < 7; ++i)
    {
        thickCoeffs[i] = Coeffs::makeBandPass(fs, thickCandidates[i], 4.0f);
        thickDetectors[i].prepare(spec);
        thickDetectors[i].coefficients = thickCoeffs[i];
    }
    std::fill(std::begin(thickEma), std::end(thickEma), 0.0f);
    thickAlpha = 1.0f - (float) std::exp(-((double) spec.maximumBlockSize / fs) / 0.05);

    detectBuffer.setSize(1, (int) spec.maximumBlockSize * 16, false, false, true);

    auto setup = [&spec](auto& s, float init, double t)
    {
        s.reset(spec.sampleRate, t);
        s.setCurrentAndTargetValue(init);
    };
    setup(boostSmooth,      boostParam->load(),      kSmoothTime);
    setup(deboxSmooth,      deboxParam->load(),      kSmoothTime);
    setup(claritySmooth,    clarityParam->load(),    kSmoothTime);
    setup(airSmooth,        airParam->load(),        kSmoothTime);
    setup(thickFreqSmooth,  thickTargetFreq,         kFreqSmoothTime);
    setup(airFreqSmooth,    airTargetFreq,           0.1f);

    updateCoefficients((int) spec.maximumBlockSize);
}

void VoiceEq::reset()
{
    for (auto& f : lowShelfFilters) f.reset();
    for (auto& f : airFilters)     f.reset();
    deboxBand.reset();
    clarityBand.reset();
    for (auto& f : thickDetectors) f.reset();
    std::fill(std::begin(thickEma), std::end(thickEma), 0.0f);
    std::fill(std::begin(deboxFreqDisplay), std::end(deboxFreqDisplay), 0.0f);
    std::fill(std::begin(clarityFreqDisplay), std::end(clarityFreqDisplay), 0.0f);
    deboxFreqDisplay[0].store(400.0f);
    clarityFreqDisplay[0].store(4000.0f);
}

void VoiceEq::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    const auto& inputBlock = context.getInputBlock();
    auto& outputBlock = context.getOutputBlock();


    if (inputBlock.getNumSamples() == 0)
        return;

    detectSmartFrequencies(inputBlock);

    // 每块更新参数目标
    boostSmooth.setTargetValue(boostParam->load());
    deboxSmooth.setTargetValue(deboxParam->load());
    claritySmooth.setTargetValue(clarityParam->load());
    airSmooth.setTargetValue(airParam->load());

    const float freq = airFreqParam->load() > 0.5f ? 22000.0f : 13000.0f;
    airFreqSmooth.setTargetValue(freq);

    updateCoefficients((int) outputBlock.getNumSamples());

    // 每通道串行处理各段（就地）
    const auto numChannels = outputBlock.getNumChannels();
    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        auto monoView = outputBlock.getSingleChannelBlock(ch);
        auto monoCtx = juce::dsp::ProcessContextReplacing<float>(monoView);

        lowShelfFilters[ch].process(monoCtx);
        for (int p = 0; p < kMaxPeaks; ++p)
            deboxBand.filters[p][ch].process(monoCtx);
        for (int p = 0; p < kMaxPeaks; ++p)
            clarityBand.filters[p][ch].process(monoCtx);
        airFilters[ch].process(monoCtx);
    }
}

void VoiceEq::detectSmartFrequencies(const juce::dsp::AudioBlock<const float>& block)
{
    // 仅用左声道检测（人声素材左右通常一致，省一半开销）
    auto monoBlock = block.getSingleChannelBlock(0);

    const float fs = (float) dspRate->load();
    deboxBand.runDetectors(monoBlock, detectBuffer, fs);
    clarityBand.runDetectors(monoBlock, detectBuffer, fs);

    // Thick 基频检测：基频区能量最大处（基频天然能量高，直接取峰值带）
    {
        auto* det = detectBuffer.getWritePointer(0);
        const auto numSamples = (int) monoBlock.getNumSamples();
        const float fs = (float) dspRate->load();
        const float alpha = 1.0f - std::exp(-((double) numSamples / fs) / 0.05);
        for (int i = 0; i < 7; ++i)
            writeBandPass(thickCoeffs[i]->getRawCoefficients(), fs, thickCandidates[i], 4.0f);
        for (int i = 0; i < 7; ++i)
        {
            juce::dsp::AudioBlock<float> outBlock(detectBuffer);
            thickDetectors[i].process(juce::dsp::ProcessContextNonReplacing<float>(monoBlock, outBlock));
            float energy = 0.0f;
            for (int n = 0; n < numSamples; ++n)
                energy += det[n] * det[n];
            thickEma[i] += alpha * (energy / (float) numSamples - thickEma[i]);
        }
        int best = 0;
        for (int i = 1; i < 7; ++i)
            if (thickEma[i] > thickEma[best])
                best = i;
        // 低频搁架提升区在中心之下 → 中心设在基频 1.25 倍（基频落在提升区拿满增益）
        thickTargetFreq = jmin(400.0f, thickCandidates[best] * 1.25f);
        thickFreqSmooth.setTargetValue(thickTargetFreq);
        thickFreqDisplay.store(thickTargetFreq);
    }

    deboxBand.detect(deboxCandidates);
    clarityBand.detect(clarityCandidates);

    // 平滑目标 + UI 显示
    for (int p = 0; p < kMaxPeaks; ++p)
    {
        if (deboxBand.active[p])
        {
            deboxBand.freqSmooth[p].setTargetValue(deboxBand.targetFreq[p]);
            deboxBand.qSmooth[p].setTargetValue(deboxBand.targetQ[p]);
            deboxFreqDisplay[p].store(deboxBand.targetFreq[p]);
        }
        else
        {
            deboxFreqDisplay[p].store(0.0f);
        }

        if (clarityBand.active[p])
        {
            clarityBand.freqSmooth[p].setTargetValue(clarityBand.targetFreq[p]);
            clarityBand.qSmooth[p].setTargetValue(clarityBand.targetQ[p]);
            clarityFreqDisplay[p].store(clarityBand.targetFreq[p]);
        }
        else
        {
            clarityFreqDisplay[p].store(0.0f);
        }
    }
}

void VoiceEq::updateCoefficients(int numSamples)
{
    const float boostGain   = boostSmooth.skip(numSamples);
    const float deboxGain   = deboxSmooth.skip(numSamples);
    const float clarityGain = claritySmooth.skip(numSamples);
    const float airGain     = airSmooth.skip(numSamples);
    const float thickFreq   = thickFreqSmooth.skip(numSamples);
    const float airFreq     = airFreqSmooth.skip(numSamples);

    // 有效采样率 = 宿主采样率 × 当前超采样倍率（由处理器原子更新）
    const double fs = dspRate->load();

    // Thick：搁架中心跟随智能锁定的基频（男声/女声自动适配）
    writeShelf(lowShelfCoeffs->getRawCoefficients(), fs, thickFreq, 0.71, boostGain, true);
    deboxBand.updateCoeffs(numSamples, deboxGain, fs);
    clarityBand.updateCoeffs(numSamples, clarityGain, fs);
    writeShelf(airCoeffs->getRawCoefficients(), fs, airFreq, 0.71, airGain, false);
}

void VoiceEq::debugGetCoeffs(int type, float f, float q, float gainDb, float fs, float out[5])
{
    switch (type)
    {
        case 0: writeShelf(out, fs, f, q, gainDb, true);  break;
        case 1: writeShelf(out, fs, f, q, gainDb, false); break;
        case 2: writePeak(out, fs, f, q, gainDb);         break;
        default: writeBandPass(out, fs, f, q);            break;
    }
}

void VoiceEq::writeShelf(float* c, double fs, double f, double Q, double gainDb, bool low)
{
    const double A = shelfAmp(gainDb);
    const double aminus1 = A - 1.0;
    const double aplus1 = A + 1.0;
    const double omega = 2.0 * juce::MathConstants<double>::pi * jmax(f, 2.0) / fs;
    const double coso = std::cos(omega);
    const double beta = std::sin(omega) * std::sqrt(A) / Q;
    const double a1mc = aminus1 * coso;

    double b0, b1, b2, a0, a1, a2;
    if (low)
    {
        b0 = A * (aplus1 - a1mc + beta);
        b1 = A * 2.0 * (aminus1 - aplus1 * coso);
        b2 = A * (aplus1 - a1mc - beta);
        a0 = aplus1 + a1mc + beta;
        a1 = -2.0 * (aminus1 + aplus1 * coso);
        a2 = aplus1 + a1mc - beta;
    }
    else
    {
        b0 = A * (aplus1 + a1mc + beta);
        b1 = A * -2.0 * (aminus1 + aplus1 * coso);
        b2 = A * (aplus1 + a1mc - beta);
        a0 = aplus1 - a1mc + beta;
        a1 = 2.0 * (aminus1 - aplus1 * coso);
        a2 = aplus1 - a1mc - beta;
    }

    // raw 数组布局：[b0,b1,b2,a1,a2]，a0 归一化为 1
    const double invA0 = 1.0 / a0;
    c[0] = (float) (b0 * invA0);
    c[1] = (float) (b1 * invA0);
    c[2] = (float) (b2 * invA0);
    c[3] = (float) (a1 * invA0);
    c[4] = (float) (a2 * invA0);
}

void VoiceEq::writeBandPass(float* c, double fs, double f, double Q)
{
    // RBJ bandpass（constant 0dB peak），raw 数组布局：[b0,b1,b2,a1,a2]，a0 归一化为 1
    const double w0 = 2.0 * juce::MathConstants<double>::pi * jmax(f, 2.0) / fs;
    const double alpha = std::sin(w0) / (Q * 2.0);
    const double c2 = -2.0 * std::cos(w0);
    const double invA0 = 1.0 / (1.0 + alpha);
    c[0] = (float) (alpha * invA0);
    c[1] = 0.0f;
    c[2] = (float) (-alpha * invA0);
    c[3] = (float) (c2 * invA0);
    c[4] = (float) ((1.0 - alpha) * invA0);
}

void VoiceEq::writePeak(float* c, double fs, double f, double Q, double gainDb)
{
    const double A = shelfAmp(gainDb);
    const double omega = 2.0 * juce::MathConstants<double>::pi * jmax(f, 2.0) / fs;
    const double alpha = std::sin(omega) / (Q * 2.0);
    const double c2 = -2.0 * std::cos(omega);
    const double aA = alpha * A;
    const double aOA = alpha / A;

    // raw 数组布局：[b0,b1,b2,a1,a2]，a0 归一化为 1
    const double invA0 = 1.0 / (1.0 + aOA);
    c[0] = (float) ((1.0 + aA) * invA0);
    c[1] = (float) (c2 * invA0);
    c[2] = (float) ((1.0 - aA) * invA0);
    c[3] = (float) (c2 * invA0);
    c[4] = (float) ((1.0 - aOA) * invA0);
}
