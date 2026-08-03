#pragma once

#include <JuceHeader.h>

// 棱角 <-> 圆滑（瞬态整形）：-100 圆滑（削起音），+100 棱角（抬起音）
// 包络上升沿检测瞬态：e 快速上涨 = 起音事件；稳态包络不变 = 不动作
class VoiceEdge final
{
public:
    VoiceEdge(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(const juce::dsp::ProcessContextReplacing<float>& context);

private:
    std::atomic<float>* amountParam;

    std::atomic<double>* dspRate; // OS 采样率（模块在超采样链内运行）
    double sampleRate = 48000.0;
    float envAttack = 0.01f, envRelease = 0.01f;   // 包络（~1ms / ~50ms）
    float gainSmoothCoeff = 0.01f;                 // 增益平滑（~1.5ms，降调制失真）
    float env[2] { 0.0f, 0.0f };                   // 包络状态（每通道）
    float gainSmooth[2] { 1.0f, 1.0f };            // 平滑后的增益（每通道）

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceEdge)
};
