# MoonVoc v0.2 升级设计文档

> 日期：2026-08-07　目标：解决 4 个用户反馈问题（Clarity 刺挠 / 整体声音"怪" / UI 氛围 / 月亮看不清）。基于 v0.1 交接文档（docs/HANDOFF.md）续写。

## 0. 一句话总结

**Clarity 频点锁定消除扫动 + 刺耳峰限增益；全链 0 值严格直通 + 超采样换线性相位 FIR；UI 加动态氛围 + 旋钮微升级；月亮居中放大裁成圆月 + 右上角程序化血月替换为环形品牌徽章。** 涉及文件：`Source/dsp/VoiceEq.*`、`Source/PluginProcessor.cpp`、各 DSP 模块 0 值审计、`Source/PluginEditor.cpp`、`Source/UI/MoonVocLookAndFeel.*`、`test/HeadlessTest.cpp`。

---

## 1. 问题 ① Clarity 刺挠修复（峰锁定 + 限增益）

### 1.1 根因

Clarity（`VoiceEq::clarityBand`）动态扫描 2k~8k 共振峰并提升。用户症状是"**哇—哇—频点扫动，开一点就有**"——这是动态频点追踪不稳定产生的**频率调制**（类似自动哇音），与增益大小无关，所以小值也听得见。

现状代码（[VoiceEq.cpp](E:/VST Effects Plugin Collection/moonvoc/Source/dsp/VoiceEq.cpp)）：
- `detect()` 每块重新找峰（局部最大 + 对比度 > 2.0，取前 3 个），峰在块间跳动；
- 抛物线插值定位 + 250ms 平滑只能减弱不能消除跳变；
- Q 自适应最高 3.5，增益最高 +12dB，窄带扫动被放大。

### 1.2 设计

**（1）峰锁定（核心）**——`SmartBand` 增加锁定状态，`detect()` 不再每块全局重选：

```
状态：lockedIndex（锁定峰的候选带 index，-1=未锁）
      lockedBaseline（锁定时的带能量基线）
      unlockTimer（衰减确认计数）
```

- 锁定中：仅检查 `lockedIndex ± 1` 带邻域。
  - 若邻域内该峰仍是局部峰 **且** 能量相对基线衰减 < 6dB → 保持锁定，频点只在邻域内重新插值（限制在 `lockedIndex ± 1` 带内，杜绝跳变）；
  - 否则 `unlockTimer++`；连续 ≥ 8 块（约 0.4s）衰减则解锁，回到全局检测。
- 全局检测时：选对比度最高的峰作为新锁定峰；**与锁定峰间距 < kPeakSpacing 的候选峰一律忽略**（防止锁定峰旁再开一路）。
- 无信号（底噪检查）时立即解锁、全部直通。

**（2）限增益**——`detect()` 记录每个峰的 `targetContrast[]`；`updateCoeffs()` 计算有效增益：

```
effectiveGain = userGain × (1.0 - 0.5 × clamp((contrast - 2.0) / (5.0 - 2.0), 0, 1))
```

对比度 2.0 → 满增益；对比度 ≥ 5.0 → 打对折。**越扎眼的共振峰提升越少**，双保险。

**（3）稳定化**：
- 检测器 EMA 时间常数 50ms → **120ms**（`runDetectors` 内 alpha 计算处）；
- 频点/Q 平滑 `kFreqSmoothTime` 0.25s → **0.5s**（`VoiceEq.cpp` 顶部常量）。

**（4）DeBox 同样受益**：`deboxBand` 走同一 `SmartBand` 结构，峰锁定逻辑天然生效，行为不变但更稳。

### 1.3 涉及文件与验证

- `Source/dsp/VoiceEq.h`：`SmartBand` 加锁定状态成员。
- `Source/dsp/VoiceEq.cpp`：`detect()` / `runDetectors()` / `updateCoeffs()` 改造。
- `test/HeadlessTest.cpp` 新增：
  1. **锁定稳定**：构造"两个交替出现的窄带峰"信号，跑足够块数，断言锁定峰 index 不来回跳；
  2. **限增益**：高对比度峰输入，断言实际 peaking 增益 < 用户增益（通过 `debugGetCoeffs` 对比）。
