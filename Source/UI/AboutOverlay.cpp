#include "AboutOverlay.h"

namespace
{
    constexpr int kCardW      = 1080;  // 卡片宽（设计坐标）
    constexpr int kImageColW  = 440;   // 左栏（插画）宽
    constexpr int kTextPad    = 38;    // 右栏左右内边距
    constexpr int kTopPad     = 34;
    constexpr int kBotPad     = 34;
    constexpr int kCardMinH   = 360;
    constexpr int kCardMaxH   = 656;   // 画布 672，上下各留 8
    constexpr int kCloseSize  = 28;
    constexpr int kCloseInset = 14;

    // JUCE 9 的 AttributedString::lineSpacing 是「额外」行距（加在行高之上），不是倍数
    float extraLineSpacing (const juce::Font& f, float multiple)
    {
        return f.getHeight() * juce::jmax (0.0f, multiple - 1.0f);
    }
}

// ------------------------------------------------------------------ InfoBadge

InfoBadge::InfoBadge()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void InfoBadge::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);

    if (hover)
    {
        g.setColour (Theme::cardGlobalDeep.withAlpha (0.14f));
        g.fillEllipse (r);
    }

    g.setColour (Theme::cardGlobalDeep);
    g.drawEllipse (r, 1.4f);
    g.setFont (Theme::uiFont (14.0f));
    g.drawText ("i", getLocalBounds(), juce::Justification::centred);
}

void InfoBadge::mouseEnter (const juce::MouseEvent&) { hover = true;  repaint(); }
void InfoBadge::mouseExit  (const juce::MouseEvent&) { hover = false; repaint(); }
void InfoBadge::mouseUp    (const juce::MouseEvent&) { if (onClick) onClick(); }

// --------------------------------------------------------------- AboutOverlay

juce::AttributedString AboutOverlay::attributeOf (const Block& b)
{
    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::byWord);   // JUCE 9 走 Unicode 换行规则，中文可断
    as.setLineSpacing (extraLineSpacing (b.font, b.lineSpacingMultiple));
    as.append (b.text, b.font, b.colour);
    return as;
}

AboutOverlay::AboutOverlay()
{
    artwork = juce::ImageCache::getFromMemory (BinaryData::artwork_png, BinaryData::artwork_pngSize);
    setWantsKeyboardFocus (true);
    setInterceptsMouseClicks (true, true);   // 显示时必须挡住底下的旋钮
    setVisible (false);
}

void AboutOverlay::setLanguage (bool zhIn)
{
    zh = zhIn;
    recomputeMetrics();
    repaint();
}

void AboutOverlay::open (bool animate)
{
    recomputeMetrics();
    setVisible (true);
    toFront (true);          // 控件是后加入 canvas 的，必须置顶才盖得住
    grabKeyboardFocus();     // Esc 关闭需要键盘焦点

    if (animate) { opacity = 0.0f; startTimerHz (60); }
    else         { opacity = 1.0f; stopTimer(); }

    repaint();
}

void AboutOverlay::close()
{
    stopTimer();
    opacity = 1.0f;
    hoveringClose = false;
    setVisible (false);
}

void AboutOverlay::timerCallback()
{
    opacity += 1.0f / 9.0f;                       // 60Hz × 9 帧 ≈ 150ms
    if (opacity >= 1.0f) { opacity = 1.0f; stopTimer(); }
    repaint();
}

void AboutOverlay::resized()
{
    recomputeMetrics();
}

void AboutOverlay::rebuildBlocks()
{
    // 分隔符是 UTF-8（·），必须走 S8() —— 裸 const char* 会被按 ASCII 解码
    const juce::String versionLine = Strings::get (Strings::kVersion, zh) + " "
                                   + JucePlugin_VersionString + S8("   ·   TUJZMIXING");

    const auto body = Theme::uiFont (11.5f);

    blocks = {
        // 文本, 字体, 颜色, 行距倍数, 渐变标题, （bounds 由量算填）
        { "MoonVoc",                              Theme::fontBrand (32.0f),   Theme::accent,          1.15f, true,  {} },
        { versionLine,                            Theme::fontLabel (11.5f),   Theme::dimColour(),     1.20f, false, {} },
        { Strings::get (Strings::kAboutBlurb, zh), Theme::uiFont (13.5f),     Theme::textMain,        1.62f, false, {} },
        { Strings::get (Strings::kAboutChainTag, zh), Theme::fontSection (9.5f), Theme::cardGlobalDeep, 1.20f, false, {} },
        { Strings::get (Strings::kAboutModules, zh), body,                   Theme::textMain,        1.80f, false, {} },
        { Strings::get (Strings::kAboutNoteTag, zh), Theme::fontSection (9.5f), Theme::cardGlobalDeep, 1.20f, false, {} },
        { Strings::get (Strings::kAboutNote, zh), body,                      Theme::dimColour(),     1.60f, false, {} },
        { Strings::get (Strings::kAboutContact, zh) + "\n" + Strings::get (Strings::kAboutCredits, zh),
                                                  body,                      Theme::dimColour(),     1.60f, false, {} },
    };

    // 块间距（跟着上面顺序）：标题→版本 6；版本→介绍 22；介绍→标签 26；标签→内容 8；
    // 内容→标签 26；标签→内容 8；内容→联系版权 22
    static const int gaps[] = { 0, 6, 22, 26, 8, 26, 8, 22 };
    jassert (blocks.size() == (size_t) (sizeof (gaps) / sizeof (gaps[0])));

    const int textW = kCardW - kImageColW - kTextPad * 2;   // 564

    int y = 0;
    for (size_t i = 0; i < blocks.size(); ++i)
    {
        auto& b = blocks[i];

        int h = 0;
        if (b.gradient)
        {
            h = juce::roundToInt (std::ceil (b.font.getHeight()));   // 单行标题
        }
        else
        {
            juce::TextLayout layout;
            layout.createLayout (attributeOf (b), (float) textW);
            h = (int) std::ceil (layout.getHeight());
        }

        y += gaps[i];
        b.bounds = { 0, y, textW, h };
        y += h;
    }
}

