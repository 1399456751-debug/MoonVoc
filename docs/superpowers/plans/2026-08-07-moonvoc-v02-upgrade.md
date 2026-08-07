# MoonVoc v0.2 升级实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 4 个用户反馈问题——Clarity 频点扫动刺挠、整体声音"怪"（闷/脏/平淡）、UI 氛围单调、月亮看不清——升级为 v0.2。

**Architecture:** DSP 侧在 [VoiceEq.cpp](E:/VST Effects Plugin Collection/moonvoc/Source/dsp/VoiceEq.cpp) 给共振峰检测加峰锁定 + 限增益消除扫动；在 [PluginProcessor.cpp](E:/VST Effects Plugin Collection/moonvoc/Source/PluginProcessor.cpp) 把超采样 IIR 半带换成线性相位 FIR；UI 侧在 [PluginEditor.cpp](E:/VST Effects Plugin Collection/moonvoc/Source/PluginEditor.cpp) 加动态氛围 + 月亮居中圆月 + 右上角品牌徽章，[MoonVocLookAndFeel.cpp](E:/VST Effects Plugin Collection/moonvoc/Source/UI/MoonVocLookAndFeel.cpp) 旋钮/下拉微升级。全部用 HeadlessTest 回归 + UiSnapshot 布局断言验证。

**Tech Stack:** JUCE 9（headless，`C:\JUCE`）、C++17、MSVC 14.44 + CMake 4.4.2 + Ninja、Windows 11。

## Global Constraints

- **构建命令**（bash 里调 cmd 有引号坑，必须走 PowerShell）：
  `powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'"`
