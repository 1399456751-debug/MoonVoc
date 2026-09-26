#include "PluginEditor.h"

namespace
{
    constexpr int kEdge      = 16;   // 外边距
    constexpr int kGap       = 16;   // 卡片间距
    constexpr int kPad       = 20;   // 卡片内边距
    constexpr int kKnobHero  = 120;  // hero 旋钮直径（Compression / Edge）
    constexpr int kKnobStd   = 64;   // 标准旋钮直径
}

MoonVocEditor::MoonVocEditor(MoonVocProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // 主题 LookAndFeel（旋钮/开关/下拉全部自绘）
    lookAndFeel = std::make_unique<MoonVocLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());

    setupSectionTitle(sectionGlobal,  "Global");
    setupSectionTitle(sectionEq,      "EQ");
    setupSectionTitle(sectionComp,    "Compressor");
    setupSectionTitle(sectionDeEss,   "De-Esser");
    setupSectionTitle(sectionSat,     "Saturate");
    setupSectionTitle(sectionEdge,    "Edge");
    setupSectionTitle(sectionMonitor, "Monitor");
    setupSectionTitle(sectionOs,      "Engine");

    // 全局
    setupSlider(inputGainSlider,  inputGainLabel,  "Input");
    setupSlider(headroomSlider,   headroomLabel,   "Headroom");
    setupSlider(outputGainSlider, outputGainLabel, "Output");
    setupCombo(oversamplingBox, { "2x", "4x", "8x", "16x" });
    oversamplingLabel.setText("Oversampling", juce::dontSendNotification);
    oversamplingLabel.setJustificationType(juce::Justification::centredRight);
    oversamplingLabel.setColour(juce::Label::textColourId, Theme::textDim);
    oversamplingLabel.setFont(Theme::fontLabel(12.0f));
    addAndMakeVisible(oversamplingLabel);

    // EQ
    setupSlider(boostSlider,   boostLabel,   "Thick");
    setupSlider(deboxSlider,   deboxLabel,   "De-Box");
    setupSlider(claritySlider, clarityLabel, "Clarity");
    setupSlider(airSlider,     airLabel,     "Air", true);
    deboxFreqLabel.setJustificationType(juce::Justification::centred);
    deboxFreqLabel.setColour(juce::Label::textColourId, Theme::accent);
    deboxFreqLabel.setFont(Theme::fontLabel(11.0f));
    addAndMakeVisible(deboxFreqLabel);
    clarityFreqLabel.setJustificationType(juce::Justification::centred);
    clarityFreqLabel.setColour(juce::Label::textColourId, Theme::accent);
    clarityFreqLabel.setFont(Theme::fontLabel(11.0f));
    addAndMakeVisible(clarityFreqLabel);
    thickFreqLabel.setJustificationType(juce::Justification::centred);
    thickFreqLabel.setColour(juce::Label::textColourId, Theme::accent);
    thickFreqLabel.setFont(Theme::fontLabel(11.0f));
    addAndMakeVisible(thickFreqLabel);
    setupCombo(airFreqBox, { "16 kHz", "22 kHz" });

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

    setSize(1280, 720);
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
    s.setColour(juce::Slider::textBoxHighlightColourId, Theme::accent.withAlpha(0.25f));
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
    l.setJustificationType(juce::Justification::centredLeft);
    l.setColour(juce::Label::textColourId, Theme::textDim.withAlpha(0.95f));
    l.setFont(Theme::fontSection(13.0f));
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

    // 几何专项
    check(std::abs(deboxSlider.getBounds().getCentreX() - boostSlider.getBounds().getCentreX()
                 - (claritySlider.getBounds().getCentreX() - deboxSlider.getBounds().getCentreX())) <= 1,
          "EQ knobs evenly spaced");
    check(boostSlider.getBounds().getCentreY() == airSlider.getBounds().getCentreY(), "EQ knobs same y");
    check(std::abs(airFreqBox.getBounds().getCentreX() - airSlider.getBounds().getCentreX()) <= 1,
          "airFreq centred under air");
    check(compAmountSlider.getBounds().getWidth() == compAmountSlider.getBounds().getHeight()
          && compAmountSlider.getBounds().getWidth() == kKnobHero, "comp hero 120 sq");
    check(edgeSlider.getBounds().getWidth() == edgeSlider.getBounds().getHeight()
          && edgeSlider.getBounds().getWidth() == kKnobHero, "edge hero 120 sq");
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
    g.setFont(Theme::fontLabel(12.0f));
    g.setColour(Theme::textDim);
    g.drawText(name, juce::Rectangle<int>(r.getX() - 44, r.getY(), 40, r.getHeight()),
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
    g.setFont(Theme::fontValue(13.0f));
    g.setColour(Theme::textMain);
    g.drawText(juce::String(levelDb, 1) + " dB",
               juce::Rectangle<int>(r.getRight() + 8, r.getY() - 1, 60, r.getHeight() + 2),
               juce::Justification::centredLeft);
}

