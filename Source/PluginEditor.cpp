#include "PluginEditor.h"
#include "Logo.h"

using namespace juce;

namespace
{
    constexpr int row1H = 200, row2H = 170;
    constexpr int minW = 696, minH = 510, maxW = 4000, maxH = 3000;
    const Rectangle<int> logoArea (96, 10, 58, 60);
    const Rectangle<int> titleArea (96, 10, 340, 60);
    bool askedThisRun = false;

    PropertiesFile::Options settingsOptions()
    {
        PropertiesFile::Options o;
        o.applicationName = "Crumble";
        o.folderName = "Primal Rage Lab";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        o.millisecondsBeforeSaving = 0;
        return o;
    }
}

String CrumbleEditor::loadSetting (const String& key, const String& fallback)
{
    PropertiesFile f (settingsOptions());
    return f.getValue (key, fallback);
}

void CrumbleEditor::saveSetting (const String& key, const String& value)
{
    PropertiesFile f (settingsOptions());
    f.setValue (key, value);
    f.saveIfNeeded();
}

void CrumbleEditor::addCombo (Component& parent, ComboBox& box, const char* paramId, const StringArray& items,
                              std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment>& att, bool)
{
    box.addItemList (items, 1);
    box.setWantsKeyboardFocus (false);
    box.setMouseClickGrabsKeyboardFocus (false);
    parent.addAndMakeVisible (box);
    att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, box);
}