- **回归测试**：`"E:/VST Effects Plugin Collection/moonvoc/build/MoonVocHeadlessTest.exe"`（非 0 退出 = 失败；`SKIP_RANDOM_PASS=1` 可跳过随机段加速）
- **UI 布局自检**：`"E:/VST Effects Plugin Collection/moonvoc/build/MoonVocUiSnapshot.exe"`（打印所有控件坐标）
- **离线渲染**：`"E:/VST Effects Plugin Collection/moonvoc/build/MoonVocRender.exe" --test out.wav`
- 项目路径含空格：所有命令用引号包裹路径。
- 全链 DSP 模块跑在超采样链内：**所有滤波器系数、时间常数、检测器必须按当前 `dspRate`（OS 采样率）动态更新**，禁止 prepare 时定死。检测器 EMA alpha 每块按 `1 - exp(-blockDur/tau)` 算（`blockDur = numSamples/dspRate`）。
- JUCE 9 差异：`IIR::Filter` 只处理单声道（每通道一份实例 + `getSingleChannelBlock` 手动循环）；系数用公开成员 `coefficients`，预分配 Ptr + 每块重写 `getRawCoefficients()` 5 元素数组 `[b0,b1,b2,a1,a2]`；`SmoothedValue` 块处理必须 `skip(blockSize)`。
- C++17；类 PascalCase、函数/变量 camelCase；注释精简；不用裸 new/delete。
- 每个任务独立可测、独立 commit；commit message 用中文 + `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。
- **测试参数残留会污染测量**：每个测试块开头必须全量重置参数（照抄现有 `for (auto id : {...})` 模式）。

---

### Task 1: Clarity 峰锁定 + 限增益 + 追踪稳定化

**Files:**
- Modify: `Source/dsp/VoiceEq.h:29-53`（`SmartBand` 加锁定状态 + `targetContrast`）
- Modify: `Source/dsp/VoiceEq.cpp:96,101`（alpha 0.05→0.12）、`:191-206`（`updateCoeffs` 限增益）、`:114-189`（`detect` 重构峰锁定）、`:7`（`kFreqSmoothTime` 0.25→0.5）
- Modify: `test/HeadlessTest.cpp`（新增 2 项测试）

**Interfaces:**
- Consumes: `MoonVocProcessor::getEqClarityFreq(int)`（返回 `clarityFreqDisplay[i]`，0 = 无峰）
- Produces: `VoiceEq::SmartBand` 新增成员 `int lockedIndex`、`float lockedBaseline`、`int unlockTimer`、`float targetContrast[3]`

#### 设计说明

锁定的核心规则：
- 锁定一个峰后，`detect()` 只在其 `±1` 候选带邻域内滑动，频点永不在大范围切换 → 扫动消失。
- 锁定峰能量相对基线衰减 >6dB（`le < lockedBaseline * 0.5`）连续 8 块才解锁 → 不会因短暂波动跳变。
- 锁定峰强制占 `active[0]`，其余候选峰仍可动态填充 `active[1]`/`active[2]`（保留多峰同时处理能力，现有 327+560 双共振测试继续通过）。
- 限增益：`updateCoeffs` 里 `effectiveGain = gainDb × (1 - 0.5 × clamp((contrast-2)/3))`，对比度 2→满增益、≥5→50%。

- [ ] **Step 1: `VoiceEq.h` 给 `SmartBand` 加锁定状态**

在 `SmartBand` 内 `float defaultFreq = 400.0f;` 之后加：

```cpp
// 峰锁定状态（消除频点扫动；-1=未锁定）
int lockedIndex = -1;
float lockedBaseline = 0.0f;
int unlockTimer = 0;
float targetContrast[3] { 0.0f, 0.0f, 0.0f }; // 各峰对比度（限增益用）
```

- [ ] **Step 2: `VoiceEq.cpp` 改两处常量**

`kFreqSmoothTime` 0.25 → 0.5（`VoiceEq.cpp:7`）：

```cpp
constexpr float kFreqSmoothTime = 0.5f; // 智能频点平滑时间（秒）：0.25→0.5 减缓频点漂移
```

`runDetectors` 的检测器 EMA 时间常数 0.05 → 0.12（`VoiceEq.cpp:96`），prepare 里 `VoiceEq.cpp:59` 同改：

```cpp
// runDetectors 内：
const float alpha = 1.0f - std::exp(-((double) numSamples / fs) / 0.12);
// SmartBand::prepare 内：
alpha = 1.0f - (float) std::exp(-((double) spec.maximumBlockSize / fs) / 0.12);
```

- [ ] **Step 3: 重构 `SmartBand::detect()` 加峰锁定**

把整个 `detect()`（`VoiceEq.cpp:114-189`）替换为：

```cpp
void VoiceEq::SmartBand::detect(const float* cands)
{
    // 底噪检查：无信号 → 全直通 + 解锁
    float maxEma = 0.0f;
    for (int i = 2; i <= 10; ++i)
        maxEma = jmax(maxEma, ema[i]);
    if (maxEma < 1.0e-8f)
    {
        for (int p = 0; p < kMaxPeaks; ++p)
            active[p] = false;
        lockedIndex = -1;
        unlockTimer = 0;
        return;
    }

    // 候选峰：局部最大 + 对比度超阈值
    struct Peak { int index; float contrast; };
    Peak peaks[13];
    int numPeaks = 0;
    for (int i = 2; i <= 10; ++i)
    {
        if (ema[i] <= ema[i - 1] || ema[i] <= ema[i + 1])
            continue;
        const float neigh = (ema[i - 2] + ema[i - 1] + ema[i + 1] + ema[i + 2]) * 0.25f;
        const float contrast = ema[i] / (neigh + 1.0e-12f);
        if (contrast > kResonanceThreshold)
            peaks[numPeaks++] = { i, contrast };
    }

    // 按对比度排序（插入排序，量小）
    for (int a = 1; a < numPeaks; ++a)
    {
        const Peak key = peaks[a];
        int b = a - 1;
        while (b >= 0 && peaks[b].contrast < key.contrast)
        {
            peaks[b + 1] = peaks[b];
            --b;
        }
        peaks[b + 1] = key;
    }

    // ---- 峰锁定：锁定峰（若仍强）强制占 active[0]，仅邻域滑动 ----
    bool lockHeld = false;
    int pickedIdx[kMaxPeaks] { -1, -1, -1 };
    if (lockedIndex >= 0)
    {
        int li = lockedIndex;
        float le = ema[li];
        for (int i = jmax(2, lockedIndex - 1); i <= jmin(10, lockedIndex + 1); ++i)
            if (ema[i] > le) { le = ema[i]; li = i; }

        if (le >= lockedBaseline * 0.5f)   // 未衰减 >6dB → 保持锁定
        {
            unlockTimer = 0;
            lockedIndex = li;              // 允许 ±1 带内滑动
            lockedBaseline = le;           // 滚动基线
            // 邻域抛物线插值（不超出 ±1 带，杜绝跳变）
            const float l = std::log(ema[li - 1]), c = std::log(ema[li]), r = std::log(ema[li + 1]);
            const float denom = l - 2.0f * c + r;
            float d = (denom > 1.0e-9f) ? 0.5f * (l - r) / denom : 0.0f;
            d = jlimit(-0.5f, 0.5f, d);
            targetFreq[0] = cands[li] * std::exp(d * std::log(cands[li + 1] / cands[li]));
            targetQ[0] = 0.9f;                                   // 锁定期 Q 温和固定
            targetContrast[0] = le / ((ema[li - 1] + ema[li + 1]) * 0.5f + 1.0e-12f);
            active[0] = true;
            pickedIdx[0] = li;
            lockHeld = true;
        }
        else if (++unlockTimer < 8)
        {
            return;                        // 确认期内保持上一帧输出，频点不跳
        }
        else
        {
            lockedIndex = -1;              // 衰减确认 → 解锁，重新全局检测
            unlockTimer = 0;
        }
    }

    // ---- 填充其余路：候选峰按对比度选，间距去重（含锁定峰） ----
    int picked = lockHeld ? 1 : 0;
    for (int a = 0; a < numPeaks && picked < kMaxPeaks; ++a)
    {
        const int i = peaks[a].index;
        bool tooClose = false;
        for (int p = 0; p < picked; ++p)
            if (std::abs(i - pickedIdx[p]) < kPeakSpacing)
            {
                tooClose = true;
                break;
            }
        if (tooClose)
            continue;

        // 对数域抛物线插值精确定位
        const float l = std::log(ema[i - 1]), c = std::log(ema[i]), r = std::log(ema[i + 1]);
        const float denom = l - 2.0f * c + r;
        float d = (denom > 1.0e-9f) ? 0.5f * (l - r) / denom : 0.0f;
        d = jlimit(-0.5f, 0.5f, d);
        targetFreq[picked] = cands[i] * std::exp(d * std::log(cands[i + 1] / cands[i]));
        targetQ[picked] = juce::jmap(peaks[a].contrast, kResonanceThreshold, 5.0f, kQMin, kQMax);
        targetContrast[picked] = peaks[a].contrast;
        active[picked] = true;
        pickedIdx[picked] = i;
        ++picked;
    }

    // 未锁定时：把对比度最高的候选峰设为新锁定峰
    if (! lockHeld && picked > 0)
    {
        lockedIndex = pickedIdx[0];
        lockedBaseline = ema[lockedIndex];
        unlockTimer = 0;
    }

    // 未使用的路 → 直通
    for (int p = picked; p < kMaxPeaks; ++p)
        active[p] = false;
}
```

- [ ] **Step 4: `updateCoeffs()` 加限增益**

把 `updateCoeffs`（`VoiceEq.cpp:191-206`）的活跃分支替换为：

```cpp
        const float freq = freqSmooth[p].skip(numSamples);
        const float q = qSmooth[p].skip(numSamples);
        // 限增益：对比度 2.0 → 满增益；≥5.0 → 50%（刺耳峰少提升）
        const float cap = 1.0f - 0.5f * jlimit(0.0f, 1.0f, (targetContrast[p] - kResonanceThreshold) / 3.0f);
        VoiceEq::writePeak(coeffs[p]->getRawCoefficients(), fs, freq, q, gainDb * cap);
