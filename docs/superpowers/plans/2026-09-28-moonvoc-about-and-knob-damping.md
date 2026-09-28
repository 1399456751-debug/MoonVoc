# MoonVoc 关于界面 + 旋钮阻尼 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给 MoonVoc 加一个插件内的「关于」浮层（插画 + 标题/版本 + 介绍 + 模块清单 + 技术注脚 + 联系与版权），用全局卡右上角的 ⓘ 按钮打开；同时把所有旋钮的拖动阻尼从 250 加重到 500。

**Architecture:** 浮层是 `MoonVocEditor::Canvas` 的子组件（不是独立窗口，宿主里不会被挤掉），覆盖整个设计区 `1496×672`。因为它活在 Canvas 里，自动跟随 UI 缩放（100~300%）与「大字」模式。文本全部走 `Strings` 中英双表；卡片高度按 `juce::TextLayout` 实测文字高度自适应，不写死。插画经 `juce_add_binary_data` 嵌入。

**Tech Stack:** C++17 / JUCE 9（`C:\JUCE`，headless 架构，API 与旧教程差异大）/ CMake + Ninja / MSVC。

**Spec:** `docs/superpowers/specs/2026-09-28-moonvoc-about-and-knob-damping-design.md`

## Global Constraints

- **平台**：Windows 11 + Git Bash；所有构建走 `powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'"`（bash 直接调 cmd 有引号坑）。项目路径含空格，**所有 shell 里的路径必须加引号**。
- **JUCE 9 差异（本次踩到的，务必照做）**：
  - `juce::AttributedString` 用 `append (text, font, colour)` 添加内容；**没有 `withFont`**（旧教程写法）。
  - `AttributedString::setLineSpacing()` 是**额外行距（加在行高之上的绝对量）**，不是倍数 —— 倍数要换算成 `font.getHeight() * (multiple - 1)`。
  - `juce::TextLayout`：`createLayout (const AttributedString&, float maxWidth)` + `getHeight()` + `draw (Graphics&, Rectangle<float>)`。
  - `juce::Font` 只有单参构造（用 `.withHeight`）；`juce::Colour` 非 constexpr（用 `const`）。
  - **`juce::Graphics::drawLine (Point, Point, float)` 不存在**（JUCE 9 只留 `drawLine (Line<float>, float)`）——
    Task 3 画 ✕ 时踩到（C2661），正确写法 `g.drawLine (juce::Line<float> (a, b), 1.4f)`。
  - `juce::String (const char*)` 按 **ASCII** 解码 —— 中文字面量必须走 `ParamIDs.h` 的 `S8()`。
- **非 ASCII 内容一律走 `S8()`** —— 中文，以及含 `·`、全角空格等非 ASCII 字符的英文串；纯 ASCII 的英文串用裸 `juce::String`。
  要避免的是「非 ASCII 字节被当 ASCII 解码」（`·` 变 `Â·`），不是「出现非 ASCII 字符」（Task 2 曾误读此规则，见 Task 2 Step 3 的注意）。新增 `Strings::Key` 必须中英双份，缺一不可（Task 2 有自检）。
- **不动**：全部 DSP、`ParamID`（不新增音频参数）、现有布局与配色、现有控件行为。
- **版本号**：保持在 `0.9.0`，不打包、不发布（发布等用户指示）。
- **测试基线**：改完必须 `MoonVocHeadlessTest.exe` EXIT=0（无 FAIL）且 `MoonVocUiSnapshot.exe` 打印 `dumpLayout: FAIL=0`。

---

### Task 1: 嵌入插画素材（二进制数据 target 更名）

插画是这次浮层的主视觉，先让它在编译期可被 `BinaryData` 引用。同时把 `juce_add_binary_data` 的 target 从 `MoonVocFonts` 改名为 `MoonVocAssets` —— 它接下来不只装字体，名字骗人会让后来者找不到插画在哪。

**Files:**
- Add: `assets/artwork.png`（从 TMIXTOOL 复制，1400×1116）
- Modify: `CMakeLists.txt`（第 34 行 `juce_add_binary_data`；第 52、97、131、166 行 `target_link_libraries` 里的 `MoonVocFonts`）

**Interfaces:**
- Consumes: 无
- Produces: 生成头 `BinaryData.h` 中的 `BinaryData::artwork_png` / `BinaryData::artwork_pngSize`（CMake target 名 `MoonVocAssets`，供 Task 3 的 `target_link_libraries` 使用）

- [x] **Step 1: 复制素材进项目**

```bash
cp "E:/VST Effects Plugin Collection/TMIXTOOL/src/gui/assets/artwork.png" \
   "E:/VST Effects Plugin Collection/moonvoc/assets/artwork.png"
ls -l "E:/VST Effects Plugin Collection/moonvoc/assets/"
```

Expected: `artwork.png` 约 700KB（1400×1116 线稿）

- [x] **Step 2: 改 CMakeLists（二进制数据块 + 改名）**

把第 33-38 行：

```cmake
# 嵌入字体（Montserrat Bold / ExtraBold，OFL 开源协议）
juce_add_binary_data(MoonVocFonts
    SOURCES
        fonts/Montserrat-Bold.ttf
        fonts/Montserrat-ExtraBold.ttf
)
```

改成：

```cmake
# 嵌入资源（Montserrat Bold/ExtraBold 字体，OFL；关于界面插画 artwork.png，TUJZMIXING 自有）
juce_add_binary_data(MoonVocAssets
    SOURCES
        fonts/Montserrat-Bold.ttf
        fonts/Montserrat-ExtraBold.ttf
        assets/artwork.png
)
```

