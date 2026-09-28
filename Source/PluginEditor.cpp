#include "PluginEditor.h"
#include "ParamIDs.h"

namespace
{
    constexpr int kEdge      = 16;   // 外边距
    constexpr int kGap       = 16;   // 卡片间距
    constexpr int kPad       = 20;   // 卡片内边距
    constexpr int kKnobHero  = 132;  // hero 旋钮直径（Compression / Edge / Reverb）
    constexpr int kKnobStd   = 64;   // 标准旋钮直径
    constexpr int kKnobGlob  = 56;   // 全局条旋钮直径
    constexpr int kTbH       = 18;   // 旋钮下方数值框条高（含在 slider bounds 内）
    constexpr int kDesignW   = MoonVocEditor::kDesignW;
    constexpr int kDesignH   = MoonVocEditor::kDesignH;
}

MoonVocEditor::MoonVocEditor(MoonVocProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // 主题 LookAndFeel（旋钮/开关/下拉全部自绘）
    lookAndFeel = std::make_unique<MoonVocLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());
    addAndMakeVisible(canvas);

    // 自由缩放：窗口可拖拽，尺寸范围 = 设计尺寸 100%~300%
    setResizable(true, true);
    setResizeLimits(kDesignW, kDesignH, kDesignW * 3, kDesignH * 3);

    setupSectionTitle(sectionGlobal,  Strings::kGlobal,   Theme::cardGlobalDeep);
    setupSectionTitle(sectionEq,      Strings::kEq,       Theme::cardEqDeep);
    setupSectionTitle(sectionComp,    Strings::kComp,     Theme::cardCompDeep);
    setupSectionTitle(sectionDeEss,   Strings::kDeEss,    Theme::cardDeEssDeep);
    setupSectionTitle(sectionReverb,  Strings::kReverb,   Theme::cardReverbDeep);
    setupSectionTitle(sectionSat,     Strings::kSat,      Theme::cardSatDeep);
    setupSectionTitle(sectionEdge,    Strings::kEdge,     Theme::cardEdgeDeep);
    setupSectionTitle(sectionMonitor, Strings::kMonitor,  Theme::cardMonitorDeep);
    setupSectionTitle(sectionOs,      Strings::kEngine,   Theme::cardEngineDeep);
    setupSectionTitle(sectionSettings, Strings::kSettings, Theme::cardGlobalDeep);

    // 全局
    setupSlider(inputGainSlider,  inputGainLabel,  Strings::kInput,    Theme::cardGlobalDeep);
    setupSlider(headroomSlider,   headroomLabel,   Strings::kHeadroom, Theme::cardGlobalDeep);
    setupSlider(outputGainSlider, outputGainLabel, Strings::kOutput,   Theme::cardGlobalDeep);
    setupCombo(oversamplingBox, { "2x", "4x", "8x", "16x" });
    registerText(oversamplingLabel, Strings::kOversampling, 14.0f, 0);
    oversamplingLabel.setJustificationType(juce::Justification::centredRight);
    oversamplingLabel.setColour(juce::Label::textColourId, Theme::dimColour());
    oversamplingLabel.setFont(Theme::fontLabel(14.0f));
    canvas.addAndMakeVisible(oversamplingLabel);

    // EQ
    setupSlider(boostSlider,   boostLabel,   Strings::kThick,   Theme::cardEqDeep);
    setupSlider(deboxSlider,   deboxLabel,   Strings::kDebox,   Theme::cardEqDeep);
    setupSlider(claritySlider, clarityLabel, Strings::kClarity, Theme::cardEqDeep);
    setupSlider(airSlider,     airLabel,     Strings::kAir,     Theme::cardEqDeep);
    for (auto* l : { &deboxFreqLabel, &clarityFreqLabel, &thickFreqLabel })
    {
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, Theme::cardEqDeep.darker(0.10f));
        l->setFont(Theme::fontLabel(12.0f));
        canvas.addAndMakeVisible(*l);
    }
    // Air 频点：两个分段小按钮（Satin 16k / Nimbus 22k）。状态完全由参数驱动，
    // 不自管 toggle（避免视觉状态与参数不一致）
    for (auto* b : { &airSatinBtn, &airNimbusBtn })
    {
        b->setClickingTogglesState(false);
        b->setRadioGroupId(0x41);
        b->getProperties().set("moonvocArcColor", (juce::int64) Theme::cardEqDeep.getARGB());
        canvas.addAndMakeVisible(*b);
    }

    // 压缩
    setupCombo(compModeBox, { "Pop", "Rap" });
    setupSlider(compAmountSlider, compAmountLabel, Strings::kCompression, Theme::cardCompDeep);
    setupSlider(compMakeupSlider, compMakeupLabel, Strings::kMakeup,      Theme::cardCompDeep);

    // 去齿音（链路位于压缩后）
    setupSlider(dsAmountSlider, dsAmountLabel, Strings::kDeEssAmount, Theme::cardDeEssDeep);
    setupSlider(dsFocusSlider,  dsFocusLabel,  Strings::kDeEssFocus,  Theme::cardDeEssDeep);

    // 混响（链路最后）
    setupSlider(reverbSlider, reverbLabel, Strings::kReverbAmt, Theme::cardReverbDeep);
    setupCombo(reverbModeBox, { "Pop", "Rap" });

    // 染色
    const juce::StringArray satTypes{ "Off", "FET", "Tube", "Tape", "Optical", "Germanium" };
    setupCombo(satTypeABox, satTypes);
    setupSlider(satAmountASlider, satAmountALabel, Strings::kDriveA, Theme::cardSatDeep);
    setupCombo(satTypeBBox, satTypes);
    setupSlider(satAmountBSlider, satAmountBLabel, Strings::kDriveB, Theme::cardSatDeep);

    // 瞬态
    setupSlider(edgeSlider, edgeLabel, Strings::kEdge, Theme::cardEdgeDeep);

    // 旁通开关（每模块一个；文字由 applyLanguage 刷新）
    const juce::String bypText = Strings::get(Strings::kBypass, true);
    setupButton(eqBypassBtn,     bypText);
    setupButton(compBypassBtn,   bypText);
    setupButton(deEssBypassBtn,  bypText);
    setupButton(satBypassBtn,    bypText);
    setupButton(edgeBypassBtn,   bypText);
    setupButton(reverbBypassBtn, bypText);

    // 设置：语言 / 大字 / 缩放
    setupCombo(langBox, { S8("中文"), "English" });
    // 9 档正好对齐 uiScale 范围 1.0~3.0 步进 0.25（ComboBoxAttachment 按整段归一化映射）
    setupCombo(scaleBox, { "100%", "125%", "150%", "175%", "200%", "225%", "250%", "275%", "300%" });
    setupButton(largeFontBtn, Strings::get(Strings::kLargeFont, true));
    langLabel.setJustificationType(juce::Justification::centred);
    langLabel.setColour(juce::Label::textColourId, Theme::dimColour());
    langLabel.setFont(Theme::fontLabel(12.0f));
    canvas.addAndMakeVisible(langLabel);
    scaleLabel.setJustificationType(juce::Justification::centred);
    scaleLabel.setColour(juce::Label::textColourId, Theme::dimColour());
    scaleLabel.setFont(Theme::fontLabel(12.0f));
    canvas.addAndMakeVisible(scaleLabel);

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
    // Air 频点：choice 参数（0=Satin 16k / 1=Nimbus 22k）驱动两个分段按钮
    airFreqAtt = std::make_unique<juce::ParameterAttachment>(
        *processorRef.apvts.getParameter(ParamID::eqAirFreq),
        [this](float v)
        {
            const bool nimbus = v >= 0.5f;
            airSatinBtn.setToggleState(! nimbus, juce::dontSendNotification);
            airNimbusBtn.setToggleState(nimbus, juce::dontSendNotification);
        });
    airFreqAtt->sendInitialUpdate();

    airSatinBtn.onClick  = [this] { airFreqAtt->setValueAsCompleteGesture(0.0f); };
    airNimbusBtn.onClick = [this] { airFreqAtt->setValueAsCompleteGesture(1.0f); };

    // 压缩
    compModeAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "compMode", compModeBox);
    compAmountAtt  = std::make_unique<SliderAttachment>(processorRef.apvts, "compAmount", compAmountSlider);
    compMakeupAtt  = std::make_unique<SliderAttachment>(processorRef.apvts, "compMakeup", compMakeupSlider);

    // 去齿音
    dsAmountAtt = std::make_unique<SliderAttachment>(processorRef.apvts, "dsAmount", dsAmountSlider);
    dsFocusAtt  = std::make_unique<SliderAttachment>(processorRef.apvts, "dsFocus",  dsFocusSlider);

    // 混响
    reverbAmountAtt = std::make_unique<SliderAttachment>(processorRef.apvts, "reverbAmount", reverbSlider);
    reverbModeAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "reverbMode", reverbModeBox);

    // 染色
    satTypeAAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "satTypeA", satTypeABox);
    satAmountAAtt = std::make_unique<SliderAttachment>(processorRef.apvts, "satAmountA", satAmountASlider);
    satTypeBAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "satTypeB", satTypeBBox);
    satAmountBAtt = std::make_unique<SliderAttachment>(processorRef.apvts, "satAmountB", satAmountBSlider);

    // 瞬态
    edgeAtt    = std::make_unique<SliderAttachment>(processorRef.apvts, "edgeAmount", edgeSlider);

    // 旁通 + 设置
    eqBypassAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, "eqBypass",     eqBypassBtn);
    compBypassAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, "compBypass",   compBypassBtn);
    deEssBypassAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, "deEssBypass",  deEssBypassBtn);
    satBypassAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, "satBypass",    satBypassBtn);
    edgeBypassAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, "edgeBypass",   edgeBypassBtn);
    reverbBypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, "reverbBypass", reverbBypassBtn);
    langAtt      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "uiLanguage", langBox);
    scaleAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "uiScale", scaleBox);
    largeFontAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, "uiLargeFont", largeFontBtn);

    // 初始状态：语言 / 字体 / 缩放 + 窗口尺寸
    applyLanguage();
    applyFontMode();
    applyScaleFromParam();

    // 关于浮层 + ⓘ 入口。加入顺序不影响层级：open() 里会 toFront() 盖到所有控件之上
    infoBadge.onClick = [this] { openAbout(); };
    canvas.addAndMakeVisible (infoBadge);
    // 浮层必须用 addChildComponent：addAndMakeVisible 会覆盖构造函数里的 setVisible(false)，
    // 导致浮层开机即显示（isOpen() 就是 isVisible()），四张既有截图会被整片盖住
    canvas.addChildComponent (aboutOverlay);

    startTimerHz(10);
}

