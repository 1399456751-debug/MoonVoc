#include "VoiceDeEsser.h"
#include "../ParamIDs.h"

VoiceDeEsser::VoiceDeEsser(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate)
    : amountParam(apvts.getRawParameterValue(ParamID::dsAmount)),
      focusParam (apvts.getRawParameterValue(ParamID::dsFocus)),
      bypassParam(apvts.getRawParameterValue(ParamID::deEssBypass)),
      dspRate(&osSampleRate)
{
}

void VoiceDeEsser::prepare(const juce::dsp::ProcessSpec& spec)
{
    hostRate = spec.sampleRate;
    sampleRate = dspRate->load();
    reset();
}

void VoiceDeEsser::reset()
{
    for (auto& c : chan)
    {
        std::fill(std::begin(c.s), std::end(c.s), 0.0);
        std::fill(std::begin(c.m), std::end(c.m), 0.0);
        c.iirA = c.iirB = 0.0;
        c.ratioA = c.ratioB = 1.0;
    }
    flip = false;
    detectCountdown = 1;
    gainReduction.store(0.0f);
}

// 检测推进一次（等价原版每采样做的那段）——按 osFactor 降采样调用
void VoiceDeEsser::detect(double inL, double inR)
{
    // 先翻转：本次更新哪一组，接下来的 osFactor 个样本就读哪一组
    // （原版是同一个采样内"更新即使用"，拆成检测/应用两段后顺序必须是先翻后更）
    flip = !flip;

    const double inputs[2] { inL, inR };

    for (int ci = 0; ci < 2; ++ci)
    {
        Chan& c = chan[ci];

        // 推入新样本（原版结构：s[0] 与 s[1] 都是新样本）
        c.s[0] = inputs[ci];
        for (int x = sharpness; x > 0; --x)
            c.s[x] = c.s[x - 1];

        // 斜率的斜率
        c.m[1] = (c.s[1] - c.s[2]) * ((c.s[1] - c.s[2]) / 1.3);
        for (int x = sharpness - 1; x > 1; --x)
            c.m[x] = (c.s[x] - c.s[x + 1]) * ((c.s[x - 1] - c.s[x]) / 1.3);

        // sense = 各位置斜率变化率的连乘：任何一处平缓就把结果压掉
        double sense = std::abs(c.m[1] - c.m[2]) * sharpness * sharpness;
        for (int x = sharpness - 1; x > 0; --x)
        {
            const double mult = std::abs(c.m[x] - c.m[x + 1]) * sharpness * sharpness;
            if (mult < 1.0)
                sense *= mult;
        }

        sense = 1.0 + intensity * intensity * sense;
        if (sense > intensity) sense = intensity;
        if (sense < 1.0) sense = 1.0; // ratio 不降到 1 以下（关闭时保持干净直通）

        // 交替双 IIR：ratio 平滑（原版 speed 由 sharpness 决定）
        double& ratio = flip ? c.ratioA : c.ratioB;
        ratio = ratio * (1.0 - speedRate) + sense * speedRate;
        if (ratio > depthLimit) ratio = depthLimit;
    }
}

void VoiceDeEsser::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& outputBlock = context.getOutputBlock();
    if (outputBlock.getNumSamples() == 0)
        return;

    sampleRate = dspRate->load();

    // 旁通：强度平滑归零（bypassMix → 0 时 ratio 恒 1，逐样本直通）
    {
        const float bypTarget = bypassParam->load() > 0.5f ? 0.0f : 1.0f;
        const double blockDur = (double) outputBlock.getNumSamples() / jmax(1.0, sampleRate);
        const float bypAlpha = 1.0f - (float) std::exp(-blockDur / 0.01);
        bypassMix += bypAlpha * (bypTarget - bypassMix);
    }

    // OS 降采样因子：检测路径在宿主采样率尺度运行
    const int osFactor = jmax(1, (int) std::lround(sampleRate / jmax(1.0, hostRate)));

    // 参数映射（Focus：0 = 低频刺 ~3-5kHz，1 = 高频刺 ~6-10kHz）
    const double amount = jlimit(0.0f, 100.0f, amountParam->load()) / 100.0;
    const double focus  = jlimit(0.0f, 100.0f, focusParam->load()) / 100.0;

    // 原版 overallscale 语义 —— 必须用 hostRate 而非 dspRate：
    // 检测路径已降采样到宿主采样率尺度，相邻检测步的斜率对应宿主率的一个采样间隔。
    // 用 dspRate 会让超采样倍率越高、去齿音越弱（16x 下除以 17.4，几乎失效）。
    const double scale = jmax(1.0, hostRate) / 44100.0;

    const double byp = bypassMix;
    intensity  = std::pow(amount, 5.0) * (8192.0 / scale) * byp;
    sharpness  = jlimit(2, 40, (int) std::lround(24.0 + focus * 16.0));
    speedRate  = 0.1 / (double) sharpness;
    depthLimit = 1.0 / (0.35 - focus * 0.3 + 0.0001);
    iirAmount  = (0.06 + focus * 0.24) / (double) osFactor; // 按 OS 因子缩放保等效截止频率
    depthLimit = 1.0 + (depthLimit - 1.0) * byp * amount;    // 关闭时上限收到 1.0（无削减）

    const size_t chs = (size_t) jmin((int) outputBlock.getNumChannels(), 2);
    const int numSamples = (int) outputBlock.getNumSamples();

    float* data[2] { nullptr, nullptr };
    for (size_t ch = 0; ch < chs; ++ch)
        data[ch] = outputBlock.getChannelPointer(ch);

    float grMax = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        // 检测路径（按 osFactor 降采样；读的是本样本的输入值）
        if (--detectCountdown <= 0)
        {
            detectCountdown = osFactor;
            detect((double) (chs > 0 ? data[0][n] : 0.0f),
                   (double) (chs > 1 ? data[1][n] : data[0][n]));
        }

        for (size_t ch = 0; ch < chs; ++ch)
        {
            Chan& c = chan[jmin(ch, (size_t) 1)];
            const double in = (double) data[ch][n];

            // 交替双 IIR（iirAmount 已按 OS 因子缩放）
            double& iir = flip ? c.iirA : c.iirB;
            double& ratio = flip ? c.ratioA : c.ratioB;
            iir = iir * (1.0 - iirAmount) + in * iirAmount;

            double out = in;
            if (ratio > 1.0)
                out = iir + (in - iir) / ratio; // 动态 IIR 内插削减

            // 取最负的削减量（jmax 会永远返回 0，因为削减量是负值）
            grMax = jmin(grMax, (float) (20.0 * std::log10(1.0 / jmax(1.0, ratio))));
            data[ch][n] = (float) out;
        }
    }

    gainReduction.store(grMax);
}