CrumbleEditor::CrumbleEditor (CrumbleProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    using namespace ParamIDs;
    setLookAndFeel (&laf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    content.addAndMakeVisible (gear);
    gear.onClick = [this] { showSettingsMenu(); };
    content.addAndMakeVisible (scope);
    content.addAndMakeVisible (inCol);
    content.addAndMakeVisible (outCol);

    for (auto* b : { &crumbleBlock, &bitBlock, &colorBlock, &compBlock, &tremBlock, &mixBlock, &clipBlock })
    {
        content.addAndMakeVisible (*b);
        b->onDragStart = [this] (Block& blk) { dragStart (blk); };
        b->onDragMove = [this] (Block& blk, int dx) { dragMove (blk, dx); };
        b->onDragEnd = [this] (Block& blk) { dragEnd (blk); };
    }

    auto& apvts = proc.apvts;

    // Crumble
    addCombo (crumbleBlock, modeBox, ParamIDs::mode, Tables::modes, modeAtt, false);
    freq.init (crumbleBlock, apvts, "Frequency", ParamIDs::freq, false);
    rate.init (crumbleBlock, apvts, "Rate", ParamIDs::rate, false);
    texture.init (crumbleBlock, apvts, "Texture", ParamIDs::texture, false);
    amount.init (crumbleBlock, apvts, "Amount", ParamIDs::amount, false);
    stereo.init (crumbleBlock, apvts, "Stereo", ParamIDs::stereo, false);
    feedback.init (crumbleBlock, apvts, "Feedback", ParamIDs::feedback, false);

    // Bitcrush
    bcFreq.init (bitBlock, apvts, "Frequency", ParamIDs::bcFreq, false);
    bits.init (bitBlock, apvts, "Bits", ParamIDs::bits, false);
    quantize.init (bitBlock, apvts, "Quantize", ParamIDs::quantize, false);
    dither.init (bitBlock, apvts, "Dither", ParamIDs::dither, false);
    adcQ.init (bitBlock, apvts, "ADC Q", ParamIDs::adcQ, false);
    dacQ.init (bitBlock, apvts, "DAC Q", ParamIDs::dacQ, false);

    // Color
    addCombo (colorBlock, driveTypeBox, ParamIDs::driveType, Tables::driveTypes, driveTypeAtt, true);
    colTarget = std::make_unique<Segmented> (*apvts.getParameter (ParamIDs::colTarget), Tables::targets, true);
    colorBlock.addAndMakeVisible (*colTarget);
    drive.init (colorBlock, apvts, "Drive", ParamIDs::drive, true);
    tone.init (colorBlock, apvts, "Tone", ParamIDs::tone, true);
    colMix.init (colorBlock, apvts, "Mix", ParamIDs::colMix, true);

    // Comp
    addCombo (compBlock, compModeBox, ParamIDs::compMode, Tables::compModes, compModeAtt, true);
    compTarget = std::make_unique<Segmented> (*apvts.getParameter (ParamIDs::compTarget), Tables::targets, true);
    compBlock.addAndMakeVisible (*compTarget);
    ottDepth.init (compBlock, apvts, "Depth", ParamIDs::ottDepth, true);
    ottTime.init (compBlock, apvts, "Time", ParamIDs::ottTime, true);
    ottUp.init (compBlock, apvts, "Up", ParamIDs::ottUp, true);
    ottDown.init (compBlock, apvts, "Down", ParamIDs::ottDown, true);
    glueThresh.init (compBlock, apvts, "Thresh", ParamIDs::glueThresh, true);
    glueRatio.init (compBlock, apvts, "Ratio", ParamIDs::glueRatio, true);
    glueAttack.init (compBlock, apvts, "Attack", ParamIDs::glueAttack, true);
    glueRelease.init (compBlock, apvts, "Release", ParamIDs::glueRelease, true);
    compMix.init (compBlock, apvts, "Mix", ParamIDs::compMix, true);
    // Touching the comp knobs shows the compressor on the screen: Glue on the waveform, OTT as three bands
    for (auto* k : { &ottDepth, &ottTime, &ottUp, &ottDown, &glueThresh, &glueRatio, &glueAttack, &glueRelease, &compMix })
    {
        auto* sl = &k->slider;
        auto kind = [this, k]
        {
            if (k == &compMix) return roundToInt (proc.apvts.getRawParameterValue (ParamIDs::compMode)->load()) == 1 ? 1 : 2;
            return (k == &glueThresh || k == &glueRatio || k == &glueAttack || k == &glueRelease) ? 1 : 2;
        };
        sl->onDragStart = [this, kind] { scope.touchComp (kind(), true); };
        sl->onDragEnd = [this, kind] { scope.touchComp (kind(), false); };
        sl->onValueChange = [this, kind, sl] { if (sl->isMouseOverOrDragging()) scope.touchComp (kind(), sl->isMouseButtonDown()); };
    }
    content.addMouseListener (this, true);   // grabbing any control outside the compressor sends its picture away

    // Tremolo
    addCombo (tremBlock, trShapeBox, ParamIDs::trShape, Tables::trShapes, trShapeAtt, true);
    trTarget = std::make_unique<Segmented> (*apvts.getParameter (ParamIDs::trTarget), Tables::targets, true);
    tremBlock.addAndMakeVisible (*trTarget);
    trDepth.init (tremBlock, apvts, "Depth", ParamIDs::trDepth, true);
    trRate.init (tremBlock, apvts, String::fromUTF8 ("Rate \xe2\x96\xbe"), ParamIDs::trRate, true);
    trMs.init (tremBlock, apvts, String::fromUTF8 ("Rate \xe2\x96\xbe"), ParamIDs::trMs, true);
    trStereo.init (tremBlock, apvts, "Stereo", ParamIDs::trPan, true);
    for (auto* k : { &trRate, &trMs })
    {
        k->label.setInterceptsMouseClicks (true, false);
        k->label.setMouseCursor (MouseCursor::PointingHandCursor);
        k->label.onClick = [this]
        {
            const int current = roundToInt (proc.apvts.getRawParameterValue (ParamIDs::trRateMode)->load());
            PopupMenu m;
            m.addItem (1, "Sync (note values)", true, current == 0);
            m.addItem (2, "Free (milliseconds)", true, current == 1);
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (current == 0 ? &trRate.label : &trMs.label),
                             [this] (int r) { if (r > 0) setChoice (ParamIDs::trRateMode, r - 1); });
        };
    }

    // Mix
    autoGainButton.setWantsKeyboardFocus (false);
    autoGainButton.getProperties().set ("fontSize", 12.0);
    mixBlock.addAndMakeVisible (autoGainButton);
    autoGainAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamIDs::autoGain, autoGainButton);
    mix.init (mixBlock, apvts, "Mix", ParamIDs::mix, true);

    // Clip
    clipMode = std::make_unique<Segmented> (*apvts.getParameter (ParamIDs::clipMode), Tables::clipModes, true);
    clipBlock.addAndMakeVisible (*clipMode);
    ceiling.init (clipBlock, apvts, "Ceiling", ParamIDs::ceiling, true);
    ceiling.slider.onDragStart = [this] { scope.touchCeiling (true); };
    ceiling.slider.onDragEnd = [this] { scope.touchCeiling (false); };
    ceiling.slider.onValueChange = [this]
    {
        if (ceiling.slider.isMouseOverOrDragging())
            scope.touchCeiling (ceiling.slider.isMouseButtonDown());
    };

    // Overlays
    content.addChildComponent (about);
    content.addChildComponent (tour);
    content.addChildComponent (welcome);
    tour.resolveTarget = [this] (const String& name) { return tourTarget (name); };
    tour.onStep = [this] (const String& name)
    {
        if (name == "pills" || name == "viewSwitch")
            scope.setView (ScopeView::View::eq);
        else if (name == "scope" || name == "follow")
            scope.setView (ScopeView::View::wave);
    };
    tour.onFinish = [] { saveSetting ("tour", "never"); };
    welcome.onLanguage = [this] (Help::Lang l)
    {
        lang = l;
        saveSetting ("helpLang", l == Help::Lang::ru ? "ru" : "en");
        applyHelp();
    };
    welcome.onChoice = [this] (int choice)
    {
        welcome.setVisible (false);
        if (choice == 0)
            tour.start (lang);
        else if (choice == 2)
            saveSetting ("tour", "never");
    };

    scope.onBarsChanged = [] (int i) { saveSetting ("bars", String (i)); };
    scope.setBarsIndex (loadSetting ("bars", "2").getIntValue());

    // Settings
    helpOn = loadSetting ("help", "on") != "off";
    lang = loadSetting ("helpLang", "en") == "ru" ? Help::Lang::ru : Help::Lang::en;
    shownBitFirst = tentativeBitFirst = proc.apvts.getRawParameterValue (order)->load() > 0.5f;
    {
        const int idx = jlimit (0, 5, roundToInt (proc.apvts.getRawParameterValue (chain)->load()));
        for (int k = 0; k < 3; ++k)
            shownChain[k] = tentativeChain[k] = Tables::chainOrders[idx][k];
    }
    applyTheme (Theme::byName (loadSetting ("theme", "Asphalt")), false);
    welcome.setLang (lang);
    applyHelp();
    // Window size: this plugin's last size, else the last size used anywhere, else the old scale setting
    int w = proc.editorW.load(), h = proc.editorH.load();
    if (w <= 0 || h <= 0)
    {
        w = loadSetting ("width", "0").getIntValue();
        h = loadSetting ("height", "0").getIntValue();
    }
    if (w <= 0 || h <= 0)
    {
        const float sc = jlimit (0.6f, 2.0f, loadSetting ("scale", "1.0").getFloatValue());
        w = roundToInt (baseW * sc);
        h = roundToInt (baseH * sc);
    }
    w = jlimit (minW, maxW, w);
    h = jlimit (minH, maxH, h);

    // Free resizing: any shape; the screen and the blocks stretch to fill it
    setResizable (true, true);
    setResizeLimits (minW, minH, maxW, maxH);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (0.0);
    setSize (w, h);
    constructed = true;
    openedMs = Time::getMillisecondCounterHiRes();

    // Some hosts resize the window right after opening it; put our size back once
    Timer::callAfterDelay (350, [safe = Component::SafePointer<CrumbleEditor> (this), w, h]
    {
        if (safe != nullptr && (safe->getWidth() != w || safe->getHeight() != h))
            safe->setSize (w, h);
    });

    if (loadSetting ("tour", "") != "never" && ! askedThisRun)
    {
        askedThisRun = true;
        showWelcome();
    }

    timerCallback();
    startTimerHz (30);
}

