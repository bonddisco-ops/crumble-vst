#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    constexpr int X0 = 96, X1 = 1064;
    constexpr int row1Y = 330, row2Y = 532, rowH = 190;
    constexpr int crumbleW = 414, bitW = 492, swapW = X1 - X0 - crumbleW - bitW;
    const Rectangle<int> logoArea (96, 10, 56, 58);
}

String CrumbleEditor::loadSetting (const String& key, const String& fallback)
{
    PropertiesFile::Options o;
    o.applicationName = "Crumble";
    o.folderName = "Primal Rage Lab";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    PropertiesFile f (o);
    return f.getValue (key, fallback);
}

void CrumbleEditor::saveSetting (const String& key, const String& value)
{
    PropertiesFile::Options o;
    o.applicationName = "Crumble";
    o.folderName = "Primal Rage Lab";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.millisecondsBeforeSaving = 0;
    PropertiesFile f (o);
    f.setValue (key, value);
    f.saveIfNeeded();
}

void CrumbleEditor::addCombo (Component& parent, ComboBox& box, const char* paramId, const StringArray& items,
                              std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment>& att)
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

    logoRaw = ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    // Header
    content.addAndMakeVisible (presetBar);
    content.addAndMakeVisible (gear);
    gear.onClick = [this] { showSettingsMenu(); };

    // Scope and meters
    content.addAndMakeVisible (scope);
    content.addAndMakeVisible (inCol);
    content.addAndMakeVisible (outCol);

    // Blocks
    for (auto* b : { &crumbleBlock, &bitBlock, &filterBlock, &colorBlock, &tremBlock, &mixBlock, &clipBlock })
        content.addAndMakeVisible (*b);
    content.addAndMakeVisible (swapButton);
    swapButton.onClick = [this]
    {
        auto* param = proc.apvts.getParameter (order);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        param->endChangeGesture();
    };

    auto& apvts = proc.apvts;
    // Crumble
    addCombo (crumbleBlock, modeBox, mode, Tables::modes, modeAtt);
    freq.init (crumbleBlock, apvts, "Frequency", ParamIDs::freq);
    rate.init (crumbleBlock, apvts, "Rate", ParamIDs::rate);
    texture.init (crumbleBlock, apvts, "Texture", ParamIDs::texture);
    amount.init (crumbleBlock, apvts, "Amount", ParamIDs::amount);
    stereo.init (crumbleBlock, apvts, "Stereo", ParamIDs::stereo);
    feedback.init (crumbleBlock, apvts, "Feedback", ParamIDs::feedback);

    // Bitcrush
    bcFreq.init (bitBlock, apvts, "Frequency", ParamIDs::bcFreq);
    bits.init (bitBlock, apvts, "Bits", ParamIDs::bits);
    quantize.init (bitBlock, apvts, "Quantize", ParamIDs::quantize);
    dither.init (bitBlock, apvts, "Dither", ParamIDs::dither);
    adcQ.init (bitBlock, apvts, "ADC Q", ParamIDs::adcQ);
    dacQ.init (bitBlock, apvts, "DAC Q", ParamIDs::dacQ);

    // Wet filter
    fltTarget = std::make_unique<Segmented> (*apvts.getParameter (ParamIDs::fltTarget), Tables::targets);
    filterBlock.addAndMakeVisible (*fltTarget);
    lowCut.init (filterBlock, apvts, "Low Cut", ParamIDs::lowCut);
    highCut.init (filterBlock, apvts, "High Cut", ParamIDs::highCut, true);

    // Color
    addCombo (colorBlock, driveTypeBox, ParamIDs::driveType, Tables::driveTypes, driveTypeAtt);
    colTarget = std::make_unique<Segmented> (*apvts.getParameter (ParamIDs::colTarget), Tables::targets);
    colorBlock.addAndMakeVisible (*colTarget);
    drive.init (colorBlock, apvts, "Drive", ParamIDs::drive);
    tone.init (colorBlock, apvts, "Tone", ParamIDs::tone);
    squash.init (colorBlock, apvts, "Squash", ParamIDs::squash);

    // Tremolo
    addCombo (tremBlock, trShapeBox, ParamIDs::trShape, Tables::trShapes, trShapeAtt);
    trTarget = std::make_unique<Segmented> (*apvts.getParameter (ParamIDs::trTarget), Tables::targets);
    tremBlock.addAndMakeVisible (*trTarget);
    trDepth.init (tremBlock, apvts, "Depth", ParamIDs::trDepth);
    trRate.init (tremBlock, apvts, "Rate", ParamIDs::trRate);
    trPan.init (tremBlock, apvts, "Pan", ParamIDs::trPan);

    // Mix
    autoGainButton.setWantsKeyboardFocus (false);
    mixBlock.addAndMakeVisible (autoGainButton);
    autoGainAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamIDs::autoGain, autoGainButton);
    mix.init (mixBlock, apvts, "Mix", ParamIDs::mix);

    // Clip
    clipMode = std::make_unique<Segmented> (*apvts.getParameter (ParamIDs::clipMode), Tables::clipModes);
    clipBlock.addAndMakeVisible (*clipMode);
    ceiling.init (clipBlock, apvts, "Ceiling", ParamIDs::ceiling);

    allKnobs = { &freq, &rate, &texture, &amount, &stereo, &feedback, &bcFreq, &bits, &quantize, &dither, &adcQ, &dacQ,
                 &lowCut, &highCut, &drive, &tone, &squash, &trDepth, &trRate, &trPan, &mix, &ceiling };

    content.addChildComponent (about);

    // Size and theme from the saved settings
    shownOrder = proc.apvts.getRawParameterValue (order)->load() > 0.5f;
    applyTheme (Theme::byName (loadSetting ("theme", "Asphalt")), false);
    scale = jlimit (0.6f, 2.0f, loadSetting ("scale", "1.0").getFloatValue());

    setResizable (true, true);
    setResizeLimits ((int) (baseW * 0.6f), (int) (baseH * 0.6f), baseW * 2, baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (roundToInt (baseW * scale), roundToInt (baseH * scale));

    timerCallback();
    startTimerHz (30);
}

