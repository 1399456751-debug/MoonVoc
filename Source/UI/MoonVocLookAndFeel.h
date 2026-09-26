#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

// MoonVoc UI 主题：莫兰迪色系卡片 + 青蓝珊瑚品牌（现代简约可爱）
namespace Theme
{
    const juce::Colour bg         { 0xfff2f0ec }; // 米白底（非纯白）
    const juce::Colour panel      { 0xfffcfbf9 }; // 弹窗/下拉亮灰白
    const juce::Colour panelEdge  { 0xffe3e0d9 }; // 浅灰描边
    const juce::Colour knobBody   { 0xffffffff }; // 旋钮体白
    const juce::Colour knobBodyHi { 0xfff0ede6 }; // 旋钮体底部微暖灰（渐变低端）
    const juce::Colour accent     { 0xff2dd4bf }; // 青蓝 teal：品牌/电平/指示灯
    const juce::Colour accent2    { 0xffff6b6b }; // 珊瑚橙：品牌渐变端/警示
    const juce::Colour textMain   { 0xff1c2733 }; // 深炭主文字
    const juce::Colour textDim    { 0xff7a8494 }; // 冷灰次文字
    const juce::Colour danger     { 0xffe5484d }; // 爆红
    const juce::Colour okGreen    { 0xff2fbf8f }; // 电平绿段
    const juce::Colour warnYellow { 0xffe8a13a }; // 电平黄段
    const juce::Colour track      { 0xffe9e6df }; // 电平槽底（浅暖灰）
    const juce::Colour shadow     { 0x2e1c2733 }; // 卡片/旋钮柔和投影（深炭 18% alpha）

    // 莫兰迪模块色板：卡片底色（浅浊） + 深色强调（区标题/数值弧/指针/卡片描边）
    const juce::Colour cardGlobal     { 0xfff3eee5 }; // 燕麦暖沙
    const juce::Colour cardGlobalDeep { 0xffa58a66 };
    const juce::Colour cardDeEss      { 0xfff8ecea }; // 豆沙粉
    const juce::Colour cardDeEssDeep  { 0xffc07f7f };
    const juce::Colour cardEq         { 0xffedf3e8 }; // 鼠尾草绿
    const juce::Colour cardEqDeep     { 0xff7da172 };
    const juce::Colour cardComp       { 0xffeaf1f6 }; // 雾霾蓝
    const juce::Colour cardCompDeep   { 0xff6f9bbd };
    const juce::Colour cardSat        { 0xfff7efe3 }; // 焦糖杏
    const juce::Colour cardSatDeep    { 0xffc79462 };
    const juce::Colour cardEdge       { 0xfff1edf5 }; // 香芋紫
    const juce::Colour cardEdgeDeep   { 0xff9c84ba };
    const juce::Colour cardMonitor    { 0xffedf1ee }; // 冷雾灰
    const juce::Colour cardMonitorDeep{ 0xff7f8d89 };
    const juce::Colour cardEngine     { 0xfff3f1ea }; // 米灰
    const juce::Colour cardEngineDeep { 0xff9a947f };

    // 字体（Montserrat Bold，OFL 开源，嵌入 BinaryData；SemiBold/Medium 死重已删）
    inline juce::Typeface::Ptr boldTf()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::MontserratBold_ttf, BinaryData::MontserratBold_ttfSize);
        return tf;
    }

    inline juce::Font fontTitle(float h = 30.0f)   { return juce::Font(boldTf()).withHeight(h); }
    inline juce::Font fontSection(float h = 15.0f) { return juce::Font(boldTf()).withHeight(h); }
    inline juce::Font fontLabel(float h = 14.0f)   { return juce::Font(boldTf()).withHeight(h); }
    inline juce::Font fontValue(float h = 15.0f)   { return juce::Font(boldTf()).withHeight(h); }
}

class MoonVocLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MoonVocLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;

    juce::Font getComboBoxFont(juce::ComboBox&) override { return Theme::fontLabel(14.0f); }
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    juce::Label* createSliderTextBox(juce::Slider&) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;

    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator,
                           bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
};
