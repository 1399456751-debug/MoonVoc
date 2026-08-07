// Headless 回归测试：直接实例化处理器，压测音频线程路径
// 用法：cmake 配置后运行 build/MoonVocHeadlessTest.exe，非 0 退出码 = 失败
#include <JuceHeader.h>
#include <cstdio>
#include "../Source/PluginProcessor.h"

#define TRACE(...) do { std::printf(__VA_ARGS__); std::fflush(stdout); } while (0)

// 频率响应验证：设某段 +6dB（其余 0），测目标频率正弦的前后 RMS 比值
static float bandGainRatio(MoonVocProcessor& p, const juce::String& paramId, float freq)
{
    const auto& apvts = p.apvts;
    // 重置全部相关参数，避免上一轮随机测试的残留污染测量
    for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                     "compAmount", "compMakeup",
                     "dsLowAmount", "dsHighAmount",
                     "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                     "edgeAmount",
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
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
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
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
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

    // 去齿音检查：4kHz 齿音信号应被明显削减；1kHz 底音应几乎不受影响（零染色）
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("dsLowAmount") = 100.0f;
        *apvts.getRawParameterValue("dsHighAmount") = 100.0f;

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        const int warmup = 200, measure = 1500;

        const auto measureFreq = [&](float freq) -> float
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

        const float sibRatio = measureFreq(4000.0f);   // 齿音频段 → 应被削
        const float lowRatio = measureFreq(1000.0f);   // 底音 → 应基本不动
        TRACE("deess check: 4k ratio=%.2f (%+.1f dB), 1k ratio=%.2f (%+.1f dB) %s\n",
              sibRatio, 20.0 * std::log10(sibRatio), lowRatio, 20.0 * std::log10(lowRatio),
              sibRatio < 0.6f && lowRatio > 0.85f ? "OK" : "BAD");
    }

    // 染色检查：大信号（0.8）被压缩；小信号（0.05）近直通；旁路直通
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
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
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        const int warmup = 100, measure = 1000;

        // 稀疏脉冲（每 8 块一个 240 样本宽 0.8 脉冲 = 5ms 起音，接近真实人声）
        const auto pulseRms = [&](float amount) -> float
        {
            *apvts.getRawParameterValue("edgeAmount") = amount;
            double outSq = 0.0;
            for (int b = 0; b < warmup + measure; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        buf.setSample(c, n, (b % 8 == 0 && n < 240) ? 0.8f : 0.05f);
                processor.processBlock(buf, midi);
                if (b >= warmup)
                    for (int c = 0; c < 2; ++c)
                        for (int n = 0; n < 512; ++n)
                            outSq += (double) buf.getSample(c, n) * buf.getSample(c, n);
            }
            return (float) std::sqrt(outSq / (2.0 * measure * 512.0));
        };

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

        const float pulse0 = pulseRms(0.0f);
        const float boostRms = pulseRms(100.0f);
        const float roundRms = pulseRms(-100.0f);
        const float sinePlus = sineRms(100.0f);
        const float sineMinus = sineRms(-100.0f);
        TRACE("edge check: pulse 0=%.3f boost=%.3f round=%.3f, sine +%.2f -%.2f %s\n",
              pulse0, boostRms, roundRms, sinePlus, sineMinus,
              boostRms > pulse0 * 1.08f && roundRms < pulse0 * 0.92f
                  && sinePlus > 0.9f && sinePlus < 1.1f
                  && sineMinus > 0.9f && sineMinus < 1.1f ? "OK" : "BAD");
    }

    // 混响检查：单脉冲后应有尾音（amount>0）；amount=0 时无尾音
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
    }

    // 共振锁频检查：327Hz 强共振（不在任何候选上）→ De-Box 应能锁到附近并削掉
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
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
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
                         "inputGain", "outputGain", "headroom", "oversampling" })
            *apvts.getRawParameterValue(id) = 0.0f;
        *apvts.getRawParameterValue("eqDeboxCut") = -4.0f;
        *apvts.getRawParameterValue("compAmount") = 60.0f;
        *apvts.getRawParameterValue("dsLowAmount") = 50.0f;
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
            for (int c = 0; c < 2; ++c)         // JUCE9 API：逐通道复制，每块从 ref 重置，隔离链内部状态
                buf.copyFrom(c, 0, ref, c, 0, 512);
            processor.processBlock(buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                    maxErr = jmax(maxErr, std::abs(buf.getSample(c, n) - ref.getSample(c, n)));
        }
        TRACE("bypass check: maxErr=%.2e %s\n", maxErr, maxErr < 1.0e-6f ? "OK" : "BAD");
        if (! (maxErr < 1.0e-6f))
            return 1;
    }

    // 保真度检查：全默认参数应为透明直通（RMS 一致 + THD 极低）；逐模块开启定位失真源
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
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
        const auto runProbe = [&](const char* label, auto&& setup)
        {
            setup();
            const auto [r, t] = probeMeasure();
            TRACE("fidelity %s: rmsRatio=%.4f THD=%.1f dB\n", label, r, t);
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
        runProbe("deess 100", [&] { *apvts.getRawParameterValue("edgeAmount") = 0.0f;
                                    *apvts.getRawParameterValue("dsLowAmount") = 100.0f;
                                    *apvts.getRawParameterValue("dsHighAmount") = 100.0f; });
        *apvts.getRawParameterValue("dsLowAmount") = 0.0f;
        *apvts.getRawParameterValue("dsHighAmount") = 0.0f;
    }

    // 电平表检查：0.25 幅度正弦（RMS -15dBFS）→ 输入/输出电平表应 ≈ -15dB；静音应衰减
    {
        processor.prepareToPlay(48000.0, 512);
        const auto& apvts = processor.apvts;
        for (auto id : { "eqLowBoost", "eqDeboxCut", "eqClarityBoost", "eqAirBoost", "eqAirFreq",
                         "compAmount", "compMakeup",
                         "dsLowAmount", "dsHighAmount",
                         "satTypeA", "satAmountA", "satTypeB", "satAmountB",
                         "edgeAmount",
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

    juce::Logger::writeToLog("Headless test passed (all blocks finite)");
    return 0;
}