CrumbleEditor::~CrumbleEditor()
{
    stopTimer();
    saveSetting ("scale", String (scale, 3));
    setLookAndFeel (nullptr);
}

void CrumbleEditor::resized()
{
    scale = (float) getWidth() / (float) baseW;
    content.setTransform (AffineTransform::scale (scale));
    content.setBounds (0, 0, baseW, baseH);

    presetBar.setBounds (466, 24, 330, 30);
    gear.setBounds (X1 - 40, 24, 40, 30);
    scope.setBounds (X0, 80, X1 - X0, 238);
    inCol.setBounds (10, 10, 72, baseH - 20);
    outCol.setBounds (baseW - 82, 10, 72, baseH - 20);
    about.setBounds (0, 0, baseW, baseH);

    // Row 2
    const int widths[5] = { 164, 236, 236, 130, 130 };
    const int gap = (X1 - X0 - (164 + 236 + 236 + 130 + 130)) / 4;
    int x = X0;
    Block* row2[5] = { &filterBlock, &colorBlock, &tremBlock, &mixBlock, &clipBlock };
    for (int i = 0; i < 5; ++i)
    {
        row2[i]->setBounds (x, row2Y, widths[i], rowH);
        x += widths[i] + gap;
    }
    if (clipBlock.getRight() != X1)
        clipBlock.setBounds (X1 - 130, row2Y, 130, rowH);

    // Inside the blocks
    const int ctlY = 32, knobY = 62;
    modeBox.setBounds (12, ctlY, 120, 24);
    for (int i = 0; i < 5; ++i)
    {
        Knob* k[5] = { &freq, &texture, &amount, &stereo, &feedback };
        k[i]->setBounds (12 + i * 78, knobY, 78);
        if (i == 0)
            rate.setBounds (12, knobY, 78);
    }
    Knob* bk[6] = { &bcFreq, &bits, &quantize, &dither, &adcQ, &dacQ };
    for (int i = 0; i < 6; ++i)
        bk[i]->setBounds (12 + i * 78, knobY, 78);

    fltTarget->setBounds (12, ctlY, fltTarget->getIdealWidth(), 24);
    lowCut.setBounds (10, knobY, 72);
    highCut.setBounds (82, knobY, 72);

    driveTypeBox.setBounds (12, ctlY, 100, 24);
    colTarget->setBounds (120, ctlY, colTarget->getIdealWidth(), 24);
    drive.setBounds (10, knobY, 72);
    tone.setBounds (82, knobY, 72);
    squash.setBounds (154, knobY, 72);

    trShapeBox.setBounds (12, ctlY, 106, 24);
    trTarget->setBounds (126, ctlY, trTarget->getIdealWidth(), 24);
    trDepth.setBounds (10, knobY, 72);
    trRate.setBounds (82, knobY, 72);
    trPan.setBounds (154, knobY, 72);

    autoGainButton.setBounds (8, ctlY, 116, 24);
    mix.setBounds (26, knobY, 78);

    clipMode->setBounds (12, ctlY, clipMode->getIdealWidth(), 24);
    ceiling.setBounds (26, knobY, 78);

    layoutRows (false);
}

