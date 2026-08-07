#include "VoiceDeEsser.h"
#include "../ParamIDs.h"

namespace
{
    // 候选齿音频点：模块A 3k~5k，模块B 5k+
    constexpr float lowCandidates[3]  { 3200.0f, 4000.0f, 4800.0f };
    constexpr float highCandidates[3] { 5500.0f, 7500.0f, 10000.0f };

    // 检测包络/增益平滑系数（时间常数 ms → per-sample k）
    float timeToCoeff(float ms, double sampleRate)
    {
        return 1.0f - std::exp(-1.0f / (ms * 0.001 * sampleRate));
    }
}

VoiceDeEsser::VoiceDeEsser(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate)
    : lowAmountParam (apvts.getRawParameterValue(ParamID::dsLowAmount)),
      highAmountParam(apvts.getRawParameterValue(ParamID::dsHighAmount)),
      dspRate(&osSampleRate)
{
    bands[0].baseFreq = 4000.0f;
    bands[0].isHighBand = false;
    bands[0].targetFreq = 4000.0f;
    bands[1].baseFreq = 7500.0f;
    bands[1].isHighBand = true;
    bands[1].targetFreq = 7500.0f;
}

void VoiceDeEsser::Band::prepare(const juce::dsp::ProcessSpec& spec, float fs,
                                 const juce::dsp::IIR::Coefficients<float>::Ptr& cutCoeffs)
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    coeffs = cutCoeffs;

    // 智能锁频候选带通（Q=6 窄带，仅检测）；fs 必须传 OS 采样率（模块在超采样链内运行）
    const float* cands = isHighBand ? highCandidates : lowCandidates;
    for (int i = 0; i < 3; ++i)
    {
        candidateCoeffs[i] = Coeffs::makeBandPass(fs, cands[i], 6.0f);
        candidates[i].prepare(spec);
        candidates[i].coefficients = candidateCoeffs[i];
    }
    std::fill(std::begin(candidateEma), std::end(candidateEma), 0.0f);
    // 时间常数 ~50ms，按块更新（勿乘 blockSize——否则收敛慢 512 倍）
    detectAlpha = 1.0f - (float) std::exp(-((double) spec.maximumBlockSize / fs) / 0.05);

    // 齿音检测带通（固定 baseFreq，Q=1.5 覆盖齿音频段；频点锁定只作用于削减 peaking）
    sibFilter.prepare(spec);
    sibFilter.coefficients = Coeffs::makeBandPass(fs, baseFreq, 1.5f);
    // 检测在超采样链内，block 最多膨胀 16 倍
    sibBuffer.setSize(1, (int) spec.maximumBlockSize * 16, false, false, true);

    freqSmooth.reset(fs, 0.25);
    freqSmooth.setCurrentAndTargetValue(targetFreq);

    // 削减滤波器：每通道一个
    cutFilters.clear();
    for (size_t ch = 0; ch < spec.numChannels; ++ch)
    {
        cutFilters.emplace_back(coeffs);
        cutFilters.back().prepare(spec);
    }
}

void VoiceDeEsser::Band::reset()
{
    for (auto& f : candidates) f.reset();
    for (auto& f : cutFilters) f.reset();
    sibFilter.reset();
    sibEnv = 0.0f;
    fullEnv = 0.0f;
    gainDb = 0.0f;
}

void VoiceDeEsser::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    const float fs = (float) dspRate->load();
    for (int i = 0; i < 2; ++i)
    {
        bands[i].coeffs = Coeffs::makePeakFilter(fs, bands[i].targetFreq, 2.0f, 1.0f);
        bands[i].prepare(spec, fs, bands[i].coeffs);

        // 时间常数：齿音瞬态快 attack 慢 release；增益平滑同（用 OS 率，模块在超采样链内）
        const float atk = bands[i].isHighBand ? 0.4f : 0.8f;
        const float rel = 120.0f;
        bands[i].sibAttack  = timeToCoeff(atk, fs);
        bands[i].sibRelease = timeToCoeff(rel, fs);
        bands[i].gainAttack  = timeToCoeff(atk, fs);
        bands[i].gainRelease = timeToCoeff(rel, fs);
    }

    reset();
}

void VoiceDeEsser::reset()
{
    for (auto& b : bands)
        b.reset();
}