// 文本注册：语言/字体切换时统一刷新
void MoonVocEditor::registerText(juce::Label& l, Strings::Key key, float base, int kind)
{
    textEntries.push_back({ &l, key, base, kind });
}

void MoonVocEditor::setupSlider(juce::Slider& s, juce::Label& l, Strings::Key key, juce::Colour arcColour)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, kTbH);
    s.setTextValueSuffix("");
    s.setDoubleClickReturnValue(true, s.getValue());
    // 拖动阻尼：默认 250px 拖满整个取值范围，手感过滑；500 = 拖满需 500px。
    // 注意 juce::Slider 的滚轮步进用同一常量换算，滚轮也会同比变稳（预期行为）
    s.setMouseDragSensitivity (500);
    s.setColour(juce::Slider::textBoxTextColourId, Theme::textMain);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxHighlightColourId, arcColour.withAlpha(0.25f));
    // 模块主题色：LAF 画数值弧/指针/轨道环时读取
    s.getProperties().set("moonvocArcColor", (juce::int64) arcColour.getARGB());
    canvas.addAndMakeVisible(s);

    registerText(l, key, 14.0f, 0);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, Theme::textMain);
    l.setFont(Theme::fontLabel());
    canvas.addAndMakeVisible(l);
}

void MoonVocEditor::setupCombo(juce::ComboBox& c, const juce::StringArray& items)
{
    c.addItemList(items, 1);
    c.setColour(juce::ComboBox::textColourId, Theme::textMain);
    c.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    c.setColour(juce::ComboBox::arrowColourId, Theme::accent);
    canvas.addAndMakeVisible(c);
}

