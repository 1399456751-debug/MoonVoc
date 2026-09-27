#pragma once

#include <JuceHeader.h>
#include "../ParamIDs.h" // S8（UTF-8 字面量解码）

// UI 文本表（中/英双语，uiLanguage 参数驱动；默认中文）
namespace Strings
{
    enum Key
    {
        kGlobal, kEq, kComp, kReverb, kSat, kEdge, kMonitor, kEngine, kSettings,
        kInput, kHeadroom, kOutput,
        kThick, kDebox, kClarity, kAir, kAirFreq16, kAirFreq22,
        kCompression, kMakeup, kStylePop, kStyleRap,
        kReverbAmt, kModePop, kModeRap,
        kDriveA, kDriveB,
        kBypass, kOversampling, kLanguage, kLargeFont, kScale, kLevel
    };

    inline juce::String get(Key k, bool zh)
    {
        switch (k)
        {
            case kGlobal:      return zh ? S8("全局")     : S8("Global");
            case kEq:          return S8("EQ");
            case kComp:        return zh ? S8("压缩")     : S8("Compressor");
            case kReverb:      return zh ? S8("混响")     : S8("Reverb");
            case kSat:         return zh ? S8("染色")     : S8("Saturate");
            case kEdge:        return zh ? S8("瞬态")     : S8("Edge");
            case kMonitor:     return zh ? S8("监测")     : S8("Monitor");
            case kEngine:      return zh ? S8("引擎")     : S8("Engine");
            case kSettings:    return zh ? S8("设置")     : S8("Settings");
            case kInput:       return zh ? S8("输入")     : S8("Input");
            case kHeadroom:    return zh ? S8("余量")     : S8("Headroom");
            case kOutput:      return zh ? S8("输出")     : S8("Output");
            case kThick:       return zh ? S8("低频厚度") : S8("Thick");
            case kDebox:       return zh ? S8("去盒子音") : S8("De-Box");
            case kClarity:     return zh ? S8("清晰度")   : S8("Clarity");
            case kAir:         return zh ? S8("空气感")   : S8("Air");
            case kAirFreq16:   return S8("16 kHz");
            case kAirFreq22:   return S8("22 kHz");
            case kCompression: return zh ? S8("压缩量")   : S8("Compression");
            case kMakeup:      return zh ? S8("补偿")     : S8("Makeup");
            case kStylePop:    return zh ? S8("柔光")     : S8("Glow");
            case kStyleRap:    return zh ? S8("锻造")     : S8("Forge");
            case kReverbAmt:   return zh ? S8("混响量")   : S8("Reverb");
            case kModePop:     return zh ? S8("薄纱")     : S8("Veil");
            case kModeRap:     return zh ? S8("深渊")     : S8("Abyss");
            case kDriveA:      return zh ? S8("驱动A")    : S8("Drive A");
            case kDriveB:      return zh ? S8("驱动B")    : S8("Drive B");
            case kBypass:      return zh ? S8("旁通")     : S8("BYP");
            case kOversampling:return zh ? S8("超采样")   : S8("Oversampling");
            case kLanguage:    return zh ? S8("语言")     : S8("Language");
            case kLargeFont:   return zh ? S8("大字")     : S8("Large");
            case kScale:       return zh ? S8("缩放")     : S8("Scale");
            case kLevel:       return zh ? S8("电平")     : S8("LEVEL");
        }
        return {};
    }
}