```

（`targetContrast` 未激活时不会走到这里，因为 `!active[p]` 已提前 `continue`。）

- [ ] **Step 5: 写峰锁定测试（先失败）**

在 `test/HeadlessTest.cpp` 的 `main()` 里，`// 系数公式对比` 块之前插入：

```cpp
    // Clarity 峰锁定检查：f1=3150 恒定强、f2=2500 周期性增强 → 锁定应保持 f1，不来回跳
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup", "compMode",
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("eqClarityBoost") = 6.0f;

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 3000; ++b)
        {
            const float f2Amp = (b % 400 < 200) ? 0.32f : 0.03f; // f2 周期性增强
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                {
                    const float t = (float) (b * 512 + n) / 48000.0f;
                    buf.setSample(c, n, 0.30f * std::sin(2.0f * 3.14159f * 3150.0f * t)
                                       + f2Amp * std::sin(2.0f * 3.14159f * 2500.0f * t));
                }
            processor.processBlock(buf, midi);
        }
        const float locked = processor.getEqClarityFreq(0);
        TRACE("clarity lock check: locked=%d Hz (expect ~3150) %s\n",
              (int) locked, std::abs(locked - 3150.0f) < 250.0f ? "OK" : "BAD");
    }
```

- [ ] **Step 6: 写限增益测试（先失败）**

接在上一步测试块之后：