void VoiceDeEsser::detectSmartFrequencies(Band& band, const juce::dsp::AudioBlock<const float>& block)
{
    auto monoBlock = block.getSingleChannelBlock(0);
    const auto numSamples = (int) monoBlock.getNumSamples();
    const float* cands = band.isHighBand ? highCandidates : lowCandidates;

    const float fs = (float) dspRate->load();
    // alpha 按当前块实际时长实时算（OS 倍率热切换后仍保持 ~50ms 收敛）
    const float alpha = 1.0f - std::exp(-((double) numSamples / fs) / 0.05);

    // 候选带通系数每块按当前 OS 率重写（RBJ bandpass，Q=6）
    for (int i = 0; i < 3; ++i)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * jmax((double) cands[i], 2.0) / fs;
        const double a = std::sin(w0) / (6.0 * 2.0);
        const double c2 = -2.0 * std::cos(w0);
        const double invA0 = 1.0 / (1.0 + a);
        auto* c = band.candidateCoeffs[i]->getRawCoefficients();
        c[0] = (float) (a * invA0);
        c[1] = 0.0f;
        c[2] = (float) (-a * invA0);
        c[3] = (float) (c2 * invA0);
        c[4] = (float) ((1.0 - a) * invA0);
    }

    for (int i = 0; i < 3; ++i)
    {
        juce::dsp::AudioBlock<float> outBlock(band.sibBuffer);
        band.candidates[i].process(juce::dsp::ProcessContextNonReplacing<float>(monoBlock, outBlock));
        const float* d = band.sibBuffer.getReadPointer(0);
        float energy = 0.0f;
        for (int n = 0; n < numSamples; ++n)
            energy += d[n] * d[n];
        band.candidateEma[i] += alpha * (energy / (float) numSamples - band.candidateEma[i]);
    }

    int best = 0;
    for (int i = 1; i < 3; ++i)
        if (band.candidateEma[i] > band.candidateEma[best])
            best = i;
    band.freqSmooth.setTargetValue(cands[best]);
}

void VoiceDeEsser::updateCoefficients(Band& band, int numSamples)
{
    const double fs = dspRate->load();
    const float freq = band.freqSmooth.skip(numSamples);
    auto* c = band.coeffs->getRawCoefficients();

    // 动态 peaking：增益 0dB ~ -24dB，Q=2（与 JUCE makePeakFilter 同公式）
    const double A = std::sqrt(juce::Decibels::gainWithLowerBound(juce::Decibels::decibelsToGain(band.gainDb), -60.0f));
    const double omega = 2.0 * juce::MathConstants<double>::pi * jmax((double) freq, 2.0) / fs;
    const double alpha = std::sin(omega) / (2.0 * 2.0);
    const double c2 = -2.0 * std::cos(omega);
    const double aA = alpha * A;
    const double aOA = alpha / A;
    const double invA0 = 1.0 / (1.0 + aOA);
    c[0] = (float) ((1.0 + aA) * invA0);
    c[1] = (float) (c2 * invA0);
    c[2] = (float) ((1.0 - aA) * invA0);
    c[3] = (float) (c2 * invA0);
    c[4] = (float) ((1.0 - aOA) * invA0);
}

void VoiceDeEsser::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& outputBlock = context.getOutputBlock();


    if (outputBlock.getNumSamples() == 0)
        return;

    const auto numSamples = (int) outputBlock.getNumSamples();
    const auto numChannels = (int) outputBlock.getNumChannels();
    const int chs = jmin(numChannels, 2);

    for (int b = 0; b < 2; ++b)
    {
        Band& band = bands[b];
        const float amount = (b == 0 ? lowAmountParam : highAmountParam)->load() / 100.0f;
        if (amount <= 0.0f)
        {
            band.gainDb = 0.0f;
            band.sibEnv = band.fullEnv = 0.0f;
            continue;
        }

        detectSmartFrequencies(band, outputBlock);

        // 齿音检测：锁定 baseFreq 带通输出 + 全带峰值包络（stereo linked 检测用左声道）
        auto monoView = outputBlock.getSingleChannelBlock(0);
        juce::dsp::AudioBlock<float> sibOut(band.sibBuffer);
        band.sibFilter.process(juce::dsp::ProcessContextNonReplacing<float>(monoView, sibOut));
        const float* sibData = band.sibBuffer.getReadPointer(0);
        const float* inData = monoView.getChannelPointer(0);

        const float maxCut = -24.0f * amount;
        float* chans[2] { nullptr, nullptr };
        for (int ch = 0; ch < chs; ++ch)
            chans[ch] = outputBlock.getChannelPointer((size_t) ch);

        for (int n = 0; n < numSamples; ++n)
        {
            // 包络（检测路径，单通道）
            const float sib = std::abs(sibData[n]);
            const float full = std::abs(inData[n]);
            band.sibEnv  += (sib  > band.sibEnv  ? band.sibAttack  : band.sibRelease) * (sib  - band.sibEnv);
            band.fullEnv += (full > band.fullEnv ? band.sibAttack  : band.sibRelease) * (full - band.fullEnv);

            // 齿音特征 = 齿音频段相对全带突出（相对阈值，自适应输入电平）
            const float rel = 20.0f * std::log10((float) band.sibEnv + 1.0e-6f)
                            - 20.0f * std::log10((float) band.fullEnv + 1.0e-6f);
            const float over = rel - (-25.0f);
            const float targetDb = over > 0.0f ? jmax(maxCut, -over * 1.2f) : 0.0f;

            // 增益平滑（快 attack 慢 release）
            const float gk = targetDb < band.gainDb ? band.gainAttack : band.gainRelease;
            band.gainDb += gk * (targetDb - band.gainDb);

            // 每 32 样本重算 peaking 系数（零分配）
            if ((n & 31) == 0)
                updateCoefficients(band, 32);

            // 应用削减（所有通道）
            for (int ch = 0; ch < chs; ++ch)
                chans[ch][n] = band.cutFilters[ch].processSample(chans[ch][n]);
        }
    }

    // GR 表（两频段削减量之和，负值 dB）
    gainReduction.store(bands[0].gainDb + bands[1].gainDb);
}