- 回归：原有 21 项全绿（尤其 EQ 双共振削减、频响基线）。

---

## 2. 问题 ② 整体声音"怪"（闷 / 脏 / 平淡）

### 2.1 根因

用户："和干声对比觉得不对，闷、不亮、脏、平淡"。最可能两个来源：
1. **某模块在参数 0 时并非严格直通**（如增益 0 但仍带入平滑假象、系数非 [1,0,0,0,0]、检测器影响输出）；
2. **IIR 半带超采样在 16x 下的相位失真与高频衰减**（级联半带 IIR 相位累积，人声发虚发闷）。

### 2.2 设计

**（1）0 值严格直通审计（先做，工作量小、收益直接）**：
逐模块检查 `amount=0` / `type=Off` / 中性状态下的输出必须**逐样本等于输入**：
- `VoiceDeEsser`：dsLowAmount/dsHighAmount 均为 0 时严格直通（削减系数为 1:1，无平滑假象）；
- `VoiceEq`：四段增益 0 时严格 `[1,0,0,0,0]`（含 Air 段 0dB 搁架）——已基本满足，复核检测器不污染输出；
- `VoiceComp`：compAmount 0 时 ratio 严格 1:1、无增益漂移、无 click；
- `VoiceSat`：type=Off 严格直通；
- `VoiceEdge`：edgeAmount 0 时严格直通。
发现任何非严格直通即修复，并加测试断言（全 0 参数下逐样本 `|out - in| < 1e-7`）。

**（2）超采样 IIR → 线性相位 FIR**：
- `PluginProcessor.cpp` 中 `Oversampling` 的滤波器类型 `filterHalfBandPolyphaseIIR` → **`filterHalfBandPolyphaseFIR`**（保留 polyphase 以控 CPU；若通带仍不理想再试 `filterHalfBandFIR`）。
- 同步更新：
  - `setLatencySamples()`（FIR 半带延迟不同，用 `getLatencyInSamples()` 动态取值）；
  - 超采样倍率热切换处的 latency 上报；
  - 性能：16x 下 FIR 更贵，若实测单核负载明显超标（> 15% 于一块 512@48k），回退方案：高倍率（16x）用 FIR、低倍率（2x/4x）用 IIR，或全链 FIR 但 16x 下允许处理器提示。**以离线渲染 + CPU 实测为准，不拍脑袋。**
- THD/频响基线重新测量（FIR 与 IIR 数值不同，测试断言值需按新基线更新）。

**（3）高频/相位最终验证**：
- 默认全链直通频响：干声 vs 处理差 < 0.1dB（2Hz~20kHz）；
- 21 项回归全绿；
- `MoonVocRender.exe` 渲染"干声 / 新处理"两版对比，用户试听确认"闷/脏"消失。

### 2.3 风险

- 换 FIR 会改变延迟与 CPU，可能影响宿主补偿与实时表现 → 以实测数据决策。
- 测试基线值（THD 等）会变 → 更新 HeadlessTest 断言，不硬凑旧值。

---

## 3. 问题 ③ UI 氛围升级 + 控件微升级

### 3.1 现状

布局已定（三列面板 26/46/30%），用户反馈"丑"，方向是**华丽炫酷科技感**。本次**不动布局**（面板矩形/旋钮坐标不变），只做氛围 + 旋钮细节微升级。

### 3.2 氛围设计（`PluginEditor.cpp` paint，全部轻量 sin 波动，不增 repaint）

1. **星云光斑**：背景加 2~3 个大半径紫色径向渐变光斑，位置随 `indicatorPhase` 缓慢漂移（深度感）；
2. **月亮光晕呼吸**：月亮外圈光晕半径随 `sin(phase)` 呼吸；
3. **能量粒子流**：面板间竖向连线上的光点沿连线流动（用 phase 做线参数 t）；
4. **电平驱动氛围**：背景辉光 / 星星 alpha 随 `inputLevelDb` 微调（有信号时背景"活"起来）；
5. **面板悬停发光**：Editor 记录 `mouseMove` 位置，`paint` 中鼠标所在面板描边提亮；
6. **标题脉动**：MoonVoc 标题辉光 alpha 随 phase 缓慢呼吸。