void CrumbleEditor::layoutRows (bool animate)
{
    const bool bitFirst = shownOrder;
    const Rectangle<int> crumbleR (bitFirst ? X1 - crumbleW : X0, row1Y, crumbleW, rowH);
    const Rectangle<int> bitR (bitFirst ? X0 : X1 - bitW, row1Y, bitW, rowH);
    const Rectangle<int> swapR (bitFirst ? X0 + bitW : X0 + crumbleW, row1Y, swapW, rowH);
    if (animate)
    {
        auto& anim = Desktop::getInstance().getAnimator();
        anim.animateComponent (&crumbleBlock, crumbleR, 1.0f, 260, false, 1.0, 0.0);
        anim.animateComponent (&bitBlock, bitR, 1.0f, 260, false, 1.0, 0.0);
    }
    else
    {
        crumbleBlock.setBounds (crumbleR);
        bitBlock.setBounds (bitR);
    }
    swapButton.setBounds (swapR.withSizeKeepingCentre (48, 48));
}

void CrumbleEditor::timerCallback()
{
    // Meters
    inCol.push (proc.inPeak[0].exchange (0.0f), proc.inPeak[1].exchange (0.0f));
    outCol.push (proc.outPeak[0].exchange (0.0f), proc.outPeak[1].exchange (0.0f));

    // Block order changed (button, automation or preset)
    const bool order = proc.apvts.getRawParameterValue (ParamIDs::order)->load() > 0.5f;
    if (order != shownOrder)
    {
        shownOrder = order;
        layoutRows (true);
    }

    // Mode-dependent knobs
    const int mode = roundToInt (proc.apvts.getRawParameterValue (ParamIDs::mode)->load());
    freq.slider.setVisible (mode != 1);
    freq.label.setVisible (mode != 1);
    rate.slider.setVisible (mode == 1);
    rate.label.setVisible (mode == 1);

    // Dim the contents of switched-off blocks
    auto dim = [] (std::initializer_list<Knob*> knobs, std::initializer_list<Component*> others, bool off)
    {
        for (auto* k : knobs) k->setDimmed (off);
        for (auto* c : others) c->setAlpha (off ? 0.4f : 1.0f);
    };
    const bool crOff = ! crumbleBlock.isOn();
    dim ({ &freq, &rate, &amount, &stereo, &feedback }, { &modeBox }, crOff);
    texture.setDimmed (crOff || mode != 2);
    dim ({ &bcFreq, &bits, &quantize, &dither, &adcQ, &dacQ }, {}, ! bitBlock.isOn());
    dim ({ &lowCut, &highCut }, { fltTarget.get() }, ! filterBlock.isOn());
    dim ({ &drive, &tone, &squash }, { &driveTypeBox, colTarget.get() }, ! colorBlock.isOn());
    dim ({ &trDepth, &trRate, &trPan }, { &trShapeBox, trTarget.get() }, ! tremBlock.isOn());
    dim ({ &ceiling }, { clipMode.get() }, ! clipBlock.isOn());
}

void CrumbleEditor::applyTheme (const Theme& t, bool remember)
{
    theme = &t;
    laf.setTheme (t);
    for (auto* k : allKnobs)
        k->label.setColour (Label::textColourId, t.mute);
    if (remember)
        saveSetting ("theme", t.name);
    buildBackground();
    tintLogo();
    about.logo = logo;
    content.sendLookAndFeelChange();
    for (auto* k : allKnobs)
        k->label.setColour (Label::textColourId, t.mute);
    content.repaint();
}

void CrumbleEditor::setScale (float s, bool remember)
{
    scale = s;
    setSize (roundToInt (baseW * s), roundToInt (baseH * s));
    if (remember)
        saveSetting ("scale", String (s, 3));
}

void CrumbleEditor::showSettingsMenu()
{
    PopupMenu themes, sizes, menu;
    int id = 1;
    for (const auto& t : Theme::all())
        themes.addItem (id++, t.name, true, &t == theme);
    const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
    for (int i = 0; i < 5; ++i)
        sizes.addItem (100 + i, String (roundToInt (scales[i] * 100)) + "%", true, std::abs (scale - scales[i]) < 0.01f);
    menu.addSubMenu ("Theme", themes);
    menu.addSubMenu ("Size", sizes);
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&gear), [this, scales] (int r)
    {
        if (r >= 1 && r <= (int) Theme::all().size())
            applyTheme (Theme::all()[(size_t) (r - 1)], true);
        else if (r >= 100 && r < 105)
            setScale (scales[r - 100], true);
    });
}

void CrumbleEditor::Content::mouseDown (const MouseEvent& e)
{
    if (logoArea.contains (e.getPosition()))
    {
        editor.about.setVisible (true);
        editor.about.toFront (false);
    }
}

