#include "PluginEditor.h"

namespace
{
    constexpr int kTitleWidth = 100;
}

MoonVocEditor::MoonVocEditor(MoonVocProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // 主题 LookAndFeel（旋钮/开关/下拉全部自绘）
    lookAndFeel = std::make_unique<MoonVocLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());

    // 星野（确定性随机）
    {
        juce::Random srng(20260804);
        for (auto& s : stars)
        {
            s.x = srng.nextFloat();
            s.y = srng.nextFloat();
            s.size = 0.6f + srng.nextFloat() * 1.3f;
        }
    }

    // 月亮背景图（从嵌入资源加载）
    {
        juce::MemoryBlock mb(BinaryData::moon_jpg, BinaryData::moon_jpgSize);
        juce::MemoryInputStream mis(mb, false);
        moonImage = juce::ImageFileFormat::loadFrom(mis);
    }

    // 金属拉丝纹理（预渲染：暗紫底 + 细水平拉丝噪点）
    metalTexture = juce::Image(juce::Image::RGB, 512, 512, true);
    {
        juce::Graphics mg(metalTexture);
        juce::Random rng;
        for (int y = 0; y < 512; y += 3)
        {
            mg.setColour(juce::Colours::white.withAlpha(0.012f + rng.nextFloat() * 0.02f));
            mg.drawHorizontalLine(y, 0.0f, 512.0f);
        }
        mg.setColour(juce::Colours::black.withAlpha(0.10f));
        for (int y = 1; y < 512; y += 9)
            mg.drawHorizontalLine(y, 0.0f, 512.0f);
    }

    setupSectionTitle(sectionGlobal, "Global");
    setupSectionTitle(sectionEq, "EQ");
    setupSectionTitle(sectionComp, "Compressor");
    setupSectionTitle(sectionDeEss, "De-Esser");
    setupSectionTitle(sectionSat, "Saturate");
    setupSectionTitle(sectionEdge, "Edge");
    // 全局
    setupSlider(inputGainSlider,  inputGainLabel,  "Input");
    setupSlider(headroomSlider,   headroomLabel,   "Headroom");
    setupSlider(outputGainSlider, outputGainLabel, "Output");
    setupCombo(oversamplingBox, { "Off", "2x", "4x", "8x", "16x" });

    // EQ
    setupSlider(boostSlider,   boostLabel,   "Thick");
    setupSlider(deboxSlider,   deboxLabel,   "De-Box");
    setupSlider(claritySlider, clarityLabel, "Clarity");
    setupSlider(airSlider,     airLabel,     "Air", true);
    deboxFreqLabel.setJustificationType(juce::Justification::centred);
    deboxFreqLabel.setColour(juce::Label::textColourId, Theme::accent);
    deboxFreqLabel.setFont(10.0f);
    addAndMakeVisible(deboxFreqLabel);
    clarityFreqLabel.setJustificationType(juce::Justification::centred);
    clarityFreqLabel.setColour(juce::Label::textColourId, Theme::accent);
    clarityFreqLabel.setFont(10.0f);
    addAndMakeVisible(clarityFreqLabel);
    thickFreqLabel.setJustificationType(juce::Justification::centred);
    thickFreqLabel.setColour(juce::Label::textColourId, Theme::accent);
    thickFreqLabel.setFont(10.0f);
    addAndMakeVisible(thickFreqLabel);
    setupCombo(airFreqBox, { "13 kHz", "22 kHz" });

    // 压缩
    setupCombo(compModeBox, { "Pop", "Rap" });
    setupSlider(compAmountSlider, compAmountLabel, "Compression", true);
    setupSlider(compMakeupSlider, compMakeupLabel, "Makeup", true);

    // 去齿音
    setupSlider(dsLowSlider,  dsLowLabel,  "3-5k", true);
    setupSlider(dsHighSlider, dsHighLabel, "5k+", true);

    // 染色
    const juce::StringArray satTypes{ "Off", "FET", "Tube", "Tape", "Optical", "Germanium" };
    setupCombo(satTypeABox, satTypes);
    setupSlider(satAmountASlider, satAmountALabel, "Drive A", true);
    setupCombo(satTypeBBox, satTypes);
    setupSlider(satAmountBSlider, satAmountBLabel, "Drive B", true);

    // 瞬态
    setupSlider(edgeSlider, edgeLabel, "Edge");

    // 全局
    oversamplingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "oversampling", oversamplingBox);
    inputGainAtt  = std::make_unique<SliderAttachment>(processorRef.apvts, "inputGain",  inputGainSlider);
    headroomAtt   = std::make_unique<SliderAttachment>(processorRef.apvts, "headroom",   headroomSlider);
    outputGainAtt = std::make_unique<SliderAttachment>(processorRef.apvts, "outputGain", outputGainSlider);

    // EQ
    boostAtt   = std::make_unique<SliderAttachment>(processorRef.apvts, "eqLowBoost",     boostSlider);
    deboxAtt   = std::make_unique<SliderAttachment>(processorRef.apvts, "eqDeboxCut",     deboxSlider);
    clarityAtt = std::make_unique<SliderAttachment>(processorRef.apvts, "eqClarityBoost", claritySlider);
    airAtt     = std::make_unique<SliderAttachment>(processorRef.apvts, "eqAirBoost",     airSlider);
    airFreqAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "eqAirFreq", airFreqBox);

    // 压缩
    compModeAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "compMode", compModeBox);
    compAmountAtt  = std::make_unique<SliderAttachment>(processorRef.apvts, "compAmount", compAmountSlider);
    compMakeupAtt  = std::make_unique<SliderAttachment>(processorRef.apvts, "compMakeup", compMakeupSlider);

    // 去齿音
    dsLowAtt   = std::make_unique<SliderAttachment>(processorRef.apvts, "dsLowAmount",  dsLowSlider);
    dsHighAtt  = std::make_unique<SliderAttachment>(processorRef.apvts, "dsHighAmount", dsHighSlider);

    // 染色
    satTypeAAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "satTypeA", satTypeABox);
    satAmountAAtt = std::make_unique<SliderAttachment>(processorRef.apvts, "satAmountA", satAmountASlider);
    satTypeBAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "satTypeB", satTypeBBox);
    satAmountBAtt = std::make_unique<SliderAttachment>(processorRef.apvts, "satAmountB", satAmountBSlider);

    // 瞬态
    edgeAtt    = std::make_unique<SliderAttachment>(processorRef.apvts, "edgeAmount", edgeSlider);

    setSize(1280, 660);
    startTimerHz(10);
}

