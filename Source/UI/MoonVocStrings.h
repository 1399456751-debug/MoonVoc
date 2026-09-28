#pragma once

#include <JuceHeader.h>
#include "../ParamIDs.h" // S8（UTF-8 字面量解码）

// UI 文本表（中/英双语，uiLanguage 参数驱动；默认中文）
namespace Strings
{
    enum Key
    {
        kGlobal, kEq, kComp, kDeEss, kReverb, kSat, kEdge, kMonitor, kEngine, kSettings,
        kInput, kHeadroom, kOutput,
        kThick, kDebox, kClarity, kAir, kAirSatin, kAirNimbus,
        kCompression, kMakeup, kStylePop, kStyleRap,
        kDeEssAmount, kDeEssFocus,
        kReverbAmt, kModePop, kModeRap,
        kDriveA, kDriveB,
        kBypass, kOversampling, kLanguage, kLargeFont, kScale, kLevel,
        kVersion, kAboutBlurb, kAboutChainTag, kAboutModules, kAboutNoteTag, kAboutNote,
        kAboutContact, kAboutCredits,
        kCount   // 哨兵：必须保持在最后（自检遍历用）
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
            // Air 频点：Satin = 16kHz（搁架起点低、过渡缓，厚实丝滑）；Nimbus = 22kHz（更靠上、通透飘逸）
            case kAirSatin:    return zh ? S8("缎面") : S8("Satin");
            case kAirNimbus:   return zh ? S8("雨云") : S8("Nimbus");
            case kCompression: return zh ? S8("压缩量")   : S8("Compression");
            case kMakeup:      return zh ? S8("补偿")     : S8("Makeup");
            case kStylePop:    return zh ? S8("柔光")     : S8("Glow");
            case kStyleRap:    return zh ? S8("锻造")     : S8("Forge");
            case kDeEss:       return zh ? S8("去齿音")   : S8("De-Ess");
            case kDeEssAmount: return zh ? S8("去齿音量") : S8("De-Ess");
            case kDeEssFocus:  return zh ? S8("齿音频段") : S8("Focus");
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
            case kVersion:      return zh ? S8("版本") : S8("VERSION");
            case kAboutBlurb:
                return zh ? S8("MoonVoc 把整条人声链收进一个窗口：四段智能 EQ、三级母带式压缩、齿音控制、染色、瞬态整形与混响，按真实的混音顺序排列，每一环都可独立旁通。它不替你决定声音，只把每个决定做得干净利落。")
                          : S8("MoonVoc gathers an entire vocal chain into one window — four-band intelligent EQ, three-stage mastering compression, de-essing, saturation, transient shaping and reverb — arranged in the order a mix actually happens, every stage independently bypassable. It does not decide the sound for you; it just makes each decision clean.");
            case kAboutChainTag:return zh ? S8("信号链") : S8("SIGNAL CHAIN");
            case kAboutModules:
                return zh ? S8("四段智能 EQ　Thick · De-Box · Clarity · Air\n三级压缩　FET · 光电 · 并行\n齿音控制　→　双槽染色　→　瞬态整形　→　混响")
                          : S8("Four-band intelligent EQ　Thick · De-Box · Clarity · Air\nThree-stage compression　FET · Optical · Parallel\nDe-Ess → Dual-stage saturation → Transient → Reverb");
            case kAboutNoteTag: return zh ? S8("透明") : S8("TRANSPARENCY");
            case kAboutNote:
                return zh ? S8("所有算法以透明为基准：参数归零时，信号逐样本还原；超采样链采用线性相位 FIR 半带滤波，4x 下残余失真低于 −85 dB。")
                          : S8("Every algorithm is built around transparency: with all parameters at zero the signal is returned sample for sample, and the oversampling stage uses linear-phase FIR half-band filters, keeping residual distortion below −85 dB at 4x.");
            case kAboutContact:
                return zh ? S8("反馈与建议　1399456751@qq.com　·　github.com/1399456751-debug")
                          : S8("Feedback　1399456751@qq.com　·　github.com/1399456751-debug");
            case kAboutCredits:
                return zh ? S8("© 2026 TUJZMIXING　·　基于 JUCE 构建　·　去齿音改编自 Airwindows DeBess（MIT，© Chris Johnson）")
                          : S8("© 2026 TUJZMIXING　·　Built on JUCE　·　De-Esser adapted from Airwindows DeBess (MIT, © Chris Johnson)");
        }
        return {};
    }

    // 自检：所有 key 在中英两种语言下都必须非空（漏翻译即红）
    inline bool allKeysFilled()
    {
        for (int i = 0; i < (int) kCount; ++i)
            if (get ((Key) i, true).isEmpty() || get ((Key) i, false).isEmpty())
                return false;

        return true;
    }
}
