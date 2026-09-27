#pragma once

#include <JuceHeader.h>

// 棱角 <-> 圆滑（瞬态整形）：-100 圆滑（压扁起音），+100 棱角（抬起起音）
// 双包络差值检测（SPL Transient Designer 原理）：快包络抓住字头、慢包络跟节目电平，
// 两者之差即瞬态强度 —— 效果持续整个字头，而非一瞬间
class VoiceEdge final
{
public:
    VoiceEdge(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(const juce::dsp::ProcessContextReplacing<float>& context);

private:
    std::atomic<float>* amountParam;
    std::atomic<float>* bypassParam;

    std::atomic<double>* dspRate; // OS 采样率（模块在超采样链内运行）
    double sampleRate = 48000.0;
    float bypassMix = 1.0f; // 1=正常 0=旁通（块级平滑，无 click）

    // 双包络（快 0.3ms/15ms，慢 25ms/250ms），每通道独立
    float fastEnv[2] { 0.0f, 0.0f };
    float slowEnv[2] { 0.0f, 0.0f };
    float fastAttack = 0.01f, fastRelease = 0.01f;
    float slowAttack = 0.01f, slowRelease = 0.01f;
    float gainSmooth[2] { 1.0f, 1.0f };
    float gainSmoothCoeff = 0.01f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceEdge)
};
