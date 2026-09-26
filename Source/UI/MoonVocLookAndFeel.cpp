#include "MoonVocLookAndFeel.h"

namespace
{
    constexpr float kStartAngle = -2.35619f; // -135°
    constexpr float kEndAngle   =  2.35619f; // +135°
    constexpr float kPi         = juce::MathConstants<float>::pi;
}

MoonVocLookAndFeel::MoonVocLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, Theme::textMain);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, Theme::accent.withAlpha(0.25f));
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::textColourId, Theme::textMain);
    setColour(juce::ComboBox::arrowColourId, Theme::accent);
    setColour(juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour(juce::PopupMenu::textColourId, Theme::textMain);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::accent.withAlpha(0.12f));
    setColour(juce::PopupMenu::highlightedTextColourId, Theme::textMain);
}

// 旋钮：轻拟物 —— track 弧环 + 青橙渐变数值弧 + 白体微渐变 + 顶部光晕 + 短指针
void MoonVocLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                          float, float, float, juce::Slider& slider)
{
    // 以 min(w,h) 为中心的正方形区域（保证正圆）
    const float size = (float) jmin(w, h);
    const juce::Rectangle<float> area((float) x + (w - size) * 0.5f,
                                      (float) y + (h - size) * 0.5f, size, size);
    const auto centre = area.getCentre();
    const float arcR = size * 0.44f;   // 弧环半径

    const double v   = slider.getValue();
    const double mn  = slider.getMinimum();
    const double mx  = slider.getMaximum();
    const bool twoSided = mn < 0.0;
    float valueAngle;
    if (twoSided)
        valueAngle = kStartAngle + (float) (135.0 * juce::degreesToRadians(1.0) * (v / mx));
    else
        valueAngle = kStartAngle + (kEndAngle - kStartAngle) * (float) ((v - mn) / (mx - mn));
    const float zeroAngle = kStartAngle;

    // 1) 弧环轨道（270° 整环，浅暖灰，圆头）
    juce::Path trackArc;
    trackArc.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, kStartAngle, kEndAngle, true);
    g.setColour(Theme::track);
    g.strokePath(trackArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    // 2) 数值弧（青 → 珊瑚渐变，从 0 到当前值）
    if (std::abs(valueAngle - zeroAngle) > 0.001f)
    {
        juce::Path valArc;
        const float a0 = jmin(zeroAngle, valueAngle);
        const float a1 = jmax(zeroAngle, valueAngle);
        valArc.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, a0, a1, true);
        // 沿弧两端做青橙渐变
        const juce::Point<float> p0 { centre.x + arcR * std::cos(a0), centre.y + arcR * std::sin(a0) };
        const juce::Point<float> p1 { centre.x + arcR * std::cos(a1), centre.y + arcR * std::sin(a1) };
        juce::ColourGradient grad(Theme::accent, p0, Theme::accent2, p1, false);
        g.setGradientFill(grad);
        g.strokePath(valArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // 3) 旋钮体投影（向下偏 2px 的柔和暗影，纵向微压扁）
    const float bodyD = arcR * 2.0f - 12.0f;
    const auto body = juce::Rectangle<float>(centre.x - bodyD * 0.5f, centre.y - bodyD * 0.5f, bodyD, bodyD);
    {
        g.setColour(Theme::shadow);
        const auto sh = body.translated(0.0f, 2.0f);
        g.fillEllipse(sh.withTrimmedTop(1.5f).withTrimmedBottom(-1.5f));
    }

    // 4) 旋钮体（白 → 微暖灰 垂直微渐变 + 浅描边）
    juce::ColourGradient bodyGrad(Theme::knobBody, centre.x, body.getY(),
                                  Theme::knobBodyHi, centre.x, body.getBottom(), false);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(body);
    g.setColour(Theme::panelEdge);
    g.drawEllipse(body, 1.0f);

    // 5) 顶部光晕高光（轻拟物关键：白色小椭圆贴顶）
    {
        const auto hl = juce::Rectangle<float>(body.getX() + body.getWidth() * 0.20f,
                                               body.getY() + body.getHeight() * 0.10f,
                                               body.getWidth() * 0.60f, body.getHeight() * 0.30f);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.fillEllipse(hl);
    }

    // 6) 短指针（从中心 0.55r 到 0.82r，accent 色，圆头）
    {
        const float r0 = bodyD * 0.5f * 0.55f;
        const float r1 = bodyD * 0.5f * 0.82f;
        const juce::Point<float> q0 { centre.x + r0 * std::cos(valueAngle), centre.y + r0 * std::sin(valueAngle) };
        const juce::Point<float> q1 { centre.x + r1 * std::cos(valueAngle), centre.y + r1 * std::sin(valueAngle) };
        g.setColour(Theme::accent);
        juce::Path ptr;
        ptr.startNewSubPath(q0);
        ptr.lineTo(q1);
        g.strokePath(ptr, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // 7) 悬停细环（弧环外 2px，accent 淡环，无脉冲）
    if (slider.isMouseOverOrDragging())
    {
        g.setColour(Theme::accent.withAlpha(0.14f));
        const float hr = arcR + 4.0f;
        g.drawEllipse(centre.x - hr, centre.y - hr, hr * 2.0f, hr * 2.0f, 1.5f);
    }
}

// 标签/数值：纯文本（无辉光），小字号抬到 13
void MoonVocLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    auto font = label.getFont();
    if (font.getHeight() > 1.0f && font.getHeight() < 13.0f)
        font = font.withHeight(13.0f);

    g.setFont(font);
    g.setColour(label.findColour(juce::Label::textColourId));
    g.drawText(label.getText(), label.getLocalBounds().toFloat(), label.getJustificationType());
}

// 开关：iOS pill（轨道 + 白滑块）
void MoonVocLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                                          bool, bool down)
{
    const auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    // 轨道 36×20
    const juce::Rectangle<float> trackR { r.getX(), r.getCentreY() - 10.0f, 36.0f, 20.0f };
    g.setColour(on ? Theme::accent : Theme::track);
    g.fillRoundedRectangle(trackR, 10.0f);

    // 滑块（白圆 d=16，on 居右 / off 居左，带轻投影）
    const float kx = on ? trackR.getRight() - 18.0f : trackR.getX() + 2.0f;
    const juce::Rectangle<float> knobR { kx, trackR.getY() + 2.0f, 16.0f, 16.0f };
    g.setColour(Theme::shadow);
    g.fillEllipse(knobR.translated(0.0f, 1.0f));
    g.setColour(Theme::knobBody);
    g.fillEllipse(knobR);

    // 标签文字
    g.setColour(on ? Theme::textMain : Theme::textDim);
    g.setFont(Theme::fontLabel(12.0f));
    g.drawText(b.getButtonText(), juce::Rectangle<float>(trackR.getRight() + 8.0f, r.getY(),
                                                         r.getWidth() - trackR.getRight() - 10.0f,
                                                         r.getHeight()),
               juce::Justification::centredLeft);
}

// 下拉框：白底圆角胶囊 + 青蓝 chevron 箭头
void MoonVocLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                      int bx, int by, int bw, int bh, juce::ComboBox& cb)
{
    const auto r = juce::Rectangle<int>(0, 0, w, h).toFloat();

    g.setColour(Theme::panel);
    g.fillRoundedRectangle(r, 8.0f);
    if (cb.isMouseOver())
    {
        g.setColour(Theme::knobBody);
        g.fillRoundedRectangle(r, 8.0f);
    }
    g.setColour(cb.isMouseOver() ? Theme::accent.withAlpha(0.6f) : Theme::panelEdge);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    // 选中文字由 JUCE 内部 Label 绘制（ComboBox::textColourId = textMain），此处不重复画

    // chevron 箭头（向下 V）
    juce::Path chev;
    const float cx = (float) bx + bw * 0.5f, cy = (float) by + bh * 0.5f;
    chev.startNewSubPath(cx - 3.5f, cy - 1.5f);
    chev.lineTo(cx, cy + 2.0f);
    chev.lineTo(cx + 3.5f, cy - 1.5f);
    g.setColour(cb.isMouseOver() ? Theme::accent2 : Theme::accent);
    g.strokePath(chev, juce::PathStrokeType(1.6f));
}

