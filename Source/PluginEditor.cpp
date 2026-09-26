#include "PluginEditor.h"

namespace
{
    constexpr int kEdge      = 16;   // 外边距
    constexpr int kGap       = 16;   // 卡片间距
    constexpr int kPad       = 20;   // 卡片内边距
    constexpr int kKnobHero  = 120;  // hero 旋钮直径（Compression / Edge）
    constexpr int kKnobStd   = 64;   // 标准旋钮直径
    constexpr int kKnobGlob  = 56;   // 全局条旋钮直径
    constexpr int kTbH       = 18;   // 旋钮下方数值框条高（含在 slider bounds 内）
}

MoonVocEditor::MoonVocEditor(MoonVocProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // 主题 LookAndFeel（旋钮/开关/下拉全部自绘）
    lookAndFeel = std::make_unique<MoonVocLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());

    setupSectionTitle(sectionGlobal,  "Global",     Theme::cardGlobalDeep);
    setupSectionTitle(sectionEq,      "EQ",         Theme::cardEqDeep);
    setupSectionTitle(sectionComp,    "Compressor", Theme::cardCompDeep);
    setupSectionTitle(sectionDeEss,   "De-Esser",   Theme::cardDeEssDeep);
    setupSectionTitle(sectionSat,     "Saturate",   Theme::cardSatDeep);
    setupSectionTitle(sectionEdge,    "Edge",       Theme::cardEdgeDeep);
    setupSectionTitle(sectionMonitor, "Monitor",    Theme::cardMonitorDeep);
    setupSectionTitle(sectionOs,      "Engine",     Theme::cardEngineDeep);

    // 全局
    setupSlider(inputGainSlider,  inputGainLabel,  "Input",    Theme::cardGlobalDeep);
    setupSlider(headroomSlider,   headroomLabel,   "Headroom", Theme::cardGlobalDeep);
    setupSlider(outputGainSlider, outputGainLabel, "Output",   Theme::cardGlobalDeep);
    setupCombo(oversamplingBox, { "2x", "4x", "8x", "16x" });
    oversamplingLabel.setText("Oversampling", juce::dontSendNotification);
    oversamplingLabel.setJustificationType(juce::Justification::centredRight);
    oversamplingLabel.setColour(juce::Label::textColourId, Theme::textDim);
    oversamplingLabel.setFont(Theme::fontLabel(14.0f));
    addAndMakeVisible(oversamplingLabel);

    // EQ
    setupSlider(boostSlider,   boostLabel,   "Thick",   Theme::cardEqDeep);
    setupSlider(deboxSlider,   deboxLabel,   "De-Box",  Theme::cardEqDeep);
    setupSlider(claritySlider, clarityLabel, "Clarity", Theme::cardEqDeep);
    setupSlider(airSlider,     airLabel,     "Air",     Theme::cardEqDeep);
    deboxFreqLabel.setJustificationType(juce::Justification::centred);
    deboxFreqLabel.setColour(juce::Label::textColourId, Theme::cardEqDeep.darker(0.10f));
    deboxFreqLabel.setFont(Theme::fontLabel(12.0f));
    addAndMakeVisible(deboxFreqLabel);
    clarityFreqLabel.setJustificationType(juce::Justification::centred);
    clarityFreqLabel.setColour(juce::Label::textColourId, Theme::cardEqDeep.darker(0.10f));
    clarityFreqLabel.setFont(Theme::fontLabel(12.0f));
    addAndMakeVisible(clarityFreqLabel);
    thickFreqLabel.setJustificationType(juce::Justification::centred);
    thickFreqLabel.setColour(juce::Label::textColourId, Theme::cardEqDeep.darker(0.10f));
    thickFreqLabel.setFont(Theme::fontLabel(12.0f));
    addAndMakeVisible(thickFreqLabel);
    setupCombo(airFreqBox, { "16 kHz", "22 kHz" });

    // 压缩
    setupCombo(compModeBox, { "Pop", "Rap" });
    setupSlider(compAmountSlider, compAmountLabel, "Compression", Theme::cardCompDeep);
    setupSlider(compMakeupSlider, compMakeupLabel, "Makeup",      Theme::cardCompDeep);

    // 去齿音
    setupSlider(dsLowSlider,  dsLowLabel,  "3-5k", Theme::cardDeEssDeep);
    setupSlider(dsHighSlider, dsHighLabel, "5k+",  Theme::cardDeEssDeep);

    // 染色
    const juce::StringArray satTypes{ "Off", "FET", "Tube", "Tape", "Optical", "Germanium" };
    setupCombo(satTypeABox, satTypes);
    setupSlider(satAmountASlider, satAmountALabel, "Drive A", Theme::cardSatDeep);
    setupCombo(satTypeBBox, satTypes);
    setupSlider(satAmountBSlider, satAmountBLabel, "Drive B", Theme::cardSatDeep);

    // 瞬态
    setupSlider(edgeSlider, edgeLabel, "Edge", Theme::cardEdgeDeep);

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

    setSize(1280, 720);
    startTimerHz(10);
}

