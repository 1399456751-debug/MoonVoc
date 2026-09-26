#pragma once

#include <JuceHeader.h>

// JUCE 的 String(const char*) 按 ASCII 处理（非 UTF-8），中文字面量必须显式按 UTF-8 解码
inline juce::String S8(const char* utf8Text) { return juce::String::fromUTF8(utf8Text); }

// 参数 ID 定死后永不修改（影响宿主工程兼容）
namespace ParamID
{
    // 全局
    inline const juce::String inputGain    { "inputGain" };
    inline const juce::String outputGain   { "outputGain" };
    inline const juce::String headroom     { "headroom" };
    inline const juce::String oversampling { "oversampling" };

    // EQ（四段，MÄAG EQ4 风格 + 智能频点）
    inline const juce::String eqLowBoost    { "eqLowBoost" };     // 厚度 0~+6dB
    inline const juce::String eqDeboxCut    { "eqDeboxCut" };     // 去盒子音 ±6dB（智能频点）
    inline const juce::String eqClarityBoost{ "eqClarityBoost" }; // 清晰度 0~+6dB（智能频点）
    inline const juce::String eqAirBoost    { "eqAirBoost" };     // 空气感 0~+6dB
    inline const juce::String eqAirFreq     { "eqAirFreq" };      // 16k / 22k

    // 压缩（双模式）
    inline const juce::String compMode     { "compMode" };   // Pop / Rap
    inline const juce::String compAmount   { "compAmount" }; // 0~100%
    inline const juce::String compMakeup   { "compMakeup" }; // 输出补偿 0~+12dB

    // 瞬态（圆滑 <-> 棱角）
    inline const juce::String edgeAmount  { "edgeAmount" };  // -100~+100

    // 染色（可选两种叠加）
    inline const juce::String satTypeA   { "satTypeA" };
    inline const juce::String satAmountA { "satAmountA" };
    inline const juce::String satTypeB   { "satTypeB" };
    inline const juce::String satAmountB { "satAmountB" };

    // 混响（双模式，链路最后）
    inline const juce::String reverbAmount { "reverbAmount" }; // wet 0~100%
    inline const juce::String reverbMode   { "reverbMode" };   // Pop / Rap

    // 旁通（默认 false = 不旁通）
    inline const juce::String eqBypass     { "eqBypass" };
    inline const juce::String compBypass   { "compBypass" };
    inline const juce::String satBypass    { "satBypass" };
    inline const juce::String edgeBypass   { "edgeBypass" };
    inline const juce::String reverbBypass { "reverbBypass" };

    // UI 设置
    inline const juce::String uiLanguage  { "uiLanguage" };  // 0=中文 1=English
    inline const juce::String uiLargeFont { "uiLargeFont" }; // 老年大字
    inline const juce::String uiScale     { "uiScale" };     // 1.0~3.0
}
