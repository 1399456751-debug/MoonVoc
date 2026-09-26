#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

// MoonVoc UI 主题：亮色卡片 + 青蓝珊瑚（现代简约，对标苹果/FabFilter）
namespace Theme
{
    const juce::Colour bg         { 0xfff2f0ec }; // 米白底（非纯白）
    const juce::Colour panel      { 0xfffcfbf9 }; // 卡片亮灰白
    const juce::Colour panelEdge  { 0xffe3e0d9 }; // 浅灰描边
    const juce::Colour knobBody   { 0xffffffff }; // 旋钮体白
    const juce::Colour knobBodyHi { 0xfff0ede6 }; // 旋钮体底部微暖灰（渐变低端）
    const juce::Colour accent     { 0xff2dd4bf }; // 青蓝 teal：数值弧/激活态/电平
    const juce::Colour accent2    { 0xffff6b6b }; // 珊瑚橙：渐变端/高亮/警示
    const juce::Colour textMain   { 0xff1c2733 }; // 深炭主文字
    const juce::Colour textDim    { 0xff7a8494 }; // 冷灰次文字
    const juce::Colour danger     { 0xffe5484d }; // 爆红
    const juce::Colour okGreen    { 0xff2fbf8f }; // 电平绿段
    const juce::Colour warnYellow { 0xffe8a13a }; // 电平黄段
    const juce::Colour track      { 0xffe9e6df }; // 弧环/电平槽底（浅暖灰）
    const juce::Colour shadow     { 0x2e1c2733 }; // 卡片/旋钮柔和投影（深炭 18% alpha）

    // 字体（Montserrat Bold，OFL 开源，嵌入 BinaryData；SemiBold/Medium 死重已删）
    inline juce::Typeface::Ptr boldTf()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::MontserratBold_ttf, BinaryData::MontserratBold_ttfSize);
        return tf;
    }

    inline juce::Font fontTitle(float h = 30.0f)   { return juce::Font(boldTf()).withHeight(h); }
    inline juce::Font fontSection(float h = 15.0f) { return juce::Font(boldTf()).withHeight(h); }
    inline juce::Font fontLabel(float h = 13.0f)   { return juce::Font(boldTf()).withHeight(h); }
    inline juce::Font fontValue(float h = 14.0f)   { return juce::Font(boldTf()).withHeight(h); }
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

    juce::Font getComboBoxFont(juce::ComboBox&) override { return Theme::fontLabel(13.0f); }
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;

    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator,
                           bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
};