void MoonVocEditor::setupSlider(juce::Slider& s, juce::Label& l, const juce::String& text, juce::Colour arcColour)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, kTbH);
    s.setTextValueSuffix("");
    s.setDoubleClickReturnValue(true, s.getValue());
    s.setColour(juce::Slider::textBoxTextColourId, Theme::textMain);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxHighlightColourId, arcColour.withAlpha(0.25f));
    // 模块主题色：LAF 画数值弧/指针/轨道环时读取
    s.getProperties().set("moonvocArcColor", (juce::int64) arcColour.getARGB());
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

void MoonVocEditor::setupSectionTitle(juce::Label& l, const juce::String& text, juce::Colour deep)
{
    l.setText(text.toUpperCase(), juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centredLeft);
    l.setColour(juce::Label::textColourId, deep.darker(0.25f));
    l.setFont(Theme::fontSection(15.0f));
    addAndMakeVisible(l);
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

    // 电平表/指示灯/背景呼吸动画
    indicatorPhase += 0.18f;
    meterPeakIn  = jmax(meterPeakIn - 0.7f,  processorRef.inputLevelDb.load());
    meterPeakOut = jmax(meterPeakOut - 0.7f, processorRef.outputLevelDb.load());
    repaint();
}

// resized 里预渲染静态背景（渐变 + 抽象几何装饰），paint 只贴图
void MoonVocEditor::renderBackground()
{
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0)
        return;

    bgCache = juce::Image(juce::Image::ARGB, w, h, true);
    juce::Graphics g(bgCache);

    // 垂直微渐变（顶略亮 → 底米白）
    juce::ColourGradient grad(Theme::bg.brighter(0.4f), 0.0f, 0.0f,
                              Theme::bg, 0.0f, (float) h, false);
    g.setGradientFill(grad);
    g.fillRect(0, 0, w, h);

    // 静态抽象装饰 1：右上青蓝大圆角色块（低 alpha，旋转 15°，部分出血）
    {
        g.setColour(Theme::accent.withAlpha(0.06f));
        const juce::Rectangle<float> r { (float) w - 320.0f, -120.0f, 380.0f, 380.0f };
        juce::AffineTransform t = juce::AffineTransform::rotation(
            juce::degreesToRadians(15.0f), r.getCentreX(), r.getCentreY());
        g.fillRoundedRectangle(r.transformedBy(t), 60.0f);
    }
    // 静态抽象装饰 2：左下珊瑚大圆弧（低 alpha 扇环描边）
    {
        g.setColour(Theme::accent2.withAlpha(0.05f));
        juce::Path arc;
        arc.addCentredArc(-40.0f, (float) h + 40.0f, 200.0f, 200.0f, 0.0f, 0.0f, 1.4f, true);
        g.strokePath(arc, juce::PathStrokeType(40.0f));
    }
}