- [x] **Step 3: 把 4 处 `MoonVocFonts` 链接改成 `MoonVocAssets`**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && grep -n "MoonVocFonts" CMakeLists.txt
```

Expected: 4 行命中。逐一改成 `MoonVocAssets`（第 52 行的 `MoonVoc`、第 97 行 `MoonVocHeadlessTest`、第 131 行 `MoonVocRender`、第 166 行 `MoonVocUiSnapshot`）。

改完复查（应无输出）：

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && grep -n "MoonVocFonts" CMakeLists.txt
```

- [x] **Step 4: 重新配置并构建**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'"
```

Expected: 构建成功（会有一轮较长的 reconfigure，因为 target 改名了）

- [x] **Step 5: 验证 BinaryData 里出现了插画符号**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && grep -rn "artwork_png" build/ --include="BinaryData.h" | head -3
```

Expected: 打印出 `artwork_png` 与 `artwork_pngSize` 的声明

- [x] **Step 6: 回归测试仍全绿**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && ./build/MoonVocHeadlessTest.exe | tail -5; echo "EXIT=$?"
```

Expected: `EXIT=0`，无 `FAIL`

- [x] **Step 7: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && git add assets/artwork.png CMakeLists.txt && git commit -F - <<'EOF'
build: 嵌入关于界面插画，二进制数据 target 更名 MoonVocAssets

- assets/artwork.png（TUJZMIXING 插画，1400×1116）入库
- juce_add_binary_data 目标 MoonVocFonts → MoonVocAssets（不再只装字体）
EOF
```

---

### Task 2: 字符串表扩充（含中英缺项自检）

**Files:**
- Modify: `Source/UI/MoonVocStrings.h`
- Test: `test/HeadlessTest.cpp`

**Interfaces:**
- Consumes: 无
- Produces: `Strings::Key` 新增 `kVersion, kAboutBlurb, kAboutChainTag, kAboutModules, kAboutNoteTag, kAboutNote, kAboutContact, kAboutCredits`，以及枚举末尾的哨兵 `kCount` 与自检函数 `bool Strings::allKeysFilled()`（中英任一语言缺项返回 false）

- [x] **Step 1: 写失败的测试**

在 `test/HeadlessTest.cpp` 顶部 include 区加：

```cpp
#include "../Source/UI/MoonVocStrings.h"
```

在 `int main()` 内、`TRACE("--- ...")` 之类的最开始处（找一个明显是开头的行之后）插入：

```cpp
    // 文本表自检：新增 key 必须中英双份，漏一个就红
    {
        const bool ok = Strings::allKeysFilled();
        TRACE("strings table filled: %s\n", ok ? "OK" : "FAIL");
        if (! ok) return 1;
    }
```

- [x] **Step 2: 运行测试确认失败**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'" 2>&1 | tail -20
```

Expected: **编译失败** —— `no member named 'allKeysFilled' in namespace 'Strings'`

- [x] **Step 3: 实现（改 `Source/UI/MoonVocStrings.h`）**

枚举（第 9-19 行）在 `kScale, kLevel` 之后补新 key 与哨兵：

```cpp
        kBypass, kOversampling, kLanguage, kLargeFont, kScale, kLevel,
        kVersion, kAboutBlurb, kAboutChainTag, kAboutModules, kAboutNoteTag, kAboutNote,
        kAboutContact, kAboutCredits,
        kCount   // 哨兵：必须保持在最后（自检遍历用）
```

`get()` 的 switch 在 `case kLevel:` 之后补：

```cpp
            case kVersion:      return zh ? S8("版本") : S8("VERSION");
            case kAboutBlurb:
                return zh ? S8("MoonVoc 把整条人声链收进一个窗口：四段智能 EQ、三级母带式压缩、齿音控制、染色、瞬态整形与混响，按真实的混音顺序排列，每一环都可独立旁通。它不替你决定声音，只把每个决定做得干净利落。")
                          : juce::String("MoonVoc gathers an entire vocal chain into one window - four-band intelligent EQ, three-stage mastering compression, de-essing, saturation, transient shaping and reverb - arranged in the order a mix actually happens, every stage independently bypassable. It does not decide the sound for you; it just makes each decision clean.");
            case kAboutChainTag:return zh ? S8("信号链") : S8("SIGNAL CHAIN");
            case kAboutModules:
                return zh ? S8("四段智能 EQ　Thick · De-Box · Clarity · Air\n三级压缩　FET · 光电 · 并行\n齿音控制　→　双槽染色　→　瞬态整形　→　混响")
                          : S8("Four-band intelligent EQ　Thick · De-Box · Clarity · Air\nThree-stage compression　FET · Optical · Parallel\nDe-Ess -> Dual-stage saturation -> Transient -> Reverb");
            case kAboutNoteTag: return zh ? S8("透明") : S8("TRANSPARENCY");
            case kAboutNote:
                return zh ? S8("所有算法以透明为基准：参数归零时，信号逐样本还原；超采样链采用线性相位 FIR 半带滤波，4x 下残余失真低于 −85 dB。")
                          : juce::String("Every algorithm is built around transparency: with all parameters at zero the signal is returned sample for sample, and the oversampling stage uses linear-phase FIR half-band filters, keeping residual distortion below -85 dB at 4x.");
            case kAboutContact:
                return zh ? S8("反馈与建议　1399456751@qq.com　·　github.com/1399456751-debug")
                          : S8("Feedback　1399456751@qq.com　·　github.com/1399456751-debug");
            case kAboutCredits:
                return zh ? S8("© 2026 TUJZMIXING　·　基于 JUCE 构建　·　去齿音改编自 Airwindows DeBess（MIT，© Chris Johnson）")
                          : S8("(c) 2026 TUJZMIXING　·　Built on JUCE　·　De-Esser adapted from Airwindows DeBess (MIT, (c) Chris Johnson)");
