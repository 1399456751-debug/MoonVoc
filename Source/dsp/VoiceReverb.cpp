#include "VoiceReverb.h"
#include "../ParamIDs.h"

VoiceReverb::VoiceReverb(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>&)
    : amountParam(apvts.getRawParameterValue(ParamID::reverbAmount)),
      modeParam(apvts.getRawParameterValue(ParamID::reverbMode)),
      bypassParam(apvts.getRawParameterValue(ParamID::reverbBypass))
{
    // 注意：juce::Reverb 内部 dry×2、wet×3 标定 —— dry 0.5 = 干声 1:1 直通；
    // wet 最大 0.33（×3 = 1.0 满幅湿声），由 updateParams 里按 amount 缩放
    current.dryLevel = 0.5f;
}

void VoiceReverb::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reverb.reset();
}

void VoiceReverb::reset()
{
    reverb.reset();
    wetSmooth = 0.0f;
}

// 每块更新：wet 平滑（含旁通淡出） + 模式参数表（Pop 中型空间 / Rap 大空间长尾）
void VoiceReverb::updateParams(int numSamples)
{
    // 旁通：wet 平滑淡出（reverb 干路本就 1:1，wet→0 即完全透明）
    const float bypTarget = bypassParam->load() > 0.5f ? 0.0f : 1.0f;
    const double blockDur = (double) numSamples / jmax(48000.0, sampleRate);
    const float bypAlpha = 1.0f - (float) std::exp(-blockDur / 0.01);
    bypassMix += bypAlpha * (bypTarget - bypassMix);

    const float wetTarget = jlimit(0.0f, 1.0f, amountParam->load() / 100.0f) * bypassMix;
    const float alpha = 1.0f - (float) std::exp(-blockDur / 0.02); // ~20ms
    wetSmooth += alpha * (wetTarget - wetSmooth);

    const bool rap = modeParam->load() > 0.5f;
    const float roomSize = rap ? 0.92f : 0.55f;
    const float damping  = rap ? 0.30f : 0.50f;

    if (std::abs(current.wetLevel - wetSmooth) > 0.002f || current.roomSize != roomSize
        || current.damping != damping)
    {
        current.roomSize = roomSize;
        current.damping  = damping;
        current.wetLevel = wetSmooth * 0.33f; // juce 内部 ×3 → 100% 时满幅湿声
        current.width    = 1.0f;
        reverb.setParameters(current);
    }
}

void VoiceReverb::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    updateParams((int) context.getInputBlock().getNumSamples());
    reverb.process(context);
}
