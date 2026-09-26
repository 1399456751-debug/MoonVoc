// UI 截图工具：离线渲染 MoonVoc 编辑器为 PNG，用于视觉验证
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    MoonVocProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setSize(1280, 720);

    // 演示参数：通过 setValueNotifyingHost 触发 attachment 同步到 Slider
    auto setP = [&](const char* id, float v)
    {
        if (auto* p = processor.apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    setP("inputGain", 2.0f);
    setP("eqLowBoost", 6.0f);
    setP("eqDeboxCut", -4.0f);
    setP("eqClarityBoost", 5.0f);
    setP("eqAirBoost", 7.0f);
    setP("compAmount", 55.0f);
    setP("compMakeup", 3.0f);
    setP("dsLowAmount", 60.0f);
    setP("dsHighAmount", 45.0f);
    setP("satTypeA", 1.0f);   // FET
    setP("satAmountA", 70.0f);
    setP("edgeAmount", 35.0f);
    setP("oversampling", 3.0f); // 16x

    // 跑一些音频块（正弦），产生电平表/GR/指示灯读数
    {
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 200; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                    buf.setSample(c, n, 0.5f * std::sin(2.0 * 3.14159265 * 440.0 * (double) ((b * 512 + n) % 48000) / 48000.0));
            processor.processBlock(buf, midi);
        }
    }

    editor->resized(); // 离线组件需手动布局

    // 触发 timer 逻辑，让电平表/锁频标签更新到截图
    if (auto* me = dynamic_cast<MoonVocEditor*>(editor.get()))
    {
        me->dumpLayout();
        for (int i = 0; i < 30; ++i) me->demoTick();
    }
    else
        std::printf("not a MoonVocEditor\n");

    // 渲染编辑器为 PNG（离线快照，供模型肉眼检查 UI）
    {
        auto img = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        juce::File out(juce::File::getCurrentWorkingDirectory().getChildFile("ui_snapshot.png"));
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
            std::printf("FAILED to open output file\n");
        }
    }
    return 0;
}