```

在 `namespace Strings` 内、`get()` 之后加自检：

```cpp
    // 自检：所有 key 在中英两种语言下都必须非空（漏翻译即红）
    inline bool allKeysFilled()
    {
        for (int i = 0; i < (int) kCount; ++i)
            if (get ((Key) i, true).isEmpty() || get ((Key) i, false).isEmpty())
                return false;

        return true;
    }
```

> **注意（规则修正 2026-09-28）**：真正的规则是「**非 ASCII 内容一律走 `S8()`**」，不是「英文串必须纯 ASCII」。
> 初版计划写成后者，实现者据此把英文串里的 `·` 与全角空格换成了 `-` 和半角空格，结果与已定稿的排版不一致
> （版本行 `VERSION 0.9.0 · TUJZMIXING` 用 `·`，模块清单却用 `-`）——**这是计划文本的错，不是实现者的错**。
> 正确做法：英文串里**含** `·`、全角空格等非 ASCII 字符时，与中文一样包 `S8()`；纯 ASCII 的英文串保持裸 `juce::String`。
> 要避免的是 `juce::String(const char*)` 按 ASCII 逐字节解码非 ASCII 字节（`·` 会显示成 `Â·`），而不是避免出现非 ASCII 字符。

- [x] **Step 4: 运行测试确认通过**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'" 2>&1 | tail -20 && cd "E:/VST Effects Plugin Collection/moonvoc" && ./build/MoonVocHeadlessTest.exe | head -3
```

Expected: 编译通过，输出 `strings table filled: OK`，EXIT=0

- [x] **Step 5: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && git add Source/UI/MoonVocStrings.h test/HeadlessTest.cpp && git commit -F - <<'EOF'
i18n: 关于界面文案入表（中英双份）+ 缺项自检

- 新增 kVersion/kAboutBlurb/kAboutChainTag/kAboutModules/kAboutNoteTag/
  kAboutNote/kAboutContact/kAboutCredits 与哨兵 kCount
- HeadlessTest 断言 allKeysFilled()，漏翻译直接红
EOF
```

---

### Task 3: AboutOverlay 组件（浮层本体）

浮层组件独立成型并单独自检 —— 版面量算是这次最容易出错的环节（自适应高度、两栏不重叠、文字块不重叠），先在组件层面锁死，再接进编辑器。

**Files:**
- Create: `Source/UI/AboutOverlay.h`
- Create: `Source/UI/AboutOverlay.cpp`
- Modify: `CMakeLists.txt`（4 个 target 的源文件列表）
- Test: `test/UiSnapshot.cpp`

**Interfaces:**
- Consumes: `BinaryData::artwork_png`（Task 1）、`Strings::k*`（Task 2）、`Theme::uiFont/fontBrand/fontLabel/fontSection/dimColour/accent/panel/panelEdge/textMain/cardGlobalDeep/largeFontMode/useCjkFont`
- Produces:
  - `class InfoBadge : public juce::Component`，公开成员 `std::function<void()> onClick`
  - `class AboutOverlay : public juce::Component, private juce::Timer`，公开：
    - `struct Block { juce::String text; juce::Font font; juce::Colour colour; float lineSpacingMultiple; bool gradient; juce::Rectangle<int> bounds; };`
    - `void setLanguage (bool zh)`
    - `void open (bool animate = true)`
    - `void close()`
    - `bool isOpen() const`
    - `juce::Rectangle<int> getCardBounds() const`
    - `juce::Rectangle<int> getImageBounds() const`
    - `juce::Rectangle<int> getRightColumnBounds() const`
    - `const std::vector<Block>& getBlocks() const`
  - 构造：`AboutOverlay()`，无参

- [x] **Step 1: 写失败的测试**

在 `test/UiSnapshot.cpp` 的 include 区加：

```cpp
#include "../Source/UI/AboutOverlay.h"
```

在 `int main()` 内、创建 `MoonVocProcessor processor;` 之前插入独立自检函数调用与定义（放在 `writePng` 之后、`main` 之前）：

```cpp
// 浮层版面自检：卡片必须落在画布内、高度被钳制、两栏与文字块不重叠
static int checkAboutOverlay()
{
    int fail = 0;
    auto check = [&] (bool ok, const char* what)
    {
        if (! ok) { std::printf("FAIL: %s\n", what); ++fail; }
    };

    const juce::Rectangle<int> canvas (0, 0, MoonVocEditor::kDesignW, MoonVocEditor::kDesignH);

    AboutOverlay ov;
    ov.setBounds (canvas);

    for (const bool zh : { true, false })      // 中英两套文案都要放得下
        for (const bool large : { false, true }) // 大字模式字号 ×1.4
        {
            Theme::useCjkFont   = zh;
            Theme::largeFontMode = large;
            ov.setLanguage (zh);

            const auto card = ov.getCardBounds();
            check (canvas.contains (card), "about card inside canvas");
            check (card.getHeight() >= 360 && card.getHeight() <= 656, "about card height clamped");
            check (card.getWidth() == 1080, "about card width 1080");
            check (ov.getImageBounds().getHeight() <= card.getHeight(), "artwork fits card height");
            check (ov.getRightColumnBounds().getX() >= ov.getImageBounds().getRight(),
                   "about columns disjoint");

            const auto& blocks = ov.getBlocks();
            for (size_t i = 0; i < blocks.size(); ++i)
                for (size_t j = i + 1; j < blocks.size(); ++j)
                    check (! blocks[i].bounds.intersects (blocks[j].bounds),
                           "about text blocks disjoint");

            // 点击归属：卡片内部不关、✕ 关、遮罩关（中英/大字下坐标都会变，四种组合全覆盖）
            check (! ov.shouldCloseOnClickAt (card.getCentre()), "card interior click keeps overlay open");
            check (ov.shouldCloseOnClickAt (ov.getCloseBounds().getCentre()), "close button click closes");
            check (ov.shouldCloseOnClickAt ({ 4, 4 }), "backdrop click closes");
        }

    Theme::largeFontMode = false;
    Theme::useCjkFont = true;
    std::printf ("checkAboutOverlay: FAIL=%d\n", fail);
    return fail;
}
```

在 `main()` 里 `juce::ScopedJuceInitialiser_GUI guiInit;` 之后插入：

```cpp
    if (checkAboutOverlay() != 0)
    {
        std::printf("about overlay check failed\n");
        return 1;
    }
