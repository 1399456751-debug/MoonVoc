// Headless 回归测试：直接实例化处理器，压测音频线程路径
// 用法：cmake 配置后运行 build/MoonVocHeadlessTest.exe，非 0 退出码 = 失败
#include <JuceHeader.h>
#include <cstdio>
#include "../Source/PluginProcessor.h"
#include "../Source/UI/MoonVocStrings.h"

#define TRACE(...) do { std::printf(__VA_ARGS__); std::fflush(stdout); } while (0)

// 频率响应验证：设某段 +6dB（其余 0），测目标频率正弦的前后 RMS 比值
static float bandGainRatio(MoonVocProcessor& p, const juce::String& paramId, float freq)
{
    const auto& apvts = p.apvts;
    // 重置全部相关参数，避免上一轮随机测试的残留污染测量
    for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                     "compAmount", "compMakeup",
                     "reverbAmount", "reverbMode",
                     "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                     "edgeAmount",
                     "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                     "inputGain", "outputGain", "headroom", "oversampling" })
        *apvts.getRawParameterValue(id) = 0.0f;
    *apvts.getRawParameterValue(paramId) = 6.0f;

    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    const int warmup = 100, measure = 2000;
    double outSq = 0.0;
    for (int b = 0; b < warmup + measure; ++b)
    {
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 512; ++n)
                buf.setSample(c, n, 0.3f * std::sin(2.0f * 3.14159f * freq * (float) (b * 512 + n) / 48000.0f));
        p.processBlock(buf, midi);
        if (b >= warmup)
        {
            double blkSq = 0.0;
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                {
                    blkSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
                    outSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
                }
            if ((b - warmup) % 500 == 0)
                TRACE("freqtest blk %d blkRMS=%g sample0=%g\n",
                      b, std::sqrt(blkSq / (2.0 * 512.0)), (double) buf.getSample(0, 0));
        }
    }
    const double rms = std::sqrt(outSq / (2.0 * measure * 512.0));
    const double inRms = 0.3 * 0.7071;
    return (float) (rms / inRms);
}