void MoonVocEditor::setupSlider(juce::Slider& s, juce::Label& l, const juce::String& text, bool singleSided)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    s.setTextValueSuffix("");
    s.setDoubleClickReturnValue(true, s.getValue());
    s.setColour(juce::Slider::textBoxTextColourId, Theme::textMain);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxHighlightColourId, Theme::accent.withAlpha(0.3f));
    // 单向增益旋钮：0 值指针在 7 点方向；正负旋钮 0 值在 12 点方向
    s.getProperties().set("moonvocSingleSided", singleSided);
    addAndMakeVisible(s);

    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, Theme::textMain);
    l.setFont(Theme::fontLabel());
    addAndMakeVisible(l);
}

void MoonVocEditor::setupCombo(juce::ComboBox& c, const juce::StringArray& items)
{
    c.addItemList(items, 1);
    c.setColour(juce::ComboBox::textColourId, Theme::textMain);
    c.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    c.setColour(juce::ComboBox::arrowColourId, Theme::accent);
    addAndMakeVisible(c);
}

void MoonVocEditor::setupButton(juce::ToggleButton& b, const juce::String& text)
{
    b.setButtonText(text);
    b.setColour(juce::ToggleButton::textColourId, Theme::textMain);
    addAndMakeVisible(b);
}

void MoonVocEditor::setupSectionTitle(juce::Label& l, const juce::String& text)
{
    l.setText(text.toUpperCase(), juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, Theme::textMain.withAlpha(0.9f));
    l.setFont(Theme::fontSection(13.0f));
    addAndMakeVisible(l);
}

// 布局自检：打印关键控件位置（验证对齐/居中/等大）
void MoonVocEditor::dumpLayout() const
{
    auto print = [](const char* name, const juce::Rectangle<int>& r)
    {
        std::printf("%-22s x=%4d y=%4d w=%4d h=%4d\n", name, r.getX(), r.getY(), r.getWidth(), r.getHeight());
    };
    print("boostSlider",   boostSlider.getBounds());
    print("deboxSlider",   deboxSlider.getBounds());
    print("claritySlider", claritySlider.getBounds());
    print("airSlider",     airSlider.getBounds());
    print("airFreqBox",    airFreqBox.getBounds());
    print("satTypeABox",   satTypeABox.getBounds());
    print("satTypeBBox",   satTypeBBox.getBounds());
    print("edgeSlider",    edgeSlider.getBounds());
    print("compAmountSlider", compAmountSlider.getBounds());
    print("indicatorRect", indicatorRect);
    print("oversamplingBox", oversamplingBox.getBounds());
    print("grCompRect", grCompRect);
    print("grDeessRect", grDeessRect);
    print("panelRight", panelRight);
    std::printf("edge w==h: %d  comp w==h: %d\n",
                edgeSlider.getBounds().getWidth() == edgeSlider.getBounds().getHeight(),
                compAmountSlider.getBounds().getWidth() == compAmountSlider.getBounds().getHeight());
    std::printf("satA w==satB w: %d  satA h==satB h: %d\n",
                satTypeABox.getBounds().getWidth() == satTypeBBox.getBounds().getWidth(),
                satTypeABox.getBounds().getHeight() == satTypeBBox.getBounds().getHeight());
    print("eqRowRect", eqRowRect);
    std::printf("airFreq centred: %d (centreX=%d, eqRowCX=%d)\n",
                std::abs(airFreqBox.getBounds().getCentreX() - eqRowRect.getCentreX()) <= 2,
                airFreqBox.getBounds().getCentreX(), eqRowRect.getCentreX());
}

