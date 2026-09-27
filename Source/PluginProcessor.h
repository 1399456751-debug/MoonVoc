#pragma once

#include <JuceHeader.h>
#include "dsp/VoiceEq.h"
#include "dsp/VoiceComp.h"
#include "dsp/VoiceDeEsser.h"
#include "dsp/VoiceSat.h"
#include "dsp/VoiceEdge.h"
#include "dsp/VoiceReverb.h"

// 信号链：EQ → 压缩 → 去齿音 → 染色 → 瞬态 → 混响（最后）
class MoonVocProcessor : public juce::AudioProcessor
{
public:
    MoonVocProcessor();
    ~MoonVocProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // 压缩 GR 电平表（UI/测试读取）
    float getCompGainReduction() const { return comp.gainReduction.load(); }

    // 去齿音 GR 电平表（UI/测试读取）
    float getDeEssGainReduction() const { return deEss.gainReduction.load(); }

    // 输入/输出电平表（dBFS，VU 平滑；UI/测试读取）
    std::atomic<float> inputLevelDb { -60.0f };
    std::atomic<float> outputLevelDb { -60.0f };

    // 压缩智能参数（UI/测试读取）
    float getCompCrestDb() const { return comp.crestDbDisplay.load(); }
    float getCompFastAttackMs() const { return comp.fastAttackMsDisplay.load(); }
    float getCompFastRatio() const { return comp.fastRatioDisplay.load(); }

    // EQ 智能锁定频点（UI 显示，最多 3 个，0 = 无）
    float getEqDeboxFreq(int idx) const { return eq.deboxFreqDisplay[idx].load(); }
    float getEqClarityFreq(int idx) const { return eq.clarityFreqDisplay[idx].load(); }
    float getEqThickFreq() const { return eq.thickFreqDisplay.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::dsp::Gain<float> inputGain, outputGain, headroomGain;

    std::atomic<double> dspSampleRate { 48000.0 }; // 必须先于模块成员初始化：= 宿主采样率 × factor
    double lastSampleRate = 48000.0;               // prepare 时保存，热切换不依赖 getSampleRate()

    // 电平表 VU 平滑状态（块级一阶：上升快 ~10ms，下降慢 ~300ms）
    float inLvlSmooth = -60.0f, outLvlSmooth = -60.0f;
    float lvlAttack = 0.5f, lvlRelease = 0.05f;

    VoiceEq eq;
    VoiceComp comp;
    VoiceDeEsser deEss;
    VoiceSat sat;
    VoiceEdge edge;
    VoiceReverb reverb;

    // 4 个预构建超采样器（2x/4x/8x/16x），RT 安全切换
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[4];
    std::atomic<int> currentOsIndex { 0 };
    std::atomic<float>* oversamplingParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoonVocProcessor)
};
