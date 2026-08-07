# MoonVoc 交接文档（v0.2）

> 给下一个会话/模型：读完这份文档即可无缝接手。最后更新：2026-08-07（v0.2：Clarity 峰锁定 + 线性相位 FIR 超采样 + UI 氛围升级）

## 0. 一句话总结

一体化人声处理 VST3 插件（JUCE 9 + C++17），信号链：**去齿音 → 四段智能 EQ → 智能双层压缩 → 染色 → 瞬态整形**。默认状态全链透明（THD < -92.0dB），已发布 v0.1 到 GitHub（公开）：`https://github.com/1399456751-debug/MoonVoc`。

## 1. 环境

| 项 | 值 |
|---|---|
| 项目路径 | `E:\VST Effects Plugin Collection\moonvoc`（**路径含空格**） |
| JUCE | `C:\JUCE`（**JUCE 9 新版，headless 架构，API 与网上教程差异巨大，见 §7**） |
| 编译器 | MSVC 14.44（VS2022 BuildTools：`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`） |
| 构建 | CMake 4.4.2 + Ninja（Ninja 在 BuildTools 内，`build.bat` 自动加载 vcvars64） |
| 操作系统 | Windows 11 |

**构建方式**：`powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'"`（bash 直接调 cmd 有引号坑，务必走 PowerShell）。
VST3 输出：`build\MoonVoc_artefacts\Release\VST3\MoonVoc.vst3`（COPY_PLUGIN_AFTER_BUILD 会复制到系统目录，但**宿主/PluginDoctor 开着时会失败**——需手动复制）。

**测试**：
- 回归：`build\MoonVocHeadlessTest.exe`（30 项，含系数对比/THD/各模块/压力）
- UI 布局自检：`build\MoonVocUiSnapshot.exe`（打印所有控件坐标，**布局问题用它验证，不用截图**——模型看不了图）
- 离线渲染：`build\MoonVocRender.exe 输入.wav [输出.wav]`（`--test` 生成模拟人声）
- 冒烟：`smoke-test.ps1`

## 2. 用户偏好（重要！）

- **回复用简体中文，句末加"喵~"**
- 声音定位：**绝对干净透明**，用户明确"有延迟没关系，不拿来直播"→ 默认 16x 超采样
- 用户不懂编程：汇报要通俗，改动要数据背书（用 UiSnapshot 验证布局，用 THD 验证音质）
- UI 风格：暗黑深邃 + 紫色霓虹辉光 + 月全食背景（用户亲自选图）+ 立体科技旋钮 + 神秘纹路
- 用户对 UI 细节极其挑剔（旋钮正圆/等宽/指针对齐/辉光/字号），改动后务必数据验证
- **PluginDoctor/Studio One 开着时会锁住 VST3 DLL，装不进去**——这是"UI 没变化"最常见原因，先查占用进程再装

## 3. 文件结构

```
moonvoc/
├── CMakeLists.txt          # 4 个 target：MoonVoc(插件) / HeadlessTest / Render / UiSnapshot
├── build.bat               # vcvars64 + Ninja 构建
├── Source/
│   ├── PluginProcessor.h/.cpp   # 处理器：APVTS、超采样 4 实例、电平表、链路调度
│   ├── PluginEditor.h/.cpp      # 全部 UI 布局/绘制（三大块）
│   ├── ParamIDs.h               # 参数 ID（已定死，勿改）
│   ├── UI/MoonVocLookAndFeel.h/.cpp  # 旋钮/开关/下拉/标签辉光全自绘 + Theme 色板 + 字体
│   └── dsp/                      # 6 个 DSP 模块（见 §4）
├── fonts/                   # Montserrat Bold/SemiBold/Medium（OFL，嵌入 BinaryData）
├── assets/moon.jpg          # 用户月亮底图（嵌入）
├── docs/requirements.md     # v1.0 需求文档
├── docs/HANDOFF.md          # 本文件
└── test/                    # HeadlessTest.cpp（30 项回归）/ WavRender / UiSnapshot
```

## 4. DSP 模块（链路顺序）

统一约定：**所有模块跑在超采样链内**（采样率 = dspRate = 宿主率 × factor，由 Processor 原子更新）。**超采样用线性相位 FIR 半带滤波**（v0.2 由半带 IIR 换成 FIR，相位更正、高频更干净，默认直通 THD 由 -89.5dB 再降到 -92.0dB）。构造签名统一 `(APVTS&, std::atomic<double>& osSampleRate)`。

