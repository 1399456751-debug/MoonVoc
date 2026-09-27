#pragma once

#include <JuceHeader.h>

// 去齿音 —— 移植 Airwindows DeBess（Copyright (c) 2016 airwindows, MIT License）
// 原理：维护采样斜率历史，连乘"斜率的变化率"检测齿音；方波/锯齿/正常辅音不触发。
//      削减方式是动态 IIR 内插（不是滤波器组），对非齿音内容近乎零染色。
//
// 超采样集成：检测路径按 OS 倍率降采样（每 osFactor 个 OS 样本推进一次检测），
//            使检测窗口的时间长度与原版在 44100Hz 下一致，同时避免 O(sharpness)
//            连乘在 16x 下爆 CPU。音频路径（IIR + 削减）仍全速运行。
class VoiceDeEsser final
{
public:
    VoiceDeEsser(juce::AudioProcessorValueTreeState& apvts, std::atomic<double>& osSampleRate);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(const juce::dsp::ProcessContextReplacing<float>& context);

    std::atomic<float> gainReduction { 0.0f }; // 当前削减量（负 dB），UI/测试读取

private:
    static constexpr int kMaxSharpness = 41; // 原版 sharpness 上限 40 → 历史 41 元素

    void detect(double inL, double inR); // 推进一次斜率检测（O(sharpness)）

    std::atomic<float>* amountParam;
    std::atomic<float>* focusParam;
    std::atomic<float>* bypassParam;
    std::atomic<double>* dspRate;
    double hostRate = 48000.0;
    double sampleRate = 48000.0; // 当前 dspRate
    float bypassMix = 1.0f;

    // 每通道状态（s = 采样历史，m = 斜率的历史）
    struct Chan
    {
        double s[kMaxSharpness + 1] {};
        double m[kMaxSharpness + 1] {};
        double iirA = 0.0, iirB = 0.0;
        double ratioA = 1.0, ratioB = 1.0;
    };
    Chan chan[2];
    bool flip = false;
    int detectCountdown = 1;

    // 本块参数（detect 调用前算好）
    int sharpness = 40;
    double intensity = 1.0, speedRate = 0.0025, depthLimit = 1.0, iirAmount = 0.1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceDeEsser)
};
