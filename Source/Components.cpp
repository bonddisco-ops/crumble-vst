#include "Components.h"

using namespace juce;

namespace
{
    const Theme& themeOf (const Component& c)
    {
        if (auto* l = dynamic_cast<CrumbleLookAndFeel*> (&c.getLookAndFeel()))
            return l->theme();
        return Theme::all().front();
    }

    const Colour kWidget (0xff1b1b1f);   // combo boxes and switches stay dark in every theme
    const Colour kWidgetInk (0xffececf0);

    Point<float> polar (float cx, float cy, float r, float a) { return { cx + std::sin (a) * r, cy - std::cos (a) * r }; }
}

Font uiFont (float height, bool bold, float kerning)
{
    Font f (FontOptions (height, bold ? Font::bold : Font::plain));
    if (kerning != 0.0f)
        f.setExtraKerningFactor (kerning);
    return f;
}

//==============================================================================
CrumbleLookAndFeel::CrumbleLookAndFeel()
{
    setTheme (Theme::all().front());
}

void CrumbleLookAndFeel::setTheme (const Theme& t)
{
    current = &t;
    setColour (ResizableWindow::backgroundColourId, t.bg);
    setColour (Slider::textBoxTextColourId, t.ink);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, t.accent.withAlpha (0.4f));
    setColour (Label::textColourId, t.ink);
    setColour (Label::outlineWhenEditingColourId, t.accent);
    setColour (TextEditor::backgroundColourId, kWidget);
    setColour (TextEditor::textColourId, kWidgetInk);
    setColour (TextEditor::highlightColourId, t.accent.withAlpha (0.4f));
    setColour (TextEditor::focusedOutlineColourId, t.accent);
    setColour (TextEditor::outlineColourId, Colours::white.withAlpha (0.12f));
    setColour (CaretComponent::caretColourId, t.accent);
    setColour (ComboBox::backgroundColourId, kWidget);
    setColour (ComboBox::textColourId, kWidgetInk);
    setColour (ComboBox::outlineColourId, Colours::white.withAlpha (0.10f));
    setColour (ComboBox::arrowColourId, t.accent);
    setColour (PopupMenu::backgroundColourId, kWidget);
    setColour (PopupMenu::textColourId, kWidgetInk);
    setColour (PopupMenu::headerTextColourId, t.accent);
    setColour (PopupMenu::highlightedBackgroundColourId, t.accent);
    setColour (PopupMenu::highlightedTextColourId, Colour (0xff111114));
    setColour (AlertWindow::backgroundColourId, kWidget);
    setColour (AlertWindow::textColourId, kWidgetInk);
    setColour (AlertWindow::outlineColourId, t.accent);
    setColour (TextButton::buttonColourId, Colour (0xff26262b));
    setColour (TextButton::textColourOffId, kWidgetInk);
    setColour (ToggleButton::textColourId, t.ink);
}

void CrumbleLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height, float pos,
                                           float startAngle, float endAngle, Slider& slider)
{
    const auto& t = theme();
    const auto bounds = Rectangle<int> (x, y, width, height).toFloat();
    const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 9.0f;
    if (R < 6.0f)
        return;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();

    // Value arc; a "reverse" knob (High Cut) lights up from its right end
    Path track;
    track.addCentredArc (cx, cy, R + 6.0f, R + 6.0f, 0.0f, startAngle, endAngle, true);
    g.setColour ((t.lightBg ? Colours::black : Colours::white).withAlpha (0.10f));
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const bool reverse = (bool) slider.getProperties().getWithDefault ("reverseArc", false);
    const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
    const float valueAngle = startAngle + (endAngle - startAngle) * pos;
    float a0 = startAngle, a1 = valueAngle, lit = pos;
    if (reverse)      { a0 = valueAngle; a1 = endAngle; lit = 1.0f - pos; }
    else if (bipolar) { const float mid = (startAngle + endAngle) * 0.5f; a0 = jmin (mid, valueAngle); a1 = jmax (mid, valueAngle); lit = std::abs (pos - 0.5f) * 2.0f; }
    if (lit > 0.002f)
    {
        Path value;
        value.addCentredArc (cx, cy, R + 6.0f, R + 6.0f, 0.0f, a0, a1, true);
        g.setColour (t.glow (lit, 0.35f));
        g.strokePath (value, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (t.glow (lit));
        g.strokePath (value, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Drop shadow
    {
        const float sr = R * 1.35f;
        ColourGradient shadow (Colours::black.withAlpha (t.lightBg ? 0.45f : 0.65f), cx + 3.0f, cy + 7.0f,
                               Colours::transparentBlack, cx + 3.0f + sr, cy + 7.0f, true);
        g.setGradientFill (shadow);
        g.fillEllipse (cx + 3.0f - sr, cy + 7.0f - sr, sr * 2.0f, sr * 2.0f);
    }

    if (t.gloss)
    {
        // Gold rim, black glass body, sharp highlight
        ColourGradient rim (Colour (0xfff6dc9a), cx, cy - R - 2.0f, Colour (0xffd9b96a), cx, cy + R + 2.0f, false);
        rim.addColour (0.5, Colour (0xff8a6a2a));
        g.setGradientFill (rim);
        g.fillEllipse (cx - R - 2.0f, cy - R - 2.0f, (R + 2.0f) * 2.0f, (R + 2.0f) * 2.0f);
        ColourGradient body (Colour (0xff3a3a42), cx, cy - R * 0.5f, Colour (0xff050506), cx, cy + R, true);
        g.setGradientFill (body);
        g.fillEllipse (cx - R + 0.5f, cy - R + 0.5f, R * 2.0f - 1.0f, R * 2.0f - 1.0f);
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addEllipse (cx - R + 1.0f, cy - R + 1.0f, R * 2.0f - 2.0f, R * 2.0f - 2.0f);
        g.reduceClipRegion (clip);
        ColourGradient spec (Colours::white.withAlpha (0.55f), cx, cy - R, Colours::white.withAlpha (0.0f), cx, cy, false);
        g.setGradientFill (spec);
        g.fillEllipse (cx - R * 0.75f, cy - R * 0.87f, R * 1.5f, R * 0.84f);
    }
    else
    {
        // Knurled rim, metal body lit from above, cap
        for (int i = 0; i < 44; ++i)
        {
            const float a = (float) i / 44.0f * MathConstants<float>::twoPi;
            g.setColour (i % 2 == 0 ? Colour (0xff55555b) : Colour (0xff2b2b2e));
            g.drawLine (Line<float> (polar (cx, cy, R - 1.0f, a), polar (cx, cy, R + 2.0f, a)), 2.0f);
        }
        ColourGradient body (Colour (0xff6a6a70), cx, cy - R, Colour (0xff1c1c20), cx, cy + R, false);
        body.addColour (0.5, Colour (0xff38383d));
        g.setGradientFill (body);
        g.fillEllipse (cx - R + 1.0f, cy - R + 1.0f, R * 2.0f - 2.0f, R * 2.0f - 2.0f);
        const float capR = R * 0.74f;
        ColourGradient cap (Colour (0xff8c8c93), cx - R * 0.3f, cy - R * 0.4f, Colour (0xff26262a), cx - R * 0.3f + R * 0.95f, cy - R * 0.4f, true);
        cap.addColour (0.45, Colour (0xff4a4a50));
        g.setGradientFill (cap);
        g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
        Path rimHi;
        rimHi.addCentredArc (cx, cy, capR, capR, 0.0f, -1.1f, 1.1f, true);
        g.setColour (Colours::white.withAlpha (0.18f));
        g.strokePath (rimHi, PathStrokeType (1.0f));
    }

    Path pointer;
    pointer.startNewSubPath (polar (cx, cy, R * 0.2f, valueAngle));
    pointer.lineTo (polar (cx, cy, R * 0.66f, valueAngle));
    g.setColour (t.gloss ? Colour (0xfff6dc9a) : Colour (0xfff0f0f4));
    g.strokePath (pointer, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

void CrumbleLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& button, bool highlighted, bool)
{
    const auto& t = theme();
    const auto b = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();
    const float d = 10.0f;
    const Rectangle<float> led (b.getX() + 5.0f, b.getCentreY() - d * 0.5f, d, d);
    if (on)
    {
        g.setColour (t.accent.withAlpha (0.22f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (Colour (0xff0e0e10));
    g.fillEllipse (led.expanded (1.5f));
    g.setColour (on ? t.accent : Colour (0xff3a3a40));
    g.fillEllipse (led);

    const bool onDark = (bool) button.getProperties().getWithDefault ("onDark", false);
    g.setColour ((onDark || ! t.lightBg ? kWidgetInk : t.ink).withAlpha (highlighted ? 1.0f : 0.88f));
    g.setFont (uiFont (13.0f));
    g.drawText (button.getButtonText(), b.withTrimmedLeft (d + 12.0f), Justification::centredLeft);
}

void CrumbleLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto& t = theme();
    const auto r = Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (kWidget);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 5.0f, ay - 3.0f);
    arrow.lineTo (ax, ay + 3.0f);
    arrow.lineTo (ax + 5.0f, ay - 3.0f);
    g.setColour (box.isEnabled() ? t.accent : t.accent.withAlpha (0.4f));
    g.strokePath (arrow, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

void CrumbleLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

Font CrumbleLookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (13.0f); }
Font CrumbleLookAndFeel::getPopupMenuFont()          { return uiFont (14.0f); }

Label* CrumbleLookAndFeel::createSliderTextBox (Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (uiFont (13.0f));
    label->setColour (Label::outlineColourId, Colours::transparentBlack);
    label->setColour (Label::backgroundColourId, Colours::transparentBlack);
    label->setColour (Label::textColourId, theme().ink);
    label->setColour (Label::outlineWhenEditingColourId, theme().accent);
    label->setColour (TextEditor::backgroundColourId, kWidget);
    label->setColour (TextEditor::textColourId, kWidgetInk);
    return label;
}

//==============================================================================
void Knob::init (Component& parent, AudioProcessorValueTreeState& apvts, const String& name,
                 const String& paramId, bool reverseArc)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 18);
    slider.setWantsKeyboardFocus (false);
    slider.setMouseClickGrabsKeyboardFocus (false);
    slider.getProperties().set ("reverseArc", reverseArc);
    parent.addAndMakeVisible (slider);

    label.setText (name, dontSendNotification);
    label.setFont (uiFont (12.0f, true));
    label.setJustificationType (Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    parent.addAndMakeVisible (label);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    if (auto* p = apvts.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        const auto range = p->getNormalisableRange();
        slider.getProperties().set ("bipolar", range.start < 0.0f && range.end > 0.0f && std::abs (range.start + range.end) < 0.001f);
    }
}

void Knob::setBounds (int x, int y, int w)
{
    label.setBounds (x - 6, y, w + 12, 16);
    slider.setBounds (x, y + 18, w, 92);
}

void Knob::setDimmed (bool dimmed)
{
    slider.setAlpha (dimmed ? 0.38f : 1.0f);
    label.setAlpha (dimmed ? 0.45f : 1.0f);
}

//==============================================================================
Segmented::Segmented (RangedAudioParameter& param, const StringArray& names)
    : items (names),
      attachment (param, [this] (float v) { selected = roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
}

int Segmented::getIdealWidth() const
{
    int w = 0;
    for (const auto& s : items)
        w += GlyphArrangement::getStringWidthInt (uiFont (13.0f), s) + 22;
    return w;
}

void Segmented::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const float total = (float) getIdealWidth();
    float x = 0.0f;
    const float h = (float) getHeight();
    Path outline;
    outline.addRoundedRectangle (0.5f, 0.5f, (float) getWidth() - 1.0f, h - 1.0f, 6.0f);
    g.setColour (kWidget);
    g.fillPath (outline);
    Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (outline);
    for (int i = 0; i < items.size(); ++i)
    {
        const float w = (float) (GlyphArrangement::getStringWidthInt (uiFont (13.0f), items[i]) + 22) * (float) getWidth() / total;
        const Rectangle<float> r (x, 0.0f, w, h);
        if (i == selected)
        {
            g.setColour (t.accent);
            g.fillRect (r);
        }
        g.setColour (i == selected ? Colour (0xff111114) : kWidgetInk);
        g.setFont (uiFont (13.0f));
        g.drawText (items[i], r, Justification::centred);
        x += w;
    }
    g.setColour (Colours::white.withAlpha (0.10f));
    g.strokePath (outline, PathStrokeType (1.0f));
}

void Segmented::mouseDown (const MouseEvent& e)
{
    const float total = (float) getIdealWidth();
    float x = 0.0f;
    for (int i = 0; i < items.size(); ++i)
    {
        x += (float) (GlyphArrangement::getStringWidthInt (uiFont (13.0f), items[i]) + 22) * (float) getWidth() / total;
        if ((float) e.x < x)
        {
            attachment.setValueAsCompleteGesture ((float) i);
            return;
        }
    }
}

//==============================================================================
void PowerButton::paintButton (Graphics& g, bool highlighted, bool)
{
    const auto& t = themeOf (*this);
    const auto c = getLocalBounds().toFloat().getCentre();
    const bool on = getToggleState();
    Colour col = on ? t.accent : (t.lightBg ? t.mute.withAlpha (0.7f) : Colour (0xff55555b));
    if (highlighted)
        col = col.brighter (0.2f);
    Path p;
    p.addCentredArc (c.x, c.y, 6.0f, 6.0f, 0.0f, MathConstants<float>::pi * 0.22f, MathConstants<float>::pi * 1.78f, true);
    p.startNewSubPath (c.x, c.y - 8.0f);
    p.lineTo (c.x, c.y - 1.5f);
    g.setColour (col);
    g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

void IconButton::paintButton (Graphics& g, bool highlighted, bool down)
{
    const auto& t = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    Colour col = t.ink.withAlpha (highlighted ? 1.0f : 0.8f);
    if (down)
        col = t.accent;
    const PathStrokeType stroke (2.0f, PathStrokeType::curved, PathStrokeType::rounded);
    Path p;
    switch (icon)
    {
        case Icon::prev:
            p.startNewSubPath (c.x + 3.0f, c.y - 7.0f); p.lineTo (c.x - 4.0f, c.y); p.lineTo (c.x + 3.0f, c.y + 7.0f);
            g.setColour (col); g.strokePath (p, stroke); break;
        case Icon::next:
            p.startNewSubPath (c.x - 3.0f, c.y - 7.0f); p.lineTo (c.x + 4.0f, c.y); p.lineTo (c.x - 3.0f, c.y + 7.0f);
            g.setColour (col); g.strokePath (p, stroke); break;
        case Icon::save:
        {
            const auto b = Rectangle<float> (c.x - 11.0f, c.y - 11.0f, 22.0f, 22.0f);
            g.setColour (col);
            g.drawRoundedRectangle (b, 3.0f, 1.6f);
            g.fillRect (b.getX() + 5.0f, b.getY() + 2.0f, 12.0f, 7.0f);
            g.drawRect (Rectangle<float> (b.getX() + 5.0f, b.getY() + 13.0f, 12.0f, 7.0f), 1.4f);
            break;
        }
        case Icon::gear:
        {
            g.setColour (kWidget);
            g.fillRoundedRectangle (r.reduced (0.5f), 6.0f);
            g.setColour (Colours::white.withAlpha (0.12f));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (kWidgetInk.withAlpha (highlighted ? 1.0f : 0.85f));
            for (int k = 0; k < 8; ++k)
            {
                const float a = (float) k * MathConstants<float>::pi / 4.0f;
                g.drawLine (c.x + std::cos (a) * 6.0f, c.y + std::sin (a) * 6.0f, c.x + std::cos (a) * 9.0f, c.y + std::sin (a) * 9.0f, 2.0f);
            }
            g.drawEllipse (c.x - 6.0f, c.y - 6.0f, 12.0f, 12.0f, 2.0f);
            g.drawEllipse (c.x - 2.2f, c.y - 2.2f, 4.4f, 4.4f, 1.6f);
            break;
        }
        case Icon::swap:
        {
            g.setColour (highlighted ? t.accentHi : t.accent);
            p.startNewSubPath (c.x - 16.0f, c.y - 7.0f); p.lineTo (c.x + 16.0f, c.y - 7.0f);
            p.startNewSubPath (c.x + 9.0f, c.y - 14.0f); p.lineTo (c.x + 16.0f, c.y - 7.0f); p.lineTo (c.x + 9.0f, c.y);
            p.startNewSubPath (c.x + 16.0f, c.y + 7.0f); p.lineTo (c.x - 16.0f, c.y + 7.0f);
            p.startNewSubPath (c.x - 9.0f, c.y); p.lineTo (c.x - 16.0f, c.y + 7.0f); p.lineTo (c.x - 9.0f, c.y + 14.0f);
            g.strokePath (p, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
            break;
        }
    }
}

//==============================================================================
Block::Block (const String& t, AudioProcessorValueTreeState& state, const char* powerParam)
    : title (t), apvts (state)
{
    if (powerParam != nullptr)
    {
        power = std::make_unique<PowerButton>();
        addAndMakeVisible (*power);
        powerAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, powerParam, *power);
        power->onStateChange = [this] { if (onPowerChanged) onPowerChanged (isOn()); repaint(); };
    }
    setInterceptsMouseClicks (false, true);
}

bool Block::isOn() const { return power == nullptr || power->getToggleState(); }

void Block::resized()
{
    if (power != nullptr)
        power->setBounds (6, 4, 24, 24);
}

void Block::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (t.gloss)
    {
        ColourGradient fill (Colours::white.withAlpha (0.07f), 0.0f, r.getY(), Colours::black.withAlpha (0.25f), 0.0f, r.getBottom(), false);
        fill.addColour (0.08, Colours::white.withAlpha (0.02f));
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, 10.0f);
        ColourGradient edge (Colour ((uint8) 240, 210, 140, 0.55f), 0.0f, r.getY(), Colour ((uint8) 232, 196, 106, 0.30f), 0.0f, r.getBottom(), false);
        edge.addColour (0.5, Colour ((uint8) 232, 196, 106, 0.12f));
        g.setGradientFill (edge);
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    }
    else
    {
        g.setColour (t.panelFill);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (t.panelStroke);
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    }
    g.setColour (isOn() ? t.mute : t.mute.withAlpha (0.5f));
    g.setFont (uiFont (11.0f, true, 0.2f));
    g.drawText (title, power != nullptr ? 32 : 12, 8, getWidth() - 40, 16, Justification::centredLeft);
}

//==============================================================================
ScopeView::ScopeView (CrumbleProcessor& p) : proc (p)
{
    followButton.setWantsKeyboardFocus (false);
    followButton.getProperties().set ("onDark", true);
    addAndMakeVisible (followButton);
    followAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::folOn, followButton);
    followMode = std::make_unique<Segmented> (*proc.apvts.getParameter (ParamIDs::folMode), Tables::folModes);
    addAndMakeVisible (*followMode);
    thresholdParam = proc.apvts.getParameter (ParamIDs::folThresh);
    particles.reserve (700);
    startTimerHz (30);
}

Rectangle<float> ScopeView::screen() const
{
    return getLocalBounds().toFloat().reduced (12.0f);
}

void ScopeView::resized()
{
    const auto s = screen();
    followButton.setBounds ((int) s.getX() + 18, (int) s.getY() + 14, 86, 26);
    followMode->setBounds ((int) s.getX() + 106, (int) s.getY() + 15, followMode->getIdealWidth(), 24);
}

Point<float> ScopeView::bulge (float x, float y) const
{
    const auto s = screen();
    const float cx = s.getCentreX(), cy = s.getCentreY();
    const float nx = (x - cx) / (s.getWidth() * 0.5f), ny = (y - cy) / (s.getHeight() * 0.5f);
    const float k = 1.0f - 0.08f * (nx * nx + ny * ny);
    return { cx + nx * k * s.getWidth() * 0.5f, cy + ny * k * s.getHeight() * 0.5f };
}

float ScopeView::thresholdY (bool upper) const
{
    const auto s = screen();
    const float lin = Decibels::decibelsToGain (proc.apvts.getRawParameterValue (ParamIDs::folThresh)->load());
    const float off = jmin (1.15f, lin * ampZoom) * s.getHeight() * 0.42f;
    return s.getCentreY() + (upper ? -off : off);
}

bool ScopeView::nearThreshold (float y) const
{
    if (proc.apvts.getRawParameterValue (ParamIDs::folOn)->load() < 0.5f)
        return false;
    return std::abs (y - thresholdY (true)) < 7.0f || std::abs (y - thresholdY (false)) < 7.0f;
}

void ScopeView::mouseMove (const MouseEvent& e)
{
    setMouseCursor (nearThreshold ((float) e.y) ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
}

void ScopeView::mouseDown (const MouseEvent& e)
{
    draggingThreshold = nearThreshold ((float) e.y);
    if (draggingThreshold)
        thresholdParam->beginChangeGesture();
}

void ScopeView::mouseDrag (const MouseEvent& e)
{
    if (! draggingThreshold)
        return;
    const auto s = screen();
    const float off = std::abs ((float) e.y - s.getCentreY()) / (s.getHeight() * 0.42f) / ampZoom;
    const float db = jlimit (-60.0f, 0.0f, Decibels::gainToDecibels (off, -60.0f));
    thresholdParam->setValueNotifyingHost (thresholdParam->convertTo0to1 (db));
}

void ScopeView::mouseUp (const MouseEvent&)
{
    if (draggingThreshold)
        thresholdParam->endChangeGesture();
    draggingThreshold = false;
}

void ScopeView::mouseDoubleClick (const MouseEvent&)
{
    ampZoom = 1.0f;
    timeWindow = 0.6f;
}

void ScopeView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const bool timeAxis = e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY);
    const float d = timeAxis ? (std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) : w.deltaY;
    if (timeAxis)
        timeWindow = jlimit (0.05f, 2.0f, timeWindow * std::pow (1.6f, -d * 2.0f));
    else
        ampZoom = jlimit (0.25f, 16.0f, ampZoom * std::pow (1.6f, d * 2.0f));
}

void ScopeView::timerCallback()
{
    time += 1.0f / 30.0f;
    for (auto& pt : particles)
    {
        pt.vy += 0.12f;
        pt.x += pt.vx;
        pt.y += pt.vy;
        pt.life -= 0.03f;
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(), [] (const Particle& pt) { return pt.life <= 0.0f; }),
                     particles.end());
    repaint();
}

void ScopeView::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto outer = getLocalBounds().toFloat();
    const auto s = screen();

    // Bezel
    if (t.gloss)
    {
        ColourGradient bez (Colour (0xfff3d892), 0.0f, outer.getY(), Colour (0xffe2c47a), 0.0f, outer.getBottom(), false);
        bez.addColour (0.12, Colour (0xff9a7a35));
        bez.addColour (0.5, Colour (0xff2a2214));
        bez.addColour (0.9, Colour (0xff7a5f28));
        g.setGradientFill (bez);
        g.fillRoundedRectangle (outer, 18.0f);
        g.setColour (Colour (0xff050506));
        g.fillRoundedRectangle (outer.reduced (3.0f), 16.0f);
    }
    else
    {
        g.setColour (Colour (0xff09090b));
        g.fillRoundedRectangle (outer, 18.0f);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (outer.reduced (0.5f), 18.0f, 1.0f);
    }

    Graphics::ScopedSaveState state (g);
    Path screenPath;
    screenPath.addRoundedRectangle (s, 26.0f);
    g.reduceClipRegion (screenPath);

    ColourGradient glass (Colour (0xff1d1c22), s.getCentreX(), s.getCentreY(), Colour (0xff060608),
                          s.getCentreX() + s.getWidth() * 0.6f, s.getCentreY(), true);
    g.setGradientFill (glass);
    g.fillRect (s);
    if (! t.gloss)
        for (float y = s.getY(); y < s.getBottom(); y += 3.0f)
        {
            g.setColour (Colours::black.withAlpha (0.18f));
            g.fillRect (s.getX(), y, s.getWidth(), 1.0f);
        }

    // State for the drawing
    auto rawv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const bool crOn = rawv (ParamIDs::crOn) > 0.5f, bcOn = rawv (ParamIDs::bcOn) > 0.5f, colOn = rawv (ParamIDs::colOn) > 0.5f;
    const float amount = crOn ? rawv (ParamIDs::amount) / 100.0f : 0.0f;
    const float drive = colOn ? rawv (ParamIDs::drive) / 100.0f : 0.0f;
    const bool noise = roundToInt (rawv (ParamIDs::mode)) == 2;
    const bool follow = rawv (ParamIDs::folOn) > 0.5f;
    const float intensity = jlimit (0.0f, 1.0f, amount * 0.85f + drive * 0.35f + (bcOn ? 0.25f : 0.0f));
    const float jitter = amount * amount;
    const float steps = bcOn ? jmax (2.0f, std::pow (2.0f, rawv (ParamIDs::bits) - 1.0f)) : 0.0f;

    constexpr int N = CrumbleProcessor::Scope::size;
    const int count = proc.scope.writeCount.load (std::memory_order_acquire);
    const int buckets = jlimit (16, N - 1, (int) (timeWindow / 0.0005f));
    const int points = jmin (buckets, 600);

    std::vector<float> outL ((size_t) points), outR ((size_t) points), inp ((size_t) points);
    float diff = 0.0f;
    for (int p = 0; p < points; ++p)
    {
        const int b0 = count - buckets + (int) ((int64) p * buckets / points);
        const int b1 = count - buckets + (int) ((int64) (p + 1) * buckets / points);
        float l = 0.0f, r = 0.0f, in = 0.0f;
        for (int b = b0; b < jmax (b0 + 1, b1); ++b)
        {
            const int idx = ((b % N) + N) % N;
            if (std::abs (proc.scope.out[0][idx]) > std::abs (l)) l = proc.scope.out[0][idx];
            if (std::abs (proc.scope.out[1][idx]) > std::abs (r)) r = proc.scope.out[1][idx];
            if (std::abs (proc.scope.in[idx]) > std::abs (in)) in = proc.scope.in[idx];
        }
        outL[(size_t) p] = l;
        outR[(size_t) p] = r;
        inp[(size_t) p] = in;
        diff = jmax (diff, std::abs (l - r));
    }

    const float cy = s.getCentreY(), halfH = s.getHeight() * 0.42f;
    const float x0 = s.getX() + 20.0f, span = s.getWidth() - 40.0f;
    auto makePath = [&] (const std::vector<float>& v, bool wobble, bool spawn, Colour particleCol)
    {
        Path path;
        bool started = false;
        for (int p = 0; p < points; ++p)
        {
            float val = jlimit (-1.15f, 1.15f, v[(size_t) p] * ampZoom);
            if (steps > 0.0f)
                val = std::round (val * steps) / steps;
            float y = cy - val * halfH;
            if (wobble)
                y += noise ? (rng.nextFloat() - 0.5f) * jitter * 50.0f
                           : std::sin ((float) p * 0.7f + time * 40.0f) * jitter * 22.0f;
            const auto pt = bulge (x0 + span * (float) p / (float) (points - 1), y);
            if (wobble && noise && amount > 0.5f && rng.nextFloat() < (amount - 0.5f) * 0.1f)
            {
                started = false;
                continue;
            }
            if (! started) { path.startNewSubPath (pt); started = true; }
            else           { path.lineTo (pt); }
            if (spawn && particles.size() < 650 && rng.nextFloat() < intensity * intensity * 0.04f)
                particles.push_back ({ pt.x, pt.y, (rng.nextFloat() - 0.5f) * 1.2f, rng.nextFloat() * 0.8f, 1.0f });
        }
        ignoreUnused (particleCol);
        return path;
    };

    const PathStrokeType thin (1.4f, PathStrokeType::curved, PathStrokeType::rounded);
    g.setColour (Colours::white.withAlpha (0.16f));
    g.strokePath (makePath (inp, false, false, {}), thin);

    const float lineW = 1.3f + intensity * 1.2f + drive * 0.6f;
    const PathStrokeType mainStroke (lineW, PathStrokeType::curved, PathStrokeType::rounded);
    if (rawv (ParamIDs::stereo) > 2.0f && diff > 0.002f)
    {
        g.setColour (t.accentHi.interpolatedWith (Colours::white, 0.45f).withAlpha (0.55f));
        g.strokePath (makePath (outR, true, false, {}), mainStroke);
    }
    const auto mainPath = makePath (outL, true, true, t.accentHi);
    g.setColour (t.accentDim.interpolatedWith (t.accentHi, intensity * 0.6f));
    g.strokePath (mainPath, mainStroke);

    if (follow)
    {
        // Brighter where the signal crosses the threshold
        const float yu = thresholdY (true), yl = thresholdY (false);
        {
            Graphics::ScopedSaveState hi (g);
            RectangleList<int> region;
            region.add (s.withBottom (yu).toNearestInt());
            region.add (s.withTop (yl).toNearestInt());
            g.reduceClipRegion (region);
            g.setColour (t.accentHi);
            g.strokePath (mainPath, PathStrokeType (lineW + 1.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        const float dashes[] = { 7.0f, 6.0f };
        for (float y : { yu, yl })
        {
            Path line;
            for (float x = s.getX(); x <= s.getRight(); x += 6.0f)
            {
                const auto pt = bulge (x, y);
                if (x == s.getX()) line.startNewSubPath (pt); else line.lineTo (pt);
            }
            Path dashed;
            PathStrokeType (1.5f).createDashedStroke (dashed, line, dashes, 2);
            g.setColour (t.accentHi.withAlpha (0.85f));
            g.fillPath (dashed);
        }
        const String label = String (proc.apvts.getRawParameterValue (ParamIDs::folThresh)->load(), 1) + " dB";
        const Rectangle<float> tag (s.getRight() - 84.0f, yu - 26.0f, 64.0f, 20.0f);
        g.setColour (Colour (0xff141417));
        g.fillRoundedRectangle (tag, 5.0f);
        g.setColour (t.accent.withAlpha (0.6f));
        g.drawRoundedRectangle (tag, 5.0f, 1.0f);
        g.setColour (t.accentHi);
        g.setFont (uiFont (12.0f));
        g.drawText (label, tag, Justification::centred);
    }
    else
    {
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine (roundToInt (cy), s.getX() + 16.0f, s.getRight() - 16.0f);
    }

    for (const auto& pt : particles)
    {
        g.setColour (t.accentHi.withAlpha (jlimit (0.0f, 1.0f, pt.life)));
        g.fillRect (pt.x, pt.y, 2.0f, 2.0f);
    }

    // Glass: reflection, (gloss) streak, vignette
    ColourGradient refl (Colours::white.withAlpha (t.gloss ? 0.20f : 0.09f), 0.0f, s.getY(), Colours::white.withAlpha (0.0f), 0.0f, s.getY() + s.getHeight() * 0.45f, false);
    g.setGradientFill (refl);
    g.fillEllipse (s.getX() + s.getWidth() * 0.32f - s.getWidth() * 0.36f, s.getY() + s.getHeight() * 0.12f - s.getHeight() * 0.28f,
                   s.getWidth() * 0.72f, s.getHeight() * 0.56f);
    if (t.gloss)
    {
        Path streak;
        streak.startNewSubPath (s.getX() + s.getWidth() * 0.62f, s.getY());
        streak.lineTo (s.getX() + s.getWidth() * 0.70f, s.getY());
        streak.lineTo (s.getX() + s.getWidth() * 0.58f, s.getBottom());
        streak.lineTo (s.getX() + s.getWidth() * 0.50f, s.getBottom());
        streak.closeSubPath();
        g.setColour (Colours::white.withAlpha (0.05f));
        g.fillPath (streak);
    }
    ColourGradient vig (Colours::transparentBlack, s.getCentreX(), s.getCentreY(), Colours::black.withAlpha (0.75f),
                        s.getCentreX() + s.getWidth() * 0.62f, s.getCentreY(), true);
    vig.addColour (0.45, Colours::transparentBlack);
    g.setGradientFill (vig);
    g.fillRect (s);

    // Overlay backing for the Follow controls and the hint
    g.setColour (Colour ((uint8) 10, 10, 12, 0.75f));
    g.fillRoundedRectangle ((float) followButton.getX() - 6.0f, (float) followButton.getY() - 3.0f,
                            (float) (followMode->getRight() - followButton.getX()) + 12.0f, 32.0f, 8.0f);
    g.setColour (Colour ((uint8) 165, 165, 173, 0.5f));
    g.setFont (uiFont (11.0f));
    g.drawText ("wheel: height    shift + wheel: time    double-click: reset",
                s.withTrimmedRight (18.0f).withTrimmedBottom (10.0f), Justification::bottomRight);
}

//==============================================================================
MeterColumn::MeterColumn (const String& t, AudioProcessorValueTreeState& apvts, const char* gainParam, const String& knobName)
    : title (t)
{
    knob.init (*this, apvts, knobName, gainParam);
}

void MeterColumn::resized()
{
    knob.setBounds (0, getHeight() - 122, getWidth());
}

void MeterColumn::push (float left, float right)
{
    const float in[2] = { left, right };
    for (int ch = 0; ch < 2; ++ch)
        level[ch] = jmax (in[ch], level[ch] * 0.86f);
    peakHold = jmax (peakHold, jmax (left, right));
    repaint (0, 0, getWidth(), getHeight() - 124);
}

void MeterColumn::mouseDown (const MouseEvent& e)
{
    if (e.y < 80)
    {
        peakHold = 0.0f;
        repaint();
    }
}

void MeterColumn::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (t.panelFill);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (t.panelStroke);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    g.setColour (t.ink);
    g.setFont (uiFont (13.0f, true, 0.25f));
    g.drawText (title, 0, 10, getWidth(), 20, Justification::centred);

    // Peak readout (click to reset)
    const float peakDb = Decibels::gainToDecibels (peakHold, -60.0f);
    const bool over = peakDb > 0.0f;
    const Rectangle<float> box (10.0f, 36.0f, (float) getWidth() - 20.0f, 24.0f);
    g.setColour (Colour (0xff111114));
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (over ? Colour (0xffff4d5e) : Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (box, 5.0f, 1.0f);
    g.setColour (over ? Colour (0xffff4d5e) : Colour (0xffececf0));
    g.setFont (uiFont (12.0f));
    g.drawText (peakHold <= 0.0f ? String ("-inf") : (peakDb > 0.0f ? "+" : "") + String (peakDb, 1), box, Justification::centred);

    // Meter bars: -48 dB .. +6 dB
    const float top = 74.0f, bottom = (float) getHeight() - 132.0f;
    auto yFor = [&] (float db) { return bottom - (bottom - top) * jlimit (0.0f, 1.0f, (db + 48.0f) / 54.0f); };
    for (const float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
    {
        g.setColour ((t.lightBg ? Colours::black : Colours::white).withAlpha (db == 0.0f ? 0.35f : 0.18f));
        g.fillRect (8.0f, yFor (db), 8.0f, 1.0f);
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        const Rectangle<float> bar ((float) getWidth() * 0.5f - 14.0f + (float) ch * 16.0f, top, 10.0f, bottom - top);
        g.setColour (Colour (0xff0c0c0e));
        g.fillRoundedRectangle (bar, 3.0f);
        const float y = yFor (Decibels::gainToDecibels (level[ch], -60.0f));
        if (y < bottom)
        {
            ColourGradient grad (t.accentDim.darker (0.3f), 0.0f, bottom, Colour (0xffff4d5e), 0.0f, top, false);
            grad.addColour (0.70, t.accentHi);
            grad.addColour (0.88, t.accent);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bar.withTop (y), 3.0f);
        }
    }
}

//==============================================================================
PresetBar::PresetBar (CrumbleProcessor& p) : proc (p)
{
    for (auto* b : { &prev, &next, &save })
        addAndMakeVisible (*b);
    prev.onClick = [this] { proc.presets.step (-1); repaint(); };
    next.onClick = [this] { proc.presets.step (+1); repaint(); };
    save.onClick = [this]
    {
        const auto current = proc.presets.getCurrentName();
        askForName ("Save preset", current == "Init" ? String() : current, [this] (const String& name)
        {
            const auto file = PresetManager::getFolder().getChildFile (File::createLegalFileName (name) + PresetManager::extension);
            if (file.existsAsFile() && name != proc.presets.getCurrentName())
            {
                auto opts = MessageBoxOptions().withIconType (MessageBoxIconType::QuestionIcon)
                                .withTitle ("Overwrite preset?")
                                .withMessage ("\"" + name + "\" already exists. Replace it?")
                                .withButton ("Replace").withButton ("Cancel")
                                .withAssociatedComponent (this);
                AlertWindow::showAsync (opts, [this, name] (int result) { if (result == 1) proc.presets.save (name); repaint(); });
            }
            else
            {
                proc.presets.save (name);
            }
            repaint();
        });
    };
    startTimerHz (4);
}

Rectangle<int> PresetBar::nameBox() const
{
    return { 30, 0, getWidth() - 96, getHeight() };
}

void PresetBar::resized()
{
    const auto box = nameBox();
    prev.setBounds (0, 0, 28, getHeight());
    next.setBounds (box.getRight() + 2, 0, 28, getHeight());
    save.setBounds (box.getRight() + 34, 0, 30, getHeight());
}

void PresetBar::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto box = nameBox().toFloat().reduced (0.5f);
    g.setColour (Colour (0xff141417));
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (box, 6.0f, 1.0f);
    const String name = proc.presets.getCurrentName() + (proc.presets.isModified() ? " *" : "");
    g.setColour (Colour (0xffececf0));
    g.setFont (uiFont (14.0f));
    g.drawText (name, box.reduced (14.0f, 0.0f).withTrimmedRight (20.0f), Justification::centredLeft, true);
    Path arrow;
    const float ax = box.getRight() - 18.0f, ay = box.getCentreY();
    arrow.startNewSubPath (ax - 5.0f, ay - 3.0f);
    arrow.lineTo (ax, ay + 3.0f);
    arrow.lineTo (ax + 5.0f, ay - 3.0f);
    g.setColour (t.accent);
    g.strokePath (arrow, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

void PresetBar::mouseDown (const MouseEvent& e)
{
    if (nameBox().contains (e.getPosition()))
        showMenu();
}

void PresetBar::showMenu()
{
    PopupMenu menu;
    const auto names = proc.presets.getPresetNames();
    const auto current = proc.presets.getCurrentName();
    menu.addItem (1, "Init", true, current == "Init");
    menu.addSectionHeader ("User presets");
    if (names.isEmpty())
        menu.addItem (2, "(no presets yet - press the disk icon to save)", false);
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (100 + i, names[i], true, names[i] == current);
    menu.addSeparator();
    const bool isUser = names.contains (current);
    menu.addItem (3, "Rename current...", isUser);
    menu.addItem (4, "Delete current...", isUser);
    menu.addItem (5, "Open presets folder");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMinimumWidth (nameBox().getWidth()),
        [this, names, current] (int r)
        {
            if (r == 1)
                proc.presets.loadInit();
            else if (r >= 100 && r - 100 < names.size())
                proc.presets.load (names[r - 100]);
            else if (r == 3)
                askForName ("Rename preset", current, [this, current] (const String& n) { proc.presets.rename (current, n); repaint(); });
            else if (r == 4)
            {
                auto opts = MessageBoxOptions().withIconType (MessageBoxIconType::WarningIcon)
                                .withTitle ("Delete preset?")
                                .withMessage ("Delete \"" + current + "\"? This cannot be undone.")
                                .withButton ("Delete").withButton ("Cancel")
                                .withAssociatedComponent (this);
                AlertWindow::showAsync (opts, [this, current] (int result) { if (result == 1) proc.presets.remove (current); repaint(); });
            }
            else if (r == 5)
                PresetManager::getFolder().startAsProcess();
            repaint();
        });
}

void PresetBar::askForName (const String& title, const String& initial, std::function<void (const String&)> onOk)
{
    auto* w = new AlertWindow (title, "Preset name:", MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", initial);
    w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    w->enterModalState (true, ModalCallbackFunction::create ([w, onOk] (int result)
    {
        const auto text = w->getTextEditorContents ("name").trim();
        if (result == 1 && text.isNotEmpty())
            onOk (text);
    }), true);
}

//==============================================================================
void AboutOverlay::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    g.fillAll (Colours::black.withAlpha (0.6f));
    const auto card = getLocalBounds().toFloat().withSizeKeepingCentre (380.0f, 300.0f);
    g.setColour (Colour (0xff141417));
    g.fillRoundedRectangle (card, 14.0f);
    g.setColour (t.accent.withAlpha (0.6f));
    g.drawRoundedRectangle (card.reduced (0.5f), 14.0f, 1.2f);
    if (logo.isValid())
        g.drawImage (logo, card.withHeight (120.0f).withY (card.getY() + 24.0f).reduced (120.0f, 0.0f), RectanglePlacement::centred);
    g.setColour (Colour (0xffececf0));
    g.setFont (uiFont (30.0f, true, 0.08f));
    g.drawText ("CRUMBLE", card.withY (card.getY() + 152.0f).withHeight (36.0f), Justification::centred);
    g.setColour (t.accent);
    g.setFont (uiFont (14.0f));
    g.drawText ("Version " + String (JucePlugin_VersionString), card.withY (card.getY() + 190.0f).withHeight (20.0f), Justification::centred);
    g.setColour (Colour (0xffa5a5ad));
    g.setFont (uiFont (12.0f, true, 0.3f));
    g.drawText ("PRIMAL RAGE LAB", card.withY (card.getY() + 222.0f).withHeight (18.0f), Justification::centred);
    g.setFont (uiFont (11.0f));
    g.setColour (Colour (0xff6a6a72));
    g.drawText ("Click anywhere to close", card.withY (card.getY() + 264.0f).withHeight (18.0f), Justification::centred);
}