// 布局自检：打印关键控件 bounds + 卡片归属/几何断言（FAIL 期望全 0）
void MoonVocEditor::dumpLayout() const
{
    auto print = [](const char* name, const juce::Rectangle<int>& r)
    {
        std::printf("%-22s x=%4d y=%4d w=%4d h=%4d\n", name, r.getX(), r.getY(), r.getWidth(), r.getHeight());
    };
    print("cardGlobal",  cardGlobal);
    print("cardDeEss",   cardDeEss);
    print("cardEq",      cardEq);
    print("cardComp",    cardComp);
    print("cardSat",     cardSat);
    print("cardEdge",    cardEdge);
    print("cardMonitor", cardMonitor);
    print("cardOs",      cardOs);

    int fail = 0;
    auto check = [&](bool ok, const char* what)
    {
        if (! ok) { std::printf("FAIL: %s\n", what); ++fail; }
    };

    // 卡片序与间距：5 卡横排 y/h 全等、相邻间距全 16
    check(cardDeEss.getY() == cardEq.getY() && cardEq.getY() == cardComp.getY()
          && cardComp.getY() == cardSat.getY() && cardSat.getY() == cardEdge.getY(), "5 cards y equal");
    check(cardDeEss.getHeight() == cardEq.getHeight() && cardEq.getHeight() == cardComp.getHeight()
          && cardComp.getHeight() == cardSat.getHeight() && cardSat.getHeight() == cardEdge.getHeight(), "5 cards h equal");
    check(cardEq.getX() - cardDeEss.getRight() == kGap, "gap deess-eq==16");
    check(cardComp.getX() - cardEq.getRight() == kGap, "gap eq-comp==16");
    check(cardSat.getX() - cardComp.getRight() == kGap, "gap comp-sat==16");
    check(cardEdge.getX() - cardSat.getRight() == kGap, "gap sat-edge==16");
    // 卡片 rect %4==0（4px 网格）
    for (auto* c : { &cardGlobal, &cardDeEss, &cardEq, &cardComp, &cardSat, &cardEdge, &cardMonitor, &cardOs })
        check(c->getX() % 4 == 0 && c->getY() % 4 == 0 && c->getWidth() % 4 == 0 && c->getHeight() % 4 == 0,
              "card rect %4==0");

    // 控件归属：每张卡内控件必须被该卡 contains
    auto owned = [&](const juce::Rectangle<int>& card, const char* name, const juce::Rectangle<int>& r)
    {
        if (! card.contains(r)) { std::printf("FAIL: %s not inside card\n", name); ++fail; }
    };
    owned(cardGlobal, "inputGainSlider", inputGainSlider.getBounds());
    owned(cardGlobal, "headroomSlider", headroomSlider.getBounds());
    owned(cardGlobal, "outputGainSlider", outputGainSlider.getBounds());
    owned(cardGlobal, "indicatorRect", indicatorRect);
    owned(cardDeEss, "dsLowSlider", dsLowSlider.getBounds());
    owned(cardDeEss, "dsHighSlider", dsHighSlider.getBounds());
    owned(cardEq, "boostSlider", boostSlider.getBounds());
    owned(cardEq, "deboxSlider", deboxSlider.getBounds());
    owned(cardEq, "claritySlider", claritySlider.getBounds());
    owned(cardEq, "airSlider", airSlider.getBounds());
    owned(cardEq, "airFreqBox", airFreqBox.getBounds());
    owned(cardComp, "compModeBox", compModeBox.getBounds());
    owned(cardComp, "compAmountSlider", compAmountSlider.getBounds());
    owned(cardComp, "compMakeupSlider", compMakeupSlider.getBounds());
    owned(cardSat, "satTypeABox", satTypeABox.getBounds());
    owned(cardSat, "satAmountASlider", satAmountASlider.getBounds());
    owned(cardSat, "satTypeBBox", satTypeBBox.getBounds());
    owned(cardSat, "satAmountBSlider", satAmountBSlider.getBounds());
    owned(cardEdge, "edgeSlider", edgeSlider.getBounds());
    owned(cardMonitor, "meterInRect", meterInRect);
    owned(cardMonitor, "meterOutRect", meterOutRect);
    owned(cardMonitor, "grCompRect", grCompRect);
    owned(cardMonitor, "grDeessRect", grDeessRect);
    owned(cardOs, "oversamplingBox", oversamplingBox.getBounds());

    // 几何专项：EQ 两行两列（Thick/De-Box 上行，Clarity/Air 下行）
    check(boostSlider.getY() == deboxSlider.getY(), "EQ row1 same y");
    check(claritySlider.getY() == airSlider.getY(), "EQ row2 same y");
    check(claritySlider.getY() > boostSlider.getY(), "EQ row2 below row1");
    check(boostSlider.getBounds().getCentreX() == claritySlider.getBounds().getCentreX(), "EQ col1 aligned");
    check(deboxSlider.getBounds().getCentreX() == airSlider.getBounds().getCentreX(), "EQ col2 aligned");
    check(std::abs(airFreqBox.getBounds().getCentreX() - airSlider.getBounds().getCentreX()) <= 1,
          "airFreq centred under air");
    check(compAmountSlider.getBounds().getWidth() == kKnobHero
          && compAmountSlider.getBounds().getHeight() == kKnobHero + kTbH, "comp hero 120+tb");
    check(edgeSlider.getBounds().getWidth() == kKnobHero
          && edgeSlider.getBounds().getHeight() == kKnobHero + kTbH, "edge hero 120+tb");
    check(std::abs(satTypeABox.getBounds().getCentreX() - satAmountASlider.getBounds().getCentreX()) <= 1,
          "satA aligned");
    check(std::abs(satTypeBBox.getBounds().getCentreX() - satAmountBSlider.getBounds().getCentreX()) <= 1,
          "satB aligned");
    check(meterInRect.getY() < meterOutRect.getY() && meterOutRect.getY() < grCompRect.getY()
          && grCompRect.getY() < grDeessRect.getY(), "meter rows ascending");
    check(meterInRect.getWidth() == meterOutRect.getWidth() && meterOutRect.getWidth() == grCompRect.getWidth()
          && grCompRect.getWidth() == grDeessRect.getWidth(), "meter widths equal");

    std::printf("dumpLayout: FAIL=%d\n", fail);
}