// 下拉框文字：左对齐 + 左内边距 10px，右侧留箭头区
void MoonVocLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(10, 1, box.getWidth() - 10 - box.getHeight(), box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

void MoonVocLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                           bool isSeparator, bool isActive, bool isHighlighted,
                                           bool isTicked, bool, const juce::String& text,
                                           const juce::String&, const juce::Drawable*,
                                           const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour(Theme::panelEdge);
        g.drawLine((float) area.getX() + 8.0f, (float) area.getCentreY(),
                   (float) area.getRight() - 8.0f, (float) area.getCentreY(), 1.0f);
        return;
    }

    if (! isActive)
        return;

    if (isHighlighted)
    {
        g.setColour(Theme::accent.withAlpha(0.12f));
        g.fillRoundedRectangle(area.toFloat().reduced(3.0f, 1.0f), 6.0f);
    }

    g.setColour(isHighlighted ? Theme::textMain : Theme::textDim);
    g.setFont(Theme::fontLabel(12.0f));
    g.drawText(text, area.reduced(10, 0), juce::Justification::centredLeft);

    if (isTicked)
    {
        g.setColour(Theme::accent);
        g.fillEllipse((float) area.getRight() - 16.0f, (float) area.getCentreY() - 3.0f, 6.0f, 6.0f);
    }
}