```

- [x] **Step 2: 运行确认失败**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'" 2>&1 | tail -20
```

Expected: **编译失败** —— `Cannot find source file: Source/UI/AboutOverlay.cpp` 或找不到 `AboutOverlay` 类型

- [x] **Step 3: 写 `Source/UI/AboutOverlay.h`**

```cpp
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
```

- [x] **Step 4: 写 `Source/UI/AboutOverlay.cpp`**

```cpp
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
        g.drawLine (inner.getTopLeft(),     inner.getBottomRight(), 1.4f);
        g.drawLine (inner.getTopRight(),    inner.getBottomLeft(),  1.4f);
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
```


- [x] **Step 5: 把 `Source/UI/AboutOverlay.cpp` 加进 4 个 target**

在 `CMakeLists.txt` 中每个 `Source/UI/MoonVocLookAndFeel.cpp` 后面加一行 `Source/UI/AboutOverlay.cpp`，共 4 处（`MoonVoc`、`MoonVocHeadlessTest`、`MoonVocRender`、`MoonVocUiSnapshot` 的源文件列表）。

复查命中数应为 4：

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && grep -c "UI/AboutOverlay.cpp" CMakeLists.txt
```

- [x] **Step 6: 构建并运行自检**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'" 2>&1 | tail -20 && cd "E:/VST Effects Plugin Collection/moonvoc" && ./build/MoonVocUiSnapshot.exe | head -20
```

Expected: 编译通过；输出 `checkAboutOverlay: FAIL=0`；EXIT=0

若出现 `about card inside canvas` 或 `height clamped` 的 FAIL：说明文字量高超出 656 上限，检查 `gaps[]` 与字号是否照抄（大字模式 ×1.4 后内容约 590，正常不会越界）。

- [x] **Step 7: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && git add Source/UI/AboutOverlay.h Source/UI/AboutOverlay.cpp CMakeLists.txt test/UiSnapshot.cpp && git commit -F - <<'EOF'
ui: 新增关于浮层组件 AboutOverlay（双栏 + 自适应高度 + 淡入）

- 左栏整幅插画（等比、垂直居中），右栏 8 个文字块（标题渐变/版本/介绍/
  模块/注脚/联系版权），TextLayout 实测高度 → 卡片高度钳制 360~656
- 点遮罩 / ✕ / Esc 关闭；150ms 淡入（open(false) 供截图与自检跳过动画）
- InfoBadge ⓘ 徽章（自绘，不走 LAF 分段按钮样式）
- UiSnapshot 增 checkAboutOverlay()：中英 × 标准/大字 四组合的版面断言
EOF
```

---

### Task 4: 把浮层接进编辑器

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`（构造函数、`layoutCanvas`、`applyLanguage`、`dumpLayout`）
- Test: `test/UiSnapshot.cpp`（加 3 张关于界面截图）

**Interfaces:**
- Consumes: `AboutOverlay` / `InfoBadge`（Task 3）
- Produces: `void MoonVocEditor::openAbout (bool animate = true)` —— 公开方法，Task 5 的 ⓘ 按钮与 UiSnapshot 都用它

- [x] **Step 1: 写失败的测试**

在 `test/UiSnapshot.cpp` 的 `main()` 内、现有 4 个 `shot(...)` 调用之后加：

```cpp
    // 关于浮层：中文 / 英文 / 中文大字（animate=false 关掉淡入，否则快照时还是透明的）
    const auto shotAbout = [&] (const juce::String& fileName, bool zh, bool large)
    {
        setP ("uiLanguage", zh ? 0.0f : 1.0f);
        setP ("uiLargeFont", large ? 1.0f : 0.0f);
        setP ("uiScale", 1.0f);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setSize (MoonVocEditor::kDesignW, MoonVocEditor::kDesignH);
        editor->resized();

        if (auto* me = dynamic_cast<MoonVocEditor*> (editor.get()))
        {
            me->dumpLayout();
            me->openAbout (false);
            for (int i = 0; i < 30; ++i) me->demoTick();
        }

        writePng (editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f), fileName);
    };

    shotAbout ("ui_snapshot_about.png",       true,  false);
    shotAbout ("ui_snapshot_about_en.png",    false, false);
    shotAbout ("ui_snapshot_about_large.png", true,  true);
```