// 电平表：圆角槽 + 绿→黄→红渐变填充 + 峰值竖线（横向条，标签左、数值右）
void MoonVocEditor::paintMeter(juce::Graphics& g, juce::Rectangle<int> r, float levelDb,
                               float peakDb, const juce::String& name, float)
{
    g.setFont(Theme::fontLabel(13.0f));
    g.setColour(Theme::textDim);
    g.drawText(name, juce::Rectangle<int>(r.getX() - 52, r.getY(), 48, r.getHeight()),
               juce::Justification::centredRight);

    // 圆角槽
    g.setColour(Theme::track);
    g.fillRoundedRectangle(r.toFloat(), 7.0f);

    // 电平填充（绿→黄→红渐变，裁圆角）
    const float frac = jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
    const int w = (int) (r.getWidth() * frac);
    if (w > 0)
    {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path clip;
        clip.addRoundedRectangle(r.toFloat(), 7.0f);
        g.reduceClipRegion(clip);
        juce::ColourGradient grad(Theme::okGreen.darker(0.1f), (float) r.getX(), 0.0f,
                                  Theme::danger, (float) r.getRight(), 0.0f, false);
        grad.addColour(0.55, Theme::okGreen);
        grad.addColour(0.78, Theme::warnYellow);
        g.setGradientFill(grad);
        g.fillRect(r.getX(), r.getY(), w, r.getHeight());
    }

    // 峰值保持竖线
    const float peakFrac = jlimit(0.0f, 1.0f, (peakDb + 60.0f) / 60.0f);
    const int peakX = r.getX() + (int) (r.getWidth() * peakFrac);
    g.setColour(Theme::textMain.withAlpha(0.35f));
    g.fillRect((float) peakX, (float) r.getY(), 1.5f, (float) r.getHeight());

    // 数值
    g.setFont(Theme::fontValue(14.0f));
    g.setColour(Theme::textMain);
    g.drawText(juce::String(levelDb, 1) + " dB",
               juce::Rectangle<int>(r.getRight() + 8, r.getY() - 1, 64, r.getHeight() + 2),
               juce::Justification::centredLeft);
}

// 横向 GR 表（从右往左填，标签左、数值右）
void MoonVocEditor::paintGrBar(juce::Graphics& g, juce::Rectangle<int> r, float grDb,
                               const juce::String& name, juce::Colour col)
{
    g.setFont(Theme::fontLabel(13.0f));
    g.setColour(Theme::textDim);
    g.drawText(name, juce::Rectangle<int>(r.getX() - 52, r.getY(), 48, r.getHeight()),
               juce::Justification::centredRight);

    g.setColour(Theme::track);
    g.fillRoundedRectangle(r.toFloat(), 7.0f);

    if (grDb < -0.2f)
    {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path clip;
        clip.addRoundedRectangle(r.toFloat(), 7.0f);
        g.reduceClipRegion(clip);
        const float frac = jlimit(0.0f, 1.0f, -grDb / 30.0f);
        const int w = (int) (r.getWidth() * frac);
        g.setColour(col.withAlpha(0.85f));
        g.fillRect(r.getRight() - w, r.getY(), w, r.getHeight());
    }

    g.setFont(Theme::fontValue(14.0f));
    g.setColour(Theme::textMain);
    g.drawText(juce::String(grDb, 1) + " dB",
               juce::Rectangle<int>(r.getRight() + 8, r.getY() - 1, 64, r.getHeight() + 2),
               juce::Justification::centredLeft);
}

