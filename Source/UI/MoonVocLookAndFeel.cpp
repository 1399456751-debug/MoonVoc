#include "MoonVocLookAndFeel.h"

namespace
{
    constexpr float kStartAngle = -2.35619f; // -135°
    constexpr float kEndAngle   =  2.35619f; // +135°
}

MoonVocLookAndFeel::MoonVocLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, Theme::textMain);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, Theme::accent.withAlpha(0.3f));
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::textColourId, Theme::textMain);
    setColour(juce::ComboBox::arrowColourId, Theme::accent);
    setColour(juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour(juce::PopupMenu::textColourId, Theme::textMain);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::accent.withAlpha(0.35f));
}

// 旋钮：扁平科幻 —— 细刻度线环 + 细发光弧 + 小粒子端点 + 扁平旋钮体
void MoonVocLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                          float, float, float, juce::Slider& slider)
{
    // 以 min(w,h) 为中心的正方形区域（保证正圆）
    const float size = (float) jmin(w, h);
    const juce::Rectangle<float> area((float) x + (w - size) * 0.5f,
                                      (float) y + (h - size) * 0.5f, size, size);
    const auto centre = area.getCentre();
    const float radius = size * 0.46f;

    const float tickR0 = radius * 0.90f;
    const float tickR1 = radius * 0.97f;  // 刻度线（外）

    // 显示角：所有旋钮 0 值统一指向 7 点（-135°）
    //   单向（min>=0）：0 在起点 -135°，满值顺时针到 +135°
    //   正负（min<0） ：0 在 -135°，正值顺时针转、负值逆时针转（对称双段）
    const double v   = slider.getValue();
    const double mn  = slider.getMinimum();
    const double mx  = slider.getMaximum();
    const bool twoSided = mn < 0.0;
    float valueAngle;
    if (twoSided)
        valueAngle = kStartAngle + (float) (135.0 * juce::degreesToRadians(1.0) * (v / mx)); // -135° 起，±135°
    else
        valueAngle = kStartAngle + (kEndAngle - kStartAngle) * (float) ((v - mn) / (mx - mn));
    const float zeroAngle = twoSided ? kStartAngle : kStartAngle; // 0 值位置 = -135°

    // 刻度线环：36 条（0 位置到当前值之间亮紫发光，其余暗）
    constexpr int kTicks = 36;
    constexpr float kDeg = juce::MathConstants<float>::pi / 180.0f;
    for (int i = 0; i < kTicks; ++i)
    {
        const float t = (float) i / (float) (kTicks - 1);
        const float a = kStartAngle + (kEndAngle - kStartAngle) * t;
        const bool lit = (valueAngle >= zeroAngle)
                             ? (a >= zeroAngle && a <= valueAngle)
                             : (a <= zeroAngle && a >= valueAngle);
        if (lit)
        {
            g.setColour(Theme::accent.withAlpha(0.30f));
            g.drawLine(centre.x + (tickR0 - 1.5f) * std::cos(a), centre.y + (tickR0 - 1.5f) * std::sin(a),
                       centre.x + (tickR1 + 1.5f) * std::cos(a), centre.y + (tickR1 + 1.5f) * std::sin(a), 3.0f);
            g.setColour(Theme::accentHi.withAlpha(0.9f));
            g.drawLine(centre.x + tickR0 * std::cos(a), centre.y + tickR0 * std::sin(a),
                       centre.x + tickR1 * std::cos(a), centre.y + tickR1 * std::sin(a), 1.4f);
        }
        else
        {
            g.setColour(Theme::panelEdge.withAlpha(0.85f));
            g.drawLine(centre.x + tickR0 * std::cos(a), centre.y + tickR0 * std::sin(a),
                       centre.x + tickR1 * std::cos(a), centre.y + tickR1 * std::sin(a), 1.0f);
        }
    }

    // 指针尖端光点：落在当前值刻度线上
    {
        const float pr = (tickR0 + tickR1) * 0.5f;
        const juce::Point<float> ep { centre.x + pr * std::cos(valueAngle),
                                      centre.y + pr * std::sin(valueAngle) };
        g.setColour(Theme::accent.withAlpha(0.40f));
        g.fillEllipse(ep.x - 5.5f, ep.y - 5.5f, 11.0f, 11.0f);
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillEllipse(ep.x - 1.6f, ep.y - 1.6f, 3.2f, 3.2f);
    }

    // 立体科技旋钮体：
    // 1) 金属切面外环（外高光 + 内暗槽）
    const auto ringOuter = area.reduced(radius * 0.30f);
    const auto ringInner = area.reduced(radius * 0.36f);
    g.setColour(Theme::panelEdge.darker(0.5f));
    g.fillEllipse(ringOuter);
    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawEllipse(ringOuter, 1.0f);                     // 外沿高光
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawEllipse(ringInner, 1.2f);                     // 内沿暗槽

    // 2) 立体球面旋钮体（高光偏移 + 底部反射）
    const auto body = area.reduced(radius * 0.42f);
    juce::ColourGradient sphere(Theme::knobBodyHi, centre.translated(-radius * 0.28f, -radius * 0.30f),
                                Theme::knobBody.darker(0.35f), centre.translated(radius * 0.30f, radius * 0.34f), false);
    g.setGradientFill(sphere);
    g.fillEllipse(body);
    // 左上高光（球面反光）
    g.setColour(juce::Colours::white.withAlpha(0.20f));
    g.fillEllipse(body.reduced(body.getWidth() * 0.30f).translated(-body.getWidth() * 0.05f, -body.getHeight() * 0.06f));
    // 底部紫色反射光
    juce::ColourGradient reflect(Theme::accent.withAlpha(0.20f), centre.x, body.getBottom(),
                                 juce::Colours::transparentBlack, centre.x, body.getCentreY() + body.getHeight() * 0.2f, false);
    g.setGradientFill(reflect);
    g.fillEllipse(body.reduced(body.getWidth() * 0.08f));
    // 边缘描边
    g.setColour(Theme::accent.withAlpha(0.5f));
    g.drawEllipse(body, 1.0f);

    // 3) 环绕轨道环（科技感：细紫环 + 随值旋转的轨道光点）
    const float orbR = body.getWidth() * 0.46f;
    g.setColour(Theme::accent.withAlpha(0.28f));
    g.drawEllipse(centre.x - orbR, centre.y - orbR, orbR * 2, orbR * 2, 1.0f);
    const juce::Point<float> orb { centre.x + orbR * std::cos(valueAngle + 1.5708f),
                                   centre.y + orbR * std::sin(valueAngle + 1.5708f) };
    g.setColour(Theme::accent.withAlpha(0.35f));
    g.fillEllipse(orb.x - 3.0f, orb.y - 3.0f, 6.0f, 6.0f);
    g.setColour(Theme::accentHi);
    g.fillEllipse(orb.x - 1.4f, orb.y - 1.4f, 2.8f, 2.8f);

    // 细指针线（发光：先粗暗层再亮层）
    const float ptrR = tickR0;
    const juce::Point<float> ptrEnd { centre.x + ptrR * std::cos(valueAngle),
                                      centre.y + ptrR * std::sin(valueAngle) };
    g.setColour(Theme::accent.withAlpha(0.5f));
    g.drawLine(centre.x, centre.y, ptrEnd.x, ptrEnd.y, 3.0f);
    g.setColour(Theme::accentHi);
    g.drawLine(centre.x, centre.y, ptrEnd.x, ptrEnd.y, 1.6f);

    // 悬停/拖动光圈（外扩发光，随值脉冲）
    if (slider.isMouseOverOrDragging())
    {
        const float pulse = 0.16f + 0.10f * (0.5f + 0.5f * std::sin(slider.getValue() * 4.0f));
        g.setColour(Theme::accent.withAlpha(pulse));
        g.fillEllipse(area.expanded(4.0f));
    }
}

