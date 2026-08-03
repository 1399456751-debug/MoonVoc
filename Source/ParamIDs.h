#pragma once

#include <JuceHeader.h>

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
    inline const juce::String eqAirFreq     { "eqAirFreq" };      // 13k / 22k

    // 压缩（双模式）
    inline const juce::String compMode     { "compMode" };   // Pop / Rap
    inline const juce::String compAmount   { "compAmount" }; // 0~100%
    inline const juce::String compMakeup   { "compMakeup" }; // 输出补偿 0~+12dB

    // 去齿音（双频段，免扫频）
    inline const juce::String dsLowAmount  { "dsLowAmount" };  // 3k~5k
    inline const juce::String dsHighAmount { "dsHighAmount" }; // 5k+

    // 瞬态（圆滑 <-> 棱角）
    inline const juce::String edgeAmount  { "edgeAmount" };  // -100~+100

    // 染色（可选两种叠加）
    inline const juce::String satTypeA   { "satTypeA" };
    inline const juce::String satAmountA { "satAmountA" };
    inline const juce::String satTypeB   { "satTypeB" };
    inline const juce::String satAmountB { "satAmountB" };

    // 混响（双模式）
}