// 横向 GR 表（从右往左填，标签左、数值右）
void MoonVocEditor::paintGrBar(juce::Graphics& g, juce::Rectangle<int> r, float grDb,
                               const juce::String& name, juce::Colour col)
{
    g.setFont(Theme::fontLabel(12.0f));
    g.setColour(Theme::textDim);
    g.drawText(name, juce::Rectangle<int>(r.getX() - 44, r.getY(), 40, r.getHeight()),
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

    g.setFont(Theme::fontValue(13.0f));
    g.setColour(Theme::textMain);
    g.drawText(juce::String(grDb, 1) + " dB",
               juce::Rectangle<int>(r.getRight() + 8, r.getY() - 1, 60, r.getHeight() + 2),
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

    g.setFont(Theme::fontLabel(11.0f));
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

    // 卡片：投影 + 白底圆角 + 描边 + 顶部内高光
    auto drawCard = [&](const juce::Rectangle<int>& r)
    {
        if (r.isEmpty())
            return;
        const auto rf = r.toFloat();
        // 投影（向下偏 3、向外扩 2）
        g.setColour(Theme::shadow);
        g.fillRoundedRectangle(rf.translated(0.0f, 3.0f).expanded(2.0f), 16.0f);
        // 卡片体
        g.setColour(Theme::panel);
        g.fillRoundedRectangle(rf, 14.0f);
        // 描边
        g.setColour(Theme::panelEdge);
        g.drawRoundedRectangle(rf, 14.0f, 1.0f);
        // 顶部内高光线
        g.setColour(juce::Colours::white.withAlpha(0.40f));
        g.drawHorizontalLine(r.getY() + 1, (float) (r.getX() + 12), (float) (r.getRight() - 12));
    };
    drawCard(cardGlobal);
    drawCard(cardDeEss);
    drawCard(cardEq);
    drawCard(cardComp);
    drawCard(cardSat);
    drawCard(cardEdge);
    drawCard(cardMonitor);
    drawCard(cardOs);

    // 区标题左侧 teal 小竖条
    auto drawSectionBar = [&](const juce::Label& l)
    {
        auto r = l.getBounds();
        if (r.isEmpty())
            return;
        g.setColour(Theme::accent);
        g.fillRoundedRectangle((float) (r.getX() - 8), (float) (r.getCentreY() - 7), 3.0f, 14.0f, 1.5f);
    };
    drawSectionBar(sectionGlobal);
    drawSectionBar(sectionEq);
    drawSectionBar(sectionComp);
    drawSectionBar(sectionDeEss);
    drawSectionBar(sectionSat);
    drawSectionBar(sectionEdge);
    drawSectionBar(sectionMonitor);
    drawSectionBar(sectionOs);

    // 品牌：顶部全局条内左侧 MoonVoc（青→珊瑚渐变）+ TUJZMIXING
    {
        const auto brand = cardGlobal.reduced(24, 0);
        const auto titleBox = brand.withWidth(220).withHeight(34).withY(cardGlobal.getY() + 40);
        auto titleFont = Theme::fontTitle(30.0f);
        juce::ColourGradient titleGrad(Theme::accent, (float) titleBox.getX(), 0.0f,
                                       Theme::accent2, (float) titleBox.getRight(), 0.0f, false);
        g.setFont(titleFont);
        g.setGradientFill(titleGrad);
        g.drawText("MoonVoc", titleBox, juce::Justification::centredLeft);
        const auto subBox = titleBox.translated(0.0f, 32.0f).withHeight(16);
        g.setFont(Theme::fontLabel(11.0f));
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
    auto font = Theme::fontSection(14.0f);
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

    // 顶部全局条卡片（y=16, h=128）
    cardGlobal = juce::Rectangle<int>(kEdge, kEdge, getWidth() - kEdge * 2, 128);
    {
        sectionGlobal.setBounds(cardGlobal.getX() + 236, cardGlobal.getY() + 16, 90, 18);
        // 3 个全局旋钮（品牌区 220 + 标题区之后），d=64，标签在上
        const int knobY = cardGlobal.getY() + 42;
        int kx = cardGlobal.getX() + 236;
        auto gslot = [&](juce::Slider& s, juce::Label& l)
        {
            auto slot = juce::Rectangle<int>(kx, knobY, 120, cardGlobal.getBottom() - knobY - 12);
            l.setBounds(slot.removeFromTop(16));
            s.setBounds(slot.withSizeKeepingCentre(kKnobStd, kKnobStd));
            kx += 120;
        };
        gslot(inputGainSlider,  inputGainLabel);
        gslot(headroomSlider,   headroomLabel);
        gslot(outputGainSlider, outputGainLabel);
        // 指示灯：卡片右端 44×44
        indicatorRect = juce::Rectangle<int>(cardGlobal.getRight() - 68, cardGlobal.getY() + 44, 40, 40);
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

    // cardDeEss：2 旋钮并排（垂直居中）
    {
        sectionDeEss.setBounds(cardDeEss.getX() + kPad, cardDeEss.getY() + 16, 110, 18);
        const int kY = cardDeEss.getCentreY() - 52;
        auto dslot = [&](juce::Slider& s, juce::Label& l, int x)
        {
            auto slot = juce::Rectangle<int>(x, kY, 76, 120);
            l.setBounds(slot.removeFromTop(16));
            s.setBounds(slot.withSizeKeepingCentre(kKnobStd, kKnobStd));
        };
        dslot(dsLowSlider,  dsLowLabel,  cardDeEss.getX() + 16);
        dslot(dsHighSlider, dsHighLabel, cardDeEss.getX() + 92);
    }

    // cardEq：4 旋钮等距 + lock 标签 + airFreqBox（旋钮群上移，下拉框独立底部行）
    {
        sectionEq.setBounds(cardEq.getX() + kPad, cardEq.getY() + 16, 60, 18);
        const int kY = cardEq.getY() + 56;
        const int sw = 76;
        int ex = cardEq.getX() + 16;
        auto eslot = [&](juce::Slider& s, juce::Label& l, juce::Label* lock)
        {
            auto slot = juce::Rectangle<int>(ex, kY, sw, 130);
            l.setBounds(slot.removeFromTop(16));
            auto lockArea = slot.removeFromTop(14);
            if (lock != nullptr)
                lock->setBounds(lockArea);
            s.setBounds(slot.withSizeKeepingCentre(kKnobStd, kKnobStd));
            ex += sw;
        };
        eslot(boostSlider,   boostLabel,   &thickFreqLabel);
        eslot(deboxSlider,   deboxLabel,   &deboxFreqLabel);
        eslot(claritySlider, clarityLabel, &clarityFreqLabel);
        eslot(airSlider,     airLabel,     nullptr);
        // Air 频点选择：Air 旋钮下方独立底部行（避开数值框）
        airFreqBox.setBounds(juce::Rectangle<int>(airSlider.getBounds().getCentreX() - 36,
                                                  cardEq.getY() + 200, 72, 22));
    }

    // cardComp：Style 下拉 + Compression hero + Makeup（垂直居中）
    {
        sectionComp.setBounds(cardComp.getX() + kPad, cardComp.getY() + 16, 110, 18);
        compModeBox.setBounds(cardComp.getX() + kPad, cardComp.getY() + 46, 120, 26);
        const int kY = cardComp.getCentreY() - 78;
        auto heroSlot = juce::Rectangle<int>(cardComp.getX() + 20, kY, 140, 190);
        compAmountLabel.setBounds(heroSlot.removeFromTop(16));
        compAmountSlider.setBounds(heroSlot.withSizeKeepingCentre(kKnobHero, kKnobHero));
        auto mkSlot = juce::Rectangle<int>(cardComp.getX() + 172, kY + 40, 84, 120);
        compMakeupLabel.setBounds(mkSlot.removeFromTop(16));
        compMakeupSlider.setBounds(mkSlot.withSizeKeepingCentre(kKnobStd, kKnobStd));
    }

    // cardSat：A/B 两槽竖向堆叠（垂直居中）
    {
        sectionSat.setBounds(cardSat.getX() + kPad, cardSat.getY() + 16, 120, 18);
        const int centreX = cardSat.getX() + cardSat.getWidth() / 2;
        const int topY = cardSat.getCentreY() - 112;
        // A 槽
        satTypeABox.setBounds(juce::Rectangle<int>(centreX - 75, topY, 150, 24));
        satAmountALabel.setBounds(juce::Rectangle<int>(centreX - 60, topY + 32, 120, 16));
        satAmountASlider.setBounds(juce::Rectangle<int>(centreX - kKnobStd / 2, topY + 50, kKnobStd, kKnobStd));
        // B 槽
        satTypeBBox.setBounds(juce::Rectangle<int>(centreX - 75, topY + 118, 150, 24));
        satAmountBLabel.setBounds(juce::Rectangle<int>(centreX - 60, topY + 150, 120, 16));
        satAmountBSlider.setBounds(juce::Rectangle<int>(centreX - kKnobStd / 2, topY + 168, kKnobStd, kKnobStd));
    }

    // cardEdge：1 个 hero 旋钮垂直居中
    {
        sectionEdge.setBounds(cardEdge.getX() + kPad, cardEdge.getY() + 16, 90, 18);
        edgeLabel.setBounds(juce::Rectangle<int>(cardEdge.getX(), cardEdge.getCentreY() - 74, cardEdge.getWidth(), 16));
        edgeSlider.setBounds(juce::Rectangle<int>(cardEdge.getX() + (cardEdge.getWidth() - kKnobHero) / 2,
                                                  cardEdge.getCentreY() - 56, kKnobHero, kKnobHero));
    }

    // 底部行（y=506, h=128）：cardMonitor + cardOs
    const int botY = chainY + chainH + kGap, botH = 128;
    cardMonitor = juce::Rectangle<int>(kEdge, botY, 800, botH);
    cardOs      = juce::Rectangle<int>(kEdge + 800 + kGap, botY, getWidth() - (kEdge + 800 + kGap) - kEdge, botH);
    {
        sectionMonitor.setBounds(cardMonitor.getX() + kPad, cardMonitor.getY() + 12, 110, 16);
        // 4 条横条：IN / OUT / COMP / DE-ESS（数值留右侧 66）
        const int rowH = 14, gap = 10;
        const int barX = cardMonitor.getX() + 66;
        const int barW = cardMonitor.getWidth() - 66 - 74;
        int y = cardMonitor.getY() + 36;
        meterInRect  = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        meterOutRect = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        grCompRect   = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        grDeessRect  = juce::Rectangle<int>(barX, y, barW, rowH);
    }
    {
        sectionOs.setBounds(cardOs.getX() + kPad, cardOs.getY() + 12, 110, 16);
        const int oy = cardOs.getY() + 56;
        oversamplingLabel.setBounds(juce::Rectangle<int>(cardOs.getX() + 44, oy, 110, 20));
        oversamplingBox.setBounds(juce::Rectangle<int>(cardOs.getX() + 160, oy - 3, 120, 26));
    }
}