void AboutOverlay::recomputeMetrics()
{
    if (getWidth() < kCardW || getHeight() <= 0)
        return;   // 尚未布局（编辑器构造早期会先调一次）

    rebuildBlocks();

    const int contentH = blocks.empty() ? 0 : blocks.back().bounds.getBottom();
    cardHeight = juce::jlimit (kCardMinH, kCardMaxH, contentH + kTopPad + kBotPad);

    cardBounds = juce::Rectangle<int> ((getWidth()  - kCardW) / 2,
                                       (getHeight() - cardHeight) / 2,
                                       kCardW, cardHeight);

    // 左栏整幅插画，等比、垂直居中
    if (artwork.isValid() && artwork.getWidth() > 0)
    {
        const int h = juce::roundToInt (kImageColW * (float) artwork.getHeight() / (float) artwork.getWidth());
        imageBounds = { cardBounds.getX(), cardBounds.getCentreY() - h / 2, kImageColW, h };
    }
    else
    {
        imageBounds = {};
    }

    rightColumn = { cardBounds.getX() + kImageColW + kTextPad, cardBounds.getY() + kTopPad,
                    kCardW - kImageColW - kTextPad * 2, cardHeight - kTopPad - kBotPad };

    for (auto& b : blocks)
        b.bounds.translate (rightColumn.getX(), rightColumn.getY());

    closeBounds = { cardBounds.getRight() - kCloseInset - kCloseSize, cardBounds.getY() + kCloseInset,
                    kCloseSize, kCloseSize };
}

void AboutOverlay::paint (juce::Graphics& g)
{
    if (! isVisible())
        return;

    g.setOpacity (opacity);

    // 遮罩（点它关闭）
    g.setColour (juce::Colour (0x6b1c2733));   // 深炭 42%
    g.fillAll();

    // 卡片投影 + 卡片体
    juce::DropShadow (juce::Colour (0x401c2733), 28, { 0, 10 }).drawForRectangle (g, cardBounds);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (cardBounds.toFloat(), 16.0f);

    // 左栏：白底 + 整幅插画（裁剪在卡片圆角内）
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (cardBounds.toFloat(), 16.0f);
        g.reduceClipRegion (clip);

        g.setColour (juce::Colours::white);
        g.fillRect (cardBounds.withWidth (kImageColW));

        if (artwork.isValid())
            g.drawImage (artwork, imageBounds.toFloat());
    }

    // 左右分隔线
    g.setColour (Theme::panelEdge);
    g.fillRect (cardBounds.getX() + kImageColW, cardBounds.getY(), 1, cardBounds.getHeight());

    // 右栏文字块
    for (const auto& b : blocks)
    {
        if (b.gradient)
        {
            // 渐变跨度取字形实际宽度，不是文字块宽度 —— 块宽 564 而 "MoonVoc" 只占约 73px，
            // 按块宽铺渐变会让可见部分几乎全是青蓝、珊瑚端完全看不见
            //（品牌标题同样处理，见 paintCanvas 里 titleBox 的渐变）
            const int textW = juce::GlyphArrangement::getStringWidthInt (b.font, b.text);
            const juce::ColourGradient grad (Theme::accent, (float) b.bounds.getX(), 0.0f,
                                             Theme::accent2, (float) (b.bounds.getX() + textW), 0.0f, false);
            g.setGradientFill (grad);
            g.setFont (b.font);
            g.drawText (b.text, b.bounds, juce::Justification::centredLeft);
        }
        else
        {
            juce::TextLayout layout;
            layout.createLayout (attributeOf (b), (float) b.bounds.getWidth());
            layout.draw (g, b.bounds.toFloat());
        }
    }

    // ✕ 关闭（画叉线而不是画字形：Montserrat 没有 ✕，中文模式下字形也不统一）
    {
        g.setColour (hoveringClose ? Theme::panelEdge : juce::Colours::white);
        g.fillEllipse (closeBounds.toFloat());
        g.setColour (Theme::panelEdge);
        g.drawEllipse (closeBounds.toFloat().reduced (0.5f), 1.0f);

        const auto inner = closeBounds.toFloat().reduced (9.0f);
        g.setColour (Theme::dimColour());
        // JUCE 9 去掉了 drawLine(Point, Point, thickness)，只剩 Line<float> 重载
        g.drawLine (juce::Line<float> (inner.getTopLeft(),  inner.getBottomRight()), 1.4f);
        g.drawLine (juce::Line<float> (inner.getTopRight(), inner.getBottomLeft()),  1.4f);
    }
}

void AboutOverlay::mouseMove (const juce::MouseEvent& e)
{
    const bool h = closeBounds.contains (e.getPosition());
    if (h != hoveringClose) { hoveringClose = h; repaint(); }
}

bool AboutOverlay::shouldCloseOnClickAt (juce::Point<int> p) const noexcept
{
    return closeBounds.contains (p) || ! cardBounds.contains (p);   // ✕ 或遮罩关闭；卡片内部不关
}

void AboutOverlay::mouseUp (const juce::MouseEvent& e)
{
    if (shouldCloseOnClickAt (e.getPosition()))
        close();
}

bool AboutOverlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}
