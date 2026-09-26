# MoonVoc 交接文档（beta0.5）

> 给下一个会话/模型：读完这份文档即可无缝接手。最后更新：2026-08-08（beta0.5：Windows 已编译打包发布测试，macOS CI 未跑通已删，见 §11）

## 0. 一句话总结

一体化人声处理 VST3 插件（JUCE 9 + C++17），信号链：**去齿音 → 四段智能 EQ → 智能双层压缩 → 染色 → 瞬态整形**。默认状态全链透明（THD < -92.0dB），GitHub 公开：`https://github.com/1399456751-debug/MoonVoc`。**beta0.5（版本 0.5.0）Windows 版已编译打包发用户测试，见 §11**。

## 1. 环境

| 项 | 值 |
|---|---|
| 项目路径 | `E:\VST Effects Plugin Collection\moonvoc`（**路径含空格**） |
| JUCE | `C:\JUCE`（**JUCE 9 新版，headless 架构，API 与网上教程差异巨大，见 §7**）。CMakeLists 用缓存变量 `JUCE_ROOT`（默认 `C:/JUCE`，CI 传 `-DJUCE_ROOT=` 覆盖） |
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
│                           # 版本 0.5.0；JUCE_ROOT 缓存变量（默认 C:/JUCE，见 §1）
├── build.bat               # vcvars64 + Ninja 构建
├── Source/
│   ├── PluginProcessor.h/.cpp   # 处理器：APVTS、超采样 4 实例、电平表、链路调度
│   ├── PluginEditor.h/.cpp      # 全部 UI 布局/绘制（三大块）
│   ├── ParamIDs.h               # 参数 ID（已定死，勿改）
│   ├── UI/MoonVocLookAndFeel.h/.cpp  # 旋钮/开关/下拉/标签辉光全自绘 + Theme 色板 + 字体
│   └── dsp/                      # 6 个 DSP 模块（见 §4）
├── fonts/                   # Montserrat Bold/SemiBold/Medium（OFL，嵌入 BinaryData）
├── assets/moon.jpg          # 用户月亮底图（嵌入，beta0.5 起入库）
├── dist/                    # beta0.5 打包输出（.gitignore 忽略）
├── docs/requirements.md     # v1.0 需求文档
├── docs/HANDOFF.md          # 本文件
└── test/                    # HeadlessTest.cpp（30 项回归）/ WavRender / UiSnapshot
```

## 4. DSP 模块（链路顺序）

统一约定：**所有模块跑在超采样链内**（采样率 = dspRate = 宿主率 × factor，由 Processor 原子更新）。**超采样用线性相位 FIR 半带滤波**（v0.2 由半带 IIR 换成 FIR，相位更正、高频更干净，默认直通 THD 由 -89.5dB 再降到 -92.0dB）。构造签名统一 `(APVTS&, std::atomic<double>& osSampleRate)`。

| 模块 | 文件 | 算法 | 关键参数 |
|---|---|---|---|
| 去齿音 | VoiceDeEsser | 双频段（3-5k/5k+），候选窄带锁频（3 档），齿音特征=带通包络 vs 全带包络的相对值（阈值 -25dB），动态 peaking 削减（Q=2，最大 -24dB×强度） | dsLowAmount/dsHighAmount |
| EQ | VoiceEq | 四段：Thick（智能基频 80~315Hz 锁频，搁架中心=基频×1.25）+ De-Box（13 窄带 200~800Hz 多峰检测，最多 3 峰并行削减）+ Clarity（13 窄带 2k~8k 多峰提升，**峰锁定消除频点扫动 + 刺耳峰限增益**）+ Air（16k/22k 搁架，v0.2 默认 16k 更顺滑；Q 0.5 缓坡，8k~14k 平滑爬升）。对比度>2.0 锁频，抛物线插值，Q 自适应 0.9~3.5 | eqLowBoost/eqDeboxCut/eqClarityBoost/eqAirBoost/eqAirFreq |
| 压缩 | VoiceComp | 双层串联：Fast（1176 风：快 attack 高 ratio 硬拐点，阈值 -30dB×强度）+ Smooth（LA-2A 风：慢 attack 2:1 软拐点 knee 6dB，阈值 -40dB×强度）。attack/release/ratio 由峰值因子（crest，1s EMA）智能自适应（瞬态→0.15ms/6:1，平滑→3ms/3.5:1）。参数 30ms 平滑防 click | compMode/compAmount/compMakeup |
| 染色 | VoiceSat | 6 种饱和（FET/Tube/Tape/Optical/Germanium），小信号斜率精确 1:1（干净），大信号软压缩；A+B 双槽串联；amount 用 gamma 0.7 | satTypeA/B/satAmountA/B |
| 瞬态 | VoiceEdge | 包络跟随（attack 1ms/release 50ms），瞬态强度=(输入-包络)/输入，增益平滑 1.5ms 降调制失真（THD -83dB）；±100 双向 | edgeAmount |

**已删除（用户要求）**：混响模块、AutoGain 功能、所有 Bypass 按钮（参数+DSP+UI 全清）。

**输出电平表**：Processor 每块算输入/输出 RMS → VU 平滑（attack 10ms/release 300ms）→ atomic。
**GR 表**：压缩 gainReduction + 去齿音 gainReduction（双频段削减和）。
**上帝粒子指示灯**（菱形，Output 右侧）：只监测**输入电平**——在 Input 增益后、处理链前测 RMS，压缩/EQ/染色等处理把音量压下去不影响它，作混音前的工作电平参考（偏低熄灭/完美紫闪/过高爆红，-18dB~-6dB 区间）。

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

**beta0.5 发布（2026-08-08）**：
- [x] 版本号 0.2.0 → 0.5.0（project VERSION + 3 处 VersionString + 3 处 VersionCode 0x500）
- [x] Windows 编译打包：`dist/MoonVoc_beta0.5_Win64.zip`（VST3 + Standalone + 说明），headless 复跑全绿
- [x] `.gitignore` 放行 `assets/moon.jpg`（否则 CI/换机编译缺素材）、忽略 `dist/`
- [x] 已 push GitHub main（`eb45776` release / `71cded2` workflow fix / `622095e` 删 workflow）
- [ ] macOS 未产出：GitHub Actions workflow 解析失败（on/name 未被识别），已删除，见 §11

**2026-08-15 超采样档位调整（未提交）**：
- [x] 档位 **Off/2x/4x/8x/16x → 2x/4x/8x/16x**（去掉 Off，默认仍 16x=index 3）；`osExponents` 保持 {1,2,3,4}，index 0~3 直接映射，latency 恒上报
- [x] **根因**：用户"看不到调节窗口"是因为 `drawComboBox` 不绘制选中文字、无标签 → 底部横条加 "Oversampling" 标签 + LAF 绘制当前档位文字（所有下拉框受益）
- [x] 测试适配：16x 越界 4.0→3.0；严格直通（原依赖 Off 逐样本相等）改为 **RMS 能量守恒**（2x FIR 通带纹波 ~0.15%，容差 1%）；headless 全绿 EXIT=0
- [ ] 待打包发布 / 更新用户测试包

**2026-09-26 UI 全面重构 v0.6.0（青蓝·珊瑚亮色卡片主题）**：
- [x] **根因**：用户评"旧 UI 太丑"（深紫黑 + 5层辉光/星云/星野/金属拉丝/能量连线/神秘符文/月亮照片等特效堆叠，特效盖过功能）
- [x] **新方向（用户选定）**：青蓝(#2dd4bf)·珊瑚(#ff6b6b)亮色底(#f2f0ec米白) + 水平信号链卡片 + 轻拟物旋钮 + 抽象几何背景淡动效 + 保留文字品牌去特效 + 移除月亮照片。对标 FabFilter/苹果
- [x] **调色板**：`MoonVocLookAndFeel.h` Theme 12→14 常量全换亮色系（删 accentHi，新增 accent2/track/shadow）
- [x] **布局**：窗口 1280×660→**1280×720**；顶部全局条卡(品牌+Input/Headroom/Output+指示灯) → 信号链 5 卡横排(DeEss180/EQ320/Comp272/Sat260/Edge152，间距16) → 底部 Monitor(电平+GR)+Engine(超采样) 双卡 → 页脚 logo；旋钮直径两档 120 hero(Comp/Edge)/64 标准
- [x] **LookAndFeel 全重写**：drawRotarySlider 轻拟物 7 层(track 弧环→青橙渐变数值弧→投影→白体微渐变→顶部光晕→短指针→悬停细环)；drawComboBox 白底胶囊；drawToggleButton iOS pill；删除 drawGlowText 5层辉光
- [x] **资源清理**：删 SemiBold/Medium 字体（~900KB 死重，fontTitle/Section/Label/Value 全用 Bold）+ 删 moon.jpg + 删 stars/metalTexture 死代码
- [x] **顺手修 2 个既有 bug**：indicatorRect 死赋值、右列 sectionEdge 误用（电平表进 Monitor 卡，新增 sectionMonitor/sectionOs）
- [x] **踩坑**：JUCE ComboBox 内部有子 Label 画选中文字（juce_ComboBox.cpp:275），drawComboBox 不可再画否则重叠（"Pop"→"POpp"）；演示截图需 `setValueNotifyingHost(convertTo0to1(v))` 才能触发 attachment 同步 Slider
- [x] **验证**：headless EXIT=0 全绿(31项)、UiSnapshot FAIL=0、演示态截图确认数值弧/电平/GR/指示灯/品牌渐变全部点亮
- [ ] 待打包发布 / 更新用户测试包 / git push

**2026-09-26 UI 修正 v0.6.1（用户反馈 5 项）**：
- [x] **字体太小**：数值框覆盖 `createSliderTextBox` 设 `fontValue(15)`（原走 Label 默认字体）；标签 13→14、区标题 13→15、lock 11→12、电平/GR 12/13→13/14、下拉 13→14；`drawLabel` 下限 13→12
- [x] **旋钮零值改 12 点**：`drawRotarySlider` 角度重写——零值固定角 0（12 点），正值顺时针到 +135°、负值逆时针到 -135°（双极全环/单向右半环）；**删除 `moonvocSingleSided` 属性与 twoSided 分支**
- [x] **修指针/数值弧错位 bug（根因）**：旧代码指针用 `cos/sin` 数学约定（0=3 点方向），弧环用 `addCentredArc` 的 JUCE 约定（0=12 点），差 90° → 零值指针显示在 10:30、弧从别处出发。现统一 `(sin a, -cos a)`，数值弧恒从零位出发到指针
- [x] **EQ 四旋钮改两行两列**：Thick/De-Box 上行、Clarity/Air 下行（列槽 x+24/x+168 宽 128，行 y+44/y+164），airFreqBox 在 Air 下方底部行
- [x] **莫兰迪卡片**：Theme 新增 8 组卡片色（燕麦沙/豆沙粉/鼠尾草/雾霾蓝/焦糖杏/香芋紫/冷雾灰/米灰，底色+深色成对）；卡片体填模块浅色、描边深色 30%、区标题/竖条/数值弧/指针/悬停环全用模块深色（`setupSlider` 多传 arcColour → slider 属性 `moonvocArcColor`，LAF 读取）；卡片圆角 14→16
- [x] **布局配套**：slider bounds 含数值框（kTbH=18，原 64×64 正好框导致旋钮实绘仅 46px）；全局卡 h 128→140 放下数值框；标准旋钮 d=64 实绘 64、hero 120 实绘 120
- [x] **验证**：headless EXIT=0、UiSnapshot FAIL=0（EQ 行列对齐断言更新）、截图肉眼确认零值指针全在 12 点、弧从指针零位出发、Input +2 弧在右侧
- [x] **已打包**：`dist/MoonVoc_0.6.1_Win64.zip`（4.6MB：VST3 + Standalone + 详细使用说明.txt，UTF-8 BOM+CRLF 防记事本乱码；zip 中文名 UTF-8+flag 已验证）发给朋友测试；git push 仍待做

**待办**：
- [ ] **AI 未做**：正式发布需 pluginval 验证（未安装）、macOS 移植（已尝试未成，见 §11）
- [ ] 图标/安装包/签名未做
- [ ] README.md 未写（可基于 docs/requirements.md 生成）
- [x] ~~UI 布局未动~~ → **2026-09-26 已全面重构（v0.6.0 卡片式亮色主题，见上）**
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

## 9. 测试基线（beta0.5 复跑全绿，EXIT=0 无 FAIL）

- 系数公式 vs JUCE 官方：maxErr ≤ 2.4e-7 ✓（7 组：lowShelf ±6 →1.19e-7、highShelf +3 →2.38e-7、peak ±5 →1.19e-7、bandpass →9.31e-10 / Q6 →1.19e-7）
- 默认直通 THD：**-92.0dB** ✓（FIR 超采样后比 v0.1 的 -89.5dB 更低）；压缩 100%：-117.2dB；Edge ±100：-76.9/-75.5dB；去齿音 100%：-92.0dB；饱和 FET/Tube：-48.4/-53.3dB（设计染色）
- 压缩：GR -25.1dB（100%）；智能参数：正弦→2.64ms/3.8:1，脉冲→0.79ms/5.4:1 ✓
- 去齿音：4k 削 -31.9dB / 1k 仅 -0.5dB（零染色）✓
- EQ 频响：100Hz +4.2 / 400Hz +3.4 / 3kHz +5.7 / 16kHz +3.0 ✓；Clarity 锁定 593/3150、Air 锁定 593/5934；双共振同时锁 315+560 削减 ✓
- **bypass 全链直通：maxErr=0.00（逐样本完全一致）✓**
- **flat16 频响（16x 超采样平坦度）：100/400/1k/4k/8k/15k Hz 全频段 ±0.10dB ✓**
- **Clarity 峰锁定：锁定后最差偏移 0Hz（<250）✓；刺耳峰限增益 6.3dB（2~10 区间）✓**
- 随机压力 4 种块大小 × 1500 块（共 6000 块）+ 每 200 块 OS 超采样热切换/全局增益变化 + 每 50 块随机 EQ 参数：无 NaN 无崩溃 ✓
- mono 通道 ✓；电平表 in/out -15.1dB、静音衰减 -60 ✓

## 10. 用户联系

- 反馈邮箱（写在测试包说明里）：1399456751@qq.com
- GitHub：github.com/1399456751-debug（账号名 1399456751-debug）

## 11. 版本发布记录（beta0.5，2026-08-08）

### 交付物
- **Windows zip**：`E:\VST Effects Plugin Collection\moonvoc\dist\MoonVoc_beta0.5_Win64.zip`（5.4MB）
  - 内容：`MoonVoc.vst3`（装 `C:\Program Files\Common Files\VST3\`）+ `MoonVoc.exe`（独立运行）+ `README_Install.txt`（中文安装说明）
  - 构建：build.bat（Release），headless 回归复跑全绿（EXIT=0、无 FAIL）
- **GitHub main 已同步**：`eb45776`（release beta0.5 + CMakeLists JUCE_ROOT + moon.jpg 入库 + workflow 初版）/ `71cded2`（workflow 改纯 ASCII）/ `622095e`（删除 workflow）

### 网络与 push 备忘（重要）
- **本机直连 github.com 不通**（443 被重置），必须走代理：`HTTP_PROXY/HTTPS_PROXY=http://127.0.0.1:7897`（Clash Verge，用户本机端口 7897）
- git push / gh 命令都要带代理环境变量（bash 每次调用 shell 状态不持久，需在同一命令内 export 后再执行）
- gh token 已补 `workflow` scope（`gh auth refresh -h github.com -s workflow`），以后可建/改 workflow 文件

### macOS 移植失败记录（若下次再做，先读这里）
- **硬约束**：GitHub Actions 免费版无法给 AU 签名（unsigned AU 无法加载），Mac 版只能产 **VST3 + Standalone**，且 VST3 未签名首次加载需右键"打开"或 `xattr -cr`
- **曾尝试**：写 `.github/workflows/build-mac.yml`（universal arm64+x86_64，clone JUCE 9.0.0 → cmake → 打包 upload-artifact），push 后 GitHub **未能解析 on/name**：`gh api .../actions/workflows/<file>` 的 `events` 为空、`name` 回退为文件路径名、`workflow_dispatch` 触发返回 422
- **已排查**：文件无 BOM（UTF-8 无 BOM）、行尾纯 LF（CR=0）、YAML 结构标准；注释与 name 改纯 ASCII 重 push 后仍不解析。**根因未查明**
- **建议**：下次先建一个仅含 `on: workflow_dispatch` + 一条 echo 的极简 workflow 验证 GitHub 能否解析，再逐步加内容；push tag `v*` 也可作为触发路径
