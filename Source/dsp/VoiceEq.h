#pragma once

#include <JuceHeader.h>

// 四段智能 EQ（MÄAG EQ4 风格 + 多峰处理）：
//   段1 Thick 智能基频搁架 ±12dB  —— 加厚/削低频（自动锁基频）
//   段2 智能凹陷 200~800Hz ±12dB  —— 去盒子音，同时削减 2-3 个共振峰
//   段3 智能提升 2k~8kHz ±12dB    —— 清晰度，同时提升 2 个峰
//   段4 高频搁架 13k/22k 0~+12dB  —— 空气感
// 多峰检测：13 窄带（1/4 倍频程）对比度找局部峰，对数插值精确定位，无峰路直通
// 系数零分配：预分配 Coefficients 成员，每块原地重写 raw 数组
class VoiceEq final
{
public:
    VoiceEq(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(const juce::dsp::ProcessContextReplacing<float>& context);

    // 系数调试接口（测试用）：type 0=shelf-low 1=shelf-high 2=peak 3=bandpass
    static void debugGetCoeffs(int type, float f, float q, float gainDb, float fs, float out[5]);

    // 智能锁定的频点（UI 显示用，audio thread 写 / UI 读；最多 3 个，0 = 无）
    std::atomic<float> deboxFreqDisplay[3] { { 400.0f }, { 0.0f }, { 0.0f } };
    std::atomic<float> clarityFreqDisplay[3] { { 4000.0f }, { 0.0f }, { 0.0f } };
    std::atomic<float> thickFreqDisplay { 120.0f };

private:
    struct SmartBand
    {
        void prepare(const juce::dsp::ProcessSpec& spec, const float* cands, float fs, float defaultFreq);
        void reset();
        void runDetectors(const juce::dsp::AudioBlock<const float>& monoBlock,
                          juce::AudioBuffer<float>& detectBuffer, float fs);
        void detect(const float* cands);            // 多峰选择 → targetFreq/targetQ/active
        void updateCoeffs(int numSamples, float gainDb, double fs);

        // 检测器（13 窄带）；系数预分配，每块按当前 OS 率重写（热切换后仍正确）
        juce::dsp::IIR::Filter<float> detectors[13];
        juce::dsp::IIR::Coefficients<float>::Ptr detectCoeffs[13];
        float ema[13] {};
        float alpha = 0.05f;

        // 处理：最多 3 路并行 peaking（每路独立频点/Q）
        juce::dsp::IIR::Coefficients<float>::Ptr coeffs[3];
        std::vector<juce::dsp::IIR::Filter<float>> filters[3];
        juce::SmoothedValue<float> freqSmooth[3], qSmooth[3];
        float targetFreq[3] { 0.0f, 0.0f, 0.0f };
        float targetQ[3] { 0.9f, 0.9f, 0.9f };
        bool active[3] { false, false, false };
        float defaultFreq = 400.0f;
        // 峰锁定状态（消除频点扫动；-1=未锁定）
        int lockedIndex = -1;
        float lockedBaseline = 0.0f;
        int unlockTimer = 0;
        float targetContrast[3] { 0.0f, 0.0f, 0.0f }; // 各峰对比度（限增益用）
        const float* candidates = nullptr; // 候选频率（系数重写用）
    };

    void detectSmartFrequencies(const juce::dsp::AudioBlock<const float>& block);
    void updateCoefficients(int numSamples);
    static void writeShelf(float* c, double fs, double f, double Q, double gainDb, bool low);
    static void writePeak(float* c, double fs, double f, double Q, double gainDb);
    static void writeBandPass(float* c, double fs, double f, double Q);

    std::atomic<float>* boostParam;
    std::atomic<float>* deboxParam;
    std::atomic<float>* clarityParam;
    std::atomic<float>* airParam;
    std::atomic<float>* airFreqParam;

    juce::AudioBuffer<float> detectBuffer;

    SmartBand deboxBand, clarityBand;

    // Thick 智能基频检测（80~315Hz 能量峰 → 搁架中心跟随人声基频）
    juce::dsp::IIR::Filter<float> thickDetectors[7];
    juce::dsp::IIR::Coefficients<float>::Ptr thickCoeffs[7];
    float thickEma[7] {};
    float thickAlpha = 0.05f;
    float thickTargetFreq = 120.0f;
    juce::SmoothedValue<float> thickFreqSmooth;

    float airTargetFreq = 13000.0f;
    juce::SmoothedValue<float> airFreqSmooth;

    // 主滤波器（串行：低搁架 → 凹陷 → 提升 → 空气搁架），每通道一份手动实例
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    Coeffs::Ptr lowShelfCoeffs, airCoeffs; // 预分配，原地重写
    std::vector<Filter> lowShelfFilters, airFilters;
    juce::SmoothedValue<float> boostSmooth, deboxSmooth, claritySmooth, airSmooth;

    std::atomic<double>* dspRate; // 超采样后的有效采样率（处理器按 factor 更新）

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceEq)
};
