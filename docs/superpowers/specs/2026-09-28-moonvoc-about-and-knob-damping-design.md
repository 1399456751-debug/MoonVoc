# MoonVoc 关于界面 + 旋钮阻尼 设计文档

> **状态：已实现（2026-09-28）** —— 实现计划见 `docs/superpowers/plans/2026-09-28-moonvoc-about-and-knob-damping.md`，落地记录见 `docs/HANDOFF.md` §6。
> 2026-09-28 制定。基于用户 2 项需求 + 4 轮澄清（入口位置 / 内容块 / 阻尼强度 / 版式三选一，用户选定 B 版双栏）。
> 版式与文案的可视化定稿：`.superpowers/brainstorm/1172-1790590644/content/about-b-proofread.html`（会话临时目录，已在 `.gitignore`）

## 0. 范围

版本保持在 0.9.0 之上（未定下一版号），两项改动：

1. **关于界面**：新增插件内浮层（插画 + 标题 + 版本 + 介绍 + 模块清单 + 技术注脚 + 联系与版权），由新增的 ⓘ 按钮打开
2. **旋钮阻尼加重**：所有旋钮拖动灵敏度 250 → 500（拖满整个取值范围的距离翻倍）

**不改**：全部 DSP 模块与参数（`ParamID` 一律不动，不新增音频参数）、信号链、现有布局与配色、现有控件行为。

---

## 1. 关于浮层（新增 `Source/UI/AboutOverlay.h/.cpp`）

### 1.1 形态与理由

**插件内嵌浮层**，不是 `juce::DialogWindow` 独立窗口 —— 后者在宿主里会开出游离顶层窗口，部分宿主会把它挤出屏幕或拒绝输入焦点。浮层作为 `MoonVocEditor::Canvas` 的子组件，覆盖整个设计区 `1496×672`。

因为是 Canvas 的子组件，浮层**自动跟随缩放**（100%~300% 整体放大）与**大字模式**（`Theme::uiFont()` 内部 ×1.4），无需单独处理。

### 1.2 组件结构

```
Canvas
└── AboutOverlay            // 覆盖 0,0,1496,672；默认 setVisible(false)
    ├── 遮罩                 // paint 里填充 rgba(28,39,51,0.42)，点击 → close()
    └── 卡片                 // 1080 宽，高度按内容自适应（见 1.4），水平居中
        ├── ✕ 关闭按钮       // 卡片右上角，内缩 14，直径 28
        ├── 左栏 440 宽      // 白底 + 1px 右分隔线，插画等比居中
        └── 右栏 flex 1      // 文字区，内边距 上下 34 / 左右 38
```

### 1.3 开合行为

| 动作 | 行为 |
|---|---|
| ⓘ 按钮点击 | `open()`：`setVisible(true)` → `toFront()` → `grabKeyboardFocus()` → 启动淡入 |
| 点击遮罩 | `close()`：立即隐藏（关闭不做淡出 —— 点遮罩后延迟消失会显得迟钝） |
| ✕ 按钮点击 | 同上 |
| Esc 键 | 同上（`keyPressed` 匹配 `juce::KeyPress::escapeKey`） |

**淡入**：150ms，`juce::Timer` 每 16ms 推进 `opacity`（0→1，`juce::jlimit`），paint 首行 `g.setOpacity(opacity)`。到达 1 后停表。

`setInterceptsMouseClicks(true, true)` —— 浮层显示时必须挡住底下的旋钮，不能让用户"隔着"浮层拧到控件。

### 1.4 版式（B 版双栏，设计坐标）

卡片：宽 **1080**，高 `H = clamp(内容所需高度, 360, 656)`，`x = (1496-1080)/2 = 208`，`y = (672-H)/2`。