在 `checkAboutOverlay()` 里补一条断言（放在函数内 `AboutOverlay ov;` 那段之后不行 —— 用编辑器路径断言更直接，见下一步 `dumpLayout`）。

- [x] **Step 2: 运行确认失败**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'" 2>&1 | tail -20
```

Expected: **编译失败** —— `no member named 'openAbout' in 'MoonVocEditor'`

- [x] **Step 3: 改 `Source/PluginEditor.h`**

在 include 区加（`#include "UI/MoonVocStrings.h"` 之后）：

```cpp
#include "UI/AboutOverlay.h"
```

公开区（`void dumpLayout() const;` 下方）加：

```cpp
    // 关于浮层入口（ⓘ 按钮 / 截图与自检用；animate=false 跳过淡入）
    void openAbout (bool animate = true);

    // 自检/测试读取用（与 dumpLayout 同性质的测试面，不参与生产逻辑）
    InfoBadge&    getInfoBadge()   noexcept { return infoBadge; }
    AboutOverlay& getAboutOverlay() noexcept { return aboutOverlay; }
```

私有成员区（`juce::Image bgBlurCache;` 附近）加：

```cpp
    AboutOverlay aboutOverlay;   // 「关于」浮层（覆盖整个设计区）
    InfoBadge    infoBadge;      // 全局卡右上角的 ⓘ 入口
```

- [x] **Step 4: 改 `Source/PluginEditor.cpp`**

（a）构造函数末尾（`startTimerHz (10);` 之前）加：

```cpp
    // 关于浮层 + ⓘ 入口。加入顺序不影响层级：open() 里会 toFront() 盖到所有控件之上
    infoBadge.onClick = [this] { openAbout(); };
    canvas.addAndMakeVisible (infoBadge);
    // 浮层必须用 addChildComponent：addAndMakeVisible 会覆盖构造函数里的 setVisible(false)，
    // 导致浮层开机即显示（isOpen() 就是 isVisible()），四张既有截图会被整片盖住
    canvas.addChildComponent (aboutOverlay);
```

（b）`applyLanguage()` 末尾（`canvas.repaint();` 之前）加：

```cpp
    aboutOverlay.setLanguage (zh);
```

（c）新增方法（放在 `applyScaleFromParam()` 之后）：

```cpp
void MoonVocEditor::openAbout (bool animate)
{
    aboutOverlay.setLanguage (currentZh);
    aboutOverlay.open (animate);
}
```

（d）`layoutCanvas()` 末尾（最后一个 `}` 之前）加：

```cpp
    // ⓘ 入口：全局卡右上角，右缩 16 / 上缩 14（右下 10px 外就是工作电平指示灯，不能压）
    infoBadge.setBounds (cardGlobal.getRight() - 16 - 26, cardGlobal.getY() + 14, 26, 26);
    // 关于浮层：覆盖整个设计区（setBounds 会触发 resized → 重新量算版面）
    aboutOverlay.setBounds (0, 0, kDesignW, kDesignH);
```

（e）`dumpLayout()` 末尾（`std::printf("dumpLayout: FAIL=%d\n", fail);` 之前）加：

```cpp
    // ⓘ 入口：必须在全局卡内、不压到右端指示灯、且真的接上了开合
    owned (cardGlobal, "infoBadge", infoBadge.getBounds());
    check (! infoBadge.getBounds().intersects (indicatorRect), "infoBadge clear of indicator");
    check (infoBadge.onClick != nullptr, "info badge wired to about overlay");

    // 关于浮层：覆盖画布、卡片不越界、两栏与文字块不重叠
    check (aboutOverlay.getBounds() == juce::Rectangle<int> (0, 0, kDesignW, kDesignH),
           "about overlay covers canvas");
    check (juce::Rectangle<int> (0, 0, kDesignW, kDesignH).contains (aboutOverlay.getCardBounds()),
           "about card inside canvas");
    check (aboutOverlay.getRightColumnBounds().getX() >= aboutOverlay.getImageBounds().getRight(),
           "about columns do not overlap");
    {
        const auto& bl = aboutOverlay.getBlocks();
        for (size_t i = 0; i < bl.size(); ++i)
            for (size_t j = i + 1; j < bl.size(); ++j)
                check (! bl[i].bounds.intersects (bl[j].bounds), "about text blocks do not overlap");
    }
```

- [x] **Step 5: 构建并验证**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'" 2>&1 | tail -20 && cd "E:/VST Effects Plugin Collection/moonvoc" && ./build/MoonVocUiSnapshot.exe 2>&1 | grep -E "dumpLayout: FAIL|checkAboutOverlay|snapshot written"
```

Expected: **7 次** `dumpLayout: FAIL=0`（4 张旧截图 + 3 张关于，每个状态各调一次）、`checkAboutOverlay: FAIL=0`、7 张 `snapshot written`

- [x] **Step 6: 人工看图确认（关键）**

打开 `ui_snapshot_about.png`、`ui_snapshot_about_en.png`、`ui_snapshot_about_large.png`，确认：

1. 遮罩压暗了整个面板，卡片居中、圆角、有投影
2. 左栏是**完整**的插画（人在画面里，没有上下被切）
3. 右栏从上到下：MoonVoc（青蓝→珊瑚渐变）→ VERSION 0.9.0 · TUJZMIXING → 介绍段 → SIGNAL CHAIN → 模块三行 → TRANSPARENCY → 注脚 → 联系与版权
4. 中文版文字**正常换行**（没有长条溢出卡片右缘）—— 若溢出，把 `rebuildBlocks()` 与 `paint()` 两处的 `byWord` 改成 `byChar`
5. 大字版卡片变高但仍完整落在画布内
6. ✕ 在卡片右上角

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && start "" ui_snapshot_about.png
```