CrumbleEditor::~CrumbleEditor()
{
    stopTimer();
    if (proc.editorW.load() > 0)
    {
        saveSetting ("width", String (proc.editorW.load()));
        saveSetting ("height", String (proc.editorH.load()));
    }
    tooltipWindow.reset();
    setLookAndFeel (nullptr);
}

void CrumbleEditor::showComp()
{
    scope.touchComp (roundToInt (proc.apvts.getRawParameterValue (ParamIDs::compMode)->load()) == 1 ? 1 : 2, false);
}

void CrumbleEditor::mouseDown (const MouseEvent& e)
{
    auto* c = e.eventComponent;
    if (c == nullptr || ! scope.isCompShown())
        return;
    if (c == &compBlock || compBlock.isParentOf (c) || c == &scope || scope.isParentOf (c))
        return;
    scope.dismissComp();
}

void CrumbleEditor::setChoice (const char* id, int index)
{
    if (auto* param = proc.apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
        param->endChangeGesture();
    }
}

//==============================================================================
int CrumbleEditor::widthOf (const Block* b) const
{
    if (b == &crumbleBlock) return wCrumble;
    if (b == &bitBlock) return wBit;
    if (b == &colorBlock) return wColor;
    if (b == &compBlock) return wComp;
    if (b == &tremBlock) return wTrem;
    return wMix;
}

Block* CrumbleEditor::movable (int index)
{
    return index == 0 ? &colorBlock : (index == 1 ? &compBlock : &tremBlock);
}

int CrumbleEditor::row2SlotX (const int order[3], int index) const
{
    const int widths[3] = { wColor, wComp, wTrem };
    int x = x0;
    for (int k = 0; k < 3; ++k)
    {
        if (order[k] == index)
            return x;
        x += widths[order[k]] + gap2;
    }
    return x;
}

void CrumbleEditor::layoutRow1 (bool bitFirst, bool animate, const Block* skip)
{
    const Rectangle<int> crumbleR (bitFirst ? x1 - wCrumble : x0, r1y, wCrumble, row1H);
    const Rectangle<int> bitR (bitFirst ? x0 : x1 - wBit, r1y, wBit, row1H);
    auto& anim = Desktop::getInstance().getAnimator();
    for (auto [blk, r] : { std::pair<Block*, Rectangle<int>> { &crumbleBlock, crumbleR }, { &bitBlock, bitR } })
    {
        if (blk == skip)
            continue;
        if (animate) anim.animateComponent (blk, r, 1.0f, 200, false, 1.0, 0.0);
        else         blk->setBounds (r);
    }
}

void CrumbleEditor::layoutRow2 (const int order[3], bool animate, const Block* skip)
{
    auto& anim = Desktop::getInstance().getAnimator();
    for (int i = 0; i < 3; ++i)
    {
        auto* blk = movable (i);
        if (blk == skip)
            continue;
        const Rectangle<int> r (row2SlotX (order, i), r2y, widthOf (blk), row2H);
        if (animate) anim.animateComponent (blk, r, 1.0f, 200, false, 1.0, 0.0);
        else         blk->setBounds (r);
    }
    mixBlock.setBounds (x0 + wColor + wComp + wTrem + 3 * gap2, r2y, wMix, row2H);
    clipBlock.setBounds (x1 - wClip, r2y, wClip, row2H);
}

void CrumbleEditor::dragStart (Block& b)
{
    dragging = &b;
    dragOriginX = b.getX();
    b.toFront (false);
    tentativeBitFirst = shownBitFirst;
    std::copy (shownChain, shownChain + 3, tentativeChain);
    content.repaint();
}

