// UI 截图工具：离线渲染 MoonVoc 编辑器为多张 PNG（中文/英文/大字/缩放），用于视觉验证
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/UI/AboutOverlay.h"

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

// 浮层版面自检：卡片必须落在画布内、高度被钳制、两栏与文字块不重叠
static int checkAboutOverlay()
{
    int fail = 0;
    auto check = [&] (bool ok, const char* what)
    {
        if (! ok) { std::printf("FAIL: %s\n", what); ++fail; }
    };

    const juce::Rectangle<int> canvas (0, 0, MoonVocEditor::kDesignW, MoonVocEditor::kDesignH);

    AboutOverlay ov;
    ov.setBounds (canvas);

    for (const bool zh : { true, false })      // 中英两套文案都要放得下
        for (const bool large : { false, true }) // 大字模式字号 ×1.4
        {
            Theme::useCjkFont   = zh;
            Theme::largeFontMode = large;
            ov.setLanguage (zh);

            const auto card = ov.getCardBounds();
            check (canvas.contains (card), "about card inside canvas");
            check (card.getHeight() >= 360 && card.getHeight() <= 656, "about card height clamped");
            check (card.getWidth() == 1080, "about card width 1080");
            check (ov.getImageBounds().getHeight() <= card.getHeight(), "artwork fits card height");
            check (ov.getRightColumnBounds().getX() >= ov.getImageBounds().getRight(),
                   "about columns disjoint");

            const auto& blocks = ov.getBlocks();
            for (size_t i = 0; i < blocks.size(); ++i)
                for (size_t j = i + 1; j < blocks.size(); ++j)
                    check (! blocks[i].bounds.intersects (blocks[j].bounds),
                           "about text blocks disjoint");

            // 点击归属：卡片内部不关、✕ 关、遮罩关（中英/大字下坐标都会变，四种组合全覆盖）
            check (! ov.shouldCloseOnClickAt (card.getCentre()), "card interior click keeps overlay open");
            check (ov.shouldCloseOnClickAt (ov.getCloseBounds().getCentre()), "close button click closes");
            check (ov.shouldCloseOnClickAt ({ 4, 4 }), "backdrop click closes");
        }

    Theme::largeFontMode = false;
    Theme::useCjkFont = true;
    std::printf ("checkAboutOverlay: FAIL=%d\n", fail);
    return fail;
}

// 开合交互自检：走的是真实点击的同一条通路（infoBadge.onClick、AboutOverlay 的
// shouldCloseOnClickAt 与 keyPressed），不合成 juce::MouseEvent（那需要 Desktop，不值当）
static int checkAboutInteraction (MoonVocProcessor& processor)
{
    int fail = 0;
    auto check = [&] (bool ok, const char* what)
    {
        if (! ok) { std::printf ("FAIL: %s\n", what); ++fail; }
    };

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (MoonVocEditor::kDesignW, MoonVocEditor::kDesignH);
    editor->resized();

    auto* me = dynamic_cast<MoonVocEditor*> (editor.get());
    if (me == nullptr) { std::printf ("FAIL: editor is not MoonVocEditor\n"); return 1; }

    auto& badge   = me->getInfoBadge();
    auto& overlay = me->getAboutOverlay();

    check (! overlay.isOpen(), "about overlay starts closed");

    // 点 ⓘ（animate=true 会起淡入定时器；下面 Esc 关闭会 stopTimer，不留悬挂定时器）
    check (badge.onClick != nullptr, "info badge has click handler");
    badge.onClick();
    check (overlay.isOpen(), "info badge click opens overlay");

    check (! overlay.shouldCloseOnClickAt (overlay.getCardBounds().getCentre()),
           "click inside card does not close");

    check (overlay.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)), "esc key is handled");
    check (! overlay.isOpen(), "esc closes overlay");

    me->openAbout (false);
    check (overlay.isOpen(), "openAbout(false) opens overlay");
    check (overlay.shouldCloseOnClickAt ({ 4, 4 }), "backdrop click closes");
    overlay.close();
    check (! overlay.isOpen(), "close() hides overlay");

    me->openAbout (false);
    check (overlay.shouldCloseOnClickAt (overlay.getCloseBounds().getCentre()),
           "close button click closes");
    overlay.close();
    check (! overlay.isOpen(), "overlay closed after close button");

    // 自检按参数默认值建编辑器（uiLanguage 默认 English），会改掉全局字体标志；走之前复位，
    // 与 checkAboutOverlay() 结尾的收尾一致。注意：仅复位标志不足以消除对参考截图的扰动
    // （见 main() 末尾对本函数调用位置的说明），这里保留是为了不把脏状态留给后续代码。
    Theme::useCjkFont    = true;
    Theme::largeFontMode = false;

    std::printf ("checkAboutInteraction: FAIL=%d\n", fail);
    return fail;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    if (checkAboutOverlay() != 0)
    {
        std::printf("about overlay check failed\n");
        return 1;
    }

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

    // 关于浮层：中文 / 英文 / 中文大字（animate=false 关掉淡入，否则快照时还是透明的）
    const auto shotAbout = [&] (const juce::String& fileName, bool zh, bool large)
    {
        setP ("uiLanguage", zh ? 0.0f : 1.0f);
        setP ("uiLargeFont", large ? 1.0f : 0.0f);
        setP ("uiScale", 1.0f);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setSize (MoonVocEditor::kDesignW, MoonVocEditor::kDesignH);
        editor->resized();

        if (auto* me = dynamic_cast<MoonVocEditor*> (editor.get()))
        {
            me->dumpLayout();
            me->openAbout (false);
            for (int i = 0; i < 30; ++i) me->demoTick();
        }

        writePng (editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f), fileName);
    };

    shotAbout ("ui_snapshot_about.png",       true,  false);
    shotAbout ("ui_snapshot_about_en.png",    false, false);
    shotAbout ("ui_snapshot_about_large.png", true,  true);

    // 开合自检必须放在所有截图之后：在截图之前多建一次编辑器，会让 7 张参考图里的 3 张
    // （standard / en / zoom200）出现 1~2 像素、1~2/255 的弧线抗锯齿级抖动 —— 实测只建/毁
    // 编辑器即可复现，且参数取值、控件几何、Theme 字体标志在两次运行里完全一致，复位标志
    // 也消不掉。放在最后，参考 PNG 才逐字节稳定（append 在此不影响任何已写出的图）。
    if (checkAboutInteraction (processor) != 0)
    {
        std::printf ("about interaction check failed\n");
        return 1;
    }

    return 0;
}