- [x] **Step 7: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && git add Source/PluginEditor.h Source/PluginEditor.cpp test/UiSnapshot.cpp && git commit -F - <<'EOF'
ui: 关于浮层接进编辑器（openAbout + 版面断言 + 三张截图）

- PluginEditor 持有 AboutOverlay/InfoBadge，openAbout() 公开给 ⓘ 与截图使用
- applyLanguage → aboutOverlay.setLanguage（中英切换重排）
- layoutCanvas 摆 ⓘ（1438,30,26,26）与浮层（覆盖 1496×672）
- dumpLayout 增断言：ⓘ 在卡内且不压指示灯、卡片不越界、两栏与文字块不重叠
- UiSnapshot 增中文/英文/大字三张关于界面截图
EOF
```

---

### Task 5: 开合交互的自动化验证

Task 4 已经把 ⓘ 摆好、把 `onClick` 接上、把浮层接进编辑器。这一步把"点开 → 三种方式关闭"跑成**回归测试**，而不是靠人手点：执行代理点不了 GUI 窗口，人工目视确认统一放在 Task 7 的截图验收。

**Files:**
- Modify: `test/UiSnapshot.cpp`（新增交互自检函数并在 `main()` 调用）

**Interfaces:**
- Consumes: `MoonVocEditor::getInfoBadge()` / `getAboutOverlay()` / `openAbout()`、`InfoBadge::onClick`、`AboutOverlay::isOpen()/close()/keyPressed()/shouldCloseOnClickAt()/getCardBounds()/getCloseBounds()`
- Produces: `static int checkAboutInteraction (MoonVocProcessor&)` —— UiSnapshot 内的自检函数，返回 FAIL 计数

- [x] **Step 1: 写失败的测试**

在 `test/UiSnapshot.cpp` 的 `checkAboutOverlay()` 之后加：

```cpp
// 开合交互自检：走的是真实点击的同一条通路（infoBadge.onClick、AboutOverlay 的
// shouldCloseOnClickAt 与 keyPressed），不合成 juce::MouseEvent（那需要 Desktop，不值当）
static int checkAboutInteraction (MoonVocProcessor& processor)
{
    int fail = 0;
    auto check = [&] (bool ok, const char* what)
    {
        if (! ok) { std::printf ("FAIL: %s\n", what); ++fail; }
    };

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (MoonVocEditor::kDesignW, MoonVocEditor::kDesignH);
    editor->resized();

    auto* me = dynamic_cast<MoonVocEditor*> (editor.get());
    if (me == nullptr) { std::printf ("FAIL: editor is not MoonVocEditor\n"); return 1; }

    auto& badge   = me->getInfoBadge();
    auto& overlay = me->getAboutOverlay();

    check (! overlay.isOpen(), "about overlay starts closed");

    // 点 ⓘ（animate=true 会起淡入定时器；下面 Esc 关闭会 stopTimer，不留悬挂定时器）
    check (badge.onClick != nullptr, "info badge has click handler");
    badge.onClick();
    check (overlay.isOpen(), "info badge click opens overlay");

    check (! overlay.shouldCloseOnClickAt (overlay.getCardBounds().getCentre()),
           "click inside card does not close");

    check (overlay.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)), "esc key is handled");
    check (! overlay.isOpen(), "esc closes overlay");

    me->openAbout (false);
    check (overlay.isOpen(), "openAbout(false) opens overlay");
    check (overlay.shouldCloseOnClickAt ({ 4, 4 }), "backdrop click closes");
    overlay.close();
    check (! overlay.isOpen(), "close() hides overlay");

    me->openAbout (false);
    check (overlay.shouldCloseOnClickAt (overlay.getCloseBounds().getCentre()),
           "close button click closes");
    overlay.close();
    check (! overlay.isOpen(), "overlay closed after close button");

    std::printf ("checkAboutInteraction: FAIL=%d\n", fail);
    return fail;
}
```

- [x] **Step 2: 运行确认失败**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'" 2>&1 | tail -20
```

Expected: **编译失败** —— `no member named 'getInfoBadge'` / `no member named 'shouldCloseOnClickAt'`（若 Task 3/4 未按计划提供这些接口）

- [x] **Step 3: 在 `main()` 里接上自检**

在 `processor.prepareToPlay (48000.0, 512);` 之后加：

```cpp
    if (checkAboutInteraction (processor) != 0)
    {
        std::printf ("about interaction check failed\n");
        return 1;
    }
```