void MoonVocEditor::timerCallback()
{
    // 智能锁频实时显示（最多 3 个）
    juce::String db, cl;
    for (int i = 0; i < 3; ++i)
    {
        const float df = processorRef.getEqDeboxFreq(i);
        const float cf = processorRef.getEqClarityFreq(i);
        if (df > 1.0f)
            db += (db.isEmpty() ? "" : "+") + juce::String((int) df);
        if (cf > 1.0f)
            cl += (cl.isEmpty() ? "" : "+") + juce::String((int) cf);
    }
    deboxFreqLabel.setText("lock: " + (db.isEmpty() ? juce::String("400") : db) + " Hz",
                           juce::dontSendNotification);
    thickFreqLabel.setText("lock: " + juce::String((int) processorRef.getEqThickFreq()) + " Hz",
                           juce::dontSendNotification);
    clarityFreqLabel.setText("lock: " + (cl.isEmpty() ? juce::String("4000") : cl) + " Hz",
                             juce::dontSendNotification);

    // 电平表/指示灯动画
    indicatorPhase += 0.18f;
    meterPeakIn  = jmax(meterPeakIn - 0.7f,  processorRef.inputLevelDb.load());
    meterPeakOut = jmax(meterPeakOut - 0.7f, processorRef.outputLevelDb.load());
    repaint();
}

// 电平表：横向条（与 GR 条同风格：标签左、条从左往右渐变填充、峰值竖线、数值右）
void MoonVocEditor::paintMeter(juce::Graphics& g, juce::Rectangle<int> r, float levelDb,
                               float peakDb, const juce::String& name, float)
{
    // 标签（条左侧，辉光）
    Theme::drawGlowText(g, name,
                        juce::Rectangle<int>(r.getX() - 44, r.getY(), 40, r.getHeight()).toFloat(),
                        Theme::fontLabel(12.0f), Theme::textDim.withAlpha(0.95f),
                        juce::Justification::centredRight);

    // 槽
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRect(r);
    g.setColour(Theme::panelEdge.withAlpha(0.5f));
    g.drawRect(r, 1);

    // 电平填充（从左往右，绿→黄→红渐变）
    const float frac = jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
    const int w = (int) (r.getWidth() * frac);
    if (w > 0)
    {
        juce::ColourGradient grad(Theme::okGreen.darker(0.3f), (float) r.getX(), 0.0f,
                                  Theme::danger, (float) (r.getX() + r.getWidth()), 0.0f, false);
        grad.addColour(0.55, Theme::okGreen);
        grad.addColour(0.78, Theme::warnYellow);
        g.setGradientFill(grad);
        g.fillRect(r.getX(), r.getY(), w, r.getHeight());
    }

    // 峰值保持竖线（缓慢衰减）
    const float peakFrac = jlimit(0.0f, 1.0f, (peakDb + 60.0f) / 60.0f);
    const int peakX = r.getX() + (int) (r.getWidth() * peakFrac);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.fillRect((float) peakX, (float) r.getY(), 1.5f, (float) r.getHeight());

    // 数值（条右侧框外，统一风格，辉光）
    Theme::drawGlowText(g, juce::String(levelDb, 1) + " dB",
                        juce::Rectangle<int>(r.getRight() + 4, r.getY() - 1, 58, r.getHeight() + 2).toFloat(),
                        Theme::fontValue(13.0f), Theme::textMain.withAlpha(0.95f),
                        juce::Justification::centredLeft);
}

// 横向 GR 表（从右往左填，标签左、数值右）
void MoonVocEditor::paintGrBar(juce::Graphics& g, juce::Rectangle<int> r, float grDb,
                               const juce::String& name, juce::Colour col)
{
    // 标签（条左侧，辉光）
    Theme::drawGlowText(g, name,
                        juce::Rectangle<int>(r.getX() - 44, r.getY(), 40, r.getHeight()).toFloat(),
                        Theme::fontLabel(12.0f), Theme::textDim.withAlpha(0.95f),
                        juce::Justification::centredRight);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRect(r);
    g.setColour(Theme::panelEdge.withAlpha(0.5f));
    g.drawRect(r, 1);

    if (grDb < -0.2f)
    {
        const float frac = jlimit(0.0f, 1.0f, -grDb / 30.0f);
        const int w = (int) (r.getWidth() * frac);
        g.setColour(col.withAlpha(0.85f));
        g.fillRect(r.getRight() - w, r.getY(), w, r.getHeight());
    }

    // 数值（条右侧框外，辉光）
    Theme::drawGlowText(g, juce::String(grDb, 1) + " dB",
                        juce::Rectangle<int>(r.getRight() + 4, r.getY() - 1, 58, r.getHeight() + 2).toFloat(),
                        Theme::fontValue(13.0f), Theme::textMain.withAlpha(0.95f),
                        juce::Justification::centredLeft);
}

