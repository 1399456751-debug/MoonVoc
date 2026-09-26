#pragma once

#include <JuceHeader.h>

// 混响（链路最后，宿主采样率运行，不进超采样链）：
//   单个 wet 大旋钮（0~100%）+ 两种模式 —— Pop 流行歌（中型空间中等尾）/ Rap 大混响说唱（大空间长尾）
//   旁通走 setEnabled（内部平滑，无 click）；wet 拖动做块级平滑防 zipper
class VoiceReverb final
{
public:
    VoiceReverb(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(const juce::dsp::ProcessContextReplacing<float>& context);

private:
    void updateParams(int numSamples);

    std::atomic<float>* amountParam;
    std::atomic<float>* modeParam;
    std::atomic<float>* bypassParam;

    double sampleRate = 48000.0;
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters current;
    float wetSmooth = 0.0f;  // 平滑后的 wet（块级 EMA ~20ms）
    float bypassMix = 1.0f;  // 1=正常 0=旁通（块级平滑，无 click）

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceReverb)
};
