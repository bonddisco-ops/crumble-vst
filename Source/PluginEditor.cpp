#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const juce::Colour kInk   (0xffececf0);
    const juce::Colour kMute  (0xffa5a5ad);
    const juce::Colour kPink  (0xffff73b2);
    const juce::Colour kPanel (0xff1b1b1f);

    constexpr int kEditorW = 916, kEditorH = 480;
    constexpr int kGroupTop = 266, kGroupH = 196, kCellW = 76;

    juce::Font font (float height, bool bold = false, float kerning = 0.0f)
    {
        juce::Font f (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
        if (kerning != 0.0f)
            f.setExtraKerningFactor (kerning);
        return f;
    }

    // Colour of glowing parts: dim grey-pink when low, bright pink-white when high
    juce::Colour glow (float t, float alpha = 1.0f)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        return juce::Colour::fromFloatRGBA (1.0f, (150.0f + 60.0f * (1.0f - t)) / 255.0f,
                                            (190.0f + 40.0f * (1.0f - t)) / 255.0f,
                                            (0.35f + 0.65f * t) * alpha);
    }

    juce::Point<float> polar (float cx, float cy, float radius, float angle)
    {
        return { cx + std::sin (angle) * radius, cy - std::cos (angle) * radius };
    }
}

//==============================================================================
CrumbleLookAndFeel::CrumbleLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff18181b));
    setColour (juce::Slider::textBoxTextColourId, kInk);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, kPink.withAlpha (0.4f));
    setColour (juce::TextEditor::backgroundColourId, kPanel);
    setColour (juce::TextEditor::textColourId, kInk);
    setColour (juce::TextEditor::highlightColourId, kPink.withAlpha (0.4f));
    setColour (juce::TextEditor::focusedOutlineColourId, kPink);
    setColour (juce::CaretComponent::caretColourId, kPink);
    setColour (juce::Label::textColourId, kInk);
    setColour (juce::ComboBox::backgroundColourId, kPanel);
    setColour (juce::ComboBox::textColourId, kInk);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.10f));
    setColour (juce::ComboBox::arrowColourId, kPink);
    setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setColour (juce::PopupMenu::textColourId, kInk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kPink);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff111114));
}

void CrumbleLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                           float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const auto bounds = Rectangle<int> (x, y, width, height).toFloat();
    const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 9.0f;
    if (R < 6.0f)
        return;

    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const bool enabled = slider.isEnabled();
    if (! enabled)
        g.beginTransparencyLayer (0.35f);

    const PathStrokeType arcStroke (3.0f, PathStrokeType::curved, PathStrokeType::rounded);

    // Value track and glowing value arc
    Path track;
    track.addCentredArc (cx, cy, R + 6.0f, R + 6.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.strokePath (track, arcStroke);

    if (pos > 0.002f)
    {
        Path value;
        value.addCentredArc (cx, cy, R + 6.0f, R + 6.0f, 0.0f, startAngle, startAngle + (endAngle - startAngle) * pos, true);
        g.setColour (glow (pos, 0.35f));
        g.strokePath (value, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (glow (pos));
        g.strokePath (value, arcStroke);
    }

    // Drop shadow
    {
        const float sr = R * 1.35f;
        ColourGradient shadow (Colours::black.withAlpha (0.65f), cx + 3.0f, cy + 7.0f,
                               Colours::transparentBlack, cx + 3.0f + sr, cy + 7.0f, true);
        g.setGradientFill (shadow);
        g.fillEllipse (cx + 3.0f - sr, cy + 7.0f - sr, sr * 2.0f, sr * 2.0f);
    }

    // Knurled rim
    for (int i = 0; i < 44; ++i)
    {
        const float a = (float) i / 44.0f * MathConstants<float>::twoPi;
        g.setColour (i % 2 == 0 ? Colour (0xff55555b) : Colour (0xff2b2b2e));
        g.drawLine (Line<float> (polar (cx, cy, R - 1.0f, a), polar (cx, cy, R + 2.0f, a)), 2.0f);
    }

    // Body: lit from above
    ColourGradient body (Colour (0xff6a6a70), cx, cy - R, Colour (0xff1c1c20), cx, cy + R, false);
    body.addColour (0.5, Colour (0xff38383d));
    g.setGradientFill (body);
    g.fillEllipse (cx - R + 1.0f, cy - R + 1.0f, R * 2.0f - 2.0f, R * 2.0f - 2.0f);

    // Cap with a soft highlight
    const float capR = R * 0.74f;
    ColourGradient cap (Colour (0xff8c8c93), cx - R * 0.3f, cy - R * 0.4f,
                        Colour (0xff26262a), cx - R * 0.3f + R * 0.95f, cy - R * 0.4f, true);
    cap.addColour (0.45, Colour (0xff4a4a50));
    g.setGradientFill (cap);
    g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

    Path rim;
    rim.addCentredArc (cx, cy, capR, capR, 0.0f, -1.1f, 1.1f, true);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.strokePath (rim, PathStrokeType (1.0f));

    // Pointer
    const float angle = startAngle + pos * (endAngle - startAngle);
    Path pointer;
    pointer.startNewSubPath (polar (cx, cy, R * 0.2f, angle));
    pointer.lineTo (polar (cx, cy, R * 0.66f, angle));
    g.setColour (Colour (0xfff0f0f4));
    g.strokePath (pointer, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    if (! enabled)
        g.endTransparencyLayer();
}

void CrumbleLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    using namespace juce;
    const auto b = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();
    const float alpha = button.isEnabled() ? 1.0f : 0.35f;

    const float d = 10.0f;
    const Rectangle<float> led (b.getX() + 5.0f, b.getCentreY() - d * 0.5f, d, d);

    if (on)
    {
        g.setColour (kPink.withAlpha (0.22f * alpha));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (Colour (0xff0e0e10).withAlpha (alpha));
    g.fillEllipse (led.expanded (1.5f));
    g.setColour ((on ? kPink : Colour (0xff3a3a40)).withAlpha (alpha));
    g.fillEllipse (led);

    g.setColour (kInk.withAlpha (alpha * (highlighted ? 1.0f : 0.85f)));
    g.setFont (font (13.0f));
    g.drawText (button.getButtonText(), b.withTrimmedLeft (d + 12.0f), Justification::centredLeft);
}

juce::Font CrumbleLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return font (13.0f);
}

juce::Font CrumbleLookAndFeel::getPopupMenuFont()
{
    return font (14.0f);
}

juce::Label* CrumbleLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (12.0f));
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineWhenEditingColourId, kPink);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::textColourId, kInk);
    return label;
}