void CrumbleEditor::tintLogo()
{
    if (! logoRaw.isValid())
        return;
    logo = logoRaw.convertedToFormat (Image::ARGB);
    logo.duplicateIfShared();
    Image::BitmapData bd (logo, Image::BitmapData::readWrite);
    const auto tint = theme->logo;
    for (int y = 0; y < logo.getHeight(); ++y)
        for (int x = 0; x < logo.getWidth(); ++x)
        {
            const auto c = bd.getPixelColour (x, y);
            const float a = c.getFloatAlpha();
            if (a <= 0.0f)
                continue;
            const float lum = (c.getFloatRed() + c.getFloatGreen() + c.getFloatBlue()) / 3.0f;
            const float k = theme->lightBg ? lum : jlimit (0.0f, 1.3f, 0.30f + 0.95f * lum);
            auto ch = [k] (float v) { return (uint8) jlimit (0, 255, roundToInt (v * k * 255.0f)); };
            bd.setPixelColour (x, y, Colour (ch (tint.getFloatRed()), ch (tint.getFloatGreen()), ch (tint.getFloatBlue()), a));
        }
}

void CrumbleEditor::buildBackground()
{
    const auto& t = *theme;
    const int s = 2, W = baseW * s, H = baseH * s;
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

    Random rnd (1234);
    constexpr int gw = 30, gh = 18;
    float coarse[gw * gh];
    for (auto& v : coarse)
        v = rnd.nextFloat() * 14.0f - 7.0f;
    {
        Image::BitmapData bd (background, Image::BitmapData::writeOnly);
        const float br = t.bg.getFloatRed() * 255.0f, bgc = t.bg.getFloatGreen() * 255.0f, bb = t.bg.getFloatBlue() * 255.0f;
        auto ch = [] (float f) { return (uint8) jlimit (0, 255, roundToInt (f)); };
        for (int y = 0; y < H; ++y)
        {
            const float fy = (float) y / (float) (H - 1) * (gh - 1);
            const int y0 = jmin (gh - 2, (int) fy);
            const float ty = fy - (float) y0;
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x / (float) (W - 1) * (gw - 1);
                const int x0 = jmin (gw - 2, (int) fx);
                const float tx = fx - (float) x0;
                const float c = coarse[y0 * gw + x0] * (1 - tx) * (1 - ty) + coarse[y0 * gw + x0 + 1] * tx * (1 - ty)
                              + coarse[(y0 + 1) * gw + x0] * (1 - tx) * ty + coarse[(y0 + 1) * gw + x0 + 1] * tx * ty;
                const float n = c + (rnd.nextFloat() - 0.5f) * 30.0f;
                if (rnd.nextFloat() < 0.004f)
                {
                    const float m = 0.4f + rnd.nextFloat() * 0.6f;
                    bd.setPixelColour (x, y, t.bg.interpolatedWith (t.speck, m));
                }
                else
                {
                    bd.setPixelColour (x, y, Colour (ch (br + n), ch (bgc + n), ch (bb + n * 1.1f)));
                }
            }
        }
    }

    Graphics g (background);
    ColourGradient sheen (Colour (0xffb4c4dc).withAlpha (t.lightBg ? 0.10f : 0.06f), (float) W * 0.7f, (float) H * 0.12f,
                          Colours::transparentWhite, (float) W * 1.05f, (float) H * 0.12f, true);
    g.setGradientFill (sheen);
    g.fillEllipse ((float) W * 0.35f, 0.0f, (float) W * 0.7f, (float) H * 0.3f);
    ColourGradient vig (Colours::transparentBlack, (float) W * 0.5f, (float) H * 0.45f, t.vignette, 0.0f, 0.0f, true);
    g.setGradientFill (vig);
    g.fillRect (0, 0, W, H);
}

void CrumbleEditor::paintContent (Graphics& g)
{
    const auto& t = *theme;
    g.drawImage (background, Rectangle<float> (0.0f, 0.0f, (float) baseW, (float) baseH));

    if (logo.isValid())
        g.drawImage (logo, logoArea.toFloat(), RectanglePlacement::centred);
    g.setColour (t.ink);
    g.setFont (uiFont (30.0f, true, 0.08f));
    g.drawText ("CRUMBLE", logoArea.getRight() + 12, 12, 320, 34, Justification::centredLeft);
    g.setColour (t.mute);
    g.setFont (uiFont (11.0f, true, 0.3f));
    g.drawText ("PRIMAL RAGE LAB", logoArea.getRight() + 14, 46, 320, 16, Justification::centredLeft);
}