// 工作电平指示灯：监测输入电平（Input 增益后、处理链前）；偏低熄灭 / 完美闪烁 / 过高爆红
void MoonVocEditor::paintIndicator(juce::Graphics& g, juce::Rectangle<int> r)
{
    const float inDb = processorRef.inputLevelDb.load();
    juce::Colour col;
    float alpha = 1.0f;

    if (inDb > -6.0f)
    {
        col = Theme::danger;                             // 过高：爆红
    }
    else if (inDb > -18.0f)
    {
        col = Theme::accent;                             // 完美：青蓝呼吸闪烁
        alpha = 0.5f + 0.5f * (0.5f + 0.5f * std::sin(indicatorPhase));
    }
    else
    {
        col = Theme::textDim;                            // 偏低：熄灭
        alpha = 0.35f;
    }

    // 圆角方形徽章（squircle）：光晕层 + 主体 + 白芯
    const auto rf = r.toFloat();
    g.setColour(col.withAlpha(alpha * 0.10f));
    g.fillRoundedRectangle(rf.expanded(8.0f), 14.0f);
    g.setColour(col.withAlpha(alpha * 0.28f));
    g.fillRoundedRectangle(rf.expanded(4.0f), 11.0f);
    g.setColour(col.withAlpha(alpha * 0.85f));
    g.fillRoundedRectangle(rf, 9.0f);
    g.setColour(juce::Colours::white.withAlpha(alpha * 0.95f));
    const float cd = rf.getWidth() * 0.32f;
    g.fillEllipse(rf.getCentreX() - cd * 0.5f, rf.getCentreY() - cd * 0.5f, cd, cd);

    g.setFont(Theme::fontLabel(12.0f));
    g.setColour(Theme::textDim);
    g.drawText("LEVEL", juce::Rectangle<int>(r.getX() - 4, r.getY() + r.getHeight() + 3,
                                             r.getWidth() + 8, 14),
               juce::Justification::centred);
}