void MoonVocEditor::setupButton(juce::ToggleButton& b, const juce::String& text)
{
    b.setButtonText(text);
    b.setColour(juce::ToggleButton::textColourId, Theme::textMain);
    canvas.addAndMakeVisible(b);
}

void MoonVocEditor::setupSectionTitle(juce::Label& l, Strings::Key key, juce::Colour deep)
{
    registerText(l, key, 15.0f, 1);
    l.setJustificationType(juce::Justification::centredLeft);
    l.setColour(juce::Label::textColourId, deep.darker(0.25f));
    l.setFont(Theme::fontSection(15.0f));
    canvas.addAndMakeVisible(l);
}

// 语言切换：刷新全部注册文本 + 下拉项（保持当前选中）
void MoonVocEditor::applyLanguage()
{
    currentZh = processorRef.apvts.getRawParameterValue(ParamID::uiLanguage)->load() < 0.5f;
    const bool zh = currentZh;
    Theme::useCjkFont = zh; // 中文用系统 CJK 字体（Montserrat 无中文字形）

    // 字体随语言切换重建
    for (auto& e : textEntries)
        e.label->setFont(e.kind == 1 ? Theme::fontSection(e.base) : Theme::fontLabel(e.base));
    oversamplingLabel.setFont(Theme::fontLabel(14.0f));
    for (auto* l : { &deboxFreqLabel, &clarityFreqLabel, &thickFreqLabel })
        l->setFont(Theme::fontLabel(12.0f));
    langLabel.setFont(Theme::fontLabel(12.0f));
    scaleLabel.setFont(Theme::fontLabel(12.0f));
    canvas.sendLookAndFeelChange();

    for (auto& e : textEntries)
        e.label->setText(Strings::get(e.key, zh), juce::dontSendNotification);

    auto setItems = [&](juce::ComboBox& c, const juce::StringArray& items)
    {
        const int sel = jmax(1, c.getSelectedId());
        c.clear(juce::dontSendNotification);
        c.addItemList(items, 1);
        c.setSelectedId(sel, juce::dontSendNotification);
    };
    setItems(compModeBox,   { Strings::get(Strings::kStylePop, zh), Strings::get(Strings::kStyleRap, zh) });
    setItems(reverbModeBox, { Strings::get(Strings::kModePop, zh),  Strings::get(Strings::kModeRap, zh) });
    const juce::StringArray satTypes = zh ? juce::StringArray{ S8("关"), "FET", S8("电子管"), S8("磁带"), S8("光电"), S8("锗管") }
                                          : juce::StringArray{ "Off", "FET", "Tube", "Tape", "Optical", "Germanium" };
    setItems(satTypeABox, satTypes);
    setItems(satTypeBBox, satTypes);
    airSatinBtn.setButtonText(Strings::get(Strings::kAirSatin, zh));
    airNimbusBtn.setButtonText(Strings::get(Strings::kAirNimbus, zh));

    const juce::String byp = Strings::get(Strings::kBypass, zh);
    for (auto* b : { &eqBypassBtn, &compBypassBtn, &deEssBypassBtn, &satBypassBtn, &edgeBypassBtn, &reverbBypassBtn })
        b->setButtonText(byp);
    largeFontBtn.setButtonText(Strings::get(Strings::kLargeFont, zh));

    aboutOverlay.setLanguage (zh);

    canvas.repaint();
}

// 老年大字模式：ExtraBold + 字号 ×1.4 + 高对比文字色
void MoonVocEditor::applyFontMode()
{
    Theme::largeFontMode = processorRef.apvts.getRawParameterValue(ParamID::uiLargeFont)->load() > 0.5f;

    for (auto& e : textEntries)
        e.label->setFont(e.kind == 1 ? Theme::fontSection(e.base) : Theme::fontLabel(e.base));

    oversamplingLabel.setFont(Theme::fontLabel(14.0f));
    for (auto* l : { &deboxFreqLabel, &clarityFreqLabel, &thickFreqLabel })
        l->setFont(Theme::fontLabel(12.0f));
    langLabel.setFont(Theme::fontLabel(12.0f));
    scaleLabel.setFont(Theme::fontLabel(12.0f));

    // 文字色：老年模式高对比（dim → 深炭）
    oversamplingLabel.setColour(juce::Label::textColourId, Theme::dimColour());
    langLabel.setColour(juce::Label::textColourId, Theme::dimColour());
    scaleLabel.setColour(juce::Label::textColourId, Theme::dimColour());

    // 旋钮数值框字体由 LAF 提供，通知全树刷新；字号变了要重排（文字容器宽度随字号缩放）
    canvas.sendLookAndFeelChange();
    canvas.resized();
    canvas.repaint();
}

// 缩放（参数 → 窗口）：窗口尺寸 = 设计尺寸 × uiScale
void MoonVocEditor::applyScaleFromParam()
{
    const float s = juce::jlimit(1.0f, 3.0f, processorRef.apvts.getRawParameterValue(ParamID::uiScale)->load());
    appliedScale = s;
    settingScale = true;
    setSize((int) std::lround(kDesignW * s), (int) std::lround(kDesignH * s));
    settingScale = false;
}

