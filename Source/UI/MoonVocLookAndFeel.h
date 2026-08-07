#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

// MoonVoc UI 主题：深紫黑金属拟物 + 紫色光效
namespace Theme
{
    const juce::Colour bg            { 0xff0a0710 }; // 极暗紫黑背景（与光效强对比）
    const juce::Colour panel         { 0xff150f22 }; // 更暗面板
    const juce::Colour panelEdge     { 0xff2e2347 }; // 面板描边
    const juce::Colour knobBody      { 0xff2a2040 }; // 旋钮体
    const juce::Colour knobBodyHi    { 0xff4a3a6e }; // 旋钮高光
    const juce::Colour accent        { 0xff9d6bd8 }; // 主紫
    const juce::Colour accentHi      { 0xffc9a4ff }; // 亮紫
    const juce::Colour textMain      { 0xffe8e2f2 }; // 主文字
    const juce::Colour textDim       { 0xff8a7fa8 }; // 次文字
    const juce::Colour danger        { 0xffff3b5c }; // 爆红
    const juce::Colour okGreen       { 0xff5dd39c }; // 电平绿
    const juce::Colour warnYellow    { 0xffffb84d }; // 电平黄

    // 字体（Montserrat，OFL 开源，嵌入 BinaryData）
    inline juce::Typeface::Ptr boldTf()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::MontserratBold_ttf, BinaryData::MontserratBold_ttfSize);
        return tf;
    }
    inline juce::Typeface::Ptr semiBoldTf()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::MontserratSemiBold_ttf, BinaryData::MontserratSemiBold_ttfSize);
        return tf;
    }
    inline juce::Typeface::Ptr mediumTf()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::MontserratMedium_ttf, BinaryData::MontserratMedium_ttfSize);
        return tf;
    }

    inline juce::Font fontTitle(float h = 34.0f)   { return juce::Font(boldTf()).withHeight(h); }
    inline juce::Font fontSection(float h = 17.0f) { return juce::Font(boldTf()).withHeight(h); }
    inline juce::Font fontLabel(float h = 14.0f)   { return juce::Font(boldTf()).withHeight(h); }
    inline juce::Font fontValue(float h = 15.0f)   { return juce::Font(boldTf()).withHeight(h); }

    // 统一辉光文字：5 层扩散模糊光晕 + 主层（所有文字共用）
    inline void drawGlowText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                             const juce::Font& font, juce::Colour col, juce::Justification j)
    {
        g.setFont(font);
        const float offs[3]   { 1.6f, 0.9f, 0.4f };
        const float alphas[3] { 0.08f, 0.16f, 0.30f };
        for (int i = 0; i < 3; ++i)
        {
            g.setColour(col.withAlpha(alphas[i]));
            g.drawText(text, r.translated(offs[i], offs[i]), j);
        }
        g.setColour(col);
        g.drawText(text, r, j);
    }
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

    void drawLabel(juce::Graphics&, juce::Label&) override;

    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator,
                           bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
};