- [x] **Step 4: 运行确认通过**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'" 2>&1 | tail -5 && cd "E:/VST Effects Plugin Collection/moonvoc" && ./build/MoonVocUiSnapshot.exe 2>&1 | grep -E "checkAbout|dumpLayout: FAIL"
```

Expected: `checkAboutInteraction: FAIL=0`、`checkAboutOverlay: FAIL=0`、7 次 `dumpLayout: FAIL=0`；EXIT=0

- [x] **Step 5: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && git add test/UiSnapshot.cpp && git commit -F - <<'EOF'
test: 关于浮层开合交互自动化自检

- checkAboutInteraction：初始关闭 → 点 ⓘ 打开 → 点卡片内部不关 → Esc 关
  → 点遮罩关 → 点 ✕ 关，全程断言可见性
- 不合成 MouseEvent（需要 Desktop），改走 shouldCloseOnClickAt 纯函数与 keyPressed
EOF
```

---

### Task 6: 旋钮阻尼 250 → 500

**Files:**
- Modify: `Source/PluginEditor.cpp`（`setupSlider()`，第 191-210 行）
- Modify: `Source/PluginEditor.cpp`（`dumpLayout()`）

**Interfaces:**
- Consumes: 无
- Produces: 无新接口；15 个旋钮的 `juce::Slider::getMouseDragSensitivity()` 恒为 500

- [x] **Step 1: 写失败的断言**

在 `dumpLayout()` 末尾（Task 4 加的那段之后、`std::printf("dumpLayout: FAIL=%d\n", fail);` 之前）加：

```cpp
    // 旋钮阻尼：全部 15 个旋钮统一 500（默认 250 太滑）
    for (auto* s : { &inputGainSlider, &headroomSlider, &outputGainSlider,
                     &boostSlider, &deboxSlider, &claritySlider, &airSlider,
                     &compAmountSlider, &compMakeupSlider,
                     &dsAmountSlider, &dsFocusSlider,
                     &reverbSlider, &satAmountASlider, &satAmountBSlider, &edgeSlider })
        check (s->getMouseDragSensitivity() == 500, "knob drag sensitivity == 500");
```

- [x] **Step 2: 运行确认失败**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'" 2>&1 | tail -5 && cd "E:/VST Effects Plugin Collection/moonvoc" && ./build/MoonVocUiSnapshot.exe 2>&1 | grep -c "FAIL: knob drag sensitivity"
```

Expected: 输出 `105` —— 15 个旋钮全部 FAIL，而 `dumpLayout()` 在 7 个截图状态里各调一次（15 × 7）

- [x] **Step 3: 实现**

`setupSlider()`（`Source/PluginEditor.cpp:191`）在 `s.setDoubleClickReturnValue(true, s.getValue());` 之后加：

```cpp
    // 拖动阻尼：默认 250px 拖满整个取值范围，手感过滑；500 = 拖满需 500px。
    // 注意 juce::Slider 的滚轮步进用同一常量换算，滚轮也会同比变稳（预期行为）
    s.setMouseDragSensitivity (500);
```

- [x] **Step 4: 运行确认通过**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'" 2>&1 | tail -5 && cd "E:/VST Effects Plugin Collection/moonvoc" && ./build/MoonVocUiSnapshot.exe 2>&1 | grep -E "dumpLayout: FAIL|knob drag"
```

Expected: 7 次 `dumpLayout: FAIL=0`，无 `knob drag` FAIL 行

- [x] **Step 5: 手感真机确认（人工步骤，执行代理做不了 —— 归到 Task 7 的交付验收）**

打开 Standalone，拖动 Compression / Edge / Reverb 三个 hero 旋钮与任意标准旋钮：同样一段鼠标位移，数值变化约为改前的一半；微调时更容易停在想要的数字上。

- [x] **Step 6: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && git add Source/PluginEditor.cpp && git commit -F - <<'EOF'
ui: 旋钮拖动阻尼 250 → 500（拖满范围所需像素翻倍）

- setupSlider 统一 setMouseDragSensitivity(500)，15 个旋钮全覆盖
- dumpLayout 断言全部旋钮 == 500
EOF
```

---

### Task 7: 全量回归 + 文档 + 交付验收

**Files:**
- Modify: `docs/HANDOFF.md`（§6 待办/进展）
- Modify: `docs/superpowers/specs/2026-09-28-moonvoc-about-and-knob-damping-design.md`（状态行改「已实现」）
- Modify: `docs/superpowers/plans/2026-09-28-moonvoc-about-and-knob-damping.md`（勾选进度）

**Interfaces:**
- Consumes: 前 6 个任务的全部产出
- Produces: 无

- [x] **Step 1: 跑全量测试基线**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && ./build/MoonVocHeadlessTest.exe > /tmp/ht.log 2>&1; echo "headless EXIT=$?"; grep -c FAIL /tmp/ht.log; tail -3 /tmp/ht.log
```

Expected: `EXIT=0`，`grep -c FAIL` 输出 `0`

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && ./build/MoonVocUiSnapshot.exe 2>&1 | grep -E "FAIL|checkAboutOverlay" | head -20
```

Expected: 只有 `dumpLayout: FAIL=0`（**7 次**）、`checkAboutOverlay: FAIL=0`、`checkAboutInteraction: FAIL=0`，没有任何 `FAIL:` 行

- [x] **Step 2: 更新 `docs/HANDOFF.md`**

在 §6「已知问题 / 待办」的最新小节（v0.9.0 那条之后）插入新小节：

```markdown
**2026-09-28 关于界面 + 旋钮阻尼（v0.9.0 之上，未打包）**：
- [x] **关于浮层**：`Source/UI/AboutOverlay.h/.cpp` —— 插件内浮层（非独立窗口，宿主里不会被挤掉），
      作为 Canvas 子组件覆盖 1496×672，自动跟随 100~300% 缩放与大字模式