// 标签/数值统一：5 层模糊辉光 + 字号下限（小字自动放大）
void MoonVocLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    auto font = label.getFont();
    if (font.getHeight() > 1.0f && font.getHeight() < 14.0f)
        font = font.withHeight(15.0f);

    const auto textCol = label.findColour(juce::Label::textColourId);
    Theme::drawGlowText(g, label.getText(), label.getLocalBounds().toFloat(),
                        font, textCol, label.getJustificationType());
}

// 开关：LED 风格（on = 紫色发光，off = 暗灰）
void MoonVocLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                                          bool, bool down)
{
    const auto r = b.getLocalBounds().toFloat();
    const float ledSize = 8.0f;
    const juce::Rectangle<float> led { r.getX() + 4.0f,
                                       r.getCentreY() - ledSize * 0.5f,
                                       ledSize, ledSize };

    const bool on = b.getToggleState();

    // LED 光晕 + 本体
    if (on)
    {
        g.setColour(Theme::accent.withAlpha(0.25f));
        g.fillEllipse(led.expanded(4.0f));
        g.setColour(Theme::accent);
        g.fillEllipse(led);
        g.setColour(Theme::accentHi);
        g.fillEllipse(led.reduced(2.0f));
    }
    else
    {
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(led.expanded(4.0f));
        g.setColour(juce::Colours::grey.withAlpha(0.5f));
        g.fillEllipse(led);
    }

    // 标签文字
    g.setColour(on ? Theme::textMain : Theme::textDim);
    g.setFont(juce::Font(10.5f));
    g.drawText(b.getButtonText(), juce::Rectangle<float>(led.getRight() + 6.0f, r.getY(),
                                                         r.getWidth() - led.getRight() - 10.0f,
                                                         r.getHeight()),
               juce::Justification::centredLeft);
}

