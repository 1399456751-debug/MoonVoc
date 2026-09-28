#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"
#include "MoonVocStrings.h"
#include "MoonVocLookAndFeel.h"

// ⓘ 入口徽章：全局卡右上角的小圆按钮。
// 自绘而不走 juce::TextButton —— LAF 的按钮绘制只在带 moonvocArcColor 属性时才自绘，
// 用 TextButton 会掉进默认的圆角矩形样式里，和别的分段按钮串味。
class InfoBadge : public juce::Component
{
public:
    InfoBadge();

    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;
    void mouseUp    (const juce::MouseEvent&) override;

private:
    bool hover = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InfoBadge)
};

// 「关于」浮层：整幅插画 + 标题/版本 + 介绍 + 模块清单 + 技术注脚 + 联系与版权。
// 作为 Canvas 的子组件覆盖整个设计区（1496×672），因此自动跟随 UI 缩放与大字模式。
// 卡片高度按文字实测高度自适应（钳制 360~656），不写死。
class AboutOverlay : public juce::Component, private juce::Timer
{
public:
    // 一个文字块：内容 + 排版参数 + 量算出的矩形（dumpLayout/自检读取 bounds）
    struct Block
    {
        juce::String text;
        juce::Font   font;
        juce::Colour colour;
        float        lineSpacingMultiple = 1.0f;
        bool         gradient = false;   // 标题：用品牌渐变绘制（TextLayout 不支持渐变）
        juce::Rectangle<int> bounds;
    };

    AboutOverlay();

    void setLanguage (bool zh);        // 语言变化时由 editor 调用（影响字体与量高）
    void open (bool animate = true);   // 显示并置顶；animate=false 直接不透明（截图/自检用）
    void close();

    bool isOpen() const noexcept { return isVisible(); }

    juce::Rectangle<int> getCardBounds() const noexcept        { return cardBounds; }
    juce::Rectangle<int> getImageBounds() const noexcept       { return imageBounds; }
    juce::Rectangle<int> getRightColumnBounds() const noexcept { return rightColumn; }
    juce::Rectangle<int> getCloseBounds() const noexcept       { return closeBounds; }
    const std::vector<Block>& getBlocks() const noexcept       { return blocks; }
    float getOpacity() const noexcept                          { return opacity; }

    // 这个位置被点击时该不该关闭：✕ 或卡片外（遮罩）→ true；卡片内部 → false。
    // 抽成纯函数是为了能直接断言（合成 juce::MouseEvent 需要 Desktop，不值得）
    bool shouldCloseOnClickAt (juce::Point<int> p) const noexcept;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void rebuildBlocks();     // 生成 8 个文字块（内容 + 排版参数）
    void recomputeMetrics();  // 量高 → 卡片/两栏/各块矩形（不依赖 Graphics）

    // 块 → 排版串：量高（recomputeMetrics）与绘制（paint）共用同一份排版参数，
    // 两处各写一遍迟早会漂移
    static juce::AttributedString attributeOf (const Block&);

    bool  zh = true;
    float opacity = 1.0f;
    bool  hoveringClose = false;
    int   cardHeight = 600;
    juce::Rectangle<int> cardBounds, imageBounds, rightColumn, closeBounds;
    std::vector<Block> blocks;
    juce::Image artwork;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutOverlay)
};