| 元素 | 尺寸 / 位置 |
|---|---|
| 左栏 | 宽 440，高 H，白底 `#ffffff`，右侧 1px `Theme::panelEdge` |
| 插画 | 宽 440，等比 → 高 `440 × 1116 / 1400 ≈ 351`，左栏内垂直居中 |
| 右栏文字 | `x = 左栏右 + 38`，宽 `1080-440-38×2 = 564` |
| 标题 MoonVoc | `Theme::fontBrand(32)`，青蓝→珊瑚横向渐变（`accent`→`accent2`） |
| 版本行 | `fontLabel(11.5)`，字距 0.6，`dimColour()`，格式 `VERSION 0.9.0 · TUJZMIXING`；版本号取编译期 `JucePlugin_VersionString`（已验证可用，当前 `"0.9.0"`），**不写死** |
| 介绍段 | `uiFont(13.5)`，行距 1.62 |
| 小标题（SIGNAL CHAIN / TRANSPARENCY） | `fontSection(9.5)`，字距 1.8，色 `Theme::cardGlobalDeep`（0xffa58a66） |
| 模块清单 / 注脚 / 联系版权 | `uiFont(11.5)`，行距分别 1.8 / 1.6 / 1.6；注脚与联系版权用 `dimColour()` |
| 块间距 | 版本行下 22，各小标题上 26，联系版权上 22 |
| 底部留白 | 34 |

**高度自适应**：右栏逐块用 `juce::TextLayout`（或 `GlyphArrangement::getStringWidth` + `AttributedString` 量高）算实际高度，累加得内容高；卡片高 = 该值 + 上下内边距，钳制在 `[360, 656]`。中文模式下字体换系统 CJK、大字模式下字号 ×1.4，都会自然反映到量高结果里 —— **不写死 600**。量高在 `open()` 与 `resized()` 时各算一次（不在 paint 里算），结果缓存在成员里。

超过 656 时的兜底（大字 ×1.4 下中文长文案可能触发）：按比例压缩块间距（最小 12），仍超出则截断到 656 并允许右栏内容裁剪 —— 实际不会发生（估算 ×1.4 后约 590），仅作保护。

### 1.5 文本内容（进 `Source/UI/MoonVocStrings.h`，中英双表）

新增 `Strings::Key`：`kVersion, kAboutBlurb, kAboutChainTag, kAboutModules, kAboutNoteTag, kAboutNote, kAboutContact, kAboutCredits`。
所有中文串必须走 `S8()`（`juce::String(const char*)` 按 ASCII 解码，直接传 UTF-8 会乱码 —— 项目既有铁律）。多行用 `\n`。

| Key | 中文 | English |
|---|---|---|
| kVersion | 版本 | VERSION |
| kAboutBlurb | MoonVoc 把整条人声链收进一个窗口：四段智能 EQ、三级母带式压缩、齿音控制、染色、瞬态整形与混响，按真实的混音顺序排列，每一环都可独立旁通。它不替你决定声音，只把每个决定做得干净利落。 | MoonVoc gathers an entire vocal chain into one window — four-band intelligent EQ, three-stage mastering compression, de-essing, saturation, transient shaping and reverb — arranged in the order a mix actually happens, every stage independently bypassable. It does not decide the sound for you; it just makes each decision clean. |
| kAboutChainTag | 信号链 | SIGNAL CHAIN |
| kAboutModules | 四段智能 EQ　Thick · De-Box · Clarity · Air`\n`三级压缩　FET · 光电 · 并行`\n`齿音控制　→　双槽染色　→　瞬态整形　→　混响 | Four-band intelligent EQ　Thick · De-Box · Clarity · Air`\n`Three-stage compression　FET · Optical · Parallel`\n`De-Ess → Dual-stage saturation → Transient → Reverb |
| kAboutNoteTag | 透明 | TRANSPARENCY |
| kAboutNote | 所有算法以透明为基准：参数归零时，信号逐样本还原；超采样链采用线性相位 FIR 半带滤波，4x 下残余失真低于 −85 dB。 | Every algorithm is built around transparency: with all parameters at zero the signal is returned sample for sample, and the oversampling stage uses linear-phase FIR half-band filters, keeping residual distortion below −85 dB at 4x. |
| kAboutContact | 反馈与建议　1399456751@qq.com　·　github.com/1399456751-debug | Feedback　1399456751@qq.com　·　github.com/1399456751-debug |
| kAboutCredits | © 2026 TUJZMIXING　·　基于 JUCE 构建　·　去齿音改编自 Airwindows DeBess（MIT，© Chris Johnson） | © 2026 TUJZMIXING　·　Built on JUCE　·　De-Esser adapted from Airwindows DeBess (MIT, © Chris Johnson) |

