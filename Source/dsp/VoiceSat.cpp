#include "VoiceSat.h"
#include "../ParamIDs.h"

namespace
{
    constexpr int kOff = 0, kFET = 1, kTube = 2, kTape = 3, kOptical = 4, kGermanium = 5;

    // 干湿混合（amount 0~100%）
    inline float mix(float dry, float wet, float amount)
    {
        return dry + (wet - dry) * amount;
    }
}

VoiceSat::VoiceSat(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate)
    : typeAParam   (apvts.getRawParameterValue(ParamID::satTypeA)),
      amountAParam (apvts.getRawParameterValue(ParamID::satAmountA)),
      typeBParam   (apvts.getRawParameterValue(ParamID::satTypeB)),
      amountBParam (apvts.getRawParameterValue(ParamID::satAmountB))
{
}

void VoiceSat::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
}

void VoiceSat::reset() {}

// 饱和传函：小信号斜率精确 1:1（完全干净透明），大信号软压缩/饱和（染色只在大信号出现）
// 系数为经验值，后续按听感微调
float VoiceSat::saturate(float x, int type)
{
    switch (type)
    {
        case kFET:       // 硬朗，晶体管削波
            return std::tanh(x * 3.0f) / 3.0f;

        case kTube:      // 电子管：非对称（偶次谐波暖意，小信号仍 1:1）
        {
            const float asym = x + 0.15f * x * x;
            return std::tanh(asym * 2.0f) / 2.075f;
        }

        case kTape:      // 磁带：高驱动软饱和 + 压缩感
            return std::tanh(x * 4.0f) / 4.0f;

        case kOptical:   // 光电：柔和限幅（x/(1+k|x|) 小信号斜率本就为 1）
            return x / (1.0f + 0.5f * std::abs(x));

        case kGermanium: // 锗管：奇次畸变 + 偏置的粗粝感
            return std::tanh((x - 0.15f * x * x * x) * 3.0f) / 3.0f;

        case kOff:
        default:
            return x;
    }
}

void VoiceSat::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& outputBlock = context.getOutputBlock();


    if (outputBlock.getNumSamples() == 0)
        return;

    const int typeA = (int) typeAParam->load();
    const int typeB = (int) typeBParam->load();
    // gamma 0.7：中段驱动更明显（30% 强度 ≈ 之前 45% 的效果）
    const float amountA = std::pow(jlimit(0.0f, 100.0f, amountAParam->load()) / 100.0f, 0.7f);
    const float amountB = std::pow(jlimit(0.0f, 100.0f, amountBParam->load()) / 100.0f, 0.7f);

    const bool useA = typeA != kOff && amountA > 0.0f;
    const bool useB = typeB != kOff && amountB > 0.0f;
    if (! useA && ! useB)
        return;

    const auto numChannels = outputBlock.getNumChannels();
    const auto numSamples = outputBlock.getNumSamples();

    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        auto* data = outputBlock.getChannelPointer(ch);

        for (size_t n = 0; n < numSamples; ++n)
        {
            float x = data[n];
            if (useA)
                x = mix(x, saturate(x, typeA), amountA);
            if (useB)
                x = mix(x, saturate(x, typeB), amountB);
            data[n] = x;
        }
    }
}
