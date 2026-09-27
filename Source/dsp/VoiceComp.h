#pragma once

#include <JuceHeader.h>

// 智能三级压缩（大师链结构）：
//   Stage 1 FET 峰值层（1176 风）  —— 快 attack 高 ratio 硬拐点，只抓字头过冲
//   Stage 2 光电平滑层（LA-2A 风） —— 慢 attack 2:1 软拐点 + 程序依赖释放，做"胶水"
//   Stage 3 并行密度层（New York） —— 重压支链与主信号混合，增密度不损瞬态
//
// 侧链高通 100Hz：只作用于检测路径（去低频触发 → 不抽气），不改音频路径
// 智能（保留 crest）：crest 峰值因子（300ms EMA）驱动 Stage 1 的 attack/ratio；
//                     短时瞬态检测（5ms vs 300ms EMA）让 attack 更快响应字头；
//                     程序依赖释放让压得深的段落释放更慢
class VoiceComp final
{
public:
    VoiceComp(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(const juce::dsp::ProcessContextReplacing<float>& context);

    // GR 电平表用（audio thread 写，UI 线程读）
    std::atomic<float> gainReduction { 0.0f };

    // 智能参数显示（UI/测试读取）
    std::atomic<float> crestDbDisplay { 3.0f };
    std::atomic<float> fastAttackMsDisplay { 3.0f };
    std::atomic<float> fastRatioDisplay { 3.5f };
    std::atomic<float> optoReleaseMsDisplay { 300.0f };

private:
    struct Stage
    {
        void prepare(double sr);
        void reset();
        // 智能参数目标（由 VoiceComp 每块更新）
        float attackMs = 1.0f, releaseMs = 150.0f, ratio = 4.0f, thresholdDb = -20.0f;
        // 平滑值（模式/参数切换无 click）
        float attackMsS = 1.0f, releaseMsS = 150.0f, ratioS = 4.0f;
        bool softKnee = false;
        // 状态
        double env = 0.0;
        float smoothDb = 0.0f;
        float envAttack = 0.01f, envRelease = 0.01f;
        float gainAttack = 0.01f, gainRelease = 0.01f;
    };

    void updateSmartParams(int numSamples);

    std::atomic<float>* modeParam;
    std::atomic<float>* amountParam;
    std::atomic<float>* makeupParam;
    std::atomic<float>* bypassParam;
    float bypassMix = 1.0f; // 1=正常 0=旁通（块级平滑，无 click）

    std::atomic<double>* dspRate; // OS 采样率（模块在超采样链内运行）
    double sampleRate = 48000.0;

    Stage fet, opto, para;

    // 侧链高通（每通道一个 Filter；系数预分配，每块按当前 dspRate 重写）
    juce::dsp::IIR::Filter<float> scFilter[2];
    juce::dsp::IIR::Coefficients<float>::Ptr scCoeffs;

    // 智能特征检测
    float peakEma = 0.0f, rmsEma = 0.0f, fastPeakEma = 0.0f;
    float crestDb = 3.0f;
    float transientIndex = 0.0f; // 短时/长时峰值比 → 0~1
    int mode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceComp)
};