// 上帝粒子指示灯：偏低熄灭 / 完美闪烁 / 过高爆红
void MoonVocEditor::paintIndicator(juce::Graphics& g, juce::Rectangle<int> r)
{
    const float outDb = processorRef.outputLevelDb.load();
    juce::Colour col;
    float alpha = 1.0f;

    if (outDb > -6.0f)
    {
        col = Theme::danger;                             // 过高：爆红
    }
    else if (outDb > -18.0f)
    {
        col = Theme::accentHi;                           // 完美：紫色闪烁
        alpha = 0.5f + 0.5f * (0.5f + 0.5f * std::sin(indicatorPhase));
    }
    else
    {
        col = Theme::textDim;                            // 偏低：熄灭
        alpha = 0.35f;
    }

    // 科幻菱形棱镜（旋转 45° 正方形）：光晕层 + 主体 + 亮芯
    const auto centre = r.getCentre().toFloat();
    const float half = r.getWidth() * 0.5f;
    auto diamond = [&](float s)
    {
        juce::Path p;
        p.addQuadrilateral(centre.x, centre.y - half * s,
                           centre.x + half * s, centre.y,
                           centre.x, centre.y + half * s,
                           centre.x - half * s, centre.y);
        return p;
    };

    g.setColour(col.withAlpha(alpha * 0.10f));
    g.fillPath(diamond(1.8f));
    g.setColour(col.withAlpha(alpha * 0.28f));
    g.fillPath(diamond(1.2f));
    g.setColour(col.withAlpha(alpha * 0.85f));
    g.fillPath(diamond(0.85f));
    g.setColour(juce::Colours::white.withAlpha(alpha * 0.95f));
    g.fillPath(diamond(0.32f));

    Theme::drawGlowText(g, "LEVEL",
                        r.withY(r.getY() + r.getHeight() + 2).withHeight(14).toFloat(),
                        Theme::fontLabel(11.0f), Theme::textDim.withAlpha(0.9f),
                        juce::Justification::centred);
}