void CrumbleEditor::dragMove (Block& b, int dx)
{
    if (&b == &crumbleBlock || &b == &bitBlock)
    {
        const int x = jlimit (x0, x1 - b.getWidth(), dragOriginX + dx);
        b.setTopLeftPosition (x, r1y);
        // Compare the dragged block with the other block's resting place
        const auto& other = (&b == &crumbleBlock) ? bitBlock : crumbleBlock;
        const int otherCentre = (&other == &bitBlock) ? (tentativeBitFirst ? x0 + wBit / 2 : x1 - wBit / 2)
                                                      : (tentativeBitFirst ? x1 - wCrumble / 2 : x0 + wCrumble / 2);
        const bool draggedFirst = b.getBounds().getCentreX() < otherCentre;
        const bool newBitFirst = (&b == &bitBlock) ? draggedFirst : ! draggedFirst;
        if (newBitFirst != tentativeBitFirst)
        {
            tentativeBitFirst = newBitFirst;
            layoutRow1 (tentativeBitFirst, true, &b);
        }
        return;
    }

    const int maxX = x0 + wColor + wComp + wTrem + 2 * gap2 - b.getWidth();
    b.setTopLeftPosition (jlimit (x0, maxX, dragOriginX + dx), r2y);
    int idx[3] = { 0, 1, 2 };
    float centre[3];
    for (int i = 0; i < 3; ++i)
        centre[i] = movable (i) == &b ? (float) b.getBounds().getCentreX()
                                      : (float) row2SlotX (tentativeChain, i) + (float) widthOf (movable (i)) * 0.5f;
    std::sort (idx, idx + 3, [&] (int a, int c) { return centre[a] < centre[c]; });
    if (! std::equal (idx, idx + 3, tentativeChain))
    {
        std::copy (idx, idx + 3, tentativeChain);
        layoutRow2 (tentativeChain, true, &b);
    }
}

void CrumbleEditor::dragEnd (Block& b)
{
    if (&b == &crumbleBlock || &b == &bitBlock)
    {
        shownBitFirst = tentativeBitFirst;
        setChoice (ParamIDs::order, tentativeBitFirst ? 1 : 0);
        dragging = nullptr;
        layoutRow1 (shownBitFirst, true, nullptr);
    }
    else
    {
        std::copy (tentativeChain, tentativeChain + 3, shownChain);
        setChoice (ParamIDs::chain, Tables::chainIndexFor (shownChain));
        dragging = nullptr;
        layoutRow2 (shownChain, true, nullptr);
    }
    content.repaint();
}

//==============================================================================
void CrumbleEditor::computeLayout()
{
    x0 = 96;
    x1 = lw - 96;
    const int extraH = lh - baseH;
    r1y = 449 + extraH;
    r2y = 661 + extraH;
    const int avail = x1 - x0;
    wCrumble = roundToInt ((double) (avail - 32) * 428.0 / 936.0);
    wBit = avail - 32 - wCrumble;
    gap2 = 15;
    wMix = wClip = 102;
    const double k = (double) (avail - 4 * gap2 - wMix - wClip) / 704.0;
    wColor = roundToInt (196.0 * k);
    wTrem = roundToInt (196.0 * k);
    wComp = avail - 4 * gap2 - wMix - wClip - wColor - wTrem;
}

void CrumbleEditor::resized()
{
    const float s = jmax (0.3f, jmin ((float) getWidth() / (float) baseW, (float) getHeight() / (float) baseH));
    scale = s;
    lw = jmax (baseW, (int) std::ceil ((float) getWidth() / s - 0.01f));
    lh = jmax (baseH, (int) std::ceil ((float) getHeight() / s - 0.01f));
    content.setTransform (AffineTransform::scale (s));
    content.setBounds (0, 0, lw, lh);
    computeLayout();

    presetBar.setBounds (x0 + 370 + (lw - baseW) / 2, 24, 330, 30);
    gear.setBounds (x1 - 40, 24, 40, 30);
    scope.setBounds (x0, 80, x1 - x0, 357 + (lh - baseH));
    inCol.setBounds (10, 10, 72, lh - 20);
    outCol.setBounds (lw - 82, 10, 72, lh - 20);
    about.setBounds (0, 0, lw, lh);
    tour.setBounds (0, 0, lw, lh);
    welcome.setBounds (0, 0, lw, lh);

    if (dragging == nullptr)
    {
        layoutRow1 (shownBitFirst, false, nullptr);
        layoutRow2 (shownChain, false, nullptr);
    }
    layoutInside();

    if (bgW != lw || bgH != lh)
    {
        if (! constructed)
            buildBackground();
        else
        {
            bgDirty = true;
            bgDirtyMs = Time::getMillisecondCounterHiRes();
        }
    }

    // Remember the size the user sees (not sizes the host sets while the window is hidden)
    if (constructed && isShowing() && Time::getMillisecondCounterHiRes() - openedMs > 500.0)
    {
        proc.editorW.store (getWidth());
        proc.editorH.store (getHeight());
    }
}

// Knobs spread evenly across a block; on wide blocks they stay grouped in the middle
static void spreadKnobs (std::initializer_list<Knob*> knobs, int blockW, int pad, int knobW, int y, int maxSlot)
{
    const int n = (int) knobs.size();
    const int slot = jmin (maxSlot, (blockW - 2 * pad) / n);
    const int startX = (blockW - slot * n) / 2;
    int i = 0;
    for (auto* k : knobs)
        k->setBounds (startX + (i++) * slot + (slot - knobW) / 2, y, knobW);
}