//==============================================================================
WaveDisplay::WaveDisplay (CrumbleProcessor& p)
    : proc (p),
      pAmount (p.apvts.getRawParameterValue ("amount")),
      pDrive  (p.apvts.getRawParameterValue ("drive")),
      pSquash (p.apvts.getRawParameterValue ("squash")),
      pStereo (p.apvts.getRawParameterValue ("stereo")),
      pMode   (p.apvts.getRawParameterValue ("mode"))
{
    setInterceptsMouseClicks (false, false);
    particles.reserve (700);
    startTimerHz (30);
}

void WaveDisplay::timerCallback()
{
    time += 1.0f / 30.0f;
    for (auto& pt : particles)
    {
        pt.vy += 0.12f;
        pt.x += pt.vx;
        pt.y += pt.vy;
        pt.life -= 0.03f;
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(),
                                     [] (const Particle& pt) { return pt.life <= 0.0f; }),
                     particles.end());
    repaint();
}

void WaveDisplay::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto r = getLocalBounds().toFloat();

    g.setColour (Colours::black.withAlpha (0.38f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    g.saveState();
    g.reduceClipRegion (r.reduced (1.0f).toNearestInt());

    const float amount = pAmount->load() / 100.0f;
    const float drive  = pDrive->load() / 100.0f;
    const float squash = pSquash->load() / 100.0f;
    const float stereo = pStereo->load() / 100.0f;
    const bool noise = roundToInt (pMode->load()) == 1;
    const float t = jlimit (0.0f, 1.0f, amount * 0.85f + drive * 0.35f + squash * 0.15f);

    const float cy = r.getCentreY();
    const float halfH = r.getHeight() * 0.42f;
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (roundToInt (cy), r.getX() + 8.0f, r.getRight() - 8.0f);

    // Copy the latest waveform, oldest first
    constexpr int N = CrumbleProcessor::Scope::size;
    const int count = proc.scope.writeCount.load (std::memory_order_acquire);
    float vals[2][N];
    float diff = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        const int idx = (count + i) % N;
        vals[0][i] = proc.scope.data[0][idx];
        vals[1][i] = proc.scope.data[1][idx];
        diff = jmax (diff, std::abs (vals[0][i] - vals[1][i]));
    }

    const float br = (90.0f + 165.0f * t) / 255.0f;
    const Colour mainCol = Colour::fromFloatRGBA (br, br * (1.0f - 0.55f * t), br * (1.0f - 0.3f * t), 1.0f);
    const Colour sideCol = Colour::fromFloatRGBA (br * (1.0f - 0.45f * t), br * (1.0f - 0.15f * t), br, 0.8f);
    const float jitter = amount * amount;
    const float lineW = 1.0f + t * 1.2f + drive * 0.6f;

    auto drawChannel = [&] (const float* v, Colour col, bool spawn)
    {
        Path path;
        bool started = false;
        const float x0 = r.getX() + 8.0f, span = r.getWidth() - 16.0f;
        for (int i = 0; i < N; ++i)
        {
            const float x = x0 + span * (float) i / (float) (N - 1);
            const float s = v[i];
            const float shaped = (s < 0.0f ? -1.0f : 1.0f) * std::pow (jmin (1.0f, std::abs (s)), 0.8f);
            const float off = noise ? (rng.nextFloat() - 0.5f) * jitter * 60.0f
                                    : std::sin ((float) i * 0.7f + time * 40.0f) * jitter * 26.0f;
            const float y = cy - shaped * halfH + off;

            if (noise && amount > 0.5f && rng.nextFloat() < (amount - 0.5f) * 0.12f)
            {
                started = false;   // the line breaks apart
                continue;
            }
            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }

            if (spawn && particles.size() < 650 && rng.nextFloat() < t * t * 0.05f)
                particles.push_back ({ x, y, (rng.nextFloat() - 0.5f) * 1.2f, rng.nextFloat() * 0.8f, 1.0f });
        }
        g.setColour (col);
        g.strokePath (path, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
    };

    if (stereo > 0.02f && diff > 0.002f)
        drawChannel (vals[1], sideCol, false);
    drawChannel (vals[0], mainCol, true);

    for (const auto& pt : particles)
    {
        g.setColour (mainCol.withAlpha (jlimit (0.0f, 1.0f, pt.life)));
        g.fillRect (pt.x, pt.y, 2.0f, 2.0f);
    }

    g.restoreState();
}