// 下拉框：深色面板 + 紫色箭头
void MoonVocLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                      int bx, int by, int bw, int bh, juce::ComboBox& cb)
{
    const auto r = juce::Rectangle<int>(0, 0, w, h).toFloat();

    g.setColour(Theme::panel.darker(0.25f));
    g.fillRoundedRectangle(r, 4.0f);
    if (cb.isMouseOver())
    {
        g.setColour(Theme::panel.brighter(0.10f));
        g.fillRoundedRectangle(r, 4.0f);
    }
    g.setColour(cb.isMouseOver() ? Theme::accent.withAlpha(0.8f) : Theme::panelEdge);
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    // 箭头
    juce::Path arrow;
    const float cx = (float) bx + bw * 0.5f, cy = (float) by + bh * 0.5f;
    arrow.addTriangle(cx - 3.0f, cy - 1.0f, cx + 3.0f, cy - 1.0f, cx, cy + 2.5f);
    g.setColour(Theme::accent);
    g.fillPath(arrow);
}

void MoonVocLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                           bool isSeparator, bool isActive, bool isHighlighted,
                                           bool isTicked, bool, const juce::String& text,
                                           const juce::String&, const juce::Drawable*,
                                           const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour(Theme::panelEdge.withAlpha(0.6f));
        g.drawLine((float) area.getX(), (float) area.getCentreY(),
                   (float) area.getRight(), (float) area.getCentreY(), 1.0f);
        return;
    }

    if (! isActive)
        return;

    if (isHighlighted)
    {
        g.setColour(Theme::accent.withAlpha(0.35f));
        g.fillRoundedRectangle(area.toFloat().reduced(2.0f), 4.0f);
    }

    g.setColour(isHighlighted ? Theme::textMain : Theme::textDim);
    g.setFont(juce::Font(12.0f));
    g.drawText(text, area.reduced(8, 0), juce::Justification::centredLeft);

    if (isTicked)
    {
        g.setColour(Theme::accentHi);
        g.fillEllipse((float) area.getRight() - 16.0f, (float) area.getCentreY() - 3.0f, 6.0f, 6.0f);
    }
}