void CrumbleEditor::layoutInside()
{
    modeBox.setBounds (14, 32, 124, 24);
    spreadKnobs ({ &freq, &texture, &amount, &stereo, &feedback }, wCrumble, 14, 80, 76, 118);
    rate.setBounds (freq.slider.getX(), 76, 80);
    spreadKnobs ({ &bcFreq, &bits, &quantize, &dither, &adcQ, &dacQ }, wBit, 14, 80, 76, 118);

    const int cy = 32, ky = 62;
    driveTypeBox.setBounds (10, cy, 92, 22);
    colTarget->setBounds (108, cy, colTarget->getIdealWidth(), 22);
    spreadKnobs ({ &drive, &tone, &colMix }, wColor, 11, 58, ky, 86);

    compModeBox.setBounds (10, cy, 92, 22);
    compTarget->setBounds (108, cy, compTarget->getIdealWidth(), 22);
    spreadKnobs ({ &ottDepth, &ottTime, &ottUp, &ottDown, &compMix }, wComp, 11, 58, ky, 86);
    spreadKnobs ({ &glueThresh, &glueRatio, &glueAttack, &glueRelease, &compMix }, wComp, 11, 58, ky, 86);

    trShapeBox.setBounds (10, cy, 96, 22);
    trTarget->setBounds (112, cy, trTarget->getIdealWidth(), 22);
    spreadKnobs ({ &trDepth, &trRate, &trStereo }, wTrem, 11, 58, ky, 86);
    trMs.setBounds (trRate.slider.getX(), ky, 58);

    autoGainButton.setBounds (6, cy, 94, 22);
    mix.setBounds ((wMix - 58) / 2, ky, 58);
    clipMode->setBounds (10, cy, clipMode->getIdealWidth(), 22);
    ceiling.setBounds ((wClip - 58) / 2, ky, 58);
}

void CrumbleEditor::timerCallback()
{
    using namespace ParamIDs;
    auto raw = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };

    if (bgDirty && Time::getMillisecondCounterHiRes() - bgDirtyMs > 250.0)
    {
        bgDirty = false;
        buildBackground();
        content.repaint();
    }

    // Comp switched on or to another mode: Glue is set up on the waveform, OTT on its band view
    {
        const int cOn = raw (compOn) > 0.5f ? 1 : 0, cMode = roundToInt (raw (compMode));
        if (lastCompOn >= 0 && cOn == 1 && (cOn != lastCompOn || cMode != lastCompMode))
            showComp();
        lastCompOn = cOn;
        lastCompMode = cMode;
    }

    inCol.push (proc.inPeak[0].exchange (0.0f), proc.inPeak[1].exchange (0.0f));
    outCol.push (proc.outPeak[0].exchange (0.0f), proc.outPeak[1].exchange (0.0f));

    // Block order changed from outside (preset, automation)
    if (dragging == nullptr)
    {
        const bool bitFirst = raw (order) > 0.5f;
        if (bitFirst != shownBitFirst)
        {
            shownBitFirst = tentativeBitFirst = bitFirst;
            layoutRow1 (bitFirst, true, nullptr);
            content.repaint();
        }
        const int idx = jlimit (0, 5, roundToInt (raw (chain)));
        if (! std::equal (shownChain, shownChain + 3, Tables::chainOrders[idx]))
        {
            std::copy (Tables::chainOrders[idx], Tables::chainOrders[idx] + 3, shownChain);
            std::copy (shownChain, shownChain + 3, tentativeChain);
            layoutRow2 (shownChain, true, nullptr);
            content.repaint();
        }
    }

    // Knobs that depend on a mode
    const int crMode = roundToInt (raw (mode));
    freq.setVisible (crMode != 1);
    rate.setVisible (crMode == 1);
    const bool glueMode = roundToInt (raw (compMode)) == 1;
    for (auto* k : { &ottDepth, &ottTime, &ottUp, &ottDown })
        k->setVisible (! glueMode);
    for (auto* k : { &glueThresh, &glueRatio, &glueAttack, &glueRelease })
        k->setVisible (glueMode);
    const bool freeRate = roundToInt (raw (trRateMode)) == 1;
    trRate.setVisible (! freeRate);
    trMs.setVisible (freeRate);

    // Dim switched-off blocks
    auto dim = [] (std::initializer_list<Knob*> knobs, std::initializer_list<Component*> others, bool off)
    {
        for (auto* k : knobs) k->setDimmed (off);
        for (auto* c : others) c->setAlpha (off ? 0.4f : 1.0f);
    };
    const bool crOff = ! crumbleBlock.isOn();
    dim ({ &freq, &rate, &amount, &stereo, &feedback }, { &modeBox }, crOff);
    texture.setDimmed (crOff || crMode != 2);
    dim ({ &bcFreq, &bits, &quantize, &dither, &adcQ, &dacQ }, {}, ! bitBlock.isOn());
    dim ({ &drive, &tone, &colMix }, { &driveTypeBox }, ! colorBlock.isOn());
    dim ({ &ottDepth, &ottTime, &ottUp, &ottDown, &glueThresh, &glueRatio, &glueAttack, &glueRelease, &compMix }, { &compModeBox }, ! compBlock.isOn());
    dim ({ &trDepth, &trRate, &trMs, &trStereo }, { &trShapeBox }, ! tremBlock.isOn());
    dim ({ &ceiling }, {}, ! clipBlock.isOn());

    // Wet / FX only mean something when Crumble or Bitcrush is on
    const bool fx = raw (crOn) > 0.5f || raw (bcOn) > 0.5f;
    for (auto* s : { colTarget.get(), compTarget.get(), trTarget.get() })
    {
        s->setLocked (! fx, 1);
        s->setTooltip (helpOn ? Help::tip (fx ? "wetAll" : "wetOff", lang) : String());
    }
    scope.setFxAvailable (fx);
    clipMode->setAlpha (clipBlock.isOn() ? 1.0f : 0.4f);
}

