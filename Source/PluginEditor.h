#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/MoonVocLookAndFeel.h"

// UI：需求布局（左 Edge+染色 / 中压缩+EQ / 右去齿音+电平表）+ 上帝粒子指示灯 + 锁频显示
class MoonVocEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MoonVocEditor(MoonVocProcessor&);
    ~MoonVocEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    // 布局自检（打印关键控件 bounds，供程序化验证）
    void dumpLayout() const;

private:
    void timerCallback() override;
    void setupSlider(juce::Slider& s, juce::Label& l, const juce::String& text, bool singleSided = false);
    void setupCombo(juce::ComboBox& c, const juce::StringArray& items);
    void setupButton(juce::ToggleButton& b, const juce::String& text);
    void setupSectionTitle(juce::Label& l, const juce::String& text);
    void paintMeter(juce::Graphics& g, juce::Rectangle<int> r, float levelDb, float peakDb,
                    const juce::String& name, float grDb);
    void paintIndicator(juce::Graphics& g, juce::Rectangle<int> r);
    void paintGrBar(juce::Graphics& g, juce::Rectangle<int> r, float grDb,
                    const juce::String& name, juce::Colour col);

    MoonVocProcessor& processorRef;
    std::unique_ptr<MoonVocLookAndFeel> lookAndFeel;
    float indicatorPhase = 0.0f; // 指示灯闪烁相位
    float meterPeakIn = -60.0f, meterPeakOut = -60.0f; // 电平峰值保持
    juce::Image metalTexture;    // 金属拉丝纹理（预渲染缓存）
    juce::Rectangle<int> panelGlobal, panelLeft, panelMid, panelRight; // 面板区域（resized 记录，paint 绘制）
    juce::Rectangle<int> meterInRect, meterOutRect; // 电平表位置（resized 计算，paint 绘制）
    juce::Rectangle<int> indicatorRect;             // 指示灯位置（Output 右侧，resized 计算）
    juce::Rectangle<int> eqRowRect;                 // EQ 行区域（自检用）
    juce::Rectangle<int> grCompRect, grDeessRect;   // GR 表（压缩/去齿音）
    // 星野（确定性随机，相对坐标）
    struct Star { float x, y, size; };
    std::array<Star, 42> stars;
    juce::Image moonImage; // 月亮背景图（最底层半透明）

    // 背景漂浮粒子（相对坐标 0~1，随窗口缩放）

    // 区段标题
    juce::Label sectionGlobal, sectionEq, sectionComp, sectionDeEss, sectionSat, sectionEdge;

    // 全局
    juce::Slider inputGainSlider, headroomSlider, outputGainSlider;
    juce::Label inputGainLabel, headroomLabel, outputGainLabel;
    juce::ComboBox oversamplingBox;

    // EQ
    juce::Slider boostSlider, deboxSlider, claritySlider, airSlider;
    juce::Label boostLabel, deboxLabel, clarityLabel, airLabel;
    juce::Label deboxFreqLabel, clarityFreqLabel;   // 智能锁频显示
    juce::Label thickFreqLabel;
    juce::ComboBox airFreqBox;

    // 压缩
    juce::ComboBox compModeBox;
    juce::Slider compAmountSlider, compMakeupSlider;
    juce::Label compAmountLabel, compMakeupLabel;

    // 去齿音
    juce::Slider dsLowSlider, dsHighSlider;
    juce::Label dsLowLabel, dsHighLabel;

    // 染色
    juce::ComboBox satTypeABox, satTypeBBox;
    juce::Slider satAmountASlider, satAmountBSlider;
    juce::Label satAmountALabel, satAmountBLabel;

    // 瞬态
    juce::Slider edgeSlider;
    juce::Label edgeLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> inputGainAtt, headroomAtt, outputGainAtt;
    std::unique_ptr<SliderAttachment> boostAtt, deboxAtt, clarityAtt, airAtt;
    std::unique_ptr<SliderAttachment> compAmountAtt, compMakeupAtt, dsLowAtt, dsHighAtt;
    std::unique_ptr<SliderAttachment> satAmountAAtt, satAmountBAtt, edgeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAtt, airFreqAtt, compModeAtt, satTypeAAtt, satTypeBAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoonVocEditor)
};
