#include "VoiceEdge.h"
#include "../ParamIDs.h"

namespace
{
    // 瞬态最大增益变化（amount ±100 → 起音增益 0.2x ~ 1.8x）
    constexpr float kMaxBoost = 0.8f;

    float timeToCoeff(float ms, double sampleRate)
    {
        return 1.0f - std::exp(-1.0f / (ms * 0.001 * sampleRate));
    }
}

VoiceEdge::VoiceEdge(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate)
    : amountParam  (apvts.getRawParameterValue(ParamID::edgeAmount)),
      dspRate(&osSampleRate)
{
}

void VoiceEdge::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = dspRate->load(); // OS 采样率（模块在超采样链内运行）
    envAttack = timeToCoeff(1.0f, sampleRate);   // 包络 attack 1ms（跟得上起音）
    envRelease = timeToCoeff(50.0f, sampleRate); // 包络 release 50ms
    gainSmoothCoeff = timeToCoeff(1.5f, sampleRate); // 增益平滑 1.5ms（限制变化率降失真，同时保留瞬态效果）
    reset();
}

void VoiceEdge::reset()
{
    env[0] = env[1] = 0.0f;
    gainSmooth[0] = gainSmooth[1] = 1.0f;
}

void VoiceEdge::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& outputBlock = context.getOutputBlock();


    if (outputBlock.getNumSamples() == 0)
        return;

    const float amount = jlimit(-100.0f, 100.0f, amountParam->load()) / 100.0f; // -1 ~ +1

    const auto numChannels = outputBlock.getNumChannels();
    const auto numSamples = outputBlock.getNumSamples();

    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        auto* data = outputBlock.getChannelPointer(ch);
        const size_t si = ch < 2 ? ch : 1;
        float& e = env[si];
        float& gs = gainSmooth[si];

        for (size_t n = 0; n < numSamples; ++n)
        {
            const float x = data[n];
            const float a = std::abs(x);

            // 包络跟随（快 attack 中速 release）
            e += (a > e ? envAttack : envRelease) * (a - e);

            // 瞬态强度 = 输入瞬时值超出包络的比例（起音瞬间 a>>e → 1；稳态 a≈e → 0）
            float rising = (a - e) / (a + 1.0e-6f);
            rising = jlimit(0.0f, 1.0f, rising);

            // 增益平滑：限制变化率（3ms），降低时变增益的调制失真
            const float target = 1.0f + amount * kMaxBoost * rising;
            gs += gainSmoothCoeff * (target - gs);
            data[n] = x * gs;
        }
    }
}
