#pragma once

#include <JuceHeader.h>

// 智能双层压缩（每组内 1176 风格快层 → LA-2A 风格平滑层串联）：
//   Fast 层：快 attack、高 ratio、硬拐点 —— 抓瞬态/过冲
//   Smooth 层：慢 attack、2:1、软拐点 —— 平滑节目电平
//   attack/release/ratio 由输入峰值因子（crest factor）智能自适应：
//   瞬态丰富 → attack 快、ratio 高（抓瞬态）；平滑 → attack 慢（保护音头）
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

private:
    struct Layer
    {
        void prepare(double sr);
        void reset();
        // 智能参数目标（由 VoiceComp 每块更新）
        float attackMs = 1.0f, releaseMs = 150.0f, ratio = 4.0f;
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

    double sampleRate = 48000.0;
    Layer fastLayer, smoothLayer;

    // 智能特征检测：峰值因子（crest = 20log10(peak/rms)），滑动 EMA ~1s
    float peakEma = 0.0f, rmsEma = 0.0f;
    float crestDb = 3.0f;
    float crestAlpha = 0.01f;

    int mode = -1; // 上次应用的模式
    float modeRatioBoost = 0.0f; // Rap 模式 ratio 基数 +1.5

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceComp)
};