//==============================================================================
void CrumbleEditor::prepareButton (juce::Button& b)
{
    b.setWantsKeyboardFocus (false);
    b.setMouseClickGrabsKeyboardFocus (false);
    addAndMakeVisible (b);
}

void CrumbleEditor::prepareCombo (juce::ComboBox& c)
{
    c.setWantsKeyboardFocus (false);
    c.setMouseClickGrabsKeyboardFocus (false);
    addAndMakeVisible (c);
}

void CrumbleEditor::initKnob (Knob& k, const juce::String& name, const juce::String& paramId)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kCellW, 18);
    k.slider.setWantsKeyboardFocus (false);
    k.slider.setMouseClickGrabsKeyboardFocus (false);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, kInk);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (font (12.0f, true));
    k.label.setColour (juce::Label::textColourId, kMute);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramId, k.slider);

    if (auto* param = proc.apvts.getParameter (paramId))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

CrumbleEditor::CrumbleEditor (CrumbleProcessor& p)
    : AudioProcessorEditor (&p), proc (p), wave (p)
{
    setLookAndFeel (&laf);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    addAndMakeVisible (wave);

    modeBox.addItemList (proc.apvts.getParameter ("mode")->getAllValueStrings(), 1);
    prepareCombo (modeBox);
    modeAttachment = std::make_unique<ComboBoxAttachment> (proc.apvts, "mode", modeBox);

    rateBox.addItemList (CrumbleProcessor::rateNames, 1);
    prepareCombo (rateBox);
    rateAttachment = std::make_unique<ComboBoxAttachment> (proc.apvts, "rate", rateBox);

    prepareButton (syncButton);
    syncAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "sync", syncButton);
    prepareButton (autoGainButton);
    autoGainAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "autogain", autoGainButton);

    initKnob (freq,    "Frequency", "freq");
    initKnob (texture, "Texture",   "texture");
    initKnob (amount,  "Amount",    "amount");
    initKnob (stereo,  "Stereo",    "stereo");
    initKnob (lowCut,  "Low Cut",   "lowcut");
    initKnob (highCut, "High Cut",  "highcut");
    initKnob (drive,   "Drive",     "drive");
    initKnob (squash,  "Squash",    "squash");
    initKnob (mix,     "Mix",       "mix");
    initKnob (output,  "Output",    "output");

    groups = { { "CRUMBLE", {} }, { "BAND", {} }, { "COLOR", {} }, { "OUTPUT", {} } };

    setSize (kEditorW, kEditorH);
    buildBackground();

    timerCallback();
    startTimerHz (10);
}

CrumbleEditor::~CrumbleEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CrumbleEditor::timerCallback()
{
    const bool sine = juce::roundToInt (proc.apvts.getRawParameterValue ("mode")->load()) == 0;
    const bool sync = proc.apvts.getRawParameterValue ("sync")->load() > 0.5f;

    syncButton.setEnabled (sine);
    rateBox.setEnabled (sine && sync);
    freq.slider.setEnabled (! (sine && sync));
    texture.slider.setEnabled (! sine);
}

