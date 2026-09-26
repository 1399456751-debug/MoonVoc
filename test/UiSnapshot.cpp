// UI 截图工具：离线渲染 MoonVoc 编辑器为多张 PNG（中文/英文/大字/缩放），用于视觉验证
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static void writePng(const juce::Image& img, const juce::String& fileName)
{
    juce::File out(juce::File::getCurrentWorkingDirectory().getChildFile(fileName));
    out.deleteFile();
    juce::FileOutputStream fos(out);
    if (fos.openedOk())
    {
        juce::PNGImageFormat png;
        png.writeImageToStream(img, fos);
        std::printf("snapshot written: %s\n", out.getFullPathName().toRawUTF8());
    }
    else
    {
        std::printf("FAILED to open output file: %s\n", fileName.toRawUTF8());
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    MoonVocProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto setP = [&](const char* id, float v)
    {
        if (auto* p = processor.apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    };

    // 演示参数（产生有内容的界面：数值弧/电平表/GR/锁频标签）
    setP("inputGain", 2.0f);
    setP("eqLowBoost", 6.0f);
    setP("eqDeboxCut", -4.0f);
    setP("eqClarityBoost", 5.0f);
    setP("eqAirBoost", 7.0f);
    setP("compAmount", 55.0f);
    setP("compMakeup", 3.0f);
    setP("reverbAmount", 40.0f);
    setP("reverbMode", 1.0f);
    setP("satTypeA", 1.0f);   // FET
    setP("satAmountA", 70.0f);
    setP("edgeAmount", 35.0f);
    setP("oversampling", 3.0f); // 16x

    // 跑一些音频块，产生电平表/GR/指示灯读数
    {
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 200; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                    buf.setSample(c, n, 0.5f * std::sin(2.0 * 3.14159265 * 440.0
                                                        * (double) ((b * 512 + n) % 48000) / 48000.0));
            processor.processBlock(buf, midi);
        }
    }

    // 逐个状态截图：改设置参数 → 新建 editor → 布局 → 渲染
    const auto shot = [&](const juce::String& fileName, bool zh, bool large, float scale)
    {
        setP("uiLanguage", zh ? 0.0f : 1.0f);
        setP("uiLargeFont", large ? 1.0f : 0.0f);
        setP("uiScale", scale);

        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        editor->setSize((int) std::lround(1280.0f * scale), (int) std::lround(720.0f * scale));
        editor->resized();

        if (auto* me = dynamic_cast<MoonVocEditor*>(editor.get()))
        {
            me->dumpLayout();
            for (int i = 0; i < 30; ++i) me->demoTick();
        }

        writePng(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), fileName);
    };

    shot("ui_snapshot.png",            true,  false, 1.0f);  // 中文标准
    shot("ui_snapshot_large.png",      true,  true,  1.0f);  // 中文大字（老年版）
    shot("ui_snapshot_en.png",         false, false, 1.0f);  // English
    shot("ui_snapshot_zoom200.png",    true,  false, 2.0f);  // 中文 200% 缩放

    return 0;
}