//==============================================================================
void CrumbleEditor::applyTheme (const Theme& t, bool remember)
{
    theme = &t;
    laf.setTheme (t);
    if (remember)
        saveSetting ("theme", t.name);
    buildBackground();
    content.sendLookAndFeelChange();
    for (auto* k : { &freq, &rate, &texture, &amount, &stereo, &feedback, &bcFreq, &bits, &quantize, &dither, &adcQ, &dacQ,
                     &drive, &tone, &colMix, &ottDepth, &ottTime, &ottUp, &ottDown, &glueThresh, &glueRatio, &glueAttack,
                     &glueRelease, &compMix, &trDepth, &trStereo, &mix, &ceiling, &inCol.knob, &outCol.knob })
        k->label.setColour (Label::textColourId, t.mute);
    for (auto* k : { &trRate, &trMs })
        k->label.setColour (Label::textColourId, t.ink);
    welcome.setLang (lang);
    content.repaint();
}

void CrumbleEditor::applyHelp()
{
    if (helpOn && tooltipWindow == nullptr)
    {
        tooltipWindow = std::make_unique<TooltipWindow> (this, 600);
        tooltipWindow->setLookAndFeel (&laf);
    }
    else if (! helpOn)
    {
        tooltipWindow.reset();
    }

    auto tip = [this] (const char* key) { return helpOn ? Help::tip (key, lang) : String(); };
    inCol.knob.setTooltip (tip ("input"));
    outCol.knob.setTooltip (tip ("output"));
    inCol.setTooltip (tip ("inMeter"));
    outCol.setTooltip (tip ("outMeter"));
    presetBar.setTooltip (tip ("presetName"));
    presetBar.prev.setTooltip (tip ("prev"));
    presetBar.next.setTooltip (tip ("next"));
    presetBar.save.setTooltip (tip ("save"));
    gear.setTooltip (tip ("gear"));
    for (auto* b : { &crumbleBlock, &bitBlock, &colorBlock, &compBlock, &tremBlock })
    {
        b->setTooltip (tip ("drag"));
        if (auto* pw = b->getPowerButton())
            pw->setTooltip (tip ("power"));
    }
    if (auto* pw = clipBlock.getPowerButton())
        pw->setTooltip (tip ("power"));

    modeBox.setTooltip (tip ("mode"));
    freq.setTooltip (tip ("freq"));
    rate.setTooltip (tip ("rateSync"));
    texture.setTooltip (tip ("texture"));
    amount.setTooltip (tip ("amount"));
    stereo.setTooltip (tip ("stereo"));
    feedback.setTooltip (tip ("feedback"));
    bcFreq.setTooltip (tip ("bcFreq"));
    bits.setTooltip (tip ("bits"));
    quantize.setTooltip (tip ("quantize"));
    dither.setTooltip (tip ("dither"));
    adcQ.setTooltip (tip ("adcQ"));
    dacQ.setTooltip (tip ("dacQ"));
    driveTypeBox.setTooltip (tip ("driveType"));
    drive.setTooltip (tip ("drive"));
    tone.setTooltip (tip ("tone"));
    colMix.setTooltip (tip ("colMix"));
    compModeBox.setTooltip (tip ("compMode"));
    ottDepth.setTooltip (tip ("ottDepth"));
    ottTime.setTooltip (tip ("ottTime"));
    ottUp.setTooltip (tip ("ottUp"));
    ottDown.setTooltip (tip ("ottDown"));
    glueThresh.setTooltip (tip ("glueThresh"));
    glueRatio.setTooltip (tip ("glueRatio"));
    glueAttack.setTooltip (tip ("glueAttack"));
    glueRelease.setTooltip (tip ("glueRelease"));
    compMix.setTooltip (tip ("compMix"));
    trShapeBox.setTooltip (tip ("trShape"));
    trDepth.setTooltip (tip ("trDepth"));
    trRate.setTooltip (tip ("trRate"));
    trMs.setTooltip (tip ("trRate"));
    trStereo.setTooltip (tip ("trStereo"));
    autoGainButton.setTooltip (tip ("autoGain"));
    mix.setTooltip (tip ("mix"));
    clipMode->setTooltip (tip ("clipMode"));
    ceiling.setTooltip (tip ("ceiling"));
    scope.setHelp (helpOn, lang);
    timerCallback();
}

void CrumbleEditor::setScale (float s, bool)
{
    setSize (roundToInt (baseW * s), roundToInt (baseH * s));
}