```cpp
    // Clarity 限增益检查：单一强共振（高对比度）→ 实际提升显著低于用户 +12dB
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup", "compMode",
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("eqClarityBoost") = 12.0f;

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        const int warmup = 400, measure = 2000;   // warmup 足够让检测器锁定共振峰
        double outSq = 0.0;
        for (int b = 0; b < warmup + measure; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                {
                    const float t = (float) (b * 512 + n) / 48000.0f;
                    buf.setSample(c, n, 0.25f * std::sin(2.0f * 3.14159f * 3150.0f * t));
                }
            processor.processBlock(buf, midi);
            if (b >= warmup)
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        outSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
        }
        const float ratio = (float) std::sqrt(outSq / (2.0 * measure * 512.0)) / (0.25f * 0.7071f);
        const float gainDb = 20.0f * std::log10(ratio);
        TRACE("clarity cap check: gain=%.1f dB (expect < 10, > 2) %s\n",
              gainDb, gainDb < 10.0f && gainDb > 2.0f ? "OK" : "BAD");
    }
```

- [ ] **Step 7: 构建并跑回归**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'"
"E:/VST Effects Plugin Collection/moonvoc/build/MoonVocHeadlessTest.exe"
```

Expected: 新加 `clarity lock check OK`、`clarity cap check OK`；原有 21 项（含 EQ 双共振、频响、THD）全 `OK`；exit 0。

- [ ] **Step 8: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc"
git add Source/dsp/VoiceEq.h Source/dsp/VoiceEq.cpp test/HeadlessTest.cpp
git commit -m "dsp: Clarity 峰锁定消除频点扫动 + 刺耳峰限增益

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: 全链 0 值严格直通测试

**Files:**
- Modify: `test/HeadlessTest.cpp`（新增默认直通测试）

**Interfaces:**
- Consumes: `MoonVocProcessor`（全参数 0、oversampling 0 时的逐样本直通）
- Produces: 无新接口

#### 设计说明

把"参数全 0 时链必须数学直通"固化成测试。当前代码审查结论：DeEsser `amount<=0` 时 `continue` 不碰音频、Comp `amount=0` 时增益恒 1.0、Edge `amount=0` 时目标增益恒 1.0、Sat `type=Off` 直接 return、EQ 增益 0 时系数为 `[1,0,0,0,0]`——应全部满足。若测试暴露某模块不直通，用二分法（临时跳过某模块 process）定位修复。

- [ ] **Step 1: 写默认直通测试**

在 `// 保真度检查` 块之前插入：

```cpp
    // 默认全 0 严格直通：oversampling=0 时输出必须逐样本等于输入（float 精度内）
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup", "compMode",
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;

        juce::AudioBuffer<float> ref(2, 512), buf(2, 512);
        juce::MidiBuffer midi;
        // 多样化信号：正弦 + 噪声 + 稀疏脉冲
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 512; ++n)
                ref.setSample(c, n, 0.3f * std::sin(2.0f * 3.14159f * 937.5f * (float) n / 48000.0f)
                                   + 0.05f * (rng.nextFloat() * 2.0f - 1.0f)
                                   + (n < 8 ? 0.4f : 0.0f));

        float maxErr = 0.0f;
        for (int b = 0; b < 50; ++b)
        {
            buf.copyFrom(ref, 0, 0, 0, 512);   // 每块从 ref 复制，隔离链内部状态
            processor.processBlock(buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                    maxErr = jmax(maxErr, std::abs(buf.getSample(c, n) - ref.getSample(c, n)));
        }
        TRACE("bypass check: maxErr=%.2e %s\n", maxErr, maxErr < 1.0e-6f ? "OK" : "BAD");
    }
```

- [ ] **Step 2: 构建并跑，确认通过**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'"
"E:/VST Effects Plugin Collection/moonvoc/build/MoonVocHeadlessTest.exe"
```

Expected: `bypass check: maxErr < 1e-6 OK`。

**若失败**：这是 bug 信号。用二分法定位——在 `PluginProcessor::processBlock` 里临时注释掉 `deEsser/eq/comp/sat/edge.process` 中的一段，逐个恢复，找出引入误差的模块，修复其 0 值直通路径，再重跑。修复后同样 commit。

- [ ] **Step 3: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc"
git add test/HeadlessTest.cpp
git commit -m "test: 新增全链 0 值严格直通回归断言

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: 超采样 IIR 换线性相位 FIR

**Files:**
- Modify: `Source/PluginProcessor.cpp:93-94`（`Oversampling` 滤波器类型）
- Modify: `test/HeadlessTest.cpp`（新增 16x 频响平直测试 + 基线重测）

**Interfaces:**
- Consumes: `juce::dsp::Oversampling<float>`（构造参数 `filterHalfBandFIREquiripple`）
- Produces: latency 由 `oversamplers[i]->getLatencyInSamples()` 动态上报（代码已是动态，无需改）

#### 设计说明

`filterHalfBandPolyphaseIIR`（非线相位）在 16x 级联下相位失真累积，导致人声"闷/虚"。换成线性相位 `filterHalfBandFIREquiripple`。latency 上报两处（`prepareToPlay:111` 和热切换 `:181`）都已动态取 `getLatencyInSamples()`，改类型后自动正确。CPU 更高，16x 下用 `--test` 渲染验证不超时。

- [ ] **Step 1: 改滤波器类型**

`PluginProcessor.cpp:94`：

```cpp
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
```

- [ ] **Step 2: 写 16x 频响平直测试**

在 `// 频率响应方向检查` 块之后插入（新增辅助函数加在文件顶部 `bandGainRatio` 旁）：