| 模块 | 文件 | 算法 | 关键参数 |
|---|---|---|---|
| 去齿音 | VoiceDeEsser | 双频段（3-5k/5k+），候选窄带锁频（3 档），齿音特征=带通包络 vs 全带包络的相对值（阈值 -25dB），动态 peaking 削减（Q=2，最大 -24dB×强度） | dsLowAmount/dsHighAmount |
| EQ | VoiceEq | 四段：Thick（智能基频 80~315Hz 锁频，搁架中心=基频×1.25）+ De-Box（13 窄带 200~800Hz 多峰检测，最多 3 峰并行削减）+ Clarity（13 窄带 2k~8k 多峰提升，**峰锁定消除频点扫动 + 刺耳峰限增益**）+ Air（13k/22k 搁架）。对比度>2.0 锁频，抛物线插值，Q 自适应 0.9~3.5 | eqLowBoost/eqDeboxCut/eqClarityBoost/eqAirBoost/eqAirFreq |
| 压缩 | VoiceComp | 双层串联：Fast（1176 风：快 attack 高 ratio 硬拐点，阈值 -30dB×强度）+ Smooth（LA-2A 风：慢 attack 2:1 软拐点 knee 6dB，阈值 -40dB×强度）。attack/release/ratio 由峰值因子（crest，1s EMA）智能自适应（瞬态→0.15ms/6:1，平滑→3ms/3.5:1）。参数 30ms 平滑防 click | compMode/compAmount/compMakeup |
| 染色 | VoiceSat | 6 种饱和（FET/Tube/Tape/Optical/Germanium），小信号斜率精确 1:1（干净），大信号软压缩；A+B 双槽串联；amount 用 gamma 0.7 | satTypeA/B/satAmountA/B |
| 瞬态 | VoiceEdge | 包络跟随（attack 1ms/release 50ms），瞬态强度=(输入-包络)/输入，增益平滑 1.5ms 降调制失真（THD -83dB）；±100 双向 | edgeAmount |

**已删除（用户要求）**：混响模块、AutoGain 功能、所有 Bypass 按钮（参数+DSP+UI 全清）。

**输出电平表**：Processor 每块算输入/输出 RMS → VU 平滑（attack 10ms/release 300ms）→ atomic。
**GR 表**：压缩 gainReduction + 去齿音 gainReduction（双频段削减和）。

## 5. UI 结构（PluginEditor）

- 布局：顶部标题（MoonVoc 44px 辉光+渐变+副标+两侧能量纹路）→ 全局行（Input/Headroom/Output + 右侧菱形指示灯）→ 三列（左 Edge+染色 / 中压缩+EQ / 右去齿音+四条横向条 IN/OUT/COMP/DE-ESS）→ 底部超采样下拉
- 背景：极暗紫黑 + 月亮底图（20% 半透明全屏）+ 右上角环形品牌徽章（v0.2 替换原程序化血月）+ 星野呼吸 + 金属拉丝 + 动态辉光氛围（v0.2）
- 旋钮：金属切面环 + 立体球面 + 环绕轨道光点 + 发光刻度线（36 条）+ 指针（**0 值统一 7 点方向**，正负旋钮正值顺时针/负值逆时针）
- 字体：全部 Montserrat Bold + 5 层模糊辉光（Theme::drawGlowText）
- 布局验证：`UiSnapshot.exe` 打印控件坐标（Air 框中心=EQ 行中心、四旋钮等宽 y 一致、Edge 150 正方、Comp 130 正方、Drive 下拉等大）

## 6. 已知问题 / 待办

**v0.2 已完成（2026-08-07）**：
- [x] Clarity 扫动消除：峰锁定后频点最差偏移 0 Hz
- [x] 整体怪的高频相位：超采样半带 IIR → 线性相位 FIR
- [x] UI 氛围升级：月全食背景 + 动态辉光 + 星野呼吸
- [x] 月亮圆月主视觉 + 右上角环形品牌徽章

**待办**：
- [ ] **AI 未做**：正式发布需 pluginval 验证（未安装）、macOS 移植（需求里有）
- [ ] 图标/安装包/签名未做
- [ ] README.md 未写（可基于 docs/requirements.md 生成）
- [ ] UI 布局未动，待用户后续反馈是否重构布局
- [ ] Tube 非对称饱和含微小 DC 分量（设计取舍，未加 DC blocker——会伤低频相位）
- [ ] 压缩 crest 检测在 1s 平滑，快速风格变化响应偏慢（可调 crestAlpha）
- [ ] 电平表 VU 上升 10ms 对瞬时峰值略钝（峰值保持线已补）
- [ ] UI 性能：全界面 10Hz repaint + 辉光多层文字（label 多时 CPU 略高，可接受）

## 7. JUCE 9 API 差异（必读，网上的旧教程基本全错）