void CrumbleEditor::showSettingsMenu()
{
    PopupMenu themes, sizes, help, langs, menu;
    int id = 1;
    for (const auto& t : Theme::all())
        themes.addItem (id++, t.name, true, &t == theme);
    const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
    for (int i = 0; i < 5; ++i)
        sizes.addItem (100 + i, String (roundToInt (scales[i] * 100)) + "%", true,
                       std::abs (getWidth() - roundToInt (baseW * scales[i])) <= 2 && std::abs (getHeight() - roundToInt (baseH * scales[i])) <= 2);
    help.addItem (200, "On", true, helpOn);
    help.addItem (201, "Off", true, ! helpOn);
    langs.addItem (300, "English", true, lang == Help::Lang::en);
    langs.addItem (301, String::fromUTF8 ("Русский"), true, lang == Help::Lang::ru);
    menu.addSubMenu ("Theme", themes);
    menu.addSubMenu ("Size", sizes);
    menu.addSubMenu ("Help", help);
    menu.addSubMenu ("Help language", langs);
    menu.addSeparator();
    menu.addItem (400, "Start tour");
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&gear), [this, scales] (int r)
    {
        if (r >= 1 && r <= (int) Theme::all().size())
            applyTheme (Theme::all()[(size_t) (r - 1)], true);
        else if (r >= 100 && r < 105)
            setScale (scales[r - 100], true);
        else if (r == 200 || r == 201)
        {
            helpOn = r == 200;
            saveSetting ("help", helpOn ? "on" : "off");
            applyHelp();
        }
        else if (r == 300 || r == 301)
        {
            lang = r == 301 ? Help::Lang::ru : Help::Lang::en;
            saveSetting ("helpLang", r == 301 ? "ru" : "en");
            welcome.setLang (lang);
            applyHelp();
        }
        else if (r == 400)
            tour.start (lang);
    });
}

void CrumbleEditor::Content::mouseDown (const MouseEvent& e)
{
    if (titleArea.contains (e.getPosition()))
    {
        editor.about.setVisible (true);
        editor.about.toFront (false);
    }
}

Rectangle<float> CrumbleEditor::tourTarget (const String& name)
{
    auto area = [this] (Component& c) { return content.getLocalArea (&c, c.getLocalBounds()).toFloat(); };
    auto knobArea = [this] (Knob& k, Component& parent) { return content.getLocalArea (&parent, k.getArea()).toFloat(); };
    if (name == "inCol")      return area (inCol);
    if (name == "outCol")     return area (outCol);
    if (name == "presets")    return area (presetBar);
    if (name == "gear")       return area (gear);
    if (name == "scope")      return area (scope);
    if (name == "follow")     return content.getLocalArea (&scope, scope.followArea()).toFloat();
    if (name == "pills")      return content.getLocalArea (&scope, scope.pillsArea()).toFloat();
    if (name == "viewSwitch") return content.getLocalArea (&scope, scope.viewSwitchArea().expanded (6, 5)).toFloat();
    if (name == "crumble")    return area (crumbleBlock);
    if (name == "bitcrush")   return area (bitBlock);
    if (name == "color")      return area (colorBlock);
    if (name == "comp")       return area (compBlock);
    if (name == "tremolo")    return area (tremBlock);
    if (name == "mix")        return area (mixBlock);
    if (name == "clip")       return area (clipBlock);
    if (name == "mode")       return area (modeBox).expanded (4.0f);
    if (name == "amount")     return knobArea (amount, crumbleBlock).expanded (4.0f);
    if (name == "texture")    return knobArea (freq, crumbleBlock).getUnion (knobArea (texture, crumbleBlock)).expanded (4.0f);
    if (name == "stereo")     return knobArea (stereo, crumbleBlock).getUnion (knobArea (feedback, crumbleBlock)).expanded (4.0f);
    if (name == "chain")      return area (colorBlock).getUnion (area (compBlock)).getUnion (area (tremBlock));
    return {};
}