void MoonVocEditor::paint(juce::Graphics& g)
{
    // 静态背景（渐变 + 抽象装饰）缓存贴图
    g.drawImageAt(bgCache, 0, 0);

    // 背景呼吸动效：叠 2 个装饰的呼吸层（同形状、低 alpha，每帧 2 次 fill）
    {
        const float breathe = 0.7f + 0.3f * std::sin(indicatorPhase * 0.35f);
        g.setColour(Theme::accent.withAlpha(0.06f * breathe));
        const juce::Rectangle<float> r1 { (float) getWidth() - 320.0f, -120.0f, 380.0f, 380.0f };
        juce::AffineTransform t = juce::AffineTransform::rotation(
            juce::degreesToRadians(15.0f), r1.getCentreX(), r1.getCentreY());
        g.fillRoundedRectangle(r1.transformedBy(t), 60.0f);

        g.setColour(Theme::accent2.withAlpha(0.05f * breathe));
        juce::Path arc;
        arc.addCentredArc(-40.0f, (float) getHeight() + 40.0f, 200.0f, 200.0f, 0.0f, 0.0f, 1.4f, true);
        g.strokePath(arc, juce::PathStrokeType(40.0f));
    }

    // 卡片：投影 + 莫兰迪底色圆角 + 模块色淡描边 + 顶部内高光
    auto drawCard = [&](const juce::Rectangle<int>& r, juce::Colour bgC, juce::Colour deepC)
    {
        if (r.isEmpty())
            return;
        const auto rf = r.toFloat();
        // 投影（向下偏 3、向外扩 2）
        g.setColour(Theme::shadow);
        g.fillRoundedRectangle(rf.translated(0.0f, 3.0f).expanded(2.0f), 18.0f);
        // 卡片体
        g.setColour(bgC);
        g.fillRoundedRectangle(rf, 16.0f);
        // 描边（模块深色 30%）
        g.setColour(deepC.withAlpha(0.30f));
        g.drawRoundedRectangle(rf, 16.0f, 1.2f);
        // 顶部内高光线
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawHorizontalLine(r.getY() + 1, (float) (r.getX() + 14), (float) (r.getRight() - 14));
    };
    drawCard(cardGlobal,  Theme::cardGlobal,  Theme::cardGlobalDeep);
    drawCard(cardDeEss,   Theme::cardDeEss,   Theme::cardDeEssDeep);
    drawCard(cardEq,      Theme::cardEq,      Theme::cardEqDeep);
    drawCard(cardComp,    Theme::cardComp,    Theme::cardCompDeep);
    drawCard(cardSat,     Theme::cardSat,     Theme::cardSatDeep);
    drawCard(cardEdge,    Theme::cardEdge,    Theme::cardEdgeDeep);
    drawCard(cardMonitor, Theme::cardMonitor, Theme::cardMonitorDeep);
    drawCard(cardOs,      Theme::cardEngine,  Theme::cardEngineDeep);

    // 区标题左侧模块色小竖条
    auto drawSectionBar = [&](const juce::Label& l, juce::Colour c)
    {
        auto r = l.getBounds();
        if (r.isEmpty())
            return;
        g.setColour(c);
        g.fillRoundedRectangle((float) (r.getX() - 8), (float) (r.getCentreY() - 8), 3.0f, 16.0f, 1.5f);
    };
    drawSectionBar(sectionGlobal,  Theme::cardGlobalDeep);
    drawSectionBar(sectionEq,      Theme::cardEqDeep);
    drawSectionBar(sectionComp,    Theme::cardCompDeep);
    drawSectionBar(sectionDeEss,   Theme::cardDeEssDeep);
    drawSectionBar(sectionSat,     Theme::cardSatDeep);
    drawSectionBar(sectionEdge,    Theme::cardEdgeDeep);
    drawSectionBar(sectionMonitor, Theme::cardMonitorDeep);
    drawSectionBar(sectionOs,      Theme::cardEngineDeep);

    // 品牌：顶部全局条内左侧 MoonVoc（青→珊瑚渐变）+ TUJZMIXING
    {
        const auto brand = cardGlobal.reduced(24, 0);
        const auto titleBox = brand.withWidth(220).withHeight(34).withY(cardGlobal.getY() + 48);
        auto titleFont = Theme::fontTitle(30.0f);
        juce::ColourGradient titleGrad(Theme::accent, (float) titleBox.getX(), 0.0f,
                                       Theme::accent2, (float) titleBox.getRight(), 0.0f, false);
        g.setFont(titleFont);
        g.setGradientFill(titleGrad);
        g.drawText("MoonVoc", titleBox, juce::Justification::centredLeft);
        const auto subBox = titleBox.translated(0.0f, 32.0f).withHeight(16);
        g.setFont(Theme::fontLabel(12.5f));
        g.setColour(Theme::textDim);
        g.drawText("T U J Z M I X I N G", subBox, juce::Justification::centredLeft);
    }

    // 工作电平指示灯（cardGlobal 右端）
    paintIndicator(g, indicatorRect);

    // 电平表 + GR 表（cardMonitor）
    paintMeter(g, meterInRect,  processorRef.inputLevelDb.load(),  meterPeakIn,  "IN",  0.0f);
    paintMeter(g, meterOutRect, processorRef.outputLevelDb.load(), meterPeakOut, "OUT", processorRef.getCompGainReduction());
    paintGrBar(g, grCompRect,  processorRef.getCompGainReduction(), "COMP",  Theme::accent);
    paintGrBar(g, grDeessRect, processorRef.getDeEssGainReduction(), "DE-ESS", Theme::accent2);

    // 页脚 LOGO（右下角，纯文本无辉光）
    auto logoBox = getLocalBounds().removeFromBottom(20);
    auto font = Theme::fontSection(15.0f);
    const int dspW = juce::GlyphArrangement::getStringWidthInt(font, "-DSP");
    auto dspBox = logoBox.removeFromRight(dspW);
    auto tjmBox = logoBox.removeFromRight(110);
    g.setFont(font);
    g.setColour(Theme::textDim);
    g.drawText("TUJZMIXING", tjmBox, juce::Justification::centredRight);
    g.setColour(Theme::accent);
    g.drawText("-DSP", dspBox, juce::Justification::centredLeft);
}