void MoonVocEditor::openAbout (bool animate)
{
    aboutOverlay.setLanguage (currentZh);
    aboutOverlay.open (animate);
}

// 编辑器本体：只负责把 Canvas 摆好并按缩放施加 transform（JUCE 推荐做法）
void MoonVocEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::bg);
}

void MoonVocEditor::resized()
{
    const float s = (float) getWidth() / (float) kDesignW;
    currentScale = s;
    canvas.setBounds(0, 0, kDesignW, kDesignH);
    canvas.setTransform(juce::AffineTransform::scale(s));

    // 用户拖拽窗口（非参数驱动）→ 把缩放写回 uiScale 参数，保存状态后下次打开还原
    if (! settingScale && std::abs(s - appliedScale) > 0.005f)
    {
        appliedScale = s;
        if (auto* prm = processorRef.apvts.getParameter(ParamID::uiScale))
            prm->setValueNotifyingHost(prm->convertTo0to1(juce::jlimit(1.0f, 3.0f, s)));
    }
}

void MoonVocEditor::timerCallback()
{
    // 设置参数轮询（预设加载/撤销也会走到这里）
    {
        const bool zh = processorRef.apvts.getRawParameterValue(ParamID::uiLanguage)->load() < 0.5f;
        if (zh != currentZh)
            applyLanguage();

        const bool big = processorRef.apvts.getRawParameterValue(ParamID::uiLargeFont)->load() > 0.5f;
        if (big != Theme::largeFontMode)
            applyFontMode();

        const float s = processorRef.apvts.getRawParameterValue(ParamID::uiScale)->load();
        if (std::abs(s * (float) kDesignW - (float) getWidth()) > 1.0f)
            applyScaleFromParam();
    }

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
    const juce::String lockPrefix = currentZh ? S8("锁定 ") : juce::String("lock: ");
    deboxFreqLabel.setText(lockPrefix + (db.isEmpty() ? juce::String("400") : db) + " Hz",
                           juce::dontSendNotification);
    thickFreqLabel.setText(lockPrefix + juce::String((int) processorRef.getEqThickFreq()) + " Hz",
                           juce::dontSendNotification);
    clarityFreqLabel.setText(lockPrefix + (cl.isEmpty() ? juce::String("4000") : cl) + " Hz",
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
    const int w = kDesignW, h = kDesignH;
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

    // 毛玻璃底：1/4 降采样 → 高斯模糊 → 放回原尺寸。
    // 只在 renderBackground（resized）时算一次，拖旋钮/重绘零开销。
    {
        const int sw = jmax(1, w / 4), sh = jmax(1, h / 4);
        juce::Image small(juce::Image::ARGB, sw, sh, true);
        {
            juce::Graphics sg(small);
            sg.drawImage(bgCache, juce::Rectangle<int>(0, 0, sw, sh).toFloat(),
                         juce::RectanglePlacement::stretchToFit);
        }
        juce::ImageConvolutionKernel kernel(17);
        kernel.createGaussianBlur(8.0f);
        juce::Image blurred(juce::Image::ARGB, sw, sh, true);
        kernel.applyToImage(blurred, small, juce::Rectangle<int>(0, 0, sw, sh));

        bgBlurCache = juce::Image(juce::Image::ARGB, w, h, true);
        juce::Graphics bg(bgBlurCache);
        bg.drawImage(blurred, juce::Rectangle<int>(0, 0, w, h).toFloat(),
                     juce::RectanglePlacement::stretchToFit);
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
    print("cardEq",      cardEq);
    print("cardComp",    cardComp);
    print("cardDeEss",   cardDeEss);
    print("cardSat",     cardSat);
    print("cardEdge",    cardEdge);
    print("cardReverb",  cardReverb);
    print("cardMonitor", cardMonitor);
    print("cardOs",      cardOs);

    int fail = 0;
    auto check = [&](bool ok, const char* what)
    {
        if (! ok) { std::printf("FAIL: %s\n", what); ++fail; }
    };

    // 卡片序与间距：6 卡横排 y/h 全等、相邻间距全 16、混响在最右
    check(cardEq.getY() == cardComp.getY() && cardComp.getY() == cardDeEss.getY()
          && cardDeEss.getY() == cardSat.getY() && cardSat.getY() == cardEdge.getY()
          && cardEdge.getY() == cardReverb.getY(), "6 cards y equal");
    check(cardEq.getHeight() == cardComp.getHeight() && cardComp.getHeight() == cardDeEss.getHeight()
          && cardDeEss.getHeight() == cardSat.getHeight() && cardSat.getHeight() == cardEdge.getHeight()
          && cardEdge.getHeight() == cardReverb.getHeight(), "6 cards h equal");
    check(cardComp.getX() - cardEq.getRight() == kGap, "gap eq-comp==16");
    check(cardDeEss.getX() - cardComp.getRight() == kGap, "gap comp-deess==16");
    check(cardSat.getX() - cardDeEss.getRight() == kGap, "gap deess-sat==16");
    check(cardEdge.getX() - cardSat.getRight() == kGap, "gap sat-edge==16");
    check(cardReverb.getX() - cardEdge.getRight() == kGap, "gap edge-reverb==16");
    check(cardReverb.getRight() <= kDesignW - kEdge, "chain fits width");
    check(kDesignH == 672, "design height 672");
    check(cardMonitor.getBottom() + kGap + 20 <= kDesignH, "footer fits");
    // 卡片 rect %4==0（4px 网格）
    for (auto* c : { &cardGlobal, &cardEq, &cardComp, &cardDeEss, &cardSat, &cardEdge, &cardReverb, &cardMonitor, &cardOs })
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
    owned(cardEq, "boostSlider", boostSlider.getBounds());
    owned(cardEq, "deboxSlider", deboxSlider.getBounds());
    owned(cardEq, "claritySlider", claritySlider.getBounds());
    owned(cardEq, "airSlider", airSlider.getBounds());
    owned(cardEq, "airSatinBtn", airSatinBtn.getBounds());
    owned(cardEq, "airNimbusBtn", airNimbusBtn.getBounds());
    owned(cardComp, "compModeBox", compModeBox.getBounds());
    owned(cardComp, "compAmountSlider", compAmountSlider.getBounds());
    owned(cardComp, "compMakeupSlider", compMakeupSlider.getBounds());
    owned(cardDeEss, "dsAmountSlider", dsAmountSlider.getBounds());
    owned(cardDeEss, "dsFocusSlider", dsFocusSlider.getBounds());
    owned(cardDeEss, "deEssBypassBtn", deEssBypassBtn.getBounds());
    owned(cardSat, "satTypeABox", satTypeABox.getBounds());
    owned(cardSat, "satAmountASlider", satAmountASlider.getBounds());
    owned(cardSat, "satTypeBBox", satTypeBBox.getBounds());
    owned(cardSat, "satAmountBSlider", satAmountBSlider.getBounds());
    owned(cardEdge, "edgeSlider", edgeSlider.getBounds());
    owned(cardReverb, "reverbSlider", reverbSlider.getBounds());
    owned(cardReverb, "reverbModeBox", reverbModeBox.getBounds());
    // 旁通开关归属 + 设置控件归属
    owned(cardEq, "eqBypassBtn", eqBypassBtn.getBounds());
    owned(cardComp, "compBypassBtn", compBypassBtn.getBounds());
    owned(cardSat, "satBypassBtn", satBypassBtn.getBounds());
    owned(cardEdge, "edgeBypassBtn", edgeBypassBtn.getBounds());
    owned(cardReverb, "reverbBypassBtn", reverbBypassBtn.getBounds());
    owned(cardGlobal, "langBox", langBox.getBounds());
    owned(cardGlobal, "scaleBox", scaleBox.getBounds());
    owned(cardGlobal, "largeFontBtn", largeFontBtn.getBounds());
    owned(cardGlobal, "sectionSettings", sectionSettings.getBounds());
    owned(cardMonitor, "meterInRect", meterInRect);
    owned(cardMonitor, "meterOutRect", meterOutRect);
    owned(cardMonitor, "grCompRect", grCompRect);
    owned(cardMonitor, "grDeEssRect", grDeEssRect);
    owned(cardOs, "oversamplingBox", oversamplingBox.getBounds());

    // 几何专项：EQ 两行两列（Thick/De-Box 上行，Clarity/Air 下行）
    check(boostSlider.getY() == deboxSlider.getY(), "EQ row1 same y");
    check(claritySlider.getY() == airSlider.getY(), "EQ row2 same y");
    check(claritySlider.getY() > boostSlider.getY(), "EQ row2 below row1");
    check(boostSlider.getBounds().getCentreX() == claritySlider.getBounds().getCentreX(), "EQ col1 aligned");
    check(deboxSlider.getBounds().getCentreX() == airSlider.getBounds().getCentreX(), "EQ col2 aligned");
    check(std::abs((airSatinBtn.getBounds().getX() + airNimbusBtn.getBounds().getRight()) / 2
                   - airSlider.getBounds().getCentreX()) <= 1,
          "air freq buttons centred under air");
    check(airSatinBtn.getBounds().getWidth() == airNimbusBtn.getBounds().getWidth(),
          "air freq buttons equal width");
    check(compAmountSlider.getBounds().getWidth() == kKnobHero
          && compAmountSlider.getBounds().getHeight() == kKnobHero + kTbH, "comp hero size");
    check(edgeSlider.getBounds().getWidth() == kKnobHero
          && edgeSlider.getBounds().getHeight() == kKnobHero + kTbH, "edge hero size");
    check(reverbSlider.getBounds().getWidth() == kKnobHero
          && reverbSlider.getBounds().getHeight() == kKnobHero + kTbH, "reverb hero size");
    check(dsAmountSlider.getBounds().getWidth() == dsFocusSlider.getBounds().getWidth(),
          "deess knobs equal width");
    check(dsAmountSlider.getBounds().getY() == dsFocusSlider.getBounds().getY(),
          "deess knobs same y");
    check(std::abs(reverbModeBox.getBounds().getCentreX() - reverbSlider.getBounds().getCentreX()) <= 1,
          "reverbMode centred over reverb");
    check(std::abs(satTypeABox.getBounds().getCentreX() - satAmountASlider.getBounds().getCentreX()) <= 1,
          "satA aligned");
    check(std::abs(satTypeBBox.getBounds().getCentreX() - satAmountBSlider.getBounds().getCentreX()) <= 1,
          "satB aligned");
    check(meterInRect.getY() < meterOutRect.getY() && meterOutRect.getY() < grCompRect.getY()
          && grCompRect.getY() < grDeEssRect.getY(), "meter rows ascending (4)");
    check(meterInRect.getWidth() == meterOutRect.getWidth() && meterOutRect.getWidth() == grCompRect.getWidth()
          && grCompRect.getWidth() == grDeEssRect.getWidth(), "meter widths equal");

    // ⓘ 入口：必须在全局卡内、不压到右端指示灯、且真的接上了开合
    owned (cardGlobal, "infoBadge", infoBadge.getBounds());
    check (! infoBadge.getBounds().intersects (indicatorRect), "infoBadge clear of indicator");
    check (infoBadge.onClick != nullptr, "info badge wired to about overlay");

    // 关于浮层：覆盖画布、卡片不越界、两栏与文字块不重叠
    check (aboutOverlay.getBounds() == juce::Rectangle<int> (0, 0, kDesignW, kDesignH),
           "about overlay covers canvas");
    check (juce::Rectangle<int> (0, 0, kDesignW, kDesignH).contains (aboutOverlay.getCardBounds()),
           "about card inside canvas");
    check (aboutOverlay.getRightColumnBounds().getX() >= aboutOverlay.getImageBounds().getRight(),
           "about columns do not overlap");
    {
        const auto& bl = aboutOverlay.getBlocks();
        for (size_t i = 0; i < bl.size(); ++i)
            for (size_t j = i + 1; j < bl.size(); ++j)
                check (! bl[i].bounds.intersects (bl[j].bounds), "about text blocks do not overlap");
    }

    // 旋钮阻尼：全部 15 个旋钮统一 500（默认 250 太滑）
    for (auto* s : { &inputGainSlider, &headroomSlider, &outputGainSlider,
                     &boostSlider, &deboxSlider, &claritySlider, &airSlider,
                     &compAmountSlider, &compMakeupSlider,
                     &dsAmountSlider, &dsFocusSlider,
                     &reverbSlider, &satAmountASlider, &satAmountBSlider, &edgeSlider })
        check (s->getMouseDragSensitivity() == 500, "knob drag sensitivity == 500");

    std::printf("dumpLayout: FAIL=%d\n", fail);
}

// 电平表：圆角槽 + 绿→黄→红渐变填充 + 峰值竖线（横向条，标签左、数值右）
void MoonVocEditor::paintMeter(juce::Graphics& g, juce::Rectangle<int> r, float levelDb,
                               float peakDb, const juce::String& name, float)
{
    const int nameW = (int) (48.0f * Theme::fontScale()); // 名字/数值区随字号缩放（大字版不裁切）
    const int valW  = (int) (64.0f * Theme::fontScale());
    g.setFont(Theme::fontLabel(13.0f));
    g.setColour(Theme::dimColour());
    g.drawText(name, juce::Rectangle<int>(r.getX() - nameW - 4, r.getY(), nameW, r.getHeight()),
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
               juce::Rectangle<int>(r.getRight() + 8, r.getY() - 1, valW, r.getHeight() + 2),
               juce::Justification::centredLeft);
}

// 横向 GR 表（从右往左填，标签左、数值右）
void MoonVocEditor::paintGrBar(juce::Graphics& g, juce::Rectangle<int> r, float grDb,
                               const juce::String& name, juce::Colour col)
{
    const int nameW = (int) (48.0f * Theme::fontScale());
    const int valW  = (int) (64.0f * Theme::fontScale());
    g.setFont(Theme::fontLabel(13.0f));
    g.setColour(Theme::dimColour());
    g.drawText(name, juce::Rectangle<int>(r.getX() - nameW - 4, r.getY(), nameW, r.getHeight()),
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
               juce::Rectangle<int>(r.getRight() + 8, r.getY() - 1, valW, r.getHeight() + 2),
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
    g.setColour(Theme::dimColour());
    g.drawText(Strings::get(Strings::kLevel, currentZh), juce::Rectangle<int>(r.getX() - 4, r.getY() + r.getHeight() + 3,
                                             r.getWidth() + 8, 14),
               juce::Justification::centred);
}

void MoonVocEditor::paintCanvas(juce::Graphics& g)
{
    // 静态背景（渐变 + 抽象装饰）缓存贴图
    g.drawImageAt(bgCache, 0, 0);

    // 背景呼吸动效：叠 2 个装饰的呼吸层（同形状、低 alpha，每帧 2 次 fill）
    {
        const float breathe = 0.7f + 0.3f * std::sin(indicatorPhase * 0.35f);
        g.setColour(Theme::accent.withAlpha(0.06f * breathe));
        const juce::Rectangle<float> r1 { (float) kDesignW - 320.0f, -120.0f, 380.0f, 380.0f };
        juce::AffineTransform t = juce::AffineTransform::rotation(
            juce::degreesToRadians(15.0f), r1.getCentreX(), r1.getCentreY());
        g.fillRoundedRectangle(r1.transformedBy(t), 60.0f);

        g.setColour(Theme::accent2.withAlpha(0.05f * breathe));
        juce::Path arc;
        arc.addCentredArc(-40.0f, (float) kDesignH + 40.0f, 200.0f, 200.0f, 0.0f, 0.0f, 1.4f, true);
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

        // 毛玻璃：裁剪圆角 → 贴模糊背景 → 叠半透明白（保留卡片色相）
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clip;
            clip.addRoundedRectangle(rf, 16.0f);
            g.reduceClipRegion(clip);
            if (bgBlurCache.isValid())
                g.drawImageAt(bgBlurCache, 0, 0);
            g.setColour(bgC.withAlpha(0.62f));
            g.fillRect(r);

            // 玻璃高光：上半部白色渐隐
            juce::ColourGradient gloss(juce::Colours::white.withAlpha(0.40f),
                                       rf.getCentreX(), rf.getY(),
                                       juce::Colours::white.withAlpha(0.0f),
                                       rf.getCentreX(), rf.getY() + rf.getHeight() * 0.45f, false);
            g.setGradientFill(gloss);
            g.fillRect(r);
        }

        // 描边（模块深色 30%）+ 顶部内高光线
        g.setColour(deepC.withAlpha(0.30f));
        g.drawRoundedRectangle(rf, 16.0f, 1.2f);
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.drawHorizontalLine(r.getY() + 1, (float) (r.getX() + 14), (float) (r.getRight() - 14));
    };
    drawCard(cardGlobal,  Theme::cardGlobal,  Theme::cardGlobalDeep);
    drawCard(cardReverb,  Theme::cardReverb,  Theme::cardReverbDeep);
    drawCard(cardEq,      Theme::cardEq,      Theme::cardEqDeep);
    drawCard(cardComp,    Theme::cardComp,    Theme::cardCompDeep);
    drawCard(cardDeEss,   Theme::cardDeEss,   Theme::cardDeEssDeep);
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
    drawSectionBar(sectionReverb,  Theme::cardReverbDeep);
    drawSectionBar(sectionSat,     Theme::cardSatDeep);
    drawSectionBar(sectionEdge,    Theme::cardEdgeDeep);
    drawSectionBar(sectionMonitor, Theme::cardMonitorDeep);
    drawSectionBar(sectionOs,      Theme::cardEngineDeep);

    // 品牌：顶部全局条内左侧 MoonVoc（青→珊瑚渐变）+ TUJZMIXING
    {
        const auto brand = cardGlobal.reduced(24, 0);
        const auto titleBox = brand.withWidth(220).withHeight(34).withY(cardGlobal.getY() + 48);
        auto titleFont = Theme::fontBrand(30.0f);
        juce::ColourGradient titleGrad(Theme::accent, (float) titleBox.getX(), 0.0f,
                                       Theme::accent2, (float) titleBox.getRight(), 0.0f, false);
        g.setFont(titleFont);
        g.setGradientFill(titleGrad);
        g.drawText("MoonVoc", titleBox, juce::Justification::centredLeft);
        const auto subBox = titleBox.translated(0.0f, 32.0f).withHeight(16);
        g.setFont(Theme::fontLabel(12.5f));
        g.setColour(Theme::dimColour());
        g.drawText("T U J Z M I X I N G", subBox, juce::Justification::centredLeft);
    }

    // 工作电平指示灯（cardGlobal 右端）
    paintIndicator(g, indicatorRect);

    // 电平表 + GR 表（cardMonitor）
    paintMeter(g, meterInRect,  processorRef.inputLevelDb.load(),  meterPeakIn,
               currentZh ? S8("输入") : juce::String("IN"),  0.0f);
    paintMeter(g, meterOutRect, processorRef.outputLevelDb.load(), meterPeakOut,
               currentZh ? S8("输出") : juce::String("OUT"), processorRef.getCompGainReduction());
    paintGrBar(g, grCompRect,  processorRef.getCompGainReduction(),
               currentZh ? S8("压缩") : juce::String("COMP"),  Theme::accent);
    paintGrBar(g, grDeEssRect, processorRef.getDeEssGainReduction(),
               currentZh ? S8("去齿音") : juce::String("DE-ESS"), Theme::accent2);

    // 页脚 LOGO（右下角，纯文本无辉光）
    auto logoBox = juce::Rectangle<int>(0, 0, kDesignW, kDesignH).removeFromBottom(20).withTrimmedRight(4);
    auto font = Theme::fontSection(15.0f);
    const int dspW = juce::GlyphArrangement::getStringWidthInt(font, "-DSP") + 2;
    auto dspBox = logoBox.removeFromRight(dspW);
    auto tjmBox = logoBox.removeFromRight((int) (112.0f * Theme::fontScale()));
    g.setFont(font);
    g.setColour(Theme::dimColour());
    g.drawText("TUJZMIXING", tjmBox, juce::Justification::centredRight);
    g.setColour(Theme::accent);
    g.drawText("-DSP", dspBox, juce::Justification::centredLeft);
}

void MoonVocEditor::layoutCanvas()
{
    renderBackground();

    // 顶部全局条卡片（y=16, h=140）
    cardGlobal = juce::Rectangle<int>(kEdge, kEdge, kDesignW - kEdge * 2, 140);
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

        // 设置区（全局条右侧：语言 / 缩放 / 大字），弱视用户也能直接找到
        sectionSettings.setBounds(cardGlobal.getX() + 620, cardGlobal.getY() + 16, 200, 20);
        langLabel.setBounds(cardGlobal.getX() + 620, cardGlobal.getY() + 38, 130, 16);
        langBox.setBounds(cardGlobal.getX() + 620, cardGlobal.getY() + 56, 130, 28);
        scaleLabel.setBounds(cardGlobal.getX() + 790, cardGlobal.getY() + 38, 130, 16);
        scaleBox.setBounds(cardGlobal.getX() + 790, cardGlobal.getY() + 56, 130, 28);
        largeFontBtn.setBounds(cardGlobal.getX() + 960, cardGlobal.getY() + 56, 160, 28);

        // 指示灯：卡片右端 40×40
        indicatorRect = juce::Rectangle<int>(cardGlobal.getRight() - 68, cardGlobal.getY() + 50, 40, 40);
    }

    // 信号链卡片行（y=172, h=320，5 卡横排：EQ→Comp→Sat→Edge→Reverb，混响在最后最右）
    const int chainY = 172, chainH = 320;
    int cx = kEdge;
    auto makeCard = [&](int w)
    {
        auto r = juce::Rectangle<int>(cx, chainY, w, chainH);
        cx += w + kGap;
        return r;
    };
    cardEq     = makeCard(320);
    cardComp   = makeCard(252);
    cardDeEss  = makeCard(200);   // 链路位于压缩后
    cardSat    = makeCard(260);
    cardEdge   = makeCard(172);
    cardReverb = makeCard(180);

    // 每张模块卡右上角的旁通开关（标题让位：width = 卡宽 - 内边距 - 旁通区）
    auto bypassSlot = [&](juce::Rectangle<int>& card, juce::ToggleButton& btn, juce::Label& title)
    {
        constexpr int kBypassW = 80;
        btn.setBounds(card.getRight() - kPad - kBypassW, card.getY() + 16, kBypassW, 20);
        title.setBounds(card.getX() + kPad, card.getY() + 16,
                        card.getWidth() - kPad * 2 - kBypassW - 4, 20);
    };
    bypassSlot(cardEq,     eqBypassBtn,     sectionEq);
    bypassSlot(cardComp,   compBypassBtn,   sectionComp);
    bypassSlot(cardDeEss,  deEssBypassBtn,  sectionDeEss);
    bypassSlot(cardSat,    satBypassBtn,    sectionSat);
    bypassSlot(cardEdge,   edgeBypassBtn,   sectionEdge);
    bypassSlot(cardReverb, reverbBypassBtn, sectionReverb);

    // cardReverb：模式下拉 + hero 大旋钮（垂直居中）
    {
        reverbModeBox.setBounds(cardReverb.getCentreX() - 65, cardReverb.getY() + 44, 130, 26);
        const int contentH = 18 + kKnobHero + kTbH; // 156
        reverbLabel.setBounds(juce::Rectangle<int>(cardReverb.getX(), cardReverb.getCentreY() - contentH / 2,
                                                   cardReverb.getWidth(), 18));
        reverbSlider.setBounds(juce::Rectangle<int>(cardReverb.getX() + (cardReverb.getWidth() - kKnobHero) / 2,
                                                    reverbLabel.getBottom(), kKnobHero, kKnobHero + kTbH));
    }

    // cardEq：两行两列（Thick/De-Box 上行，Clarity/Air 下行）+ lock 标签 + airFreqBox
    {
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
        // Air 频点：两个分段小按钮并排，整体居中于 Air 旋钮下方（宽度随字号缩放）
        const int aBtnW = (int) (44.0f * Theme::fontScale());
        constexpr int aGap = 4;
        const int aTotal = aBtnW * 2 + aGap;
        const int aX = airSlider.getBounds().getCentreX() - aTotal / 2;
        airSatinBtn.setBounds(aX, cardEq.getY() + 282, aBtnW, 22);
        airNimbusBtn.setBounds(aX + aBtnW + aGap, cardEq.getY() + 282, aBtnW, 22);
    }

    // cardComp：Style 下拉 + Compression hero + Makeup（Makeup 旋钮中心对齐 hero）
    {
        compModeBox.setBounds(cardComp.getX() + kPad, cardComp.getY() + 44, 130, 26);
        auto heroSlot = juce::Rectangle<int>(cardComp.getX() + 16, cardComp.getY() + 104, 136, 168);
        compAmountLabel.setBounds(heroSlot.removeFromTop(18));
        compAmountSlider.setBounds(heroSlot.withSizeKeepingCentre(kKnobHero, kKnobHero + kTbH));
        auto mkSlot = juce::Rectangle<int>(cardComp.getX() + 156, cardComp.getY() + 138, 84, 100);
        compMakeupLabel.setBounds(mkSlot.removeFromTop(18));
        compMakeupSlider.setBounds(mkSlot.withSizeKeepingCentre(76, kKnobStd + kTbH));
    }

    // cardDeEss：Amount + Focus 两旋钮并排（链路位于压缩后）
    {
        constexpr int kD = 80;
        auto dslot = [&](juce::Slider& s, juce::Label& l, int sx)
        {
            auto slot = juce::Rectangle<int>(sx, cardDeEss.getY() + 104, kD, 18 + kD + kTbH);
            l.setBounds(slot.removeFromTop(18));
            s.setBounds(slot.withSizeKeepingCentre(kD, kD + kTbH));
        };
        dslot(dsAmountSlider, dsAmountLabel, cardDeEss.getX() + 20);
        dslot(dsFocusSlider,  dsFocusLabel,  cardDeEss.getX() + 100);
    }

    // cardSat：A/B 两槽竖向堆叠（类型下拉 → 标签 → 旋钮）
    {
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
        const int contentH = 18 + kKnobHero + kTbH; // 156
        edgeLabel.setBounds(juce::Rectangle<int>(cardEdge.getX(), cardEdge.getCentreY() - contentH / 2,
                                                 cardEdge.getWidth(), 18));
        edgeSlider.setBounds(juce::Rectangle<int>(cardEdge.getX() + (cardEdge.getWidth() - kKnobHero) / 2,
                                                  edgeLabel.getBottom(), kKnobHero, kKnobHero + kTbH));
    }

    // 底部行（y=508, h=128）：cardMonitor + cardOs
    const int botY = chainY + chainH + kGap, botH = 128;
    cardMonitor = juce::Rectangle<int>(kEdge, botY, 800, botH);
    cardOs      = juce::Rectangle<int>(kEdge + 800 + kGap, botY, kDesignW - (kEdge + 800 + kGap) - kEdge, botH);
    {
        sectionMonitor.setBounds(cardMonitor.getX() + kPad, cardMonitor.getY() + 12, 120, 18);
        // 3 条横条：IN / OUT / COMP（标签左、数值右；宽度随字号缩放，大字版不裁切）
        // 4 条横条：IN / OUT / COMP / DE-ESS（行高与间距比 3 条时收紧以适配卡片高度）
        const int rowH = 15, gap = 9;
        const int leadW = (int) (78.0f * Theme::fontScale());
        const int tailW = (int) (86.0f * Theme::fontScale());
        const int barX = cardMonitor.getX() + leadW;
        const int barW = cardMonitor.getWidth() - leadW - tailW;
        int y = cardMonitor.getY() + 34;
        meterInRect  = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        meterOutRect = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        grCompRect   = juce::Rectangle<int>(barX, y, barW, rowH); y += rowH + gap;
        grDeEssRect  = juce::Rectangle<int>(barX, y, barW, rowH);
    }
    {
        sectionOs.setBounds(cardOs.getX() + kPad, cardOs.getY() + 12, 110, 18);
        // 下拉垂直居中（原来偏上，卡片下半部留白）
        const int osY = cardOs.getCentreY() - 14;
        oversamplingLabel.setBounds(juce::Rectangle<int>(cardOs.getX() + 44, osY + 3, 120, 22));
        oversamplingBox.setBounds(juce::Rectangle<int>(cardOs.getX() + 168, osY, 130, 28));
    }

    // ⓘ 入口：全局卡右上角，右缩 16 / 上缩 14（右下 10px 外就是工作电平指示灯，不能压）
    infoBadge.setBounds (cardGlobal.getRight() - 16 - 26, cardGlobal.getY() + 14, 26, 26);
    // 关于浮层：覆盖整个设计区（setBounds 会触发 resized → 重新量算版面）
    aboutOverlay.setBounds (0, 0, kDesignW, kDesignH);
}