void MoonVocEditor::paint(juce::Graphics& g)
{
    // 背景：深邃紫黑（中心微亮紫光斑 → 边缘沉底）+ 金属拉丝
    g.fillAll(Theme::bg);
    {
        const float cx = getWidth() * 0.5f, cy = getHeight() * 0.42f;
        juce::ColourGradient nebula(Theme::accent.withAlpha(0.06f), cx, cy,
                                    juce::Colours::transparentBlack, cx + getWidth() * 0.55f,
                                    cy + getHeight() * 0.55f, true);
        g.setGradientFill(nebula);
        g.fillRect(0, 0, getWidth(), getHeight());
        // 顶部神秘紫光带
        juce::ColourGradient topGlow(Theme::accent.withAlpha(0.10f), 0.0f, 0.0f,
                                     juce::Colours::transparentBlack, 0.0f, 130.0f, false);
        g.setGradientFill(topGlow);
        g.fillRect(0, 0, getWidth(), 130);
    }
    // 月亮背景图（最底层，全屏放大，半透明隐隐透出）
    if (moonImage.isValid())
    {
        g.setOpacity(0.20f);
        g.drawImage(moonImage, juce::Rectangle<float>(0.0f, 0.0f, (float) getWidth(), (float) getHeight()),
                    juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
    }

    // 金属纹理（盖在月亮图上，增强质感）
    g.setOpacity(0.25f);
    g.drawImage(metalTexture,
                juce::Rectangle<int>(0, 0, getWidth(), getHeight()).toFloat(),
                juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1.0f);

    // 星野（微弱闪烁点）
    for (auto& s : stars)
    {
        const juce::Point<float> p { s.x * getWidth(), s.y * getHeight() };
        g.setColour(juce::Colours::white.withAlpha(0.12f + 0.10f * (0.5f + 0.5f * std::sin(s.x * 7.3f + s.y * 3.1f + indicatorPhase * 0.5f))));
        g.fillEllipse(p.x, p.y, s.size, s.size);
    }

    // 程序化血月（右上角，暗红血月 + 血色光晕，叠加在月亮底图上）
    {
        const juce::Point<float> mc { getWidth() - 150.0f, 100.0f };
        const float mr = 46.0f;
        juce::ColourGradient halo(juce::Colour(0xff4a1020), mc.x, mc.y,
                                  juce::Colours::transparentBlack, mc.x, mc.y + mr * 3.2f, true);
        halo.addColour(0.35, juce::Colour(0xff3a0c18));
        g.setGradientFill(halo);
        g.fillEllipse(mc.x - mr * 3.0f, mc.y - mr * 3.0f, mr * 6.0f, mr * 6.0f);
        juce::ColourGradient moon(juce::Colour(0xff7a2030), mc.x - mr * 0.3f, mc.y - mr * 0.3f,
                                  juce::Colour(0xff1a060c), mc.x + mr * 0.5f, mc.y + mr * 0.5f, true);
        moon.addColour(0.5, juce::Colour(0xff4a1220));
        g.setGradientFill(moon);
        g.fillEllipse(mc.x - mr, mc.y - mr, mr * 2, mr * 2);
        g.setColour(juce::Colour(0xff8a3040));
        g.drawEllipse(mc.x - mr, mc.y - mr, mr * 2, mr * 2, 1.2f);
        g.setColour(juce::Colour(0xff20080e));
        g.fillEllipse(mc.x - mr * 0.45f, mc.y - mr * 0.3f, mr * 0.6f, mr * 0.55f);
        g.fillEllipse(mc.x + mr * 0.15f, mc.y + mr * 0.25f, mr * 0.4f, mr * 0.35f);
    }

    // 面板块：暗紫面板 + 精致描边（外细边 + 顶部内高光 + 顶部微光）
    auto drawPanel = [&](juce::Rectangle<int> r)
    {
        if (r.isEmpty())
            return;
        const auto rf = r.toFloat();
        // 面板底（更深的紫玻璃感）
        g.setColour(Theme::panel.darker(0.12f).withAlpha(0.78f));
        g.fillRoundedRectangle(rf, 6.0f);
        // 内部深邃渐变（顶亮底暗）
        juce::ColourGradient depth(Theme::panel.brighter(0.10f).withAlpha(0.5f), 0.0f, (float) r.getY(),
                                   Theme::bg.darker(0.2f).withAlpha(0.4f), 0.0f, (float) r.getBottom(), false);
        g.setGradientFill(depth);
        g.fillRoundedRectangle(rf, 6.0f);
        // 顶部紫色微光
        juce::ColourGradient glow(Theme::accent.withAlpha(0.09f), 0.0f, (float) r.getY(),
                                  juce::Colours::transparentBlack, 0.0f, (float) r.getY() + 36.0f, false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(rf, 6.0f);
        // 内高光线
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawHorizontalLine(r.getY() + 1, (float) (r.getX() + 8), (float) (r.getRight() - 8));
        // 发光描边（外层微光 + 内层细边）
        g.setColour(Theme::accent.withAlpha(0.16f));
        g.drawRoundedRectangle(rf.expanded(1.0f), 7.0f, 1.4f);
        g.setColour(Theme::panelEdge.withAlpha(0.6f));
        g.drawRoundedRectangle(rf, 6.0f, 1.0f);
    };
    drawPanel(panelGlobal);
    drawPanel(panelLeft);
    drawPanel(panelMid);
    drawPanel(panelRight);
    
    // 区标题：左侧紫色竖条（设计感）
    auto drawSectionBar = [&](const juce::Label& l)
    {
        auto r = l.getBounds();
        if (r.isEmpty())
            return;
        g.setColour(Theme::accent.withAlpha(0.85f));
        g.fillRect(r.getX() - 8, r.getCentreY() - 7, 3, 14);
    };
    drawSectionBar(sectionGlobal);
    drawSectionBar(sectionEq);
    drawSectionBar(sectionComp);
    drawSectionBar(sectionDeEss);
    drawSectionBar(sectionSat);
    drawSectionBar(sectionEdge);

    // 神秘纹路：面板右上角发光符文三角（每块面板）
    auto drawRune = [&](const juce::Rectangle<int>& r)
    {
        if (r.isEmpty())
            return;
        g.setColour(Theme::accent.withAlpha(0.38f));
        juce::Path tri;
        tri.addTriangle((float) r.getRight() - 14, (float) r.getY() + 9,
                        (float) r.getRight() - 7,  (float) r.getY() + 2,
                        (float) r.getRight() - 2,  (float) r.getY() + 9);
        g.fillPath(tri);
        g.setColour(Theme::accentHi.withAlpha(0.6f));
        g.fillEllipse((float) r.getRight() - 8, (float) r.getY() + 6, 2.4f, 2.4f);
    };
    drawRune(panelGlobal);
    drawRune(panelLeft);
    drawRune(panelMid);
    drawRune(panelRight);

    // 神秘纹路：面板间能量连线（竖向细线连接相邻面板）
    g.setColour(Theme::accent.withAlpha(0.20f));
    g.fillRect(panelGlobal.getCentreX() - 1, panelGlobal.getBottom(), 2, panelLeft.getY() - panelGlobal.getBottom());
    g.setColour(Theme::accentHi.withAlpha(0.5f));
    g.fillEllipse((float) (panelGlobal.getCentreX() - 2), (float) (panelGlobal.getBottom() + (panelLeft.getY() - panelGlobal.getBottom()) / 2) - 2, 4.0f, 4.0f);

    // 底部能量线（超采样横条上方横贯，渐隐两端）
    {
        const int ey = 556;
        juce::ColourGradient e1(Theme::accent.withAlpha(0.5f), 60.0f, (float) ey,
                                juce::Colours::transparentBlack, 320.0f, (float) ey, false);
        g.setGradientFill(e1);
        g.fillRect(60, ey, 260, 1);
        juce::ColourGradient e2(Theme::accent.withAlpha(0.5f), (float) (getWidth() - 320), (float) ey,
                                juce::Colours::transparentBlack, (float) (getWidth() - 60), (float) ey, false);
        g.setGradientFill(e2);
        g.fillRect(getWidth() - 320, ey, 260, 1);
    }

    // 标题（正中央 + 5 层模糊辉光 + 渐变主层 + 亮芯 + 副标 + 两侧装饰线）
    auto titleArea = getLocalBounds().removeFromTop(56);
    auto titleBox = titleArea.withSizeKeepingCentre(500, 46).translated(0, -4);
    auto titleFont = Theme::fontTitle(44.0f);

    Theme::drawGlowText(g, "MoonVoc", titleBox.toFloat(),
                        titleFont, Theme::accent, juce::Justification::centred);
    // 主层（亮紫渐变）
    g.setFont(titleFont);
    juce::ColourGradient titleGrad(Theme::accentHi, (float) titleBox.getX(), 0.0f,
                                   Theme::accent.darker(0.1f), (float) titleBox.getRight(), 0.0f, false);
    g.setGradientFill(titleGrad);
    g.drawText("MoonVoc", titleBox, juce::Justification::centred);
    // 亮芯（中心高光）
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawText("MoonVoc", titleBox.withTrimmedRight(2).translated(-1.0f, -1.0f), juce::Justification::centred);

    // 副标（标题下方居中，5 层辉光）
    auto subBox = titleBox.withY(titleBox.getBottom() - 16).withHeight(20);
    Theme::drawGlowText(g, "V O C A L   P R O C E S S O R   ·   M O O N V O C   S Y S T E M S",
                        subBox.toFloat(), Theme::fontLabel(14.0f), Theme::textDim.withAlpha(0.95f),
                        juce::Justification::centred);

    // 标题两侧神秘能量纹路（折线走线 + 菱形节点 + 端点光点，左右镜像）
    const int cx = getWidth() / 2;
    const int lineY = 26;
    const int lineInset = 130;
    const int lineW = (getWidth() / 2) - 260 - lineInset;
    auto drawEnergyPath = [&](int dir)
    {
        const int x0 = cx - 260 * dir;
        const int x1 = x0 - 40 * dir;
        const int x2 = x1 - 60 * dir;
        juce::Path p;
        p.startNewSubPath((float) x0, (float) lineY);
        p.lineTo((float) x1, (float) lineY);
        p.lineTo((float) x1, (float) (lineY + 22));
        p.lineTo((float) x2, (float) (lineY + 22));
        g.setColour(Theme::accent.withAlpha(0.55f));
        g.strokePath(p, juce::PathStrokeType(1.2f));
        // 节点菱形（拐角处）
        g.setColour(Theme::accentHi.withAlpha(0.85f));
        juce::Path dm;
        dm.addQuadrilateral((float) x1 - 2.5f, (float) (lineY + 22),
                            (float) x1, (float) (lineY + 22) - 2.5f,
                            (float) x1 + 2.5f, (float) (lineY + 22),
                            (float) x1, (float) (lineY + 22) + 2.5f);
        g.fillPath(dm);
        // 端点光点
        g.setColour(Theme::accent.withAlpha(0.30f));
        g.fillEllipse((float) x2 - 4.0f, (float) (lineY + 22) - 4.0f, 8.0f, 8.0f);
        g.setColour(Theme::accentHi);
        g.fillEllipse((float) x2 - 1.6f, (float) (lineY + 22) - 1.6f, 3.2f, 3.2f);
    };
    if (lineW > 40)
    {
        drawEnergyPath(-1);   // 左侧
        drawEnergyPath(1);    // 右侧
        // 中间渐变线（标题下方两侧收拢）
        juce::ColourGradient l1(Theme::accent.withAlpha(0.6f), (float) (cx - 260 - lineW), (float) lineY,
                                juce::Colours::transparentBlack, (float) (cx - 260), (float) lineY, false);
        g.setGradientFill(l1);
        g.fillRect(cx - 260 - lineW, lineY, lineW, 1);
        juce::ColourGradient l2(Theme::accent.withAlpha(0.6f), (float) (cx + 260), (float) lineY,
                                juce::Colours::transparentBlack, (float) (cx + 260 + lineW), (float) lineY, false);
        g.setGradientFill(l2);
        g.fillRect(cx + 260, lineY, lineW, 1);
    }

    // 工作电平指示灯（Output 右侧）
    paintIndicator(g, indicatorRect);

    // 电平表（位置由 resized 对齐面板计算，不越界）
    paintMeter(g, meterInRect,  processorRef.inputLevelDb.load(),  meterPeakIn,  "IN",  0.0f);
    paintMeter(g, meterOutRect, processorRef.outputLevelDb.load(), meterPeakOut, "OUT", processorRef.getCompGainReduction());

    // GR 表（压缩紫色 / 去齿音青色）
    paintGrBar(g, grCompRect,  processorRef.getCompGainReduction(), "COMP",  Theme::accent);
    paintGrBar(g, grDeessRect, processorRef.getDeEssGainReduction(), "DE-ESS", Theme::okGreen);

    // LOGO（右下角，TUJZMIXING-DSP 无空格紧贴，双色 + 辉光）
    auto logoBox = getLocalBounds().removeFromBottom(20);
    auto font = Theme::fontSection(18.0f);
    const int dspW = juce::GlyphArrangement::getStringWidthInt(font, "-DSP");
    auto dspBox = logoBox.removeFromRight(dspW);
    auto tjmBox = logoBox.removeFromRight(132);
    // 5 层模糊辉光
    Theme::drawGlowText(g, "TUJZMIXING", tjmBox.toFloat(), font,
                        Theme::accent.withAlpha(0.9f), juce::Justification::centredRight);
    Theme::drawGlowText(g, "-DSP", dspBox.toFloat(), font,
                        Theme::accentHi.withAlpha(0.95f), juce::Justification::centredLeft);
}

void MoonVocEditor::resized()
{
    auto area = getLocalBounds().reduced(14);
    area.removeFromTop(52);   // 标题/指示灯
    area.removeFromBottom(14); // LOGO

    // 底部横条：超采样 + 工作电平指示灯
    auto bottomRow = area.removeFromBottom(56);
    {
        auto b = bottomRow.removeFromLeft(180);
        oversamplingBox.setBounds(b.removeFromLeft(130).removeFromTop(26).translated(0, 4));
        // 工作电平指示灯：超采样右侧
        indicatorRect = bottomRow.withSizeKeepingCentre(44, 44).translated(0, -2);
    }
    area.removeFromTop(6);

    // 全局行（Input / Headroom / Output）
    auto globalRow = area.removeFromTop(92);
    panelGlobal = globalRow;
    sectionGlobal.setBounds(globalRow.removeFromLeft(kTitleWidth));
    {
        // 3 旋钮固定槽宽 + Output 右侧的上帝粒子指示灯
        const int sw = globalRow.getWidth() / 4;
        auto slot = [&](juce::Slider& s, juce::Label& l)
        {
            auto b = globalRow.removeFromLeft(sw).reduced(10);
            l.setBounds(b.removeFromTop(16));
            s.setBounds(b.withSizeKeepingCentre(juce::jmin(52, b.getWidth()), juce::jmin(52, b.getHeight())));
        };
        slot(inputGainSlider,  inputGainLabel);
        slot(headroomSlider,   headroomLabel);
        slot(outputGainSlider, outputGainLabel);
        indicatorRect = globalRow.withSizeKeepingCentre(40, 40);
    }
    area.removeFromTop(10);

    // 三列布局（按内容分配宽度）：左 Edge+染色 28% / 中 压缩+EQ 42% / 右 去齿音+电平表 30%
    const int colLeft  = (int) (area.getWidth() * 0.26f);
    const int colMid   = (int) (area.getWidth() * 0.46f);
    const int colRight = area.getWidth() - colLeft - colMid;
    const int rowH = (area.getHeight() - 8) / 2;

    auto col = [&](int width, juce::Rectangle<int>& panelRect, juce::Label& t1, auto&& place1,
                  juce::Label& t2, auto&& place2)
    {
        auto c = area.removeFromLeft(width).reduced(6);
        panelRect = c;
        auto r1 = c.removeFromTop(rowH);
        t1.setBounds(r1.removeFromLeft(kTitleWidth));
        place1(r1);
        c.removeFromTop(8);
        auto r2 = c.removeFromTop(c.getHeight());
        t2.setBounds(r2.removeFromLeft(kTitleWidth));
        place2(r2);
    };

    // 左列：Edge（上）+ Saturate（下）
    col(colLeft, panelLeft,  sectionEdge, [&](auto& r)
    {
        // 整行给 Edge 大旋钮（150px 正方）
        auto a = r.reduced(8);
        edgeLabel.setBounds(a.removeFromTop(16));
        edgeSlider.setBounds(a.withSizeKeepingCentre(juce::jmin(150, a.getWidth()), juce::jmin(150, a.getHeight())));
    }, sectionSat, [&](auto& r)
    {
        // 左右两槽固定等宽（下拉框大小一致）
        const int sw = r.getWidth() / 2;
        auto side = [&](juce::ComboBox& box, juce::Slider& s, juce::Label& l)
        {
            auto a = r.removeFromLeft(sw).reduced(8);
            box.setBounds(a.removeFromTop(24));
            a.removeFromTop(8);
            l.setBounds(a.removeFromTop(16));
            s.setBounds(a.withSizeKeepingCentre(juce::jmin(52, a.getWidth()), juce::jmin(52, a.getHeight())));
        };
        side(satTypeABox, satAmountASlider, satAmountALabel);
        side(satTypeBBox, satAmountBSlider, satAmountBLabel);
    });

    // 中列：压缩（上）+ EQ（下）
    col(colMid, panelMid,   sectionComp, [&](auto& r)
    {
        // Style 窄槽 + Compression 大槽（100px）+ Makeup 槽
        auto a = r.removeFromLeft((int) (r.getWidth() * 0.20f)).reduced(8);
        compModeBox.setBounds(a.removeFromTop(24));
        a.removeFromTop(8);
        auto c = r.removeFromLeft((int) (r.getWidth() * 0.46f)).reduced(8);
        compAmountLabel.setBounds(c.removeFromTop(16));
        compAmountSlider.setBounds(c.withSizeKeepingCentre(juce::jmin(130, c.getWidth()), juce::jmin(130, c.getHeight())));
        auto d = r.removeFromLeft(r.getWidth()).reduced(8);
        compMakeupLabel.setBounds(d.removeFromTop(16));
        compMakeupSlider.setBounds(d.withSizeKeepingCentre(juce::jmin(56, d.getWidth()), juce::jmin(56, d.getHeight())));
    }, sectionEq, [&](auto& r)
    {
        eqRowRect = r; // 自检
        // 先取出底部整宽行给 Air 频点选择（避免旋钮消耗后区域变空）
        auto bottomRow = r.removeFromBottom(24);
        // 4 旋钮固定槽宽（严格等宽对齐）
        const int sw = r.getWidth() / 4;
        auto slot = [&](juce::Slider& s, juce::Label& l, juce::Label* lock = nullptr)
        {
            auto b = r.removeFromLeft(sw).reduced(6);
            l.setBounds(b.removeFromTop(16));
            // lock 区固定占位（无 lock 也保留，保证四个旋钮 y 对齐）
            auto lockArea = b.removeFromTop(12);
            if (lock != nullptr)
                lock->setBounds(lockArea);
            s.setBounds(b.withSizeKeepingCentre(juce::jmin(52, b.getWidth()), juce::jmin(52, b.getHeight())));
        };
        slot(boostSlider,   boostLabel, &thickFreqLabel);
        slot(deboxSlider,   deboxLabel, &deboxFreqLabel);
        slot(claritySlider, clarityLabel, &clarityFreqLabel);
        slot(airSlider,     airLabel);
        // Air 频点选择：Air 旋钮正下方（窄框）
        airFreqBox.setBounds(airSlider.getBounds().withSizeKeepingCentre(80, 22)
                                            .withY(bottomRow.getY()));
    });

    // 右列：去齿音（上）+ 电平表（下）
    col(colRight, panelRight, sectionDeEss, [&](auto& r)
    {
        auto slot = [&](juce::Slider& s, juce::Label& l)
        {
            auto b = r.removeFromLeft(r.getWidth() / 2).reduced(8);
            l.setBounds(b.removeFromTop(16));
            s.setBounds(b.withSizeKeepingCentre(juce::jmin(52, b.getWidth()), juce::jmin(52, b.getHeight())));
        };
        slot(dsLowSlider,  dsLowLabel);
        slot(dsHighSlider, dsHighLabel);
        auto b = r.removeFromLeft(r.getWidth() / 2).reduced(8);
    }, sectionEdge, [&](auto& r)
    {
        // 四条横向条：IN / OUT（电平）→ COMP / DE-ESS（GR）
        auto m = r.reduced(8);
        const int rowH = 12, gap = 7;
        const int barX = m.getX() + 44;
        const int barW = m.getWidth() - 44 - 56;
        int y = m.getY() + 4;
        meterInRect  = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        meterOutRect = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        grCompRect   = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        grDeessRect  = juce::Rectangle<int>(barX, y, barW, rowH);
    });
}