//==============================================================================
void CrumbleEditor::buildBackground()
{
    const auto& t = *theme;
    bgW = lw;
    bgH = lh;
    const double f = jmin (2.0, std::sqrt (4.2e6 / ((double) lw * (double) lh)));
    const int W = jmax (1, roundToInt (lw * f)), H = jmax (1, roundToInt (lh * f));
    background = Image (Image::RGB, W, H, false);

    if (t.gloss)
    {
        Graphics g (background);
        ColourGradient bg (Colour (0xff1c1c21), 0.0f, 0.0f, Colour (0xff141418), 0.0f, (float) H, false);
        bg.addColour (0.45, Colour (0xff0b0b0d));
        g.setGradientFill (bg);
        g.fillAll();
        struct Sheen { float x, w, a; };
        for (const auto& sh : { Sheen { 0.18f, 0.22f, 0.07f }, Sheen { 0.62f, 0.08f, 0.05f }, Sheen { 0.80f, 0.30f, 0.04f } })
        {
            const float x0 = (float) W * sh.x, w0 = (float) W * sh.w;
            ColourGradient sg (Colours::white.withAlpha (0.0f), x0 - w0, 0.0f, Colours::white.withAlpha (0.0f), x0 + w0, (float) H * 0.3f, false);
            sg.addColour (0.5, Colour ((uint8) 255, 248, 230, sh.a));
            g.setGradientFill (sg);
            Path p;
            p.startNewSubPath (x0 - w0, 0.0f);
            p.lineTo (x0 + w0, 0.0f);
            p.lineTo (x0 + w0 - (float) W * 0.25f, (float) H);
            p.lineTo (x0 - w0 - (float) W * 0.25f, (float) H);
            p.closeSubPath();
            g.fillPath (p);
        }
        return;
    }

    // brushed metal: light from above, fine grain, horizontal brushing, a few specks
    Random rnd (1234);
    {
        Graphics g (background);
        ColourGradient body (t.bg.brighter (t.lightBg ? 0.08f : 0.28f), 0.0f, 0.0f, t.bg.darker (0.45f), 0.0f, (float) H, false);
        body.addColour (0.35, t.bg);
        g.setGradientFill (body);
        g.fillAll();
    }
    {
        Image::BitmapData bd (background, Image::BitmapData::readWrite);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const auto c = bd.getPixelColour (x, y);
                const int n = rnd.nextInt (t.lightBg ? 17 : 13) - (t.lightBg ? 8 : 6);
                auto ch = [n] (uint8 v) { return (uint8) jlimit (0, 255, (int) v + n); };
                bd.setPixelColour (x, y, Colour (ch (c.getRed()), ch (c.getGreen()), ch (c.getBlue())));
            }
    }
    Graphics g (background);
    const float k = (float) W / (float) (baseW * 2);    // brushing scales with the panel
    for (int i = 0; i < (int) (2600.0f * (float) (W * H) / (float) (baseW * baseH * 4)); ++i)
    {
        const float y = rnd.nextFloat() * (float) H, x0 = rnd.nextFloat() * (float) W;
        g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (t.lightBg ? 0.05f : 0.045f));
        g.drawLine (x0, y, x0 + (120.0f + rnd.nextFloat() * 640.0f) * k, y, 0.4f + rnd.nextFloat() * 1.4f);
    }
    if (! t.lightBg)
        for (int i = 0; i < 900; ++i)
        {
            g.setColour (t.speck.withAlpha (rnd.nextFloat() * 0.25f));
            g.fillRect ((float) rnd.nextInt (W), (float) rnd.nextInt (H), 2.0f, 2.0f);
        }
    ColourGradient key (Colours::white.withAlpha (t.lightBg ? 0.30f : 0.11f), (float) W * 0.3f, -160.0f,
                        Colours::transparentWhite, (float) W * 0.3f, -160.0f + (float) H * 1.1f, true);
    g.setGradientFill (key);
    g.fillRect (0, 0, W, H);
    ColourGradient vig (Colours::transparentBlack, (float) W * 0.5f, (float) H * 0.45f, t.vignette, 0.0f, 0.0f, true);
    g.setGradientFill (vig);
    g.fillRect (0, 0, W, H);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.fillRect (0, 0, W, 2);
}

void CrumbleEditor::paintContent (Graphics& g)
{
    const auto& t = *theme;
    g.drawImage (background, Rectangle<float> (0.0f, 0.0f, (float) lw, (float) lh));

    // Logo: vector, in the theme's colour (gold gradient in Luxe)
    const auto la = logoArea.toFloat();
    if (t.gloss)
    {
        ColourGradient gold (Colour (0xfff6dc9a), la.getX(), la.getY(), Colour (0xffa47a2c), la.getX(), la.getBottom(), false);
        gold.addColour (0.55, Colour (0xffe8c46a));
        Logo::draw (g, la, FillType (gold));
    }
    else
    {
        Logo::draw (g, la, FillType (t.logo));
    }
    g.setColour (t.ink);
    g.setFont (uiFont (30.0f, true, 0.08f));
    g.drawText ("CRUMBLE", logoArea.getRight() + 12, 12, 320, 34, Justification::centredLeft);
    g.setColour (t.mute);
    g.setFont (uiFont (11.0f, true, 0.3f));
    g.drawText ("PRIMAL RAGE LAB", logoArea.getRight() + 14, 46, 320, 16, Justification::centredLeft);

    // Arrows that show the signal path
    if (dragging != nullptr)
        return;
    auto arrow = [&] (const Component& a, const Component& b)
    {
        const float x0 = (float) a.getRight(), x1 = (float) b.getX();
        if (x1 - x0 < 8.0f)
            return;
        const float cx = 0.5f * (x0 + x1), cy = (float) a.getBounds().getCentreY();
        const float half = jmin (7.0f, (x1 - x0) * 0.35f);
        Path p;
        p.startNewSubPath (cx - half, cy);
        p.lineTo (cx + half, cy);
        p.startNewSubPath (cx + half - 4.0f, cy - 4.0f);
        p.lineTo (cx + half, cy);
        p.lineTo (cx + half - 4.0f, cy + 4.0f);
        g.setColour (t.accent.withAlpha (0.65f));
        g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    };
    if (shownBitFirst) arrow (bitBlock, crumbleBlock);
    else               arrow (crumbleBlock, bitBlock);
    const Component* row2[5] = { movable (shownChain[0]), movable (shownChain[1]), movable (shownChain[2]), &mixBlock, &clipBlock };
    for (int i = 0; i < 4; ++i)
        arrow (*row2[i], *row2[i + 1]);
}
