#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/MoonVocLookAndFeel.h"
#include "UI/MoonVocStrings.h"
#include "UI/AboutOverlay.h"

// UI：水平信号链卡片式（顶部全局条 → 模块卡片横排 → 底部 Monitor + Engine）
// 缩放实现：全部控件放在 Canvas 子容器里按设计坐标（1280×720）布局，窗口变大时对
// Canvas 施加 transform 整体放大（JUCE 官方推荐做法，不能在 editor 自身上加 transform）
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

    // 关于浮层入口（ⓘ 按钮 / 截图与自检用；animate=false 跳过淡入）
    void openAbout (bool animate = true);

    // 自检/测试读取用（与 dumpLayout 同性质的测试面，不参与生产逻辑）
    InfoBadge&    getInfoBadge()   noexcept { return infoBadge; }
    AboutOverlay& getAboutOverlay() noexcept { return aboutOverlay; }

    static constexpr int kDesignW = 1496; // 设计基准宽度（6 卡横排）
    static constexpr int kDesignH = 672;  // 设计基准高度（去掉底部大片空白）

private:
    void timerCallback() override;

    // 承载全部 UI 的子容器（设计坐标布局，transform 缩放）
    struct Canvas : juce::Component
    {
        explicit Canvas(MoonVocEditor& e) : owner(e) {}
        void paint(juce::Graphics& g) override { owner.paintCanvas(g); }
        void resized() override { owner.layoutCanvas(); }
        MoonVocEditor& owner;
    };

    void paintCanvas(juce::Graphics&);
    void layoutCanvas();
    void applyLanguage();
    void applyFontMode();
    void applyScaleFromParam();

    // 文本注册表（语言/字体切换时统一刷新）
    struct TextEntry { juce::Label* label; Strings::Key key; float base; int kind; }; // kind 0=label 1=section 2=title
    std::vector<TextEntry> textEntries;
    void registerText(juce::Label& l, Strings::Key key, float base, int kind);

    void setupSlider(juce::Slider& s, juce::Label& l, Strings::Key key, juce::Colour arcColour);
    void setupCombo(juce::ComboBox& c, const juce::StringArray& items);
    void setupButton(juce::ToggleButton& b, const juce::String& text);
    void setupSectionTitle(juce::Label& l, Strings::Key key, juce::Colour deep);
    void paintMeter(juce::Graphics& g, juce::Rectangle<int> r, float levelDb, float peakDb,
                    const juce::String& name, float grDb);
    void paintIndicator(juce::Graphics& g, juce::Rectangle<int> r);
    void paintGrBar(juce::Graphics& g, juce::Rectangle<int> r, float grDb,
                    const juce::String& name, juce::Colour col);
    void renderBackground(); // resized 里预渲染 bgCache（渐变 + 抽象装饰）

    MoonVocProcessor& processorRef;
    std::unique_ptr<MoonVocLookAndFeel> lookAndFeel;
    Canvas canvas { *this };
    float currentScale = 1.0f;   // 当前渲染缩放（窗口尺寸 / 设计尺寸）
    float appliedScale = 1.0f;   // 最近一次同步过的缩放（用于区分拖拽 vs 参数驱动）
    bool settingScale = false;   // applyScaleFromParam 内部置位，避免回写参数
    bool currentZh = true;       // 当前语言（true=中文）
    float indicatorPhase = 0.0f; // 指示灯闪烁 + 背景呼吸相位
    float meterPeakIn = -60.0f, meterPeakOut = -60.0f; // 电平峰值保持
    juce::Image bgCache;   // 静态背景缓存（渐变 + 抽象装饰），resized 重渲染
    juce::Image bgBlurCache; // 毛玻璃底（背景模糊版），只在 resized 重算

    AboutOverlay aboutOverlay;   // 「关于」浮层（覆盖整个设计区）
    InfoBadge    infoBadge;      // 全局卡右上角的 ⓘ 入口

    // 卡片区域（resized 记录，paint 绘制）
    juce::Rectangle<int> cardGlobal, cardReverb, cardEq, cardComp, cardDeEss, cardSat, cardEdge;
    juce::Rectangle<int> cardMonitor, cardOs;
    juce::Rectangle<int> meterInRect, meterOutRect; // 电平表位置（resized 计算，paint 绘制）
    juce::Rectangle<int> indicatorRect;             // 指示灯位置（cardGlobal 右端）
    juce::Rectangle<int> grCompRect;                // GR 表（压缩）
    juce::Rectangle<int> grDeEssRect;               // GR 表（去齿音）

    // 区段标题
    juce::Label sectionGlobal, sectionEq, sectionComp, sectionDeEss, sectionReverb, sectionSat, sectionEdge;
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
    juce::TextButton airSatinBtn, airNimbusBtn;     // Air 频点分段按钮（Satin 16k / Nimbus 22k）

    // 压缩
    juce::ComboBox compModeBox;
    juce::Slider compAmountSlider, compMakeupSlider;
    juce::Label compAmountLabel, compMakeupLabel;

    // 去齿音（链路位于压缩后）
    juce::Slider dsAmountSlider, dsFocusSlider;
    juce::Label dsAmountLabel, dsFocusLabel;

    // 混响（链路最后）
    juce::Slider reverbSlider;
    juce::Label reverbLabel;
    juce::ComboBox reverbModeBox;

    // 染色
    juce::ComboBox satTypeABox, satTypeBBox;
    juce::Slider satAmountASlider, satAmountBSlider;
    juce::Label satAmountALabel, satAmountBLabel;

    // 瞬态
    juce::Slider edgeSlider;
    juce::Label edgeLabel;

    // 旁通开关（每模块一个）
    juce::ToggleButton eqBypassBtn, compBypassBtn, deEssBypassBtn, satBypassBtn, edgeBypassBtn, reverbBypassBtn;

    // 设置（语言 / 大字 / 缩放）
    juce::Label sectionSettings;
    juce::ComboBox langBox, scaleBox;
    juce::Label langLabel, scaleLabel;
    juce::ToggleButton largeFontBtn;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> inputGainAtt, headroomAtt, outputGainAtt;
    std::unique_ptr<SliderAttachment> boostAtt, deboxAtt, clarityAtt, airAtt;
    std::unique_ptr<SliderAttachment> compAmountAtt, compMakeupAtt, reverbAmountAtt;
    std::unique_ptr<SliderAttachment> dsAmountAtt, dsFocusAtt;
    std::unique_ptr<SliderAttachment> satAmountAAtt, satAmountBAtt, edgeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAtt, compModeAtt, reverbModeAtt, satTypeAAtt, satTypeBAtt;
    std::unique_ptr<juce::ParameterAttachment> airFreqAtt; // Air 频点（choice 参数 → 两个分段按钮）
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqBypassAtt, compBypassAtt, deEssBypassAtt, satBypassAtt, edgeBypassAtt, reverbBypassAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> langAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> largeFontAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoonVocEditor)
};
