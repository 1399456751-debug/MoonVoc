#pragma once

#include <JuceHeader.h>

// 染色（纯净/FET/电子管/磁带/光电/锗管，A+B 双槽可叠加）
// 统一框架：饱和传函小信号斜率≈1（干净），大信号软压缩；out = x + (sat(x)-x)*amount
class VoiceSat final
{
public:
    VoiceSat(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(const juce::dsp::ProcessContextReplacing<float>& context);

private:
    static float saturate(float x, int type);

    std::atomic<float>* typeAParam;
    std::atomic<float>* amountAParam;
    std::atomic<float>* typeBParam;
    std::atomic<float>* amountBParam;
    std::atomic<float>* bypassParam;

    double sampleRate = 48000.0;
    float bypassMix = 1.0f; // 1=正常 0=旁通（块级平滑，无 click）

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceSat)
};