```cpp
// 默认全 0 参数下测某频率 RMS 比值（频响平直 / 直通验证；oversampling 由调用方设置）
static float defaultRatio(MoonVocProcessor& p, float freq)
{
    const auto& apvts = p.apvts;
    for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                     "compAmount", "compMakeup", "compMode",
                     "dsLowAmount", "dsHighAmount",
                     "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                     "edgeAmount",
                     "inputGain", "outputGain", "headroom" })
        *apvts.getRawParameterValue(id) = 0.0f;

    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    const int warmup = 200, measure = 2000;
    double outSq = 0.0;
    for (int b = 0; b < warmup + measure; ++b)
    {
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 512; ++n)
                buf.setSample(c, n, 0.25f * std::sin(2.0f * 3.14159f * freq * (float) (b * 512 + n) / 48000.0f));
        p.processBlock(buf, midi);
        if (b >= warmup)
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                    outSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
    }
    const float rms = (float) std::sqrt(outSq / (2.0 * measure * 512.0));
    return rms / (0.25f * 0.7071f);
}
```

`main()` 里加测试块：

```cpp
    // 16x 下默认链频响平直检查：100Hz~15kHz 输出/输入比值 ≈ 1.0（±0.03）
    {
        processor.prepareToPlay(48000.0, 512);
        *apvts.getRawParameterValue("oversampling") = 4.0f; // 16x
        const float freqs[] { 100.0f, 400.0f, 1000.0f, 4000.0f, 8000.0f, 15000.0f };
        bool ok = true;
        for (float f : freqs)
        {
            const float r = defaultRatio(processor, f);
            const bool fOk = std::abs(r - 1.0f) < 0.03f;
            ok = ok && fOk;
            TRACE("flat16 check: %g Hz -> x%.4f (%+.2f dB) %s\n",
                  f, r, 20.0 * std::log10(r), fOk ? "OK" : "BAD");
        }
        if (! ok) { TRACE("flat16 check: BAD\n"); return 1; }
        TRACE("flat16 check: OK\n");
    }
```

- [ ] **Step 3: 构建 + 全回归 + 渲染冒烟**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'"
"E:/VST Effects Plugin Collection/moonvoc/build/MoonVocHeadlessTest.exe"
"E:/VST Effects Plugin Collection/moonvoc/build/MoonVocRender.exe" --test build/fir_check.wav
```

Expected: `flat16 check` 各频点 `OK`；fidelity THD 断言（`defThd < -70`）仍通过（FIR 数值不同但应更优，若恰好在阈值边缘，如实更新断言值并在 commit message 说明）；渲染 8 秒测试信号无超时、输出文件生成。

**若 16x FIR 渲染极慢/崩溃**：回退为按倍率混用——`osExponents[0..1]`（2x/4x）用 `filterHalfBandFIREquiripple`、`osExponents[2..3]`（8x/16x）保持 IIR，并在此 commit 里说明取舍。

- [ ] **Step 4: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc"
git add Source/PluginProcessor.cpp test/HeadlessTest.cpp
git commit -m "dsp: 超采样半带 IIR 换线性相位 FIR（相位更正、高频更干净）

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: UI 氛围 + 月亮圆月 + 品牌徽章 + 控件微升级

**Files:**
- Modify: `Source/PluginEditor.h:44`（`mousePos` 成员 + `mouseMove` 声明）
- Modify: `Source/PluginEditor.cpp`（月亮绘制、血月→徽章、氛围元素、悬停发光）
- Modify: `Source/UI/MoonVocLookAndFeel.cpp:139-143,205-206`（旋钮光晕脉冲、下拉悬停）
- Test: `build/MoonVocUiSnapshot.exe`（布局断言）

**Interfaces:**
- Consumes: `Theme::accent/accentHi/textDim/fontLabel`、`moonImage`、`indicatorPhase`、`processorRef.inputLevelDb`
- Produces: 无新接口；**所有控件 bounds 保持不变**（UiSnapshot 输出必须与 Task 前一致）

#### 设计说明

分四块：(a) 月亮从全屏 20% 透明改为居中圆形主视觉；(b) 右上角程序化血月替换为品牌徽章；(c) 背景氛围：星云光斑、能量粒子流、电平驱动星野、标题呼吸、面板悬停发光；(d) 旋钮拖拽光晕脉冲 + 下拉悬停提亮。全部在现有 `paint()`/`timerCallback()` 10Hz 节奏内，不新增 Timer、不新增每帧分配。

- [ ] **Step 1: `PluginEditor.h` 加鼠标悬停追踪**

在 `juce::Image moonImage;` 之后加成员：

```cpp
    juce::Point<int> mousePos; // 鼠标位置（面板悬停发光用）