### 3.3 控件微升级（`MoonVocLookAndFeel.cpp`，克制不重画）

1. **旋钮**：拖拽/悬停时外圈光晕光圈脉冲扩散（现有 `isMouseOverOrDragging` 基础上加呼吸）；
2. **下拉框**：悬停时边框渐变提亮、箭头微微放大；
3. **面板圆角高光**：顶部紫光带加呼吸渐变（面板本身描边逻辑不动）。

### 3.4 验证

- `MoonVocUiSnapshot.exe` 打印布局坐标：**断言所有面板/旋钮 bounds 与改造前完全一致**（布局未动）；
- 代码审查：无新增每帧分配、无新增 Timer 频率（维持 10Hz repaint）。

---

## 4. 问题 ④ 月亮居中放大（圆形裁剪）

### 4.1 现状

`moonImage` 全屏 `stretchToFit` + 0.20 透明度，且被金属纹理（0.25）和面板层遮盖，几乎看不清。

### 4.2 设计（`PluginEditor.cpp` paint）

- **位置/大小**：月亮中心放在窗口中心偏上（`cy ≈ 42%H`），**直径 ≈ 55% 窗口高度**；
- **圆形裁剪**：`moonImage` 是纯月亮特写（用户已确认）→ 用圆形 `clipPath` 裁剪 + 外圈柔边（透明度渐变），保证正圆；
- **清晰度**：透明度 0.20 → **~0.50**；全屏金属纹理不再压住月亮区（月亮绘制在金属纹理**之上**，或降低金属纹理对该区域的遮挡）；
- **层次**：三列面板仍浮在上层（半透明），月亮透过面板隐约可见。中列面板（Comp+EQ）透明度从 0.78 略降至 ~0.60，让月亮透出；**旋钮/文字保持清晰可读**（面板底色仍足够深）；
- 月亮中心放置于窗口中心偏上（cy ≈ 42%H），**不改变任何控件 bounds**——月亮纯作为背景层绘制，下缘可落入三列面板之间的空隙，上缘露出于标题区下方背景，不触碰控件矩形。
- **右上角程序化血月替换**（用户确认）：原 `paint()` 里右上角 `(getWidth()-150, 100)` 的程序化暗红血月（半径 46px）**移除**，替换为**环形品牌徽章**——紫色发光圆环 + 环形文字（`TUJZMIXING · MOONVOC · SYSTEMS ·`）+ 中心发光星标/菱形，与右下角 `TUJZMIXING-DSP` LOGO 呼应，增强品牌感。绘制为静态元素（可随 indicatorPhase 轻微呼吸），不占控件区域。

### 4.3 验证

- `MoonVocUiSnapshot.exe` 确认控件 bounds 不变；
- 视觉确认：构建后在宿主打开（若宿主占用 DLL，先关宿主再复制）。

---

## 5. 实施顺序（一次提交，分阶段验证）

1. **阶段 A（DSP 修复）**：Clarity 峰锁定 + 限增益 + 稳定化；新增 2 项测试；回归全绿。
2. **阶段 B（直通审计）**：各模块 0 值严格直通修复 + 全 0 直通断言测试；回归全绿。
3. **阶段 C（超采样 FIR）**：换 `filterHalfBandPolyphaseFIR`，更新 latency/基线，频响/THD/CPU 实测；回归全绿。
4. **阶段 D（UI）**：氛围 + 旋钮微升级 + 月亮居中圆月；`UiSnapshot` 断言布局不变。
5. **收尾**：`MoonVocRender` 渲染 A/B 试听，提交 v0.2，更新 HANDOFF.md。

## 6. 风险与取舍汇总

| 项 | 取舍 |
|---|---|
| Clarity 峰锁定 | 智能响应变慢（<1s），换取扫动消失 |
| 限增益 | 高刺耳峰提升打折，换取安全不刺 |
| FIR 超采样 | 延迟/CPU 上涨，换取相位与高频干净；以实测为准 |
| 月亮居中 | 中列面板透明化，控件可读性优先；若遮挡严重回调 |
| UI 氛围 | 保持 10Hz repaint，无额外 CPU 负担 |