- [x] **版式（用户选定 B 双栏）**：卡片 1080 宽，左栏 440 整幅插画（等比、垂直居中、不裁切），
      右栏 564 文字（标题渐变 / 版本 / 介绍 / SIGNAL CHAIN / TRANSPARENCY / 联系版权）
- [x] **卡片高度按文字实测自适应**（TextLayout 量高，钳制 360~656），中英 + 大字都不会溢出
- [x] **入口**：全局卡右上角 ⓘ 徽章（1438,30,26,26），刻意避开右端指示灯（1412,66,40,40）
- [x] **关闭**：点遮罩 / ✕ / Esc；**淡入 150ms**（`open(false)` 供截图跳过动画）
- [x] **插画素材**：`assets/artwork.png`（与 TMIXTOOL 同一张，TUJZMIXING 自有）嵌入二进制数据；
      `juce_add_binary_data` target 更名 `MoonVocFonts` → `MoonVocAssets`
- [x] **旋钮阻尼**：`setupSlider` 统一 `setMouseDragSensitivity(500)`（默认 250 太滑）
- [x] **JUCE 9 新坑**：① `AttributedString` 用 `append(text, font, colour)`，没有 `withFont`；
      ② `setLineSpacing` 是**额外**行距不是倍数（倍数要换算 `font.getHeight() * (m-1)`）；
      ③ 英文串走裸 `juce::String`（ASCII 解码），`−85`/`→`/`©` 这类符号必须换 ASCII
- [x] **验证**：HeadlessTest EXIT=0 + allKeysFilled OK；UiSnapshot `dumpLayout: FAIL=0`（4 态）
      与 `checkAboutOverlay: FAIL=0`（中英 × 标准/大字）+ 3 张关于截图人工确认
- [ ] 待打包发布（等用户指示）
```

- [x] **Step 3: 把 spec 状态行改成已实现**

把 spec 第 3 行：

```markdown
> **状态：待实现** —— 实现计划见 `docs/superpowers/plans/2026-09-28-moonvoc-about-and-knob-damping.md`，落地后补 `docs/HANDOFF.md`。
```

改成：

```markdown
> **状态：已实现（2026-09-28）** —— 实现计划见 `docs/superpowers/plans/2026-09-28-moonvoc-about-and-knob-damping.md`，落地记录见 `docs/HANDOFF.md` §6。
```

同时把 spec §4 里「headless 测试新增断言：任一旋钮 `getMouseDragSensitivity() == 500`」改成「`dumpLayout()`（UiSnapshot）新增断言：15 个旋钮 `getMouseDragSensitivity() == 500`」—— 实际落在 UI 侧，因为 headless 测试不建编辑器。

- [x] **Step 4: 勾选计划进度并提交文档**

把本文件里已完成步骤的 `- [ ]` 改成 `- [x]`，然后：

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && git add docs/ && git commit -F - <<'EOF'
docs: HANDOFF/spec/plan 更新到「关于界面 + 旋钮阻尼」已实现

- HANDOFF §6 记录浮层版式、入口、关闭方式、JUCE 9 新坑与验证结果
- spec 状态改已实现；阻尼断言位置更正为 dumpLayout（UI 侧）
EOF
```

- [ ] **Step 5: 交付给用户肉眼验收（可视化标签页）**

把三张关于界面截图推到可视化会话的 content 目录，让用户在浏览器里直接看（服务器若已停，用同一 `--project-dir` 重启即可复用端口）：

```bash
cd "E:/VST Effects Plugin Collection/moonvoc" && \
cp ui_snapshot_about.png ui_snapshot_about_en.png ui_snapshot_about_large.png \
   ".superpowers/brainstorm/1172-1790590644/content/" 2>/dev/null || echo "会话目录已变，改用当前 .superpowers/brainstorm/*/content"
```

在新 screen 文件里用 `<img src="/files/ui_snapshot_about.png">` 展示三张图，并请用户确认：插画完整、文字换行正常、ⓘ 位置合适、整体观感。收到「可以」后再谈打包发布。

---

## 自查记录（写完计划后回看 spec）

- **spec §1.1~§1.4（浮层形态/结构/开合/版式）** → Task 3、4
- **spec §1.5（文案全表）** → Task 2
- **spec §1.6（素材与构建）** → Task 1
- **spec §2（ⓘ 按钮）** → Task 3（组件）、Task 4（摆位与断言）、Task 5（开合交互自动化验证；
  原计划的人工点击验证改成回归测试 —— 执行代理点不了 GUI，人工目视并入 Task 7 的截图验收）
- **spec §3（旋钮阻尼）** → Task 6
- **spec §4（测试与验证）** → Task 2/3/4/6 的断言 + Task 7 全量回归
- **spec §5（明确不做）** → 全计划无淡出、无独立窗口、无插画裁切、无版本号变更、无打包
- **与 spec 的一处偏差（已在 Task 7 Step 3 回写 spec）**：旋钮阻尼断言落在 `dumpLayout()`（UiSnapshot 内），
  不是 headless 测试 —— headless 不创建编辑器，拿不到 `juce::Slider`。
- **类型一致性**：`AboutOverlay::getBlocks()` 返回 `const std::vector<Block>&`，Task 3（自检）与 Task 4（dumpLayout）用法一致；
  `openAbout(bool)` 在 Task 4 定义、Task 4/5 使用，签名一致。