void MoonVocEditor::resized()
{
    renderBackground();

    // 顶部全局条卡片（y=16, h=140）
    cardGlobal = juce::Rectangle<int>(kEdge, kEdge, getWidth() - kEdge * 2, 140);
    {
        sectionGlobal.setBounds(cardGlobal.getX() + 244, cardGlobal.getY() + 16, 100, 20);
        // 3 个全局旋钮（品牌区 220 + 标题区之后），d=56，标签在上、数值在下
        int kx = cardGlobal.getX() + 244;
        auto gslot = [&](juce::Slider& s, juce::Label& l)
        {
            auto slot = juce::Rectangle<int>(kx, cardGlobal.getY() + 40, 116, 92);
            l.setBounds(slot.removeFromTop(18));
            s.setBounds(slot.withSizeKeepingCentre(72, kKnobGlob + kTbH));
            kx += 116;
        };
        gslot(inputGainSlider,  inputGainLabel);
        gslot(headroomSlider,   headroomLabel);
        gslot(outputGainSlider, outputGainLabel);
        // 指示灯：卡片右端 40×40
        indicatorRect = juce::Rectangle<int>(cardGlobal.getRight() - 68, cardGlobal.getY() + 50, 40, 40);
    }

    // 信号链卡片行（y=172, h=320，5 卡横排）
    const int chainY = 172, chainH = 320;
    int cx = kEdge;
    auto makeCard = [&](int w)
    {
        auto r = juce::Rectangle<int>(cx, chainY, w, chainH);
        cx += w + kGap;
        return r;
    };
    cardDeEss = makeCard(180);
    cardEq    = makeCard(320);
    cardComp  = makeCard(272);
    cardSat   = makeCard(260);
    cardEdge  = makeCard(152);

    // cardDeEss：2 旋钮并排（垂直居中偏下，平衡顶部标题）
    {
        sectionDeEss.setBounds(cardDeEss.getX() + kPad, cardDeEss.getY() + 16, 110, 20);
        const int slotH = 18 + kKnobStd + kTbH; // 100
        const int kY = cardDeEss.getCentreY() - slotH / 2 + 10;
        auto dslot = [&](juce::Slider& s, juce::Label& l, int x)
        {
            auto slot = juce::Rectangle<int>(x, kY, 72, slotH);
            l.setBounds(slot.removeFromTop(18));
            s.setBounds(slot.withSizeKeepingCentre(72, kKnobStd + kTbH));
        };
        dslot(dsLowSlider,  dsLowLabel,  cardDeEss.getX() + 14);
        dslot(dsHighSlider, dsHighLabel, cardDeEss.getX() + 94);
    }

    // cardEq：两行两列（Thick/De-Box 上行，Clarity/Air 下行）+ lock 标签 + airFreqBox
    {
        sectionEq.setBounds(cardEq.getX() + kPad, cardEq.getY() + 16, 60, 20);
        const int colW  = 128;
        const int col1X = cardEq.getX() + 24, col2X = cardEq.getX() + 168;
        const int row1Y = cardEq.getY() + 44, row2Y = cardEq.getY() + 164;
        auto eslot = [&](juce::Slider& s, juce::Label& l, juce::Label* lock, int sx, int sy)
        {
            auto slot = juce::Rectangle<int>(sx, sy, colW, 111);
            l.setBounds(slot.removeFromTop(16));
            auto lockArea = slot.removeFromTop(13);
            if (lock != nullptr)
                lock->setBounds(lockArea);
            s.setBounds(slot.withSizeKeepingCentre(76, kKnobStd + kTbH));
        };
        eslot(boostSlider,   boostLabel,   &thickFreqLabel,   col1X, row1Y);
        eslot(deboxSlider,   deboxLabel,   &deboxFreqLabel,   col2X, row1Y);
        eslot(claritySlider, clarityLabel, &clarityFreqLabel, col1X, row2Y);
        eslot(airSlider,     airLabel,     nullptr,           col2X, row2Y);
        // Air 频点选择：Air 旋钮下方底部行
        airFreqBox.setBounds(juce::Rectangle<int>(airSlider.getBounds().getCentreX() - 42,
                                                  cardEq.getY() + 282, 84, 22));
    }

    // cardComp：Style 下拉 + Compression hero + Makeup（Makeup 旋钮中心对齐 hero）
    {
        sectionComp.setBounds(cardComp.getX() + kPad, cardComp.getY() + 16, 120, 20);
        compModeBox.setBounds(cardComp.getX() + kPad, cardComp.getY() + 44, 130, 26);
        auto heroSlot = juce::Rectangle<int>(cardComp.getX() + 20, cardComp.getY() + 84, 140, 156);
        compAmountLabel.setBounds(heroSlot.removeFromTop(18));
        compAmountSlider.setBounds(heroSlot.withSizeKeepingCentre(kKnobHero, kKnobHero + kTbH));
        auto mkSlot = juce::Rectangle<int>(cardComp.getX() + 172, cardComp.getY() + 112, 88, 100);
        compMakeupLabel.setBounds(mkSlot.removeFromTop(18));
        compMakeupSlider.setBounds(mkSlot.withSizeKeepingCentre(76, kKnobStd + kTbH));
    }

    // cardSat：A/B 两槽竖向堆叠（类型下拉 → 标签 → 旋钮）
    {
        sectionSat.setBounds(cardSat.getX() + kPad, cardSat.getY() + 16, 120, 20);
        const int centreX = cardSat.getCentreX();
        // A 槽
        satTypeABox.setBounds(juce::Rectangle<int>(centreX - 80, cardSat.getY() + 44, 160, 22));
        satAmountALabel.setBounds(juce::Rectangle<int>(centreX - 64, cardSat.getY() + 72, 128, 18));
        satAmountASlider.setBounds(juce::Rectangle<int>(centreX - 38, cardSat.getY() + 92, 76, kKnobStd + kTbH));
        // B 槽
        satTypeBBox.setBounds(juce::Rectangle<int>(centreX - 80, cardSat.getY() + 178, 160, 22));
        satAmountBLabel.setBounds(juce::Rectangle<int>(centreX - 64, cardSat.getY() + 206, 128, 18));
        satAmountBSlider.setBounds(juce::Rectangle<int>(centreX - 38, cardSat.getY() + 226, 76, kKnobStd + kTbH));
    }

    // cardEdge：1 个 hero 旋钮垂直居中
    {
        sectionEdge.setBounds(cardEdge.getX() + kPad, cardEdge.getY() + 16, 90, 20);
        const int contentH = 18 + kKnobHero + kTbH; // 156
        edgeLabel.setBounds(juce::Rectangle<int>(cardEdge.getX(), cardEdge.getCentreY() - contentH / 2,
                                                 cardEdge.getWidth(), 18));
        edgeSlider.setBounds(juce::Rectangle<int>(cardEdge.getX() + (cardEdge.getWidth() - kKnobHero) / 2,
                                                  edgeLabel.getBottom(), kKnobHero, kKnobHero + kTbH));
    }

    // 底部行（y=508, h=128）：cardMonitor + cardOs
    const int botY = chainY + chainH + kGap, botH = 128;
    cardMonitor = juce::Rectangle<int>(kEdge, botY, 800, botH);
    cardOs      = juce::Rectangle<int>(kEdge + 800 + kGap, botY, getWidth() - (kEdge + 800 + kGap) - kEdge, botH);
    {
        sectionMonitor.setBounds(cardMonitor.getX() + kPad, cardMonitor.getY() + 12, 120, 18);
        // 4 条横条：IN / OUT / COMP / DE-ESS（标签左 52、数值右 70）
        const int rowH = 16, gap = 8;
        const int barX = cardMonitor.getX() + 78;
        const int barW = cardMonitor.getWidth() - 78 - 78;
        int y = cardMonitor.getY() + 34;
        meterInRect  = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        meterOutRect = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        grCompRect   = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        grDeessRect  = juce::Rectangle<int>(barX, y, barW, rowH);
    }
    {
        sectionOs.setBounds(cardOs.getX() + kPad, cardOs.getY() + 12, 110, 18);
        oversamplingLabel.setBounds(juce::Rectangle<int>(cardOs.getX() + 44, cardOs.getY() + 56, 120, 22));
        oversamplingBox.setBounds(juce::Rectangle<int>(cardOs.getX() + 168, cardOs.getY() + 53, 130, 28));
    }
}