1. **ProcessorChain 不接受实例构造**（模块须默认构造）→ 链路手动顺序调用
2. **IIR::Filter 只处理单声道**，且 **ProcessorDuplicator 实测不工作**（process 不写输出）→ **每通道一个 Filter 实例 + getSingleChannelBlock 手动循环**（已验证可靠）
3. 系数用公开成员 `filter.coefficients = Ptr`；**makeXXX 每块调用会堆分配**（RT 违规）→ 预分配 `Coeffs::Ptr` + 每块重写 `getRawCoefficients()` 5 元素数组 **[b0,b1,b2,a1,a2]**（a0 归一化省略），公式抄 `juce_IIRFilter.cpp` 的 ArrayCoefficients（已用测试验证 1e-7 一致）
4. **SmoothedValue**：`reset(sampleRate, seconds)` 设步数；**块处理必须用 `skip(blockSize)`**（每块一次 getNextValue 会把 50ms 平滑拉长 512 倍）；`setSmoothingTimeInSeconds` 不存在
5. **Oversampling**：构造 `(numChannels, factor指数, type, ...)`——**factor 是 2 的幂指数**（2x→1）；枚举 `filterHalfBandPolyphaseIIR`（Band 大写）；`processSamplesUp/Down` 成对调用
6. 无 `getLatencySamples()` override → 用 `setLatencySamples(int)` 主动上报
7. AudioBlock 视图：`getSingleChannelBlock(ch)` / `getSubBlock(start, n)`
8. `Colour` 非 constexpr（用 const）；`Font` 只有单参构造（用 `.withHeight()`）；Slider/ComboBox 无 setFont；`juce_add_binary_data` 需 SOURCES 关键字、生成头固定名 `BinaryData.h`
9. ProcessContext 有 `isBypassed` 成员（默认 false）

## 8. 反复踩坑的三类 bug（检查代码时重点看）

1. **块 vs 样本时间常数**：凡是"按样本定义时间常数 + 每块更新一次"的 EMA/平滑，收敛会慢 512 倍。检测器 EMA 的 alpha 必须每块按 `1 - exp(-blockDur/tau)` 实时算（blockDur = numSamples/dspRate）
2. **OS 采样率错位**：模块在 OS 链内运行，一切系数/时间常数必须用 `dspRate`（OS 率）而非 spec.sampleRate；**检测带通系数要每块重写**（OS 倍率热切换后旧系数全错位）
3. **状态残留**：测试间参数残留会污染测量（bandGainRatio 已全量重置）；THD 测量必须相干采样（468.75Hz=512 样本整数周期）

## 9. 测试基线（v0.2，30+ 项全绿）

- 系数公式 vs JUCE 官方：maxErr ≤ 2.4e-7 ✓（7 组：lowShelf ±6 →1.19e-7、highShelf +3 →2.38e-7、peak ±5 →1.19e-7、bandpass →9.31e-10 / Q6 →1.19e-7）
- 默认直通 THD：**-92.0dB** ✓（FIR 超采样后比 v0.1 的 -89.5dB 更低）；压缩 100%：-117.2dB；Edge ±100：-76.9/-75.5dB；去齿音 100%：-92.0dB；饱和 FET/Tube：-48.4/-53.3dB（设计染色）
- 压缩：GR -25.1dB（100%）；智能参数：正弦→2.64ms/3.8:1，脉冲→0.79ms/5.4:1 ✓
- 去齿音：4k 削 -31.9dB / 1k 仅 -0.5dB（零染色）✓
- EQ 频响：100Hz +4.2 / 400Hz +3.4 / 3kHz +5.7 / 13kHz +3.0 ✓；Clarity 锁定 593/3150、Air 锁定 593/5934；双共振同时锁 315+560 削减 ✓
- **bypass 全链直通：maxErr=0.00（逐样本完全一致）✓**
- **flat16 频响（16x 超采样平坦度）：100/400/1k/4k/8k/15k Hz 全频段 ±0.10dB ✓**
- **Clarity 峰锁定：锁定后最差偏移 0Hz（<250）✓；刺耳峰限增益 6.3dB（2~10 区间）✓**
- 随机压力 4 种块大小 × 1500 块（共 6000 块）+ 每 200 块 OS 超采样热切换/全局增益变化 + 每 50 块随机 EQ 参数：无 NaN 无崩溃 ✓
- mono 通道 ✓；电平表 in/out -15.1dB、静音衰减 -60 ✓

## 10. 用户联系

- 反馈邮箱（写在测试包说明里）：1399456751@qq.com
- GitHub：github.com/1399456751-debug（账号名 1399456751-debug）