```

在类声明里加 override：

```cpp
    void mouseMove(const juce::MouseEvent& e) override { mousePos = e.getPosition(); }
```

（若 `AudioProcessorEditor` 不默认接收 mouseMove，构造里加 `setMouseClickGrabsKeyboardFocus(false);` 即可——不需其他设置，事件会命中编辑器本体。）

- [ ] **Step 2: 月亮改成居中圆形主视觉**

替换 `paint()` 里的月亮块（`PluginEditor.cpp:379-385`）：

```cpp
    // 月亮背景：居中圆形主视觉（取代全屏 20% 半透明）
    if (moonImage.isValid())
    {
        const float moonDiam = getHeight() * 0.55f;
        const juce::Point<float> mc { getWidth() * 0.5f, getHeight() * 0.42f };
        const juce::Rectangle<float> mr { mc.x - moonDiam * 0.5f, mc.y - moonDiam * 0.5f,
                                          moonDiam, moonDiam };
        // 圆形裁剪 + 柔边：先画带柔边的光晕，再画圆内月轮
        juce::ColourGradient halo(Theme::accent.withAlpha(0.14f), mc.x, mc.y,
                                  juce::Colours::transparentBlack, mc.x, mc.y + moonDiam * 0.52f, true);
        g.setGradientFill(halo);
        g.fillEllipse(mc.x - moonDiam * 0.52f, mc.y - moonDiam * 0.52f,
                      moonDiam * 1.04f, moonDiam * 1.04f);
        juce::Path clip;
        clip.addEllipse(mr);
        g.saveState();
        g.reduceClipRegion(clip);
        g.setOpacity(0.50f);
        g.drawImage(moonImage, mr, juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
        g.restoreState();
    }
```

（注意：月亮画在星野之前、金属纹理之后，视觉上压在金属纹理上。将原金属纹理绘制块保持不动。）

- [ ] **Step 3: 右上角血月替换为品牌徽章**

替换整个 `// 程序化血月` 块（`PluginEditor.cpp:402-421`）：

```cpp
    // 右上角环形品牌徽章（替换程序化血月）：呼吸光晕 + 圆环 + 八角星标 + 上下文字
    {
        const juce::Point<float> mc { getWidth() - 150.0f, 100.0f };
        const float r = 30.0f;
        const float breathe = 0.5f + 0.5f * std::sin(indicatorPhase * 0.5f);
        g.setColour(Theme::accent.withAlpha(0.10f + 0.10f * breathe));
        g.fillEllipse(mc.x - r * 1.7f, mc.y - r * 1.7f, r * 3.4f, r * 3.4f);
        g.setColour(Theme::accent.withAlpha(0.5f));
        g.drawEllipse(mc.x - r, mc.y - r, r * 2, r * 2, 1.2f);
        // 八角星标（两菱形叠加）
        juce::Path star;
        star.addQuadrilateral(mc.x, mc.y - r * 0.55f, mc.x + r * 0.55f, mc.y,
                              mc.x, mc.y + r * 0.55f, mc.x - r * 0.55f, mc.y);
        star.addQuadrilateral(mc.x - r * 0.55f, mc.y, mc.x, mc.y + r * 0.55f,
                              mc.x + r * 0.55f, mc.y, mc.x, mc.y - r * 0.55f);
        g.setColour(Theme::accentHi);
        g.fillPath(star);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.fillEllipse(mc.x - 1.6f, mc.y - 1.6f, 3.2f, 3.2f);
        // 上下弧线文字
        Theme::drawGlowText(g, "TUJZMIXING",
                            juce::Rectangle<float>(mc.x - r, mc.y - r - 16, r * 2, 12).toFloat(),
                            Theme::fontLabel(9.0f), Theme::textDim, juce::Justification::centred);
        Theme::drawGlowText(g, "MOONVOC SYSTEMS",
                            juce::Rectangle<float>(mc.x - r, mc.y + r + 4, r * 2, 12).toFloat(),
                            Theme::fontLabel(9.0f), Theme::textDim, juce::Justification::centred);
    }
```

- [ ] **Step 4: 背景氛围——星云光斑 + 电平驱动星野 + 标题呼吸 + 粒子流 + 悬停发光**

(a) 在 `// 背景：深邃紫黑` 的 `fillAll` 之后、月亮之前，插入星云光斑：

```cpp
    // 星云光斑：2 个大半径紫色渐变斑，缓慢漂移（氛围层）
    {
        const juce::Point<float> c1 { getWidth() * (0.25f + 0.05f * std::sin(indicatorPhase * 0.21f)),
                                      getHeight() * (0.35f + 0.04f * std::cos(indicatorPhase * 0.17f)) };
        juce::ColourGradient n1(Theme::accent.withAlpha(0.05f), c1.x, c1.y,
                                juce::Colours::transparentBlack, c1.x + 220.0f, c1.y + 220.0f, true);
        g.setGradientFill(n1);
        g.fillEllipse(c1.x - 220.0f, c1.y - 220.0f, 440.0f, 440.0f);
        const juce::Point<float> c2 { getWidth() * (0.72f + 0.04f * std::cos(indicatorPhase * 0.13f)),
                                      getHeight() * (0.62f + 0.05f * std::sin(indicatorPhase * 0.19f)) };
        juce::ColourGradient n2(Theme::accentHi.withAlpha(0.04f), c2.x, c2.y,
                                juce::Colours::transparentBlack, c2.x + 260.0f, c2.y + 260.0f, true);
        g.setGradientFill(n2);
        g.fillEllipse(c2.x - 260.0f, c2.y - 260.0f, 520.0f, 520.0f);
    }
```

(b) 星野星星 alpha 加电平驱动（`PluginEditor.cpp:394-400` 星野循环内）：

```cpp
    const float lvlGlow = jlimit(0.0f, 1.0f, (processorRef.inputLevelDb.load() + 60.0f) / 60.0f);
    for (auto& s : stars)
    {
        const juce::Point<float> p { s.x * getWidth(), s.y * getHeight() };
        const float twinkle = 0.12f + 0.10f * (0.5f + 0.5f * std::sin(s.x * 7.3f + s.y * 3.1f + indicatorPhase * 0.5f));
        g.setColour(juce::Colours::white.withAlpha(jmin(0.9f, twinkle + 0.15f * lvlGlow)));
        g.fillEllipse(p.x, p.y, s.size, s.size);
    }
```

(c) 标题加呼吸光晕（`PluginEditor.cpp` 标题绘制 `titleBox` 之后、`drawGlowText` 之前）：

```cpp
    const float titleBreathe = 0.5f + 0.5f * std::sin(indicatorPhase * 0.7f);
    g.setColour(Theme::accent.withAlpha(0.03f + 0.03f * titleBreathe));
    g.fillRoundedRectangle(titleBox.toFloat().expanded(14.0f), 18.0f);
```

(d) 面板间能量连线加流动光点（`PluginEditor.cpp` 面板连线绘制之后）：

```cpp
    // 能量连线上的流动光点（0.55s 一趟）
    {
        const float t = juce::fmod(indicatorPhase, 1.0f);
        const float x = (float) panelGlobal.getCentreX();
        const float y = (float) panelGlobal.getBottom()
                      + ((float) panelLeft.getY() - (float) panelGlobal.getBottom()) * t;
        g.setColour(Theme::accentHi.withAlpha(0.8f));
        g.fillEllipse(x - 2.0f, y - 2.0f, 4.0f, 4.0f);
    }
```

(e) 面板悬停发光——在 `drawPanel` 四个调用（`PluginEditor.cpp:451-454`）之后追加悬停描边：

```cpp
    // 悬停发光描边（叠在面板之上）
    auto hoverEdge = [&](const juce::Rectangle<int>& r)
    {
        if (r.contains(mousePos))
        {
            g.setColour(Theme::accent.withAlpha(0.30f));
            g.drawRoundedRectangle(r.toFloat().expanded(2.0f), 8.0f, 1.5f);
        }
    };
    hoverEdge(panelGlobal); hoverEdge(panelLeft); hoverEdge(panelMid); hoverEdge(panelRight);
```

- [ ] **Step 5: 控件微升级——旋钮光晕脉冲 + 下拉悬停提亮**

`MoonVocLookAndFeel.cpp:139-143` 旋钮悬停/拖拽光圈改为带脉冲：

```cpp
    if (slider.isMouseOverOrDragging())
    {
        const float pulse = 0.16f + 0.10f * (0.5f + 0.5f * std::sin(slider.getValue() * 4.0f));
        g.setColour(Theme::accent.withAlpha(pulse));
        g.fillEllipse(area.expanded(4.0f));
    }
```

`drawComboBox`（`MoonVocLookAndFeel.cpp:198-214`）悬停时底色提亮：

```cpp
    if (cb.isMouseOver())
    {
        g.setColour(Theme::panel.brighter(0.10f));
        g.fillRoundedRectangle(r, 4.0f);
    }
```

（插在 `g.setColour(Theme::panel.darker(0.25f)); g.fillRoundedRectangle(r, 4.0f);` 之后。）

- [ ] **Step 6: 中列面板透明度让月亮透出**

`paint()` 里 `drawPanel` 的中列面板底：把 `g.fillRoundedRectangle(rf, 6.0f)` 前的面板底色 alpha 从 `0.78f` 调低。具体改 `drawPanel` lambda 的第一句填充：

```cpp
        // 面板底（更深的紫玻璃感）；中列面板更透明，让月亮透出
        const bool isMid = (r.getX() == panelMid.getX() && r.getWidth() == panelMid.getWidth());
        g.setColour(Theme::panel.darker(0.12f).withAlpha(isMid ? 0.60f : 0.78f));
        g.fillRoundedRectangle(rf, 6.0f);
```

- [ ] **Step 7: 构建 + UiSnapshot 断言布局不变**

```bash
powershell -NoProfile -Command "& 'E:\VST Effects Plugin Collection\moonvoc\build.bat'"
"E:/VST Effects Plugin Collection/moonvoc/build/MoonVocUiSnapshot.exe"
```

Expected: 输出的每个控件 bounds 与改造前的基线一致——尤其 `boostSlider/deboxSlider/claritySlider/airSlider` x/y/w/h、`edgeSlider` 150 正方、`compAmountSlider` 130 正方、`satTypeABox` 与 `satTypeBBox` 等大、`airFreqBox` 居中。逐项比对无变化（本次只改绘制，不碰 `resized()`）。

- [ ] **Step 8: 全回归 + 渲染冒烟**

```bash
"E:/VST Effects Plugin Collection/moonvoc/build/MoonVocHeadlessTest.exe"
```

Expected: 全部 OK，exit 0。

- [ ] **Step 9: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc"
git add Source/PluginEditor.h Source/PluginEditor.cpp Source/UI/MoonVocLookAndFeel.cpp
git commit -m "ui: 月亮居中圆月主视觉 + 右上角品牌徽章 + 动态氛围 + 控件微升级

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: 收尾——A/B 试听 + 文档更新

**Files:**
- Modify: `docs/HANDOFF.md`（§4 DSP 说明、§6 待办、测试基线）

- [ ] **Step 1: 渲染 A/B 对比音频**

```bash
"E:/VST Effects Plugin Collection/moonvoc/build/MoonVocRender.exe" --test build/v02_demo.wav
```

（用户用自己的干声素材再跑一次 `MoonVocRender.exe 输入.wav 输出.wav` 对比。）

- [ ] **Step 2: 更新 HANDOFF.md**

在 §4 的 VoiceEq 行补峰锁定/限增益说明；§6 待办勾掉已完成的项、新增"UI 布局未动，待用户后续反馈是否重构"；§9 测试基线按新 HeadlessTest 输出更新（含 flat16 频响、bypass 直通）。

- [ ] **Step 3: Commit**

```bash
cd "E:/VST Effects Plugin Collection/moonvoc"
git add docs/HANDOFF.md
git commit -m "docs: v0.2 交接文档更新（Clarity 峰锁定/限增益、FIR 超采样、UI 氛围）

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Self-Review 记录

- **Spec 覆盖**：① → Task 1；② 的 0 值直通 → Task 2、超采样 FIR → Task 3；③ → Task 4；④ 月亮圆月 → Task 4；右上角血月→品牌徽章 → Task 4 Step 3。设计文档 §5 的实施顺序（A/B/C/D/收尾）与 Task 1~5 一一对应。✓
- **占位符**：无 TBD/TODO；所有代码步骤均含完整实现代码。✓
- **类型一致性**：`targetContrast`、`lockedIndex`/`lockedBaseline`/`unlockTimer` 在 Task 1 定义并只在 Task 1 使用；`defaultRatio` 在 Task 3 定义并使用；`mousePos` 在 Task 4 Step 1 声明、Step 4 使用。无跨任务签名漂移。✓
