#pragma once

#include <JuceHeader.h>

// 双频段免扫频去齿音：
//   模块A 3k~5k，模块B 5k+ —— 各自动锁定齿音频点（候选窄带比能量）
//   动态 peaking 衰减：齿音频段相对全带能量超阈值才削，快 attack 慢 release，零染色
class VoiceDeEsser final
{
public:
    VoiceDeEsser(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(const juce::dsp::ProcessContextReplacing<float>& context);

    // GR 电平表用（audio thread 写，UI 线程读；两频段削减量之和）
    std::atomic<float> gainReduction { 0.0f };

private:
    struct Band
    {
        void prepare(const juce::dsp::ProcessSpec& spec, float fs,
                     const juce::dsp::IIR::Coefficients<float>::Ptr& cutCoeffs);
        void reset();

        // 智能锁频（候选窄带 + 能量平均）；系数预分配，每块按当前 OS 率重写
        juce::dsp::IIR::Filter<float> candidates[3];
        juce::dsp::IIR::Coefficients<float>::Ptr candidateCoeffs[3];
        float candidateEma[3] {};
        float detectAlpha = 0.05f;
        float targetFreq = 4000.0f;
        juce::SmoothedValue<float> freqSmooth;

        // 齿音检测（锁定中心处带通 + 全带包络）
        juce::dsp::IIR::Filter<float> sibFilter;      // 单通道检测
        juce::AudioBuffer<float> sibBuffer;
        float sibEnv = 0.0f, fullEnv = 0.0f;
        float sibAttack = 0.01f, sibRelease = 0.01f;

        // 削减（每通道一个动态 peaking，共享系数）
        juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
        std::vector<juce::dsp::IIR::Filter<float>> cutFilters;
        float gainDb = 0.0f;   // 平滑后的削减量（负值）
        float gainAttack = 0.01f, gainRelease = 0.01f;

        float baseFreq = 4000.0f;   // 候选中心（模块差异）
        bool isHighBand = false;
    };

    void detectSmartFrequencies(Band& band, const juce::dsp::AudioBlock<const float>& block);
    void updateCoefficients(Band& band, int numSamples);

    std::atomic<float>* lowAmountParam;
    std::atomic<float>* highAmountParam;

    std::atomic<double>* dspRate;
    double sampleRate = 48000.0;

    Band bands[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceDeEsser)
};