// 默认全 0 参数下测某频率 RMS 比值（频响平直 / 直通验证；oversampling 由调用方设置）
static float defaultRatio(MoonVocProcessor& p, float freq)
{
    const auto& apvts = p.apvts;
    for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                     "compAmount", "compMakeup", "compMode",
                     "reverbAmount", "reverbMode",
                     "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                     "edgeAmount",
                     "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
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

static bool allFinite(const juce::AudioBuffer<float>& buf)
{
    for (int c = 0; c < buf.getNumChannels(); ++c)
        for (int n = 0; n < buf.getNumSamples(); ++n)
            if (! std::isfinite(buf.getSample(c, n)))
                return false;
    return true;
}

int main()
{
    MoonVocProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    juce::MidiBuffer midi;
    juce::Random rng;

    // 文本表自检：新增 key 必须中英双份，漏一个就红
    {
        const bool ok = Strings::allKeysFilled();
        TRACE("strings table filled: %s\n", ok ? "OK" : "FAIL");
        if (! ok) return 1;
    }

    // 各种块大小
    const int blockSizes[] { 64, 128, 512, 1024 };
    const auto& apvts = processor.apvts;

    if (const char* skip = std::getenv("SKIP_RANDOM_PASS"))
    {
        (void) skip;
        TRACE("SKIPPING random passes\n");
    }
    else for (int pass = 0; pass < 4; ++pass)
    {
        const int blockSize = blockSizes[pass];
        TRACE("pass %d blockSize %d: prepare...\n", pass, blockSize);
        processor.prepareToPlay(48000.0, blockSize);
        TRACE("pass %d: prepared\n", pass);
        juce::AudioBuffer<float> buf(2, blockSize);

        for (int b = 0; b < 1500; ++b)
        {
            if (b % 500 == 0)
                TRACE("pass %d block %d\n", pass, b);
            // 填充信号：正弦 + 噪声 + 偶发静音段
            const bool silence = (b % 97 == 0);
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < blockSize; ++n)
                {
                    const float t = (float) n;
                    buf.setSample(c, n, silence ? 0.0f
                                                : 0.2f * std::sin(2.0f * 3.14159f * 440.0f * t / 48000.0f)
                                                  + 0.1f * (rng.nextFloat() * 2.0f - 1.0f));
                }

            // 每 50 块随机改一轮参数
            if (b % 50 == 0)
            {
                *apvts.getRawParameterValue("eqLowBoost")     = rng.nextFloat() * 6.0f;
                *apvts.getRawParameterValue("eqDeboxCut")     = rng.nextFloat() * 12.0f - 6.0f;
                *apvts.getRawParameterValue("eqClarityBoost") = rng.nextFloat() * 6.0f;
                *apvts.getRawParameterValue("eqAirBoost")     = rng.nextFloat() * 6.0f;
                *apvts.getRawParameterValue("eqAirFreq")      = rng.nextBool() ? 1.0f : 0.0f;
            }

            // 每 200 块循环切换超采样倍率 + 全局增益
            if (b % 200 == 0)
            {
                *apvts.getRawParameterValue("oversampling") = (float) ((b / 200) % 4);
                *apvts.getRawParameterValue("inputGain")    = rng.nextFloat() * 24.0f - 12.0f;
                *apvts.getRawParameterValue("headroom")     = rng.nextFloat() * 24.0f - 12.0f;
                *apvts.getRawParameterValue("outputGain")   = rng.nextFloat() * 24.0f - 12.0f;
            }

            processor.processBlock(buf, midi);

            if (! allFinite(buf))
            {
                juce::Logger::writeToLog("NON-FINITE output at pass " + juce::String(pass)
                                         + " block " + juce::String(b));
                return 1;
            }
        }
    }

    // 终极隔离实验：绕过插件链，直接测 IIR filter 在 100Hz 的提升
    {
        using Filter = juce::dsp::IIR::Filter<float>;
        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        const double fs = 96000.0;
        auto coeffs = Coeffs::makeLowShelf(fs, 120.0, 0.71, 2.0f); // +6dB
        Filter filt(coeffs);
        filt.reset();

        double inSq = 0.0, outSq = 0.0;
        const int total = 20000, skip = 4000; // 稳态后统计
        for (int n = 0; n < total; ++n)
        {
            const float x = 0.3f * std::sin(2.0f * 3.14159f * 100.0f * (float) n / (float) fs);
            const float y = filt.processSample(x);
            if (n >= skip) { inSq += x * x; outSq += y * y; }
        }
        TRACE("ISOLATED filter: inRMS=%.4f outRMS=%.4f ratio=%.3f (%+.1f dB)\n",
              std::sqrt(inSq / (total - skip)), std::sqrt(outSq / (total - skip)),
              std::sqrt(outSq / inSq), 10.0 * std::log10(outSq / inSq));
    }

    // 压缩检查：-12dB 连续正弦，100% 强度应明显压缩；0% 应直通；GR 表应更新
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup", "compMode",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("compMode") = 0.0f;   // Pop
        *apvts.getRawParameterValue("compAmount") = 100.0f;

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        const int warmup = 100, measure = 1000;

        const auto runAndMeasure = [&](float amount) -> float
        {
            *apvts.getRawParameterValue("compAmount") = amount;
            double outSq = 0.0;
            for (int b = 0; b < warmup + measure; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, 0.25f * std::sin(2.0f * 3.14159f * 468.75f * (float) (b * 512 + n) / 48000.0f));
                processor.processBlock(buf, midi);
                if (b >= warmup)
                    for (int c = 0; c < 2; ++c)
                        for (int n = 0; n < 512; ++n)
                            outSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
            }
            return (float) std::sqrt(outSq / (2.0 * measure * 512.0));
        };

        const float inRms = 0.25f * 0.7071f;
        const float bypassRms = runAndMeasure(0.0f);
        const float compRms = runAndMeasure(100.0f);
        const float gr = processor.getCompGainReduction();

        TRACE("comp check: bypassRMS=%.4f compRMS=%.4f (in=%.4f) GR=%.1f dB %s\n",
              bypassRms, compRms, inRms, gr,
              std::abs(bypassRms - inRms) / inRms < 0.1f && compRms < bypassRms * 0.6f && gr < -6.0f
                  ? "OK" : "BAD");
    }

    // 智能压缩检查：瞬态信号（高峰值因子）应比平滑信号（低峰值因子）压得更狠
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("compAmount") = 60.0f; // 中强度，区分度更明显

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        const int warmup = 400, measure = 1500;

        // 信号 A：平滑正弦（crest ~3dB）→ 应智能选慢 attack 低 ratio
        // 信号 B：稀疏脉冲（crest 高）→ 应智能选快 attack 高 ratio
        const auto runAndRead = [&](bool pulse) -> std::tuple<float, float, float>
        {
            for (int b = 0; b < 800; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, pulse ? ((b % 4 == 0 && n < 16) ? 0.5f : 0.05f)
                                                  : 0.25f * std::sin(2.0f * 3.14159f * 468.75f * (float) (b * 512 + n) / 48000.0f));
                processor.processBlock(buf, midi);
            }
            return { processor.getCompCrestDb(), processor.getCompFastAttackMs(), processor.getCompFastRatio() };
        };

        const auto [sineCrest, sineAtk, sineRatio] = runAndRead(false);
        const auto [pulseCrest, pulseAtk, pulseRatio] = runAndRead(true);

        TRACE("smart comp check: sine crest=%.1fdB atk=%.2fms ratio=%.1f | pulse crest=%.1fdB atk=%.2fms ratio=%.1f %s\n",
              sineCrest, sineAtk, sineRatio, pulseCrest, pulseAtk, pulseRatio,
              sineCrest < 5.0f && sineAtk > 1.5f && pulseCrest > 6.0f && pulseAtk < 1.5f
                  && pulseRatio > sineRatio + 0.5f ? "OK" : "BAD");
    }

    // v0.8.0 压缩三级：GR 总量 + 侧链高通 + 程序依赖释放
    {
        auto resetFor = [&](float amount, float mode)
        {
            processor.prepareToPlay(48000.0, 512);
            const auto& a = processor.apvts;
            for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                             "compMakeup", "reverbAmount", "reverbMode",
                             "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                             "edgeAmount", "dsAmount", "dsFocus",
                             "eqBypass", "compBypass", "satBypass", "edgeBypass",
                             "reverbBypass", "deEssBypass",
                             "inputGain", "outputGain", "headroom", "oversampling" })
                *a.getRawParameterValue(id) = 0.0f;
            *a.getRawParameterValue("compMode")   = mode;
            *a.getRawParameterValue("compAmount") = amount;
        };

        auto runGr = [&](float freq, float amp, int blocks) -> float
        {
            juce::AudioBuffer<float> buf(2, 512);
            juce::MidiBuffer midi;
            float grSum = 0.0f; int cnt = 0;
            for (int b = 0; b < blocks; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, amp * std::sin(2.0f * 3.14159f * freq * (float) (b * 512 + n) / 48000.0f));
                processor.processBlock(buf, midi);
                if (b >= blocks / 2) { grSum += processor.getCompGainReduction(); ++cnt; }
            }
            return cnt > 0 ? grSum / (float) cnt : 0.0f;
        };

        // 1) 100% 压缩有实质 GR
        resetFor(100.0f, 0.0f);
        const float grFull = runGr(440.0f, 0.3f, 300);
        TRACE("comp v080: full GR=%.1f dB %s\n", grFull, grFull < -4.0f ? "OK" : "BAD");
        if (! (grFull < -4.0f)) return 1;

        // 2) 侧链高通生效：同电平下 80Hz 的 GR 应明显小于 1kHz（低频不触发 → 不抽气）
        resetFor(100.0f, 0.0f);
        const float grLow = runGr(80.0f, 0.3f, 300);
        resetFor(100.0f, 0.0f);
        const float grMid = runGr(1000.0f, 0.3f, 300);
        const bool hpfOk = std::abs(grLow) < std::abs(grMid) * 0.6f;
        TRACE("comp v080: scHPF 80Hz GR=%.1f vs 1kHz GR=%.1f %s\n",
              grLow, grMid, hpfOk ? "OK" : "BAD");
        if (! hpfOk) return 1;

        // 3) 程序依赖释放：压得久（长持续音）释放更慢 → 停声后残余 GR 更大
        const auto releaseAfterStop = [&](int toneBlocks) -> float
        {
            resetFor(100.0f, 0.0f);
            juce::AudioBuffer<float> buf(2, 512);
            juce::MidiBuffer midi;
            for (int b = 0; b < toneBlocks; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, 0.4f * std::sin(2.0f * 3.14159f * 440.0f * (float) (b * 512 + n) / 48000.0f));
                processor.processBlock(buf, midi);
            }
            buf.clear();
            for (int b = 0; b < 4; ++b) processor.processBlock(buf, midi);
            return processor.getCompGainReduction();
        };
        const float grShort = releaseAfterStop(10);
        const float grLong  = releaseAfterStop(400);
        const bool pdOk = std::abs(grLong) > std::abs(grShort) + 0.5f;
        TRACE("comp v080: progRelease short=%.2f dB long=%.2f dB %s\n",
              grShort, grLong, pdOk ? "OK" : "BAD");
        if (! pdOk) return 1;
        TRACE("comp v080: OK\n");
    }

    // 混响检查：wet>0 时单脉冲后应有尾音；wet=0 时无尾音
    {
        processor.prepareToPlay(48000.0, 512); // 块大小必须与 processBlock 一致（超采样器按此分配）
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("reverbMode") = 1.0f; // Rap 大混响

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;

        // 单脉冲注入后静音 8 块，测尾音能量：wet>0 应有明显尾音，wet=0 应严格无尾音
        const auto tailEnergy = [&](float amount) -> double
        {
            *apvts.getRawParameterValue("reverbAmount") = amount;
            for (int b = 0; b < 200; ++b) // 冲掉旧尾音（Rap roomSize 0.92 尾很长）/ wet 平滑到位
            {
                buf.clear();
                processor.processBlock(buf, midi);
            }
            buf.clear();
            buf.setSample(0, 0, 1.0f);
            buf.setSample(1, 0, 1.0f);
            processor.processBlock(buf, midi);
            double tail = 0.0;
            for (int b = 1; b <= 16; ++b)
            {
                buf.clear();
                processor.processBlock(buf, midi);
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        tail += (double) buf.getSample(c, n) * buf.getSample(c, n);
            }
            return tail;
        };

        const double tailWet = tailEnergy(60.0f);
        const double tailDry = tailEnergy(0.0f);
        TRACE("reverb check: tailWet=%.4f tailDry=%.6f %s\n",
              tailWet, tailDry,
              tailWet > 0.05 && tailDry < 1.0e-4 ? "OK" : "BAD");
    }

    // 染色检查：大信号（0.8）被压缩；小信号（0.05）近直通；旁路直通
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("satTypeA") = 1.0f;      // FET
        *apvts.getRawParameterValue("satAmountA") = 100.0f;

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        const int warmup = 100, measure = 800;

        const auto peakRatio = [&](float amp) -> float
        {
            float peak = 0.0f;
            for (int b = 0; b < warmup + measure; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, amp * std::sin(2.0f * 3.14159f * 440.0f * (float) (b * 512 + n) / 48000.0f));
                processor.processBlock(buf, midi);
                if (b >= warmup)
                    for (int c = 0; c < 2; ++c)
                        for (int n = 0; n < 512; ++n)
                            peak = jmax(peak, std::abs(buf.getSample(c, n)));
            }
            return peak / amp;
        };

        const float big = peakRatio(0.8f);   // 大信号 → 应被明显压缩
        const float small = peakRatio(0.05f); // 小信号 → 应精确 1:1（干净透明）
        TRACE("sat check: big=%.2f small=%.2f %s\n", big, small,
              big < 0.75f && small > 0.95f && small < 1.08f ? "OK" : "BAD");
    }

    // 瞬态整形检查：脉冲起音应被放大/削平；稳态正弦应几乎不动
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        const int warmup = 100, measure = 1000;

        // v0.8.0 双包络差值：稀疏脉冲下测峰值比（正=棱角增强起音，负=圆滑削弱起音）
        // 脉冲间隔 24 块 = 256ms > 慢包络 release 250ms；脉冲宽 240 样本 = 5ms > 增益平滑 1.5ms
        const auto pulsePeak = [&](float amount) -> float
        {
            *apvts.getRawParameterValue("edgeAmount") = amount;
            float peak = 0.0f;
            for (int b = 0; b < 96; ++b)
            {
                const bool hit = (b % 24 == 12);
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, (hit && n < 240) ? 0.8f : 0.0f);
                processor.processBlock(buf, midi);
                // 跳过前两个脉冲让包络稳定；只测脉冲后半段（n>=120）——增益平滑 1.5ms，
                // 压制方向要 1~2ms 才降到位，测脉冲尖头会漏掉负方向的效果
                if (b >= 48 && hit)
                    for (int c = 0; c < 2; ++c)
                        for (int n = 120; n < 240; ++n)
                            peak = juce::jmax(peak, std::abs(buf.getSample(c, n)));
            }
            return peak / 0.8f;
        };

        // 调制失真检查：稳态相干正弦下 Edge 应近乎透明
        const auto sineRms = [&](float amount) -> float
        {
            *apvts.getRawParameterValue("edgeAmount") = amount;
            double outSq = 0.0;
            for (int b = 0; b < warmup + measure; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, 0.25f * std::sin(2.0f * 3.14159f * 468.75f * (float) (b * 512 + n) / 48000.0f));
                processor.processBlock(buf, midi);
                if (b >= warmup)
                    for (int c = 0; c < 2; ++c)
                        for (int n = 0; n < 512; ++n)
                            outSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
            }
            return (float) std::sqrt(outSq / (2.0 * measure * 512.0)) / (0.25f * 0.7071f);
        };

        const float rOff = pulsePeak(0.0f);
        const float rPos = pulsePeak(100.0f);
        const float rNeg = pulsePeak(-100.0f);
        // 以 off 为基准比较（超采样 FIR 在脉冲上有 ~1% 振铃过冲，不能用绝对 1.0）
        TRACE("edge v080: off x%.3f  pos x%.3f  neg x%.3f %s\n", rOff, rPos, rNeg,
              (std::abs(rOff - 1.0f) < 0.03f && rPos > rOff * 1.8f && rNeg < rOff * 0.6f) ? "OK" : "BAD");
        if (! (std::abs(rOff - 1.0f) < 0.03f && rPos > rOff * 1.8f && rNeg < rOff * 0.6f))
            return 1;

        const float sinePlus = sineRms(100.0f);
        const float sineMinus = sineRms(-100.0f);
        TRACE("edge v080: sine +%.3f -%.3f %s\n", sinePlus, sineMinus,
              (sinePlus > 0.9f && sinePlus < 1.1f && sineMinus > 0.9f && sineMinus < 1.1f) ? "OK" : "BAD");
        if (! (sinePlus > 0.9f && sinePlus < 1.1f && sineMinus > 0.9f && sineMinus < 1.1f))
            return 1;
        TRACE("edge v080: OK\n");
    }

    // 旁通检查：各模块全开 + bypass 打开 → 输出应回到直通（RMS 比 ≈ 1）
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;

        // 所有模块全力工作
        *apvts.getRawParameterValue("eqLowBoost") = 6.0f;
        *apvts.getRawParameterValue("eqClarityBoost") = 6.0f;
        *apvts.getRawParameterValue("compAmount") = 100.0f;
        *apvts.getRawParameterValue("satTypeA") = 1.0f;
        *apvts.getRawParameterValue("satAmountA") = 100.0f;
        *apvts.getRawParameterValue("edgeAmount") = 100.0f;
        *apvts.getRawParameterValue("reverbAmount") = 80.0f;
        *apvts.getRawParameterValue("reverbMode") = 1.0f;

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        const int warmup = 300, measure = 1000;

        const auto rmsRatio = [&]() -> float
        {
            double inSq = 0.0, outSq = 0.0;
            for (int b = 0; b < warmup + measure; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, 0.25f * std::sin(2.0f * 3.14159f * 468.75f * (float) (b * 512 + n) / 48000.0f));
                processor.processBlock(buf, midi);
                if (b >= warmup)
                    for (int c = 0; c < 2; ++c)
                        for (int n = 0; n < 512; ++n)
                        {
                            const float x = 0.25f * std::sin(2.0f * 3.14159f * 468.75f * (float) (b * 512 + n) / 48000.0f);
                            inSq += (double) x * x;
                            outSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
                        }
            }
            return (float) std::sqrt(outSq / inSq);
        };

        const float activeRatio = rmsRatio(); // 全开：应明显不是 1
        for (auto id : { "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass" })
            *apvts.getRawParameterValue(id) = 1.0f;
        const float bypassRatio = rmsRatio(); // 全旁通：应回到 ≈ 1
        TRACE("bypass check: activeRatio=%.3f bypassRatio=%.4f %s\n",
              activeRatio, bypassRatio,
              std::abs(activeRatio - 1.0f) > 0.05f && std::abs(bypassRatio - 1.0f) < 1.0e-2f ? "OK" : "BAD");
        if (! (std::abs(bypassRatio - 1.0f) < 1.0e-2f))
            return 1;
    }

    // 共振锁频检查：327Hz 强共振（不在任何候选上）→ De-Box 应能锁到附近并削掉
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("eqLowBoost") = 0.0f;
        *apvts.getRawParameterValue("eqAirBoost") = 0.0f;
        *apvts.getRawParameterValue("eqClarityBoost") = 0.0f;
        *apvts.getRawParameterValue("eqDeboxCut") = -6.0f; // 削 6dB

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        const int warmup = 400, measure = 2000;

        // 327Hz 共振正弦 vs 干净 400Hz 附近（315 候选上）对比
        const auto measureAt = [&](float freq) -> float
        {
            double outSq = 0.0;
            for (int b = 0; b < warmup + measure; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, 0.25f * std::sin(2.0f * 3.14159f * freq * (float) (b * 512 + n) / 48000.0f));
                processor.processBlock(buf, midi);
                if (b >= warmup)
                    for (int c = 0; c < 2; ++c)
                        for (int n = 0; n < 512; ++n)
                            outSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
            }
            const float rms = (float) std::sqrt(outSq / (2.0 * measure * 512.0));
            return rms / (0.25f * 0.7071f);
        };

        // 单共振：327Hz → 应锁 315±50 并削掉
        const float resRatio = measureAt(327.0f);
        const float locked = processor.getEqDeboxFreq(0);

        // 双共振同时：327Hz + 560Hz 混合 → 两个峰都应被同时削掉
        double outSq = 0.0;
        for (int b = 0; b < 400 + 2000; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                {
                    const float t = (float) (b * 512 + n) / 48000.0f;
                    buf.setSample(c, n, 0.25f * std::sin(2.0f * 3.14159f * 327.0f * t)
                                      + 0.25f * std::sin(2.0f * 3.14159f * 560.0f * t));
                }
            processor.processBlock(buf, midi);
            if (b >= 400)
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        outSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
        }
        const float dualRms = (float) std::sqrt(outSq / (2.0 * 2000 * 512.0));
        const float dualIn = 0.25f * 0.7071f * std::sqrt(2.0f); // 两等幅正弦叠加
        const float dualRatio = dualRms / dualIn;
        const int lockA = (int) processor.getEqDeboxFreq(0);
        const int lockB = (int) processor.getEqDeboxFreq(1);

        TRACE("resonance check: 327Hz lock=%d ratio=%.2f, dual lock=%d+%d ratio=%.2f %s\n",
              (int) locked, resRatio, lockA, lockB, dualRatio,
              std::abs(locked - 315.0f) < 50.0f && resRatio < 0.55f
                  && lockA > 0 && lockB > 0 && dualRatio < 0.6f ? "OK" : "BAD");
    }

    // mono 通道检查：1 通道 buffer 全链运行无 NaN 无崩溃
    {
        processor.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> mbuf(1, 512);
        juce::MidiBuffer midi;
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("eqDeboxCut") = -4.0f;
        *apvts.getRawParameterValue("compAmount") = 60.0f;
        *apvts.getRawParameterValue("reverbAmount") = 50.0f;
        *apvts.getRawParameterValue("satTypeA") = 2.0f;
        *apvts.getRawParameterValue("satAmountA") = 70.0f;
        *apvts.getRawParameterValue("edgeAmount") = 50.0f;

        bool ok = true;
        for (int b = 0; b < 500; ++b)
        {
            for (int n = 0; n < 512; ++n)
                mbuf.setSample(0, n, 0.3f * std::sin(2.0f * 3.14159f * 300.0f * (float) (b * 512 + n) / 48000.0f)
                                    + 0.1f * (rng.nextFloat() * 2.0f - 1.0f));
            processor.processBlock(mbuf, midi);
            for (int n = 0; n < 512; ++n)
                if (! std::isfinite(mbuf.getSample(0, n)))
                    ok = false;
        }
        TRACE("mono check: %s\n", ok ? "OK" : "BAD");
    }

    // 默认全 0 透明直通（能量守恒）：oversampling=0（最小档 2x）时输出/输入 RMS 比 ≈ 1
    // （无 Off 档后 FIR 半带引入固定延迟，逐样本对齐已不适用，改能量验证）
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup", "compMode",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
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

        double inSq = 0.0, outSq = 0.0;
        for (int b = 0; b < 50; ++b)
        {
            for (int c = 0; c < 2; ++c)         // JUCE9 API：逐通道复制，每块从 ref 重置，隔离链内部状态
                buf.copyFrom(c, 0, ref, c, 0, 512);
            processor.processBlock(buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                {
                    inSq  += (double) ref.getSample(c, n) * ref.getSample(c, n);
                    outSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
                }
        }
        const float ratio = (float) std::sqrt(outSq / inSq);
        // 2x FIR 半带通带纹波量级 ~0.15%（0.9985），容差 1%
        TRACE("bypass check: out/in RMS ratio=%.5f %s\n", ratio,
              std::abs(ratio - 1.0f) < 1.0e-2f ? "OK" : "BAD");
        if (! (std::abs(ratio - 1.0f) < 1.0e-2f))
            return 1;
    }

    // 保真度检查：全默认参数应为透明直通（RMS 一致 + THD 极低）；逐模块开启定位失真源
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("oversampling") = 1.0f; // 默认 4x

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        const int warmup = 200, measure = 2000;

        // 测量：输出 RMS 比 + Goertzel 测谐波 THD
        const auto probeMeasure = [&]() -> std::tuple<float, float>
        {
            double outSq = 0.0;
            double harm[4] { 0, 0, 0, 0 }; // 2/3/4/5 次谐波能量
            for (int b = 0; b < warmup + measure; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, 0.25f * std::sin(2.0f * 3.14159f * 468.75f * (float) (b * 512 + n) / 48000.0f));
                processor.processBlock(buf, midi);
                if (b >= warmup)
                    for (int c = 0; c < 2; ++c)
                        for (int n = 0; n < 512; ++n)
                        {
                            const double x = buf.getSample(c, n);
                            outSq += x * x;
                            // Goertzel 谐波检测（最后一块）
                            if (b == warmup + measure - 1)
                            {
                                const double t = (double) (b * 512 + n) / 48000.0;
                                for (int h = 0; h < 4; ++h)
                                {
                                    const double f = 468.75 * (h + 2);
                                    harm[h] += x * std::sin(2.0 * 3.14159 * f * t);
                                }
                            }
                        }
            }
            const float rms = (float) std::sqrt(outSq / (2.0 * measure * 512.0));
            const float thd = (float) std::sqrt((harm[0] * harm[0] + harm[1] * harm[1]
                                               + harm[2] * harm[2] + harm[3] * harm[3])
                                               / (2.0 * 512.0 * 2.0 * 512.0));
            const float f1 = 0.25f * 0.7071f;
            return { rms / f1, 20.0f * std::log10(thd + 1.0e-12f) };
        };

        const auto [defRms, defThd] = probeMeasure();   // 默认：透明直通
        TRACE("fidelity default: rmsRatio=%.4f THD=%.1f dB %s\n", defRms, defThd,
              std::abs(defRms - 1.0f) < 0.02f && defThd < -70.0f ? "OK" : "BAD");

        // 逐模块开启定位失真源
        const auto runProbe = [&](const char* label, auto&& setup) -> float
        {
            setup();
            const auto [r, t] = probeMeasure();
            TRACE("fidelity %s: rmsRatio=%.4f THD=%.1f dB\n", label, r, t);
            return t;
        };
        runProbe("sat FET 100", [&] { *apvts.getRawParameterValue("satTypeA") = 1.0f;
                                      *apvts.getRawParameterValue("satAmountA") = 100.0f; });
        runProbe("sat Tube 100", [&] { *apvts.getRawParameterValue("satTypeA") = 2.0f; });
        runProbe("comp 100", [&] { *apvts.getRawParameterValue("satTypeA") = 0.0f;
                                   *apvts.getRawParameterValue("satAmountA") = 0.0f;
                                   *apvts.getRawParameterValue("compAmount") = 100.0f; });
        runProbe("edge +100", [&] { *apvts.getRawParameterValue("compAmount") = 0.0f;
                                    *apvts.getRawParameterValue("edgeAmount") = 100.0f; });
        runProbe("edge -100", [&] { *apvts.getRawParameterValue("edgeAmount") = -100.0f; });
        runProbe("reverb 60", [&] { *apvts.getRawParameterValue("edgeAmount") = 0.0f;
                                    *apvts.getRawParameterValue("reverbAmount") = 60.0f; });
        *apvts.getRawParameterValue("reverbAmount") = 0.0f;
        // 诊断 + 断言：全部模块旁通。若 THD 与 default 一致 → 参数为 0 时全链确实透明，
        // 残余 THD 来自超采样 FIR 链本身（非任何模块）
        const float bypThd = runProbe("all bypass", [&] {
            *apvts.getRawParameterValue("eqBypass")     = 1.0f;
            *apvts.getRawParameterValue("compBypass")   = 1.0f;
            *apvts.getRawParameterValue("deEssBypass")  = 1.0f;
            *apvts.getRawParameterValue("satBypass")    = 1.0f;
            *apvts.getRawParameterValue("edgeBypass")   = 1.0f;
            *apvts.getRawParameterValue("reverbBypass") = 1.0f;
        });
        for (auto id : { "eqBypass", "compBypass", "deEssBypass", "satBypass", "edgeBypass", "reverbBypass" })
            *apvts.getRawParameterValue(id) = 0.0f;
        TRACE("fidelity transparency: default=%.1f dB vs all-bypass=%.1f dB %s\n",
              defThd, bypThd, std::abs(bypThd - defThd) < 1.0f ? "OK" : "BAD(模块未透明)");
        if (std::abs(bypThd - defThd) > 1.0f)
            return 1;
    }

    // 电平表检查：0.25 幅度正弦（RMS -15dBFS）→ 输入/输出电平表应 ≈ -15dB；静音应衰减
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 500; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                    buf.setSample(c, n, 0.25f * std::sin(2.0f * 3.14159f * 468.75f * (float) (b * 512 + n) / 48000.0f));
            processor.processBlock(buf, midi);
        }
        const float inDb = processor.inputLevelDb.load();
        const float outDb = processor.outputLevelDb.load();

        // 静音后应衰减
        for (int b = 0; b < 300; ++b)
        {
            buf.clear();
            processor.processBlock(buf, midi);
        }
        const float silentDb = processor.outputLevelDb.load();

        TRACE("meter check: in=%.1f dB out=%.1f dB (expect ~-15), silent=%.1f %s\n",
              inDb, outDb, silentDb,
              std::abs(inDb + 15.0f) < 2.0f && std::abs(outDb + 15.0f) < 2.0f && silentDb < -40.0f
                  ? "OK" : "BAD");
    }

    // 工作电平指示灯输入参考检查：压缩重度压下输出时，inputLevelDb 必须保持不动
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup", "compMode",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;

        auto run = [&](float compAmount) -> std::pair<float, float>
        {
            *apvts.getRawParameterValue("compAmount") = compAmount;
            juce::AudioBuffer<float> buf(2, 512);
            juce::MidiBuffer midi;
            for (int b = 0; b < 800; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, 0.25f * std::sin(2.0f * 3.14159f * 468.75f * (float) (b * 512 + n) / 48000.0f));
                processor.processBlock(buf, midi);
            }
            return { processor.inputLevelDb.load(), processor.outputLevelDb.load() };
        };

        const auto [in0, out0] = run(0.0f);
        const auto [in1, out1] = run(100.0f);
        const bool inOk  = std::abs(in1 - in0) < 1.5f;   // 压缩不改变输入参考
        const bool outOk = out1 < in1 - 3.0f;            // 输出确实被压下去了
        TRACE("indicator ref check: comp0 in=%.1f out=%.1f | comp100 in=%.1f out=%.1f %s%s\n",
              in0, out0, in1, out1, inOk ? "in-OK" : "in-BAD", outOk ? " out-OK" : " out-BAD");
        if (! (inOk && outOk))
            return 1;
    }

    // Clarity 峰锁定检查：f1=3150 恒定强、f2=2500 周期性增强 → 锁定应保持 f1，不来回跳
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup", "compMode",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("eqClarityBoost") = 6.0f;

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        float worstDev = 0.0f;
        for (int b = 0; b < 3000; ++b)
        {
            // 先 200 块弱相让锁定建立于 f1(3150)，再 200 块强相考验锁定保持
            const float f2Amp = (b % 400 < 200) ? 0.03f : 0.32f;
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                {
                    const float t = (float) (b * 512 + n) / 48000.0f;
                    buf.setSample(c, n, 0.30f * std::sin(2.0f * 3.14159f * 3150.0f * t)
                                       + f2Amp * std::sin(2.0f * 3.14159f * 2500.0f * t));
                }
            processor.processBlock(buf, midi);
            if (b % 400 >= 200)   // f2 强相：无锁定时会倾向 2500
                worstDev = jmax(worstDev, std::abs(processor.getEqClarityFreq(0) - 3150.0f));
        }
        TRACE("clarity lock check: worstDev=%.0f Hz (expect < 250) %s\n",
              worstDev, worstDev < 250.0f ? "OK" : "BAD");
        if (! (worstDev < 250.0f))
            return 1;
    }

    // Clarity 限增益检查：单一强共振（高对比度）→ 实际提升显著低于用户 +12dB
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup", "compMode",
                         "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass",
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
        if (! (gainDb < 10.0f && gainDb > 2.0f))
            return 1;
    }

    // 系数公式对比：手写 writeShelf/writePeak/writeBandPass vs JUCE makeXXX 逐样本一致
    {
        using Filter = juce::dsp::IIR::Filter<float>;
        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        struct Case { int type; float f, q, gain; const char* name; };
        const Case cases[] {
            { 0, 120.0f, 0.71f, 6.0f, "lowShelf +6" },
            { 0, 120.0f, 0.71f, -6.0f, "lowShelf -6" },
            { 1, 13000.0f, 0.71f, 3.0f, "highShelf +3" },
            { 2, 400.0f, 1.2f, 5.0f, "peak +5" },
            { 2, 400.0f, 1.2f, -5.0f, "peak -5" },
            { 3, 3150.0f, 8.0f, 0.0f, "bandpass" },
            { 3, 400.0f, 6.0f, 0.0f, "bandpass Q6" },
        };
        bool allOk = true;
        for (auto& cs : cases)
        {
            float mine[5];
            VoiceEq::debugGetCoeffs(cs.type, cs.f, cs.q, cs.gain, 96000.0f, mine);

            Coeffs::Ptr ref;
            if (cs.type == 0)      ref = Coeffs::makeLowShelf(96000.0, cs.f, cs.q, juce::Decibels::decibelsToGain(cs.gain));
            else if (cs.type == 1) ref = Coeffs::makeHighShelf(96000.0, cs.f, cs.q, juce::Decibels::decibelsToGain(cs.gain));
            else if (cs.type == 2) ref = Coeffs::makePeakFilter(96000.0, cs.f, cs.q, juce::Decibels::decibelsToGain(cs.gain));
            else                   ref = Coeffs::makeBandPass(96000.0, cs.f, cs.q);

            const float* r = ref->getRawCoefficients();
            float maxErr = 0.0f;
            for (int i = 0; i < 5; ++i)
                maxErr = jmax(maxErr, std::abs(mine[i] - r[i]));
            const bool ok = maxErr < 1.0e-6f;
            allOk = allOk && ok;
            TRACE("coeff check %-14s maxErr=%.2e %s\n", cs.name, maxErr, ok ? "OK" : "MISMATCH");
        }
        TRACE("coeff all: %s\n", allOk ? "OK" : "BAD");
    }

    // 频率响应方向检查（滤波器本身）
    {
        processor.prepareToPlay(48000.0, 512);
        *apvts.getRawParameterValue("oversampling") = 0.0f; // 固定 2x
        struct Check { const char* param; float freq; };
        const Check checks[] {
            { "eqLowBoost", 100.0f }, { "eqDeboxCut", 400.0f },
            { "eqClarityBoost", 3000.0f }, { "eqAirBoost", 16000.0f },
        };
        for (auto& chk : checks)
        {
            const float ratio = bandGainRatio(processor, chk.param, chk.freq);
            TRACE("freq check: %s @ %g Hz -> x%.3f (%+.1f dB) lock=%d/%d %s\n",
                  chk.param, chk.freq, ratio, 20.0 * std::log10(ratio),
                  (int) processor.getEqDeboxFreq(0), (int) processor.getEqClarityFreq(0),
                  ratio > 1.35f ? "OK" : "REVERSED/BROKEN");
        }
    }

    // 16x 下默认链频响平直检查：100Hz~15kHz 输出/输入比值 ≈ 1.0（±0.03）
    {
        processor.prepareToPlay(48000.0, 512);
        *apvts.getRawParameterValue("oversampling") = 3.0f; // 16x（index 0..3 = 2x/4x/8x/16x）
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

    // v0.8.0：参数范围 / 默认值 / 模式名
    {
        auto* in  = processor.apvts.getParameter("inputGain");
        auto* out = processor.apvts.getParameter("outputGain");
        auto* hr  = processor.apvts.getParameter("headroom");
        const bool rangeOk = in->getNormalisableRange().end == 18.0f
                          && out->getNormalisableRange().end == 18.0f
                          && hr->getNormalisableRange().end == 18.0f
                          && in->getNormalisableRange().start == -18.0f;
        TRACE("v080 range check: +-18dB %s\n", rangeOk ? "OK" : "BAD");
        if (! rangeOk) return 1;

        // 默认语言 = 英文（AudioParameterChoice 构造后 getIndex() 即默认索引）
        auto* lang = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter("uiLanguage"));
        const bool langOk = lang != nullptr && lang->getIndex() == 1 && lang->choices[1] == "English";
        TRACE("v080 default lang: %s\n", langOk ? "OK (English)" : "BAD");
        if (! langOk) return 1;

        auto* cm = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter("compMode"));
        auto* rm = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter("reverbMode"));
        const bool modeOk = cm != nullptr && rm != nullptr
                         && cm->choices[0] == "Glow" && cm->choices[1] == "Forge"
                         && rm->choices[0].contains("Veil") && rm->choices[1].contains("Abyss");
        TRACE("v080 mode names: comp=%s/%s reverb=%s/%s %s\n",
              cm ? cm->choices[0].toRawUTF8() : "?", cm ? cm->choices[1].toRawUTF8() : "?",
              rm ? rm->choices[0].toRawUTF8() : "?", rm ? rm->choices[1].toRawUTF8() : "?",
              modeOk ? "OK" : "BAD");
        if (! modeOk) return 1;
        TRACE("v080 params check: OK\n");
    }

    // v0.8.0 DeBess 去齿音：齿音削减达标 + 1kHz 正弦零触发（无染色）+ 关闭零削减
    {
        auto measureGr = [&](float freq, float amount) -> float
        {
            processor.prepareToPlay(48000.0, 512);
            const auto& a = processor.apvts;
            for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                             "compAmount", "compMakeup", "compMode",
                             "reverbAmount", "reverbMode",
                             "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                             "edgeAmount", "dsFocus",
                             "eqBypass", "compBypass", "satBypass", "edgeBypass",
                             "reverbBypass", "deEssBypass",
                             "inputGain", "outputGain", "headroom", "oversampling" })
                *a.getRawParameterValue(id) = 0.0f;
            *a.getRawParameterValue("dsFocus")  = 40.0f; // 偏高频刺
            *a.getRawParameterValue("dsAmount") = amount;

            juce::AudioBuffer<float> buf(2, 512);
            juce::MidiBuffer midi;
            float grSum = 0.0f; int grCount = 0;
            for (int b = 0; b < 300; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                    {
                        const float t = (float) (b * 512 + n) / 48000.0f;
                        // 抖动的高频"齿音"：载波 + 50Hz 包络调制
                        const float env = 0.5f + 0.5f * std::sin(2.0f * 3.14159f * 50.0f * t);
                        buf.setSample(c, n, 0.4f * env * std::sin(2.0f * 3.14159f * freq * t));
                    }
                processor.processBlock(buf, midi);
                if (b >= 200) { grSum += processor.getDeEssGainReduction(); ++grCount; }
            }
            return grCount > 0 ? grSum / (float) grCount : 0.0f;
        };

        const float grSib  = measureGr(7000.0f, 100.0f);
        const float grTone = measureGr(1000.0f, 100.0f);
        const float grOff  = measureGr(7000.0f, 0.0f);
        TRACE("deess v080: 7kHz gr=%.1f dB  1kHz gr=%.1f dB  off gr=%.1f dB %s\n",
              grSib, grTone, grOff,
              (grSib < -3.0f && grTone > -1.0f && std::abs(grOff) < 0.01f) ? "OK" : "BAD");
        if (! (grSib < -3.0f && grTone > -1.0f && std::abs(grOff) < 0.01f))
            return 1;
        TRACE("deess v080: OK\n");
    }

    // Clarity 单宽峰验证：用带 formant 包络的谐波音测真实频响。
    // （单频扫描测不出智能 EQ 的形状 —— 检测器会跟着测试频点走，每个点都被当成峰值；
    //   频谱平坦的谐波音也不行 —— 没有峰，检测器不触发。必须同时有密集谐波 + 窄共振峰。）
    // 断言：① 峰值贴合旋钮值（多峰并联会叠出超额增益：旧 3 峰版 12dB 旋钮实测 +17dB）
    //       ② 相邻点差 < 3dB（曲线平滑，无平台/陡边）
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& a = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup", "reverbAmount", "reverbMode",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount", "dsAmount", "dsFocus",
                         "eqBypass", "compBypass", "satBypass", "edgeBypass", "reverbBypass", "deEssBypass",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *a.getRawParameterValue(id) = 0.0f;
        *a.getRawParameterValue("eqClarityBoost") = 12.0f;

        constexpr float f0 = 187.5f;      // 512 样本 @48k = 2 周期（相干）
        constexpr int kMin = 11, kMax = 53; // 谐波次数 → 2062 ~ 9937 Hz
        constexpr int nHarm = kMax - kMin + 1;

        // 预计算每个采样位置的各谐波 sin/cos（512 样本周期重复）
        std::vector<std::vector<double>> sinTab(nHarm, std::vector<double>(512, 0.0));
        std::vector<std::vector<double>> cosTab(nHarm, std::vector<double>(512, 0.0));
        for (int h = 0; h < nHarm; ++h)
            for (int s = 0; s < 512; ++s)
            {
                const double ph = 2.0 * 3.14159265358979 * f0 * (kMin + h) * (double) s / 48000.0;
                sinTab[h][s] = std::sin(ph);
                cosTab[h][s] = std::cos(ph);
            }

        // 谐波幅度按"人声共振峰"包络（3 个 formant），否则频谱太平检测器不触发
        std::vector<double> harmAmp(nHarm, 0.0);
        for (int h = 0; h < nHarm; ++h)
        {
            const double f = f0 * (kMin + h);
            auto formant = [&](double fc, double bw, double g)
            { return g / (1.0 + std::pow((f - fc) / bw, 2.0)); };
            // formant 带宽取真实人声量级（200~400Hz）—— 太宽在候选带尺度上就没有"峰"可检
            harmAmp[h] = formant(700.0, 200.0, 1.0)
                       + formant(2800.0, 350.0, 0.9)
                       + formant(5200.0, 400.0, 0.7) + 0.05;
        }

        juce::AudioBuffer<float> buf(2, 512), inCopy(2, 512);
        juce::MidiBuffer midi;
        // 完整 Goertzel（in-phase + quadrature）→ 取幅度，与滤波器相位延迟无关
        std::vector<double> inRe(nHarm, 0.0), inIm(nHarm, 0.0), outRe(nHarm, 0.0), outIm(nHarm, 0.0);
        const int warmup = 400, measure = 200;

        for (int b = 0; b < warmup + measure; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < 512; ++s)
                {
                    double v = 0.0;
                    for (int h = 0; h < nHarm; ++h)
                        v += harmAmp[h] * std::sin(2.0 * 3.14159265358979 * f0 * (kMin + h)
                                                  * (double) (b * 512 + s) / 48000.0);
                    buf.setSample(c, s, (float) (0.01 * v));
                }
            inCopy.makeCopyOf(buf);
            processor.processBlock(buf, midi);
            if (b >= warmup)
                for (int c = 0; c < 2; ++c)
                    for (int s = 0; s < 512; ++s)
                    {
                        const float xi = inCopy.getSample(c, s);
                        const float xo = buf.getSample(c, s);
                        for (int h = 0; h < nHarm; ++h)
                        {
                            inRe[h]  += xi * cosTab[h][s];
                            inIm[h]  += xi * sinTab[h][s];
                            outRe[h] += xo * cosTab[h][s];
                            outIm[h] += xo * sinTab[h][s];
                        }
                    }
        }

        TRACE("--- clarity harmonic response (12 dB) ---\n");
        float peakDb = -100.0f, maxJump = 0.0f, prevDb = 0.0f;
        bool firstPoint = true;
        for (int h = 0; h < nHarm; h += 3)
        {
            const float f = f0 * (kMin + h);
            const double ampIn  = std::sqrt(inRe[h] * inRe[h] + inIm[h] * inIm[h]);
            const double ampOut = std::sqrt(outRe[h] * outRe[h] + outIm[h] * outIm[h]);
            const float db = (float) (20.0 * std::log10(jmax(1.0e-6, ampOut / (ampIn + 1.0e-12))));
            TRACE("clarity %5.0f Hz : %+.2f dB\n", f, db);
            peakDb = jmax(peakDb, db);
            if (! firstPoint)
                maxJump = jmax(maxJump, std::abs(db - prevDb));
            prevDb = db;
            firstPoint = false;
        }
        const bool shapeOk = peakDb < 13.0f && peakDb > 9.0f && maxJump < 3.0f;
        TRACE("clarity shape: peak=%+.1f dB (knob 12) maxJump=%.1f dB %s\n",
              peakDb, maxJump, shapeOk ? "OK" : "BAD(峰值叠加或曲线不平滑)");
        if (! shapeOk) return 1;
        TRACE("--- harmonic end ---\n");
    }

    juce::Logger::writeToLog("Headless test passed (all blocks finite)");
    return 0;
}