语言跟随既有 `uiLanguage` 参数。浮层在 `open()` 时读取一次语言与字号；若打开期间用户切换语言（浮层盖住设置控件，实际不可能），按现有 `applyLanguage()` 的刷新路径处理即可 —— 不额外做实时刷新。

### 1.6 素材与构建

- 复制 `E:\VST Effects Plugin Collection\TMIXTOOL\src\gui\assets\artwork.png`（1400×1116）到本项目 `assets/artwork.png`
- 加入 [CMakeLists.txt](../../../CMakeLists.txt) 的 `juce_add_binary_data(MoonVocFonts SOURCES ...)`，头名固定 `BinaryData.h`，符号名 `BinaryData::artwork_png`
- 绘制用 `juce::Image` 缓存（`juce::ImageCache::getFromMemory`），不每帧解码

---

## 2. ⓘ 按钮

- 新增轻量组件 `InfoBadge`（可放在 `AboutOverlay.h` 内或 PluginEditor 局部）：自绘圆形 + "i"，`mouseEnter/mouseExit` → `repaint()` 变底色，`mouseUp` → 回调打开浮层
- 位置：全局卡右上角，设计坐标 **`(1438, 30, 26, 26)`**
- **避让**：既有指示灯 `indicatorRect = (cardGlobal.getRight()-68, cardGlobal.getY()+50, 40, 40)` = `(1412, 66, 40, 40)`；ⓘ 底边 56 与指示灯顶边 66 留 10px 间距，不重叠（加断言锁死）
- 不使用 `juce::TextButton` —— 现有 LAF 的 `drawButtonBackground` 只在按钮带 `moonvocArcColor` 属性时自绘，混用会让圆形按钮走默认圆角矩形样式

---

## 3. 旋钮阻尼

在 `MoonVocEditor::setupSlider()`（[PluginEditor.cpp:191](../../../Source/PluginEditor.cpp)）末尾加一行：

```cpp
s.setMouseDragSensitivity (500);   // 默认 250：拖满整个取值范围所需像素，越大越"重"
```

- 覆盖全部 15 个旋钮（`setupSlider` 是所有旋钮的唯一入口），含 hero 大旋钮
- 副作用（已知且接受）：`juce::Slider::mouseWheelMove` 内部用同一常量换算步进，滚轮每格变化量同比变小 —— 方向与"更沉稳"一致
- 不改：双击归位、Shift 精细拖动、旋钮绘制与数值弧

---

## 4. 测试与验证

| 项 | 方式 |
|---|---|
| 布局 | `MoonVocUiSnapshot.exe` 新增断言：① ⓘ 按钮在全局卡内且与 `indicatorRect` 不重叠 ② 关于卡片四边在画布 `1496×672` 内 ③ 左栏与右栏不重叠 ④ 右栏各文字块两两不重叠 ⑤ 插画绘制区在左栏内 |
| 阻尼 | `dumpLayout()`（UiSnapshot）新增断言：15 个旋钮 `getMouseDragSensitivity() == 500`（实际落在 UI 侧 —— headless 测试不建编辑器） |
| 回归 | `MoonVocHeadlessTest.exe` 全绿（应无变化 —— 本次不动 DSP、不动参数） |
| 视觉 | 浮层打开状态渲染 PNG，人工确认（浏览器可视化会话标签页） |
| 构建 | `powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'"` |

**注意**：headless 测试跑的是 `AudioProcessor`，**覆盖不到 UI**，浮层正确性主要靠 UiSnapshot 断言 + 人工目视。

---

## 5. 明确不做

- 不做淡出动画、不做缩放弹出动画
- 不做独立的 About 窗口 / 菜单项
- 不改 TMIXTOOL（那边已有自己的关于界面；本次只是复用同一张插画素材）
- 不把插画裁切或调色（原图整体使用）
- 不发布新版本号 / 不打包（本次只改源码，发布待用户指示）
