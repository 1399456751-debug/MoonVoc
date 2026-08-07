#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"

using AP = juce::AudioProcessorValueTreeState;

namespace
{
    // 超采样倍率：2^x（Oversampling 的 factor 参数是 2 的幂指数）
    constexpr int osExponents[4] { 1, 2, 3, 4 };
}

MoonVocProcessor::MoonVocProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MoonVocParams", createParameterLayout()),
      deEsser(apvts, dspSampleRate), eq(apvts, dspSampleRate), comp(apvts, dspSampleRate),
      sat(apvts, dspSampleRate), edge(apvts, dspSampleRate),
      oversamplingParam(apvts.getRawParameterValue(ParamID::oversampling))
{
}

AP::ParameterLayout MoonVocProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    using Param = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool = juce::AudioParameterBool;
    auto pct = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);

    // 全局
    p.push_back(std::make_unique<Param>(ParamID::inputGain,  "Input",   -12.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<Param>(ParamID::outputGain, "Output",  -12.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<Param>(ParamID::headroom,   "Headroom", -12.0f, 12.0f, 0.0f));
    // 默认 16x：延迟可接受，追求最低失真
    p.push_back(std::make_unique<Choice>(ParamID::oversampling, "Oversampling",
        juce::StringArray{ "Off", "2x", "4x", "8x", "16x" }, 4));

    // EQ（四段智能）
    p.push_back(std::make_unique<Param>(ParamID::eqLowBoost, "Thick (bass auto)", -12.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<Param>(ParamID::eqDeboxCut, "De-Box (auto)", -12.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<Param>(ParamID::eqClarityBoost, "Clarity (auto)", -12.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<Param>(ParamID::eqAirBoost, "Air (bright)", 0.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<Choice>(ParamID::eqAirFreq, "Air Freq",
        juce::StringArray{ "16 kHz", "22 kHz" }, 0));

    // 压缩
    p.push_back(std::make_unique<Choice>(ParamID::compMode, "Style",
        juce::StringArray{ "Pop", "Rap" }, 0));
    p.push_back(std::make_unique<Param>(ParamID::compAmount, "Compression", pct, 0.0f));
    p.push_back(std::make_unique<Param>(ParamID::compMakeup, "Makeup", 0.0f, 12.0f, 0.0f));

    // 去齿音
    p.push_back(std::make_unique<Param>(ParamID::dsLowAmount, "De-Ess 3-5k", pct, 0.0f));
    p.push_back(std::make_unique<Param>(ParamID::dsHighAmount, "De-Ess 5k+", pct, 0.0f));

    // 瞬态
    p.push_back(std::make_unique<Param>(ParamID::edgeAmount, "Edge", -100.0f, 100.0f, 0.0f));

    // 染色
    const juce::StringArray satTypes{ "Off", "FET", "Tube", "Tape", "Optical", "Germanium" };
    p.push_back(std::make_unique<Choice>(ParamID::satTypeA, "Saturate A", satTypes, 0));
    p.push_back(std::make_unique<Param>(ParamID::satAmountA, "Drive A", pct, 0.0f));
    p.push_back(std::make_unique<Choice>(ParamID::satTypeB, "Saturate B", satTypes, 0));
    p.push_back(std::make_unique<Param>(ParamID::satAmountB, "Drive B", pct, 0.0f));

    return { p.begin(), p.end() };
}

void MoonVocProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lastSampleRate = sampleRate;
    const auto numChannels = (juce::uint32) getMainBusNumOutputChannels();
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32) samplesPerBlock, numChannels };
    inputGain.prepare(spec);
    outputGain.prepare(spec);
    headroomGain.prepare(spec);

    const float ramp = 0.05f;
    inputGain.setRampDurationSeconds(ramp);
    outputGain.setRampDurationSeconds(ramp);
    headroomGain.setRampDurationSeconds(ramp);

    // 电平表 VU 平滑系数（按块时长）
    const double blockDur = (double) samplesPerBlock / sampleRate;
    lvlAttack  = 1.0f - (float) std::exp(-blockDur / 0.01);  // 上升 ~10ms
    lvlRelease = 1.0f - (float) std::exp(-blockDur / 0.3);   // 下降 ~300ms

    // 预构建 4 个超采样器（2x/4x/8x/16x），参数切换零分配
    for (int i = 0; i < 4; ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>>(
            (size_t) numChannels, (size_t) osExponents[i],
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            false, true);
        oversamplers[i]->initProcessing((size_t) samplesPerBlock);
        oversamplers[i]->reset();
    }

    // 超采样参数：0=Off，1..4 = 2x/4x/8x/16x
    currentOsIndex.store(juce::jlimit(0, 4, (int) oversamplingParam->load()));
    if (currentOsIndex.load() == 0)
    {
        dspSampleRate.store(sampleRate);
        setLatencySamples(0);
    }
    else
    {
        dspSampleRate.store(sampleRate * (1 << osExponents[currentOsIndex.load() - 1]));
        setLatencySamples(oversamplers[currentOsIndex.load() - 1]->getLatencyInSamples());
    }

    // 链路在 OS 采样率下跑
    juce::dsp::ProcessSpec osSpec{ sampleRate, (juce::uint32) samplesPerBlock, numChannels };
    deEsser.prepare(osSpec);
    eq.prepare(osSpec);
    comp.prepare(osSpec);
    sat.prepare(osSpec);
    edge.prepare(osSpec);
}

void MoonVocProcessor::releaseResources()
{
    for (auto& os : oversamplers)
        os.reset();
}

bool MoonVocProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == main;
}

void MoonVocProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // 清掉多余输出通道
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    juce::dsp::AudioBlock<float> block(buffer);

    inputGain.setGainDecibels(apvts.getRawParameterValue(ParamID::inputGain)->load());
    headroomGain.setGainDecibels(apvts.getRawParameterValue(ParamID::headroom)->load());
    outputGain.setGainDecibels(apvts.getRawParameterValue(ParamID::outputGain)->load());

    // 超采样倍率切换（宿主未 re-prepare 时的安全兜底）；0=Off
    const int osIndex = juce::jlimit(0, 4, (int) oversamplingParam->load());
    if (osIndex != currentOsIndex.load())
    {
        currentOsIndex.store(osIndex);
        if (osIndex == 0)
        {
            dspSampleRate.store(lastSampleRate);
            setLatencySamples(0);
        }
        else
        {
            dspSampleRate.store(lastSampleRate * (double) (1 << osExponents[osIndex - 1]));
            setLatencySamples(oversamplers[osIndex - 1]->getLatencyInSamples());
            oversamplers[osIndex - 1]->reset();
        }
    }

    // 链路：In → Headroom → [DeEss→EQ→Comp→Sat→Edge（可超采样）] → Out
    inputGain.process(juce::dsp::ProcessContextReplacing<float>(block));

    // 输入电平（Input 增益后、处理链前，RMS）：工作电平指示灯参考，压缩等处理不影响它
    {
        const int chs = jmin(2, buffer.getNumChannels());
        double inSq = 0.0;
        for (int ch = 0; ch < chs; ++ch)
        {
            const float* d = buffer.getReadPointer(ch);
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                inSq += (double) d[n] * d[n];
        }
        const double rms = std::sqrt(inSq / (double) jmax(1, chs * buffer.getNumSamples()));
        const float db = 20.0f * std::log10((float) rms + 1.0e-9f);
        const float k = db > inLvlSmooth ? lvlAttack : lvlRelease;
        inLvlSmooth += k * (jmax(-60.0f, db) - inLvlSmooth);
        inputLevelDb.store(inLvlSmooth);
    }

    headroomGain.process(juce::dsp::ProcessContextReplacing<float>(block));

    if (currentOsIndex.load() == 0)
    {
        // Off：链直接按宿主采样率处理
        auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
        deEsser.process(ctx);
        eq.process(ctx);
        comp.process(ctx);
        sat.process(ctx);
        edge.process(ctx);
    }
    else
    {
        auto* os = oversamplers[currentOsIndex.load() - 1].get();
        auto osBlock = os->processSamplesUp(block);
        auto osContext = juce::dsp::ProcessContextReplacing<float>(osBlock);
        deEsser.process(osContext);
        eq.process(osContext);
        comp.process(osContext);
        sat.process(osContext);
        edge.process(osContext);
        os->processSamplesDown(block);
    }

    outputGain.process(juce::dsp::ProcessContextReplacing<float>(block));

    // 输出电平（处理后采样，RMS）
    {
        const int chs = jmin(2, buffer.getNumChannels());
        double outSq = 0.0;
        for (int ch = 0; ch < chs; ++ch)
        {
            const float* d = buffer.getReadPointer(ch);
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                outSq += (double) d[n] * d[n];
        }
        const double rms = std::sqrt(outSq / (double) jmax(1, chs * buffer.getNumSamples()));
        const float db = 20.0f * std::log10((float) rms + 1.0e-9f);
        const float k = db > outLvlSmooth ? lvlAttack : lvlRelease;
        outLvlSmooth += k * (jmax(-60.0f, db) - outLvlSmooth);
        outputLevelDb.store(outLvlSmooth);
    }
}

juce::AudioProcessorEditor* MoonVocProcessor::createEditor()
{
    return new MoonVocEditor(*this);
}

void MoonVocProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MoonVocProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoonVocProcessor();
}
