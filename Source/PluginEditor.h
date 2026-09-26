#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/MoonVocLookAndFeel.h"

// UI：水平信号链卡片式（顶部全局条 → 模块卡片横排 → 底部 Monitor + Engine）
class MoonVocEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MoonVocEditor(MoonVocProcessor&);
    ~MoonVocEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    // 布局自检（打印关键控件 bounds + 卡片归属断言，供程序化验证）
    void dumpLayout() const;
    // 测试用：手动推进一次 timer 逻辑（离线快照更新电平表/锁频标签）
    void demoTick() { timerCallback(); }

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
    void renderBackground(); // resized 里预渲染 bgCache（渐变 + 抽象装饰）

    MoonVocProcessor& processorRef;
    std::unique_ptr<MoonVocLookAndFeel> lookAndFeel;
    float indicatorPhase = 0.0f; // 指示灯闪烁 + 背景呼吸相位
    float meterPeakIn = -60.0f, meterPeakOut = -60.0f; // 电平峰值保持
    juce::Image bgCache;   // 静态背景缓存（渐变 + 抽象装饰），resized 重渲染

    // 卡片区域（resized 记录，paint 绘制）
    juce::Rectangle<int> cardGlobal, cardDeEss, cardEq, cardComp, cardSat, cardEdge;
    juce::Rectangle<int> cardMonitor, cardOs;
    juce::Rectangle<int> meterInRect, meterOutRect; // 电平表位置（resized 计算，paint 绘制）
    juce::Rectangle<int> indicatorRect;             // 指示灯位置（cardGlobal 右端）
    juce::Rectangle<int> grCompRect, grDeessRect;   // GR 表（压缩/去齿音）

    // 区段标题
    juce::Label sectionGlobal, sectionEq, sectionComp, sectionDeEss, sectionSat, sectionEdge;
    juce::Label sectionMonitor, sectionOs;

    // 全局
    juce::Slider inputGainSlider, headroomSlider, outputGainSlider;
    juce::Label inputGainLabel, headroomLabel, outputGainLabel;
    juce::ComboBox oversamplingBox;
    juce::Label oversamplingLabel; // 超采样下拉框标签（"Oversampling"）

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