void CrumbleEditor::buildBackground()
{
    using namespace juce;
    const int scale = 2;
    const int W = kEditorW * scale, H = kEditorH * scale;
    background = Image (Image::RGB, W, H, false);

    Random rnd (1234);
    constexpr int gw = 26, gh = 14;
    float coarse[gw * gh];
    for (auto& v : coarse)
        v = rnd.nextFloat() * 14.0f - 7.0f;

    {
        Image::BitmapData bd (background, Image::BitmapData::writeOnly);
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

                float v = 24.0f + c + (rnd.nextFloat() - 0.5f) * 30.0f;
                if (rnd.nextFloat() < 0.004f)
                    v += 60.0f + rnd.nextFloat() * 80.0f;   // wet glints

                const auto ch = [] (float f) { return (uint8) jlimit (0, 255, roundToInt (f)); };
                bd.setPixelColour (x, y, Colour (ch (v), ch (v + 1.0f), ch (v + 4.0f)));
            }
        }
    }

    Graphics gb (background);
    // Soft sheen, like light on wet asphalt
    auto sheen = [&gb] (float x, float y, float rx, float alpha)
    {
        ColourGradient grad (Colour (0xffb4c4dc).withAlpha (alpha), x, y, Colours::transparentWhite, x + rx, y, true);
        gb.setGradientFill (grad);
        gb.fillEllipse (x - rx, y - rx * 0.35f, rx * 2.0f, rx * 0.7f);
    };
    sheen ((float) W * 0.70f, (float) H * 0.16f, (float) W * 0.32f, 0.07f);
    sheen ((float) W * 0.18f, (float) H * 0.80f, (float) W * 0.25f, 0.04f);

    // Vignette
    ColourGradient vignette (Colours::transparentBlack, (float) W * 0.5f, (float) H * 0.45f,
                             Colours::black.withAlpha (0.45f), 0.0f, 0.0f, true);
    gb.setGradientFill (vignette);
    gb.fillRect (0, 0, W, H);
}

void CrumbleEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.drawImage (background, getLocalBounds().toFloat());

    // Header
    if (logo.isValid())
        g.drawImage (logo, Rectangle<float> (20.0f, 10.0f, 56.0f, 58.0f), RectanglePlacement::centred);

    g.setColour (kInk);
    g.setFont (font (30.0f, true, 0.08f));
    g.drawText ("CRUMBLE", 88, 12, 320, 34, Justification::centredLeft);
    g.setColour (kMute);
    g.setFont (font (11.0f, true, 0.3f));
    g.drawText ("PRIMAL RAGE LAB", 90, 46, 320, 16, Justification::centredLeft);

    // Control groups
    for (const auto& grp : groups)
    {
        const auto a = grp.area.toFloat();
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (a, 10.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (kMute);
        g.setFont (font (11.0f, true, 0.2f));
        g.drawText (grp.title, grp.area.getX() + 12, grp.area.getY() + 8, 200, 16, Justification::centredLeft);
    }
}

void CrumbleEditor::resized()
{
    wave.setBounds (20, 78, getWidth() - 40, 172);

    const int widths[4] = { kCellW * 4 + 20, kCellW * 2 + 20, kCellW * 2 + 20, kCellW * 2 + 20 };
    int gx = 20;
    for (size_t i = 0; i < groups.size(); ++i)
    {
        groups[i].area = { gx, kGroupTop, widths[i], kGroupH };
        gx += widths[i] + 12;
    }

    const int rowY = kGroupTop + 30;
    const auto& crumble = groups[0].area;
    modeBox.setBounds (crumble.getX() + 12, rowY, 100, 24);
    syncButton.setBounds (crumble.getX() + 122, rowY, 72, 24);
    rateBox.setBounds (crumble.getX() + 196, rowY, 80, 24);
    autoGainButton.setBounds (groups[3].area.getX() + 12, rowY, 120, 24);

    auto placeKnobs = [] (const juce::Rectangle<int>& area, std::initializer_list<Knob*> knobs)
    {
        int x = area.getX() + 10;
        for (auto* k : knobs)
        {
            k->label.setBounds (x, kGroupTop + 62, kCellW, 16);
            k->slider.setBounds (x, kGroupTop + 80, kCellW, 104);
            x += kCellW;
        }
    };
    placeKnobs (groups[0].area, { &freq, &texture, &amount, &stereo });
    placeKnobs (groups[1].area, { &lowCut, &highCut });
    placeKnobs (groups[2].area, { &drive, &squash });
    placeKnobs (groups[3].area, { &mix, &output });
}
