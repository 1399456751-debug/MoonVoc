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
      eq(apvts, dspSampleRate), comp(apvts, dspSampleRate),
      sat(apvts, dspSampleRate), edge(apvts, dspSampleRate),
      reverb(apvts, dspSampleRate),
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
    // 默认 16x：延迟可接受，追求最低失真（2x/4x/8x/16x 无 Off）
    p.push_back(std::make_unique<Choice>(ParamID::oversampling, "Oversampling",
        juce::StringArray{ "2x", "4x", "8x", "16x" }, 3));

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

    // 瞬态
    p.push_back(std::make_unique<Param>(ParamID::edgeAmount, S8("Edge 瞬态"), -100.0f, 100.0f, 0.0f));

    // 染色
    const juce::StringArray satTypes{ "Off", "FET", "Tube", "Tape", "Optical", "Germanium" };
    p.push_back(std::make_unique<Choice>(ParamID::satTypeA, S8("Saturate A 染色A"), satTypes, 0));
    p.push_back(std::make_unique<Param>(ParamID::satAmountA, S8("Drive A 驱动A"), pct, 0.0f));
    p.push_back(std::make_unique<Choice>(ParamID::satTypeB, S8("Saturate B 染色B"), satTypes, 0));
    p.push_back(std::make_unique<Param>(ParamID::satAmountB, S8("Drive B 驱动B"), pct, 0.0f));

    // 混响（链路最后）
    p.push_back(std::make_unique<Param>(ParamID::reverbAmount, S8("混响量 Reverb"), pct, 0.0f));
    p.push_back(std::make_unique<Choice>(ParamID::reverbMode, S8("混响模式 Reverb Mode"),
        juce::StringArray{ S8("流行 Pop"), S8("说唱 Rap") }, 0));

    // 旁通（默认关 = 不旁通）
    p.push_back(std::make_unique<Bool>(ParamID::eqBypass,     S8("EQ旁通 EQ Bypass"), false));
    p.push_back(std::make_unique<Bool>(ParamID::compBypass,   S8("压缩旁通 Comp Bypass"), false));
    p.push_back(std::make_unique<Bool>(ParamID::satBypass,    S8("染色旁通 Sat Bypass"), false));
    p.push_back(std::make_unique<Bool>(ParamID::edgeBypass,   S8("瞬态旁通 Edge Bypass"), false));
    p.push_back(std::make_unique<Bool>(ParamID::reverbBypass, S8("混响旁通 Reverb Bypass"), false));

    // UI 设置
    p.push_back(std::make_unique<Choice>(ParamID::uiLanguage, S8("语言 Language"),
        juce::StringArray{ S8("中文"), "English" }, 0));
    p.push_back(std::make_unique<Bool>(ParamID::uiLargeFont, S8("大字字体 Large Font"), false));
    // 连续缩放（拖窗口顺滑），下拉框给 9 个预设档
    p.push_back(std::make_unique<Param>(ParamID::uiScale, S8("界面缩放 Scale"),
        juce::NormalisableRange<float>(1.0f, 3.0f, 0.01f), 1.0f));

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

    // 超采样参数：0..3 = 2x/4x/8x/16x
    currentOsIndex.store(juce::jlimit(0, 3, (int) oversamplingParam->load()));
    dspSampleRate.store(sampleRate * (1 << osExponents[currentOsIndex.load()]));
    setLatencySamples(oversamplers[currentOsIndex.load()]->getLatencyInSamples());

    // 链路在 OS 采样率下跑
    juce::dsp::ProcessSpec osSpec{ sampleRate, (juce::uint32) samplesPerBlock, numChannels };
    eq.prepare(osSpec);
    comp.prepare(osSpec);
    sat.prepare(osSpec);
    edge.prepare(osSpec);

    // 混响在链路最后、宿主采样率下跑（不进超采样链）
    reverb.prepare(spec);
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

    // 超采样倍率切换（宿主未 re-prepare 时的安全兜底）；0..3 = 2x/4x/8x/16x
    const int osIndex = juce::jlimit(0, 3, (int) oversamplingParam->load());
    if (osIndex != currentOsIndex.load())
    {
        currentOsIndex.store(osIndex);
        dspSampleRate.store(lastSampleRate * (double) (1 << osExponents[osIndex]));
        setLatencySamples(oversamplers[osIndex]->getLatencyInSamples());
        oversamplers[osIndex]->reset();
    }

    // 链路：In → Headroom → [EQ→Comp→Sat→Edge（可超采样）] → Reverb → Out
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

    {
        auto* os = oversamplers[currentOsIndex.load()].get();
        auto osBlock = os->processSamplesUp(block);
        auto osContext = juce::dsp::ProcessContextReplacing<float>(osBlock);
        eq.process(osContext);
        comp.process(osContext);
        sat.process(osContext);
        edge.process(osContext);
        os->processSamplesDown(block);
    }

    // 混响（链路最后，宿主采样率）
    reverb.process(juce::dsp::ProcessContextReplacing<float>(block));

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
