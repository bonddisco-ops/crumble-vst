#include "Components.h"
#include "Logo.h"

using namespace juce;

namespace
{
    const Colour kWidget (0xff1b1b1f);   // combo boxes, switches and dialogs stay dark in every theme
    const Colour kWidgetInk (0xffececf0);
    const Colour kClipRed (0xffff3c46);

    Point<float> polar (float cx, float cy, float r, float a) { return { cx + std::sin (a) * r, cy - std::cos (a) * r }; }
}

Font uiFont (float height, bool bold, float kerning)
{
    Font f (FontOptions (height, bold ? Font::bold : Font::plain));
    if (kerning != 0.0f)
        f.setExtraKerningFactor (kerning);
    return f;
}

const Theme& themeOf (const Component& c)
{
    if (auto* l = dynamic_cast<CrumbleLookAndFeel*> (&c.getLookAndFeel()))
        return l->theme();
    return Theme::all().front();
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
    setColour (TextButton::buttonOnColourId, t.accent);
    setColour (TextButton::textColourOffId, kWidgetInk);
    setColour (TextButton::textColourOnId, Colour (0xff111114));
    setColour (ToggleButton::textColourId, t.ink);
    setColour (TooltipWindow::backgroundColourId, Colour (0xf2141417));
    setColour (TooltipWindow::textColourId, kWidgetInk);
    setColour (TooltipWindow::outlineColourId, t.accent.withAlpha (0.6f));
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
    const bool compact = R < 22.0f;

    Path track;
    track.addCentredArc (cx, cy, R + 6.0f, R + 6.0f, 0.0f, startAngle, endAngle, true);
    g.setColour ((t.lightBg ? Colours::black : Colours::white).withAlpha (0.10f));
    g.strokePath (track, PathStrokeType (compact ? 2.5f : 3.0f, PathStrokeType::curved, PathStrokeType::rounded));

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
        g.strokePath (value, PathStrokeType (compact ? 5.0f : 6.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (t.glow (lit));
        g.strokePath (value, PathStrokeType (compact ? 2.5f : 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    {
        const float sr = R * 1.35f;
        ColourGradient shadow (Colours::black.withAlpha (t.lightBg ? 0.45f : 0.65f), cx + 3.0f, cy + 7.0f,
                               Colours::transparentBlack, cx + 3.0f + sr, cy + 7.0f, true);
        g.setGradientFill (shadow);
        g.fillEllipse (cx + 3.0f - sr, cy + 7.0f - sr, sr * 2.0f, sr * 2.0f);
    }

    if (t.gloss)
    {
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
        const int teeth = compact ? 36 : 44;
        for (int i = 0; i < teeth; ++i)
        {
            const float a = (float) i / (float) teeth * MathConstants<float>::twoPi;
            g.setColour (i % 2 == 0 ? Colour (0xff55555b) : Colour (0xff2b2b2e));
            g.drawLine (Line<float> (polar (cx, cy, R - 1.0f, a), polar (cx, cy, R + 2.0f, a)), compact ? 1.6f : 2.0f);
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
    g.strokePath (pointer, PathStrokeType (compact ? 2.4f : 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
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
    g.setFont (uiFont ((float) button.getProperties().getWithDefault ("fontSize", 13.0)));
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
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 5.0f, ay - 3.0f);
    arrow.lineTo (ax, ay + 3.0f);
    arrow.lineTo (ax + 5.0f, ay - 3.0f);
    g.setColour (box.isEnabled() ? t.accent : t.accent.withAlpha (0.4f));
    g.strokePath (arrow, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

void CrumbleLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (3, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

Font CrumbleLookAndFeel::getComboBoxFont (ComboBox& box) { return uiFont (box.getHeight() < 24 ? 12.0f : 13.0f); }
Font CrumbleLookAndFeel::getPopupMenuFont()               { return uiFont (14.0f); }
Font CrumbleLookAndFeel::getTextButtonFont (TextButton&, int h) { return uiFont (jmin (15.0f, (float) h * 0.48f)); }

Label* CrumbleLookAndFeel::createSliderTextBox (Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    const bool compact = (bool) slider.getProperties().getWithDefault ("compact", false);
    label->setFont (uiFont (compact ? 12.0f : 13.0f));
    label->setColour (Label::outlineColourId, Colours::transparentBlack);
    label->setColour (Label::backgroundColourId, Colours::transparentBlack);
    label->setColour (Label::textColourId, theme().ink);
    label->setColour (Label::outlineWhenEditingColourId, theme().accent);
    label->setColour (TextEditor::backgroundColourId, kWidget);
    label->setColour (TextEditor::textColourId, kWidgetInk);
    return label;
}

Rectangle<int> CrumbleLookAndFeel::getTooltipBounds (const String& tipText, Point<int> screenPos, Rectangle<int> parentArea)
{
    AttributedString s;
    s.append (tipText, uiFont (13.0f), kWidgetInk);
    TextLayout tl;
    tl.createLayout (s, 300.0f);
    const int w = (int) std::ceil (tl.getWidth()) + 22, h = (int) std::ceil (tl.getHeight()) + 16;
    const int x = screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 18;
    const int y = screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8) : screenPos.y + 18;
    return Rectangle<int> (x, y, w, h).constrainedWithin (parentArea);
}

void CrumbleLookAndFeel::drawTooltip (Graphics& g, const String& text, int width, int height)
{
    const auto r = Rectangle<int> (width, height).toFloat();
    g.setColour (Colour (0xf2141417));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (theme().accent.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    AttributedString s;
    s.append (text, uiFont (13.0f), kWidgetInk);
    TextLayout tl;
    tl.createLayout (s, (float) width - 22.0f);
    tl.draw (g, r.reduced (11.0f, 8.0f));
}

//==============================================================================
void Knob::init (Component& parent, AudioProcessorValueTreeState& apvts, const String& name,
                 const String& paramId, bool isCompact, bool reverseArc)
{
    compact = isCompact;
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set ("compact", compact);
    slider.getProperties().set ("reverseArc", reverseArc);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, compact ? 62 : 84, 18);
    slider.setWantsKeyboardFocus (false);
    slider.setMouseClickGrabsKeyboardFocus (false);
    parent.addAndMakeVisible (slider);

    label.setText (name, dontSendNotification);
    label.setFont (uiFont (compact ? 11.0f : 12.0f, true));
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
    slider.setBounds (x, y + 18, w, compact ? 72 : 88);
}

void Knob::setVisible (bool v)
{
    slider.setVisible (v);
    label.setVisible (v);
}

void Knob::setDimmed (bool dimmed)
{
    slider.setAlpha (dimmed ? 0.38f : 1.0f);
    label.setAlpha (dimmed ? 0.45f : 1.0f);
}

//==============================================================================
Segmented::Segmented (RangedAudioParameter& param, const StringArray& names, bool isCompact)
    : items (names), compact (isCompact),
      attachment (param, [this] (float v) { selected = roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
}

int Segmented::getIdealWidth() const
{
    int w = 0;
    for (const auto& s : items)
        w += GlyphArrangement::getStringWidthInt (uiFont (fontSize()), s) + (compact ? 18 : 22);
    return w;
}

void Segmented::setLocked (bool shouldLock, int forcedIndex)
{
    if (shouldLock != locked || forced != forcedIndex)
    {
        locked = shouldLock;
        forced = forcedIndex;
        setAlpha (locked ? 0.4f : 1.0f);
        repaint();
    }
}

void Segmented::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const float total = (float) getIdealWidth();
    const float h = (float) getHeight();
    const int shown = locked ? forced : selected;
    Path outline;
    outline.addRoundedRectangle (0.5f, 0.5f, (float) getWidth() - 1.0f, h - 1.0f, 6.0f);
    g.setColour (kWidget);
    g.fillPath (outline);
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (outline);
        float x = 0.0f;
        for (int i = 0; i < items.size(); ++i)
        {
            const float w = (float) (GlyphArrangement::getStringWidthInt (uiFont (fontSize()), items[i]) + (compact ? 18 : 22))
                          * (float) getWidth() / total;
            const Rectangle<float> r (x, 0.0f, w, h);
            if (i == shown)
            {
                g.setColour (t.accent);
                g.fillRect (r);
            }
            g.setColour (i == shown ? Colour (0xff111114) : kWidgetInk);
            g.setFont (uiFont (fontSize()));
            g.drawText (items[i], r, Justification::centred);
            x += w;
        }
    }
    g.setColour (Colours::white.withAlpha (0.10f));
    g.strokePath (outline, PathStrokeType (1.0f));
}

void Segmented::mouseDown (const MouseEvent& e)
{
    if (locked)
        return;
    const float total = (float) getIdealWidth();
    float x = 0.0f;
    for (int i = 0; i < items.size(); ++i)
    {
        x += (float) (GlyphArrangement::getStringWidthInt (uiFont (fontSize()), items[i]) + (compact ? 18 : 22))
           * (float) getWidth() / total;
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
    }
}

//==============================================================================
Block::Block (const String& t, AudioProcessorValueTreeState& apvts, const char* powerParam, bool isMovable)
    : title (t), movable (isMovable)
{
    if (powerParam != nullptr)
    {
        power = std::make_unique<PowerButton>();
        addAndMakeVisible (*power);
        powerAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, powerParam, *power);
        power->onStateChange = [this] { repaint(); };
    }
}

bool Block::isOn() const { return power == nullptr || power->getToggleState(); }

void Block::resized()
{
    if (power != nullptr)
        power->setBounds (movable ? 22 : 6, 4, 24, 24);
}

bool Block::inHandle (const MouseEvent& e) const
{
    return movable && e.y < 30;
}

void Block::mouseMove (const MouseEvent& e)
{
    setMouseCursor (inHandle (e) ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
}

void Block::mouseDown (const MouseEvent& e)
{
    dragging = inHandle (e);
    if (dragging)
    {
        dragStartParentX = e.getEventRelativeTo (getParentComponent()).x;
        if (onDragStart)
            onDragStart (*this);
    }
}

void Block::mouseDrag (const MouseEvent& e)
{
    if (dragging && onDragMove)
        onDragMove (*this, e.getEventRelativeTo (getParentComponent()).x - dragStartParentX);
}

void Block::mouseUp (const MouseEvent&)
{
    if (dragging && onDragEnd)
        onDragEnd (*this);
    dragging = false;
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

    int textX = 12;
    if (movable)
    {
        g.setColour ((t.lightBg ? Colours::black : Colours::white).withAlpha (0.30f));
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 2; ++j)
                g.fillEllipse (10.0f + (float) j * 5.0f, 10.5f + (float) i * 5.0f, 2.6f, 2.6f);
        textX = 22;
    }
    if (power != nullptr)
        textX += 26;
    g.setColour (isOn() ? t.mute : t.mute.withAlpha (0.5f));
    g.setFont (uiFont (11.0f, true, 0.2f));
    g.drawText (title, textX, 8, getWidth() - textX - 8, 16, Justification::centredLeft);
}

//==============================================================================
FilterPill::FilterPill (AudioProcessorValueTreeState& apvts, bool isPost)
    : post (isPost),
      attachment (*apvts.getParameter (ParamIDs::filterId (isPost, ParamIDs::fOn)), [this] (float v) { on = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setWantsKeyboardFocus (false);
}

void FilterPill::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (selected ? t.accent.withAlpha (0.22f).overlaidWith (kWidget.withAlpha (0.6f)) : kWidget);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (selected ? t.accent : Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, selected ? 1.5f : 1.0f);

    const Rectangle<float> led (11.0f, r.getCentreY() - 5.0f, 10.0f, 10.0f);
    if (on)
    {
        g.setColour (t.accent.withAlpha (0.25f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (Colour (0xff0e0e10));
    g.fillEllipse (led.expanded (1.5f));
    g.setColour (on ? t.accent : Colour (0xff3a3a40));
    g.fillEllipse (led);

    g.setColour (kWidgetInk.withAlpha (selected ? 1.0f : 0.8f));
    g.setFont (uiFont (13.0f, true, 0.06f));
    g.drawText (post ? "POST" : "PRE", r.withTrimmedLeft (26.0f).withTrimmedRight (8.0f), Justification::centred);
}

void FilterPill::mouseDown (const MouseEvent& e)
{
    if (e.x < 28)
    {
        const bool turnOn = ! on;
        attachment.setValueAsCompleteGesture (turnOn ? 1.0f : 0.0f);
        if (turnOn && onTurnedOn)
            onTurnedOn();
    }
    else if (onSelect)
    {
        onSelect();
    }
}

void ViewSwitch::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    Path outline;
    outline.addRoundedRectangle (r, 6.0f);
    g.setColour (kWidget);
    g.fillPath (outline);
    const char* names[2] = { "WAVE", "EQ" };
    for (int i = 0; i < 2; ++i)
    {
        const auto cell = r.withWidth (r.getWidth() * 0.5f).withX (r.getX() + r.getWidth() * 0.5f * (float) i);
        if (i == view)
        {
            Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (outline);
            g.setColour (t.accent);
            g.fillRect (cell);
        }
        g.setColour (i == view ? Colour (0xff111114) : kWidgetInk);
        g.setFont (uiFont (12.0f, true, 0.08f));
        g.drawText (names[i], cell, Justification::centred);
    }
    g.setColour (Colours::white.withAlpha (0.10f));
    g.strokePath (outline, PathStrokeType (1.0f));
}

void ViewSwitch::mouseDown (const MouseEvent& e)
{
    const int v = e.x < getWidth() / 2 ? 0 : 1;
    if (onChange)
        onChange (v);
}

//==============================================================================
ScopeView::ScopeView (CrumbleProcessor& p)
    : proc (p), prePill (p.apvts, false), postPill (p.apvts, true)
{
    using namespace ParamIDs;
    followButton.setWantsKeyboardFocus (false);
    followButton.getProperties().set ("onDark", true);
    addAndMakeVisible (followButton);
    followAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, folOn, followButton);
    followButton.onClick = [this] { if (followButton.getToggleState()) setView (View::wave); };

    followMode = std::make_unique<Segmented> (*proc.apvts.getParameter (folMode), Tables::folModes);
    addAndMakeVisible (*followMode);
    preTarget = std::make_unique<Segmented> (*proc.apvts.getParameter (filterId (false, fTarget)), Tables::fxTargets);
    postTarget = std::make_unique<Segmented> (*proc.apvts.getParameter (filterId (true, fTarget)), Tables::fxTargets);
    addAndMakeVisible (*preTarget);
    addChildComponent (*postTarget);

    for (auto* pill : { &prePill, &postPill })
    {
        addAndMakeVisible (*pill);
        const bool post = pill == &postPill;
        pill->onSelect = [this, post] { selectFilter (post); setView (View::eq); };
        pill->onTurnedOn = [this, post] { selectFilter (post); setView (View::eq); };
    }
    addAndMakeVisible (viewSwitch);
    viewSwitch.onChange = [this] (int v) { setView (v == 1 ? View::eq : View::wave); };

    thresholdParam = proc.apvts.getParameter (folThresh);
    glueThreshParam = proc.apvts.getParameter (glueThresh);

    fftData.assign ((size_t) fftSize * 2, 0.0f);
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
    specDb.assign ((size_t) fftSize / 2, -120.0f);
    particles.reserve (400);

    selectFilter (false);
    updateTooltip();
    startTimerHz (60);
}

Rectangle<float> ScopeView::screen() const { return getLocalBounds().toFloat().reduced (12.0f); }

Rectangle<float> ScopeView::plot() const
{
    const auto s = screen();
    return { s.getX() + 54.0f, s.getY() + 50.0f, s.getWidth() - 108.0f, s.getHeight() - 96.0f };
}

Point<float> ScopeView::bulge (float x, float y) const
{
    const auto s = screen();
    const float cx = s.getCentreX(), cy = s.getCentreY();
    const float nx = (x - cx) / (s.getWidth() * 0.5f), ny = (y - cy) / (s.getHeight() * 0.5f);
    const float k = 1.0f - 0.05f * (nx * nx + ny * ny);
    return { cx + nx * k * s.getWidth() * 0.5f, cy + ny * k * s.getHeight() * 0.5f };
}

void ScopeView::resized()
{
    const auto s = screen().toNearestInt();
    followButton.setBounds (s.getX() + 18, s.getY() + 12, 86, 26);
    followMode->setBounds (s.getX() + 106, s.getY() + 13, followMode->getIdealWidth(), 24);
    viewSwitch.setBounds (s.getCentreX() - 56, s.getY() + 13, 112, 24);
    postPill.setBounds (s.getRight() - 18 - 84, s.getY() + 12, 84, 26);
    prePill.setBounds (postPill.getX() - 8 - 76, s.getY() + 12, 76, 26);
    const int tw = preTarget->getIdealWidth();
    preTarget->setBounds (prePill.getX() - 10 - tw, s.getY() + 13, tw, 24);
    postTarget->setBounds (preTarget->getBounds());
}

Rectangle<int> ScopeView::followArea() const
{
    return followButton.getBounds().getUnion (followMode->getBounds()).expanded (8, 5);
}

Rectangle<int> ScopeView::pillsArea() const
{
    return preTarget->getBounds().getUnion (postPill.getBounds()).getUnion (prePill.getBounds()).expanded (8, 5);
}

void ScopeView::selectFilter (bool post)
{
    setOttFocus (false);
    postSelected = post;
    prePill.selected = ! post;
    postPill.selected = post;
    preTarget->setVisible (! post);
    postTarget->setVisible (post);
    prePill.repaint();
    postPill.repaint();
    repaint();
}

bool ScopeView::glueOn() const
{
    return proc.apvts.getRawParameterValue (ParamIDs::compOn)->load() > 0.5f
        && roundToInt (proc.apvts.getRawParameterValue (ParamIDs::compMode)->load()) == 1;
}

bool ScopeView::ottOn() const
{
    return proc.apvts.getRawParameterValue (ParamIDs::compOn)->load() > 0.5f
        && roundToInt (proc.apvts.getRawParameterValue (ParamIDs::compMode)->load()) == 0;
}

void ScopeView::setOttFocus (bool f)
{
    if (ottFocus == f)
        return;
    ottFocus = f;
    prePill.selected = ! f && ! postSelected;
    postPill.selected = ! f && postSelected;
    preTarget->setVisible (! f && ! postSelected);
    postTarget->setVisible (! f && postSelected);
    prePill.repaint();
    postPill.repaint();
    updateTooltip();
    repaint();
}

void ScopeView::showOtt()
{
    if (! ottOn())
        return;
    setView (View::eq);
    setOttFocus (true);
}

Rectangle<float> ScopeView::ottChip() const
{
    const auto sc = screen();
    return { sc.getX() + 18.0f, sc.getBottom() - 32.0f, 150.0f, 22.0f };
}

Path ScopeView::bulgedRect (Rectangle<float> r) const
{
    Path path;
    const Point<float> c[5] = { r.getTopLeft(), r.getTopRight(), r.getBottomRight(), r.getBottomLeft(), r.getTopLeft() };
    for (int e = 0; e < 4; ++e)
        for (int k = 0; k < 8; ++k)
        {
            const float a = (float) k / 8.0f;
            const auto pt = bulge (c[e].x + (c[e + 1].x - c[e].x) * a, c[e].y + (c[e + 1].y - c[e].y) * a);
            if (e == 0 && k == 0) path.startNewSubPath (pt); else path.lineTo (pt);
        }
    path.closeSubPath();
    return path;
}

void ScopeView::setView (View v)
{
    tempWave = false;
    view = v;
    viewSwitch.view = v == View::eq ? 1 : 0;
    viewSwitch.repaint();
    updateTooltip();
    repaint();
}

void ScopeView::touchCeiling (bool dragging)
{
    ceilingDragging = dragging;
    ceilingTouchMs = Time::getMillisecondCounterHiRes();
    if (view == View::eq)
    {
        viewBeforeCeiling = View::eq;
        tempWave = true;
        view = View::wave;
        viewSwitch.view = 0;
        viewSwitch.repaint();
    }
}

void ScopeView::setHelp (bool enabled, Help::Lang l)
{
    helpOn = enabled;
    lang = l;
    auto tip = [this] (const char* key) { return helpOn ? Help::tip (key, lang) : String(); };
    followButton.setTooltip (tip ("follow"));
    followMode->setTooltip (tip ("folMode"));
    viewSwitch.setTooltip (tip ("view"));
    prePill.setTooltip (tip ("prePill"));
    postPill.setTooltip (tip ("postPill"));
    setFxAvailable (fxAvailable);
    updateTooltip();
}

void ScopeView::setFxAvailable (bool available)
{
    fxAvailable = available;
    preTarget->setLocked (! available, 1);
    postTarget->setLocked (! available, 1);
    auto tip = [this] (const char* key) { return helpOn ? Help::tip (key, lang) : String(); };
    preTarget->setTooltip (tip (available ? "preTarget" : "wetOff"));
    postTarget->setTooltip (tip (available ? "postTarget" : "wetOff"));
}

void ScopeView::updateTooltip()
{
    const char* key = view == View::wave ? "scopeWave" : (ottFocus ? "scopeOtt" : "scopeEq");
    setTooltip (helpOn ? Help::tip (key, lang) : String());
}

void ScopeView::setBarsIndex (int i)
{
    barsIndex = jlimit (0, 5, i);
    repaint();
}

float ScopeView::thresholdOffset() const
{
    const float lin = Decibels::decibelsToGain (proc.apvts.getRawParameterValue (ParamIDs::folThresh)->load());
    return jmin (1.15f, lin * ampZoom) * plot().getHeight() * 0.46f;
}

float ScopeView::glueOffset() const
{
    const float lin = Decibels::decibelsToGain (proc.apvts.getRawParameterValue (ParamIDs::glueThresh)->load());
    return jmin (1.15f, lin * ampZoom) * plot().getHeight() * 0.46f;
}

ScopeView::Grab ScopeView::waveLineAt (float y) const
{
    const float d = std::abs (y - plot().getCentreY());
    const float dg = glueOn() ? std::abs (d - glueOffset()) : 1.0e9f;
    const float df = proc.apvts.getRawParameterValue (ParamIDs::folOn)->load() > 0.5f ? std::abs (d - thresholdOffset()) : 1.0e9f;
    if (jmin (dg, df) >= 7.0f)
        return Grab::none;
    return dg <= df ? Grab::glue : Grab::thresh;
}

//------------------------------------------------------------------------------
void ScopeView::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    if (tempWave && ! ceilingDragging && now - ceilingTouchMs > 2000.0)
    {
        tempWave = false;
        view = viewBeforeCeiling;
        viewSwitch.view = view == View::eq ? 1 : 0;
        viewSwitch.repaint();
        updateTooltip();
    }

    // Particles drift with the scrolling waveform and fall softly
    const int count = proc.scope.writeCount.load (std::memory_order_acquire);
    const double bpm = proc.scope.bpm.load (std::memory_order_relaxed);
    const double buckets = jlimit (50.0, (double) CrumbleProcessor::Scope::size - 2.0, barValues[barsIndex] * 4.0 * 60.0 / bpm * 1000.0);
    const float scroll = (float) ((double) (count - lastCount) / (buckets / (double) plot().getWidth()));
    lastCount = count;
    for (auto& pt : particles)
    {
        pt.x -= scroll;
        pt.vy += 0.03f;
        pt.x += pt.vx;
        pt.y += pt.vy;
        pt.life -= 0.012f;
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(), [] (const Particle& pt) { return pt.life <= 0.0f; }),
                     particles.end());

    if (ottFocus && ! ottOn())
        setOttFocus (false);

    // Compressor meters: fast attack, slower fall, so the movement reads well
    auto follow = [] (float& shown, float target, float up, float down)
    {
        shown += (std::abs (target) > std::abs (shown) ? up : down) * (target - shown);
    };
    follow (grShown, proc.glueGrDb.load (std::memory_order_relaxed), 0.6f, 0.15f);
    glueLevelShown += 0.4f * (jmax (-100.0f, proc.glueLevelDb.load (std::memory_order_relaxed)) - glueLevelShown);
    for (int b = 0; b < 3; ++b)
    {
        follow (ottGainShown[b], proc.ottGainDb[b].load (std::memory_order_relaxed), 0.5f, 0.12f);
        ottLevelShown[b] += 0.35f * (jmax (-100.0f, proc.ottLevelDb[b].load (std::memory_order_relaxed)) - ottLevelShown[b]);
    }

    if (view == View::eq)
        updateSpectrum();
    repaint();
}

void ScopeView::updateSpectrum()
{
    if (postSelected != specPost)
    {
        std::fill (specDb.begin(), specDb.end(), -120.0f);
        specPost = postSelected;
    }
    const float* src = postSelected ? proc.spectrum.out : proc.spectrum.in;
    const int pos = proc.spectrum.writePos.load (std::memory_order_acquire);
    constexpr int mask = CrumbleProcessor::Spectrum::size - 1;
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = src[(pos - fftSize + i) & mask] * window[(size_t) i];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    for (int k = 0; k < fftSize / 2; ++k)
    {
        const float db = cdsp::gainToDb (fftData[(size_t) k] / ((float) fftSize * 0.25f));
        float& s = specDb[(size_t) k];
        s = db > s ? s + (db - s) * 0.6f : jmax (db, s - 0.9f);
    }
}

//------------------------------------------------------------------------------
float ScopeView::xForFreq (float f) const
{
    const auto p = plot();
    return p.getX() + p.getWidth() * std::log (jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log (1000.0f);
}

float ScopeView::freqForX (float x) const
{
    const auto p = plot();
    return 20.0f * std::pow (1000.0f, jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
}

float ScopeView::yForDb (float db) const
{
    const auto p = plot();
    return p.getCentreY() - db / 24.0f * p.getHeight() * 0.5f;
}

float ScopeView::dbForY (float y) const
{
    const auto p = plot();
    return (p.getCentreY() - y) / (p.getHeight() * 0.5f) * 24.0f;
}

RangedAudioParameter* ScopeView::fp (const char* name) const
{
    return proc.apvts.getParameter (ParamIDs::filterId (postSelected, name));
}

float ScopeView::fv (const char* name) const
{
    return proc.apvts.getRawParameterValue (ParamIDs::filterId (postSelected, name))->load();
}

Point<float> ScopeView::pointPos (Grab gp) const
{
    using namespace ParamIDs;
    const auto p = plot();
    const double sr = proc.currentSampleRate.load();
    auto curveDb = [&] (float f)
    {
        return cdsp::gainToDb ((float) cdsp::FilterBank::magnitude (sr, f, fv (fLc), fv (fHc), roundToInt (fv (fLcSlope)),
                                                                    roundToInt (fv (fHcSlope)), fv (fPkFreq), fv (fPkGain), fv (fPkQ)));
    };
    if (gp == Grab::lc)
    {
        const float f = fv (fLc);
        return f <= 20.05f ? Point<float> (p.getX(), yForDb (0.0f)) : Point<float> (xForFreq (f), yForDb (jmax (-24.0f, curveDb (f))));
    }
    if (gp == Grab::hc)
    {
        const float f = fv (fHc);
        return f >= 19990.0f ? Point<float> (p.getRight(), yForDb (0.0f)) : Point<float> (xForFreq (f), yForDb (jmax (-24.0f, curveDb (f))));
    }
    return { xForFreq (fv (fPkFreq)), yForDb (fv (fPkGain)) };
}

ScopeView::Grab ScopeView::hitPoint (Point<float> pos) const
{
    Grab best = Grab::none;
    float bestD = 15.0f;
    for (const auto gp : { Grab::pk, Grab::lc, Grab::hc })
    {
        const float d = bulge (pointPos (gp).x, pointPos (gp).y).getDistanceFrom (pos);
        if (d < bestD)
        {
            bestD = d;
            best = gp;
        }
    }
    return best;
}

void ScopeView::setParam (RangedAudioParameter* param, float value)
{
    if (param != nullptr)
        param->setValueNotifyingHost (param->convertTo0to1 (param->getNormalisableRange().snapToLegalValue (value)));
}

//------------------------------------------------------------------------------
void ScopeView::mouseMove (const MouseEvent& e)
{
    if (view == View::wave)
    {
        setMouseCursor (waveLineAt ((float) e.y) != Grab::none ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
        hover = Grab::none;
    }
    else if (ottFocus)
    {
        setMouseCursor (ottChip().contains (e.position) ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
    }
    else
    {
        const auto h = hitPoint (e.position);
        if (h != hover)
        {
            hover = h;
            repaint();
        }
        setMouseCursor (h != Grab::none ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
    }
}

void ScopeView::mouseDown (const MouseEvent& e)
{
    using namespace ParamIDs;
    grab = Grab::none;
    if (view == View::wave)
    {
        grab = waveLineAt ((float) e.y);
        if (grab == Grab::thresh) thresholdParam->beginChangeGesture();
        if (grab == Grab::glue)   glueThreshParam->beginChangeGesture();
        return;
    }
    if (ottOn() && ottChip().contains (e.position))
    {
        if (ottFocus)
            selectFilter (postSelected);
        else
            setOttFocus (true);
        return;
    }
    if (ottFocus)
        return;
    grab = hitPoint (e.position);
    if (grab == Grab::lc) fp (fLc)->beginChangeGesture();
    if (grab == Grab::hc) fp (fHc)->beginChangeGesture();
    if (grab == Grab::pk) { fp (fPkFreq)->beginChangeGesture(); fp (fPkGain)->beginChangeGesture(); }
}

void ScopeView::mouseDrag (const MouseEvent& e)
{
    using namespace ParamIDs;
    const auto p = plot();
    switch (grab)
    {
        case Grab::thresh:
        {
            const float off = std::abs ((float) e.y - p.getCentreY()) / (p.getHeight() * 0.46f) / ampZoom;
            setParam (thresholdParam, jlimit (-60.0f, 0.0f, Decibels::gainToDecibels (off, -60.0f)));
            break;
        }
        case Grab::glue:
        {
            const float off = std::abs ((float) e.y - p.getCentreY()) / (p.getHeight() * 0.46f) / ampZoom;
            setParam (glueThreshParam, jlimit (-40.0f, 0.0f, Decibels::gainToDecibels (off, -40.0f)));
            break;
        }
        case Grab::lc: setParam (fp (fLc), e.position.x <= p.getX() + 2.0f ? 20.0f : freqForX (e.position.x)); break;
        case Grab::hc: setParam (fp (fHc), e.position.x >= p.getRight() - 2.0f ? 20000.0f : freqForX (e.position.x)); break;
        case Grab::pk:
            setParam (fp (fPkFreq), freqForX (e.position.x));
            setParam (fp (fPkGain), jlimit (-18.0f, 18.0f, dbForY (e.position.y)));
            break;
        case Grab::none: break;
    }
}

void ScopeView::mouseUp (const MouseEvent&)
{
    using namespace ParamIDs;
    if (grab == Grab::thresh) thresholdParam->endChangeGesture();
    if (grab == Grab::glue) glueThreshParam->endChangeGesture();
    if (grab == Grab::lc) fp (fLc)->endChangeGesture();
    if (grab == Grab::hc) fp (fHc)->endChangeGesture();
    if (grab == Grab::pk) { fp (fPkFreq)->endChangeGesture(); fp (fPkGain)->endChangeGesture(); }
    grab = Grab::none;
}

void ScopeView::mouseDoubleClick (const MouseEvent& e)
{
    using namespace ParamIDs;
    if (view == View::wave)
    {
        ampZoom = 1.0f;
        barsIndex = 2;
        if (onBarsChanged)
            onBarsChanged (barsIndex);
        return;
    }
    if (ottFocus)
        return;
    switch (hitPoint (e.position))
    {
        case Grab::lc: setParam (fp (fLc), 20.0f); break;
        case Grab::hc: setParam (fp (fHc), 20000.0f); break;
        case Grab::pk: setParam (fp (fPkGain), 0.0f); setParam (fp (fPkQ), 1.0f); break;
        default: break;
    }
}

void ScopeView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    using namespace ParamIDs;
    const float d = std::abs (w.deltaY) >= std::abs (w.deltaX) ? w.deltaY : w.deltaX;
    if (d == 0.0f)
        return;
    if (view == View::eq)
    {
        if (ottFocus)
            return;
        const auto h = hitPoint (e.position);
        if (h == Grab::lc || h == Grab::hc)
        {
            auto* param = fp (h == Grab::lc ? fLcSlope : fHcSlope);
            setParam (param, (float) jlimit (0, 2, roundToInt (param->convertFrom0to1 (param->getValue())) + (d > 0 ? 1 : -1)));
        }
        else if (h == Grab::pk)
        {
            setParam (fp (fPkQ), jlimit (0.3f, 10.0f, fv (fPkQ) * std::pow (1.25f, d * 4.0f)));
        }
        return;
    }
    static float accum = 0.0f;
    if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        accum += d;
        if (std::abs (accum) >= 0.1f)
        {
            setBarsIndex (barsIndex + (accum > 0 ? -1 : 1));
            accum = 0.0f;
            if (onBarsChanged)
                onBarsChanged (barsIndex);
        }
    }
    else
    {
        ampZoom = jlimit (0.25f, 16.0f, ampZoom * std::pow (1.6f, d * 2.0f));
    }
}

//------------------------------------------------------------------------------
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

    {
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
                g.setColour (Colours::black.withAlpha (0.16f));
                g.fillRect (s.getX(), y, s.getWidth(), 1.0f);
            }

        if (view == View::wave)
            paintWave (g, t);
        else
            paintEq (g, t);

        // Glass: reflection, (Luxe) streak, vignette
        ColourGradient refl (Colours::white.withAlpha (t.gloss ? 0.18f : 0.08f), 0.0f, s.getY(), Colours::white.withAlpha (0.0f), 0.0f, s.getY() + s.getHeight() * 0.4f, false);
        g.setGradientFill (refl);
        g.fillEllipse (s.getX() + s.getWidth() * 0.32f - s.getWidth() * 0.36f, s.getY() + s.getHeight() * 0.10f - s.getHeight() * 0.24f,
                       s.getWidth() * 0.72f, s.getHeight() * 0.48f);
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
        ColourGradient vig (Colours::transparentBlack, s.getCentreX(), s.getCentreY(), Colours::black.withAlpha (0.7f),
                            s.getCentreX() + s.getWidth() * 0.62f, s.getCentreY(), true);
        vig.addColour (0.5, Colours::transparentBlack);
        g.setGradientFill (vig);
        g.fillRect (s);
    }

    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (s, 26.0f, 1.5f);

    if (view == View::eq && ottOn())
    {
    // OTT / FILTERS chip while OTT is on
        if (ottOn())
        {
            const auto chip = ottChip();
            g.setColour (Colour (0xff0c0c0f).withAlpha (0.9f));
            g.fillRoundedRectangle (chip, 6.0f);
            const auto left = chip.withWidth (chip.getWidth() * 0.4f), right = chip.withTrimmedLeft (chip.getWidth() * 0.4f);
            g.setColour (t.accent.withAlpha (0.85f));
            g.fillRoundedRectangle ((ottFocus ? left : right).reduced (2.0f), 5.0f);
            g.setFont (uiFont (11.0f, true, 0.05f));
            g.setColour (ottFocus ? Colour (0xff141417) : Colour (0xffb4b4bc));
            g.drawText ("OTT", left, Justification::centred);
            g.setColour (ottFocus ? Colour (0xffb4b4bc) : Colour (0xff141417));
            g.drawText ("FILTERS", right, Justification::centred);
        }
    }

    // Backings behind the top controls
    g.setColour (Colour ((uint8) 10, 10, 12, 0.8f));
    g.fillRoundedRectangle (followArea().toFloat(), 8.0f);
    g.fillRoundedRectangle (pillsArea().toFloat(), 8.0f);
    g.fillRoundedRectangle (viewSwitch.getBounds().toFloat().expanded (6.0f, 5.0f), 8.0f);

    g.setColour (Colour ((uint8) 165, 165, 173, 0.45f));
    g.setFont (uiFont (11.0f));
    const String hint = view == View::wave ? (glueOn() ? "drag the comp line: threshold    wheel: height    shift + wheel: bars"
                                                       : "wheel: height    shift + wheel: bars    double-click: reset")
                      : (ottFocus ? "OTT: bars from below lift the quiet parts, bars from above push the loud parts down"
                                  : "drag the points    wheel on a point: slope / width    double-click: reset");
    g.drawText (hint, s.withTrimmedRight (20.0f).withTrimmedBottom (9.0f), Justification::bottomRight);
}

void ScopeView::paintWave (Graphics& g, const Theme& t)
{
    using Scope = CrumbleProcessor::Scope;
    auto rawv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const auto p = plot();
    const float L = p.getX(), R = p.getRight(), cy = p.getCentreY();
    const float A = amp();
    const float maxV = 1.15f / ampZoom;

    // ---- Collect columns: groups of buckets anchored to absolute positions, so shapes stay still while scrolling
    const int count = proc.scope.writeCount.load (std::memory_order_acquire);
    const double bpm = jmax (20.0, proc.scope.bpm.load (std::memory_order_relaxed));
    const double buckets = jlimit (50.0, (double) Scope::size - 2.0, barValues[barsIndex] * 4.0 * 60.0 / bpm * 1000.0);
    const double bpp = buckets / (double) p.getWidth();
    const double gs = jmax (1.0, bpp);
    const double startAbs = (double) count - buckets;

    struct Col { float x, hi, lo, in, pre, gr; };
    std::vector<Col> cols;
    cols.reserve ((size_t) (p.getWidth() + 8.0f));
    const int64 gFirst = (int64) std::floor (startAbs / gs) - 1, gLast = (int64) std::floor ((double) (count - 1) / gs);
    const int64 oldest = jmax ((int64) 0, (int64) count - (int64) Scope::size + 2);
    for (int64 gi = gFirst; gi <= gLast; ++gi)
    {
        const int64 b0 = jmax (oldest, (int64) std::ceil ((double) gi * gs));
        const int64 b1 = jmin ((int64) count, (int64) std::ceil ((double) (gi + 1) * gs));
        if (b1 <= b0)
            continue;
        Col c { 0.0f, -10.0f, 10.0f, 0.0f, 0.0f, 0.0f };
        for (int64 b = b0; b < b1; ++b)
        {
            const int idx = (int) (b & (Scope::size - 1));
            c.hi = jmax (c.hi, proc.scope.hi[idx]);
            c.lo = jmin (c.lo, proc.scope.lo[idx]);
            c.in = jmax (c.in, proc.scope.in[idx]);
            c.pre = jmax (c.pre, proc.scope.preClip[idx]);
            c.gr = jmin (c.gr, proc.scope.gr[idx]);
        }
        c.x = L + (float) (((double) gi * gs + gs * 0.5 - startAbs) / bpp);
        cols.push_back (c);
    }
    // Light smoothing between neighbours
    for (size_t i = 1; i + 1 < cols.size(); ++i)
    {
        cols[i].hi = 0.25f * cols[i - 1].hi + 0.5f * cols[i].hi + 0.25f * cols[i + 1].hi;
        cols[i].lo = 0.25f * cols[i - 1].lo + 0.5f * cols[i].lo + 0.25f * cols[i + 1].lo;
    }

    auto yFor = [&] (float v) { return cy - jlimit (-maxV, maxV, v) * A; };
    auto line = [&] (float x0, float y0, float x1, float y1, int segments)
    {
        Path path;
        for (int k = 0; k <= segments; ++k)
        {
            const float a = (float) k / (float) segments;
            const auto pt = bulge (x0 + (x1 - x0) * a, y0 + (y1 - y0) * a);
            if (k == 0) path.startNewSubPath (pt); else path.lineTo (pt);
        }
        return path;
    };

    // ---- Bar and beat grid
    {
        const double bpb = bpm / 60.0 * 0.001;
        const double beatAt = proc.scope.beatAt.load (std::memory_order_relaxed);
        const double beatCount = (double) proc.scope.beatCount.load (std::memory_order_relaxed);
        const double b0 = beatAt + (startAbs - beatCount) * bpb, b1 = beatAt + ((double) count - beatCount) * bpb;
        const double pxPerBeat = 1.0 / bpb / bpp;
        for (int64 B = (int64) std::ceil (b0); B <= (int64) std::floor (b1); ++B)
        {
            const bool bar = ((B % 4) + 4) % 4 == 0;
            if (! bar && pxPerBeat < 14.0)
                continue;
            const float x = L + (float) ((beatCount + ((double) B - beatAt) / bpb - startAbs) / bpp);
            g.setColour (Colours::white.withAlpha (bar ? 0.10f : 0.04f));
            g.strokePath (line (x, p.getY() - 6.0f, x, p.getBottom() + 6.0f, 6), PathStrokeType (1.0f));
        }
    }

    const bool follow = rawv (ParamIDs::folOn) > 0.5f;

    // ---- dB scale (when Follow is off)
    if (! follow)
    {
        float lastY = -1000.0f;
        const std::vector<float> fine { 0.0f, -1.0f, -2.0f, -3.0f, -4.5f, -6.0f, -9.0f, -12.0f, -18.0f, -24.0f, -36.0f };
        const std::vector<float> coarse { 0.0f, -3.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f };
        for (const float db : ampZoom >= 2.5f ? fine : coarse)
        {
            const float lin = Decibels::decibelsToGain (db) * ampZoom;
            if (lin > 1.12f)
                continue;
            const float y = cy - lin * p.getHeight() * 0.46f;
            if (y - lastY < 20.0f || cy - y < 10.0f)
                continue;
            lastY = y;
            for (const float yy : { y, 2.0f * cy - y })
            {
                g.setColour (Colours::white.withAlpha (0.055f));
                g.strokePath (line (L, yy, R, yy, 12), PathStrokeType (1.0f));
            }
            g.setColour (Colour ((uint8) 200, 200, 210, 0.55f));
            g.setFont (uiFont (10.0f));
            const String label = db == 0.0f ? String ("0") : String (db, db == std::floor (db) ? 0 : 1);
            const auto pl = bulge (L - 26.0f, y), pr = bulge (R + 26.0f, y);
            g.drawText (label, Rectangle<float> (pl.x - 20.0f, pl.y - 7.0f, 40.0f, 14.0f), Justification::centred);
            g.drawText (label, Rectangle<float> (pr.x - 20.0f, pr.y - 7.0f, 40.0f, 14.0f), Justification::centred);
        }
    }
    g.setColour (Colours::white.withAlpha (0.07f));
    g.strokePath (line (L, cy, R, cy, 12), PathStrokeType (1.0f));

    {
        const float b = barValues[barsIndex];
        const String label = b < 1.0f ? String ("1/2 bar") : (b == 1.0f ? String ("1 bar") : String ((int) b) + " bars");
        g.setColour (Colour ((uint8) 200, 200, 210, 0.6f));
        g.setFont (uiFont (11.0f, true));
        g.drawText (label, screen().withTrimmedLeft (22.0f).withTrimmedBottom (9.0f), Justification::bottomLeft);
    }

    Graphics::ScopedSaveState clipState (g);
    g.reduceClipRegion (p.expanded (0.0f, 30.0f).toNearestInt());

    if (cols.size() < 2)
        return;

    // ---- Input ghost
    {
        Path ghost;
        bool started = false;
        for (const auto& c : cols)
        {
            const auto pt = bulge (c.x, yFor (c.in));
            if (! started) { ghost.startNewSubPath (pt); started = true; } else ghost.lineTo (pt);
        }
        started = false;
        for (const auto& c : cols)
        {
            const auto pt = bulge (c.x, yFor (-c.in));
            if (! started) { ghost.startNewSubPath (pt); started = true; } else ghost.lineTo (pt);
        }
        g.setColour (Colours::white.withAlpha (0.15f));
        g.strokePath (ghost, PathStrokeType (1.2f));
    }

    // ---- Filled waveform
    const bool crOn = rawv (ParamIDs::crOn) > 0.5f, bcOn = rawv (ParamIDs::bcOn) > 0.5f, colOn = rawv (ParamIDs::colOn) > 0.5f;
    const float intensity = jlimit (0.0f, 1.0f, (crOn ? rawv (ParamIDs::amount) / 100.0f * 0.85f : 0.0f)
                                                + (colOn ? rawv (ParamIDs::drive) / 100.0f * 0.35f : 0.0f) + (bcOn ? 0.25f : 0.0f));
    Path wave;
    wave.startNewSubPath (bulge (cols.front().x, yFor (cols.front().hi)));
    for (size_t i = 1; i < cols.size(); ++i)
        wave.lineTo (bulge (cols[i].x, yFor (cols[i].hi)));
    for (size_t i = cols.size(); i-- > 0;)
        wave.lineTo (bulge (cols[i].x, yFor (cols[i].lo)));
    wave.closeSubPath();

    ColourGradient fill (t.accentHi.withAlpha (0.50f + 0.2f * intensity), 0.0f, cy - A, t.accentHi.withAlpha (0.50f + 0.2f * intensity), 0.0f, cy + A, false);
    fill.addColour (0.5, t.accentDim.withAlpha (0.28f));
    g.setGradientFill (fill);
    g.fillPath (wave);
    g.setColour (t.accentHi.withAlpha (0.75f + 0.25f * intensity));
    g.strokePath (wave, PathStrokeType (1.3f + intensity * 0.7f, PathStrokeType::curved, PathStrokeType::rounded));

    // ---- Follow threshold lines, brighter where the signal crosses them
    if (follow)
    {
        const float off = thresholdOffset();
        {
            Graphics::ScopedSaveState hi (g);
            RectangleList<int> region;
            region.add (p.withBottom (cy - off).toNearestInt());
            region.add (p.withTop (cy + off).toNearestInt());
            g.reduceClipRegion (region);
            g.setColour (t.accentHi);
            g.fillPath (wave);
        }
        const float dashes[] = { 7.0f, 6.0f };
        for (const float y : { cy - off, cy + off })
        {
            Path dashed;
            PathStrokeType (1.5f).createDashedStroke (dashed, line (L - 20.0f, y, R + 20.0f, y, 16), dashes, 2);
            g.setColour (t.accentHi.withAlpha (0.9f));
            g.fillPath (dashed);
        }
        const String label = String (rawv (ParamIDs::folThresh), 1) + " dB";
        const auto pos = bulge (R - 40.0f, cy - off - 16.0f);
        const Rectangle<float> tag (pos.x - 34.0f, pos.y - 10.0f, 68.0f, 20.0f);
        g.setColour (Colour (0xff141417));
        g.fillRoundedRectangle (tag, 5.0f);
        g.setColour (t.accent.withAlpha (0.6f));
        g.drawRoundedRectangle (tag, 5.0f, 1.0f);
        g.setColour (t.accentHi);
        g.setFont (uiFont (12.0f));
        g.drawText (label, tag, Justification::centred);
    }

    // ---- Glue: threshold lines, the loud parts tinted, gain reduction hanging from the top
    if (glueOn())
    {
        const auto cc = t.comp();
        const float off = glueOffset();
        {
            Graphics::ScopedSaveState hi (g);
            RectangleList<int> region;
            region.add (p.withBottom (cy - off).toNearestInt());
            region.add (p.withTop (cy + off).toNearestInt());
            g.reduceClipRegion (region);
            g.setColour (cc.withAlpha (0.5f));
            g.fillPath (wave);
        }
        // Gain reduction over time
        {
            const float grScale = p.getHeight() * 0.24f / 24.0f;
            Path gr;
            gr.startNewSubPath (bulge (cols.front().x, p.getY()));
            for (const auto& c : cols)
                gr.lineTo (bulge (c.x, p.getY() + jmin (24.0f, -c.gr) * grScale));
            gr.lineTo (bulge (cols.back().x, p.getY()));
            gr.closeSubPath();
            ColourGradient gg (cc.withAlpha (0.30f), 0.0f, p.getY(), cc.withAlpha (0.05f), 0.0f, p.getY() + 24.0f * grScale, false);
            g.setGradientFill (gg);
            g.fillPath (gr);
            Path edge;
            bool started = false;
            for (const auto& c : cols)
            {
                const auto pt = bulge (c.x, p.getY() + jmin (24.0f, -c.gr) * grScale);
                if (! started) { edge.startNewSubPath (pt); started = true; } else edge.lineTo (pt);
            }
            g.setColour (cc.withAlpha (0.9f));
            g.strokePath (edge, PathStrokeType (1.4f));
        }
        for (const float y : { cy - off, cy + off })
        {
            g.setColour (cc.withAlpha (0.95f));
            g.strokePath (line (L - 20.0f, y, R + 20.0f, y, 16), PathStrokeType (1.6f));
        }
        const int ratioIdx = jlimit (0, 2, roundToInt (rawv (ParamIDs::glueRatio)));
        const String label = "COMP " + String (rawv (ParamIDs::glueThresh), 1) + " dB  " + Tables::glueRatios[ratioIdx];
        const auto pos = bulge (L + (R - L) * 0.5f, cy + off + 16.0f);
        const Rectangle<float> tag (pos.x - 70.0f, pos.y - 10.0f, 140.0f, 20.0f);
        g.setColour (Colour (0xff141417));
        g.fillRoundedRectangle (tag, 5.0f);
        g.setColour (cc.withAlpha (0.7f));
        g.drawRoundedRectangle (tag, 5.0f, 1.0f);
        g.setColour (cc);
        g.setFont (uiFont (11.0f, true));
        g.drawText (label, tag, Justification::centred);
    }

    // ---- Clipper lines while Ceiling is being changed
    const double since = Time::getMillisecondCounterHiRes() - ceilingTouchMs;
    const float clipAlpha = ceilingDragging ? 1.0f : (float) jlimit (0.0, 1.0, 1.0 - (since - 1200.0) / 800.0);
    if (clipAlpha > 0.0f)
    {
        const float ceilLin = Decibels::decibelsToGain (rawv (ParamIDs::ceiling));
        const float off = jmin (1.2f, ceilLin * ampZoom) * p.getHeight() * 0.46f;
        const float colW = jmax (1.0f, (float) (gs / bpp));
        g.setColour (kClipRed.withAlpha (0.55f * clipAlpha));
        for (const auto& c : cols)
            if (c.pre > ceilLin)
            {
                const float h = (jmin (maxV, c.pre) - ceilLin) * A;
                const auto top = bulge (c.x, cy - off - h);
                g.fillRect (top.x - colW * 0.5f, top.y, colW, h);
                const auto bot = bulge (c.x, cy + off);
                g.fillRect (bot.x - colW * 0.5f, bot.y, colW, h);
            }
        for (const float y : { cy - off, cy + off })
        {
            g.setColour (kClipRed.withAlpha (0.95f * clipAlpha));
            g.strokePath (line (L - 20.0f, y, R + 20.0f, y, 16), PathStrokeType (2.0f));
        }
        const String label = "Ceiling " + String (rawv (ParamIDs::ceiling), 1) + " dB";
        const auto pos = bulge (R - 70.0f, cy - off + 16.0f);
        const Rectangle<float> tag (pos.x - 52.0f, pos.y - 10.0f, 104.0f, 20.0f);
        g.setColour (Colour (0xff141417).withAlpha (clipAlpha));
        g.fillRoundedRectangle (tag, 5.0f);
        g.setColour (kClipRed.withAlpha (0.8f * clipAlpha));
        g.drawRoundedRectangle (tag, 5.0f, 1.0f);
        g.setColour (Colour (0xffff6670).withAlpha (clipAlpha));
        g.setFont (uiFont (11.0f));
        g.drawText (label, tag, Justification::centred);
    }

    // ---- Crumbs falling off the newest part of the wave
    if (intensity > 0.02f && particles.size() < 380)
    {
        const int spawn = rng.nextFloat() < intensity * intensity * 2.5f ? 1 + (int) (intensity * 2.0f) : 0;
        for (int k = 0; k < spawn && ! cols.empty(); ++k)
        {
            const auto& c = cols[(size_t) jmax (0, (int) cols.size() - 1 - rng.nextInt (jmin (30, (int) cols.size())))];
            const bool top = rng.nextBool();
            const auto pt = bulge (c.x, yFor (top ? c.hi : c.lo));
            particles.push_back ({ pt.x, pt.y, (rng.nextFloat() - 0.5f) * 0.3f, rng.nextFloat() * 0.3f, 1.0f });
        }
    }
    for (const auto& pt : particles)
    {
        g.setColour (t.accentHi.withAlpha (jlimit (0.0f, 1.0f, pt.life) * 0.8f));
        g.fillEllipse (pt.x - 1.0f, pt.y - 1.0f, 2.0f, 2.0f);
    }

    if (glueOn())
        paintGluePanel (g, t);
}

// A small graph in the corner: how Glue squeezes (input level -> output level), and how much right now
void ScopeView::paintGluePanel (Graphics& g, const Theme& t)
{
    auto rawv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const auto cc = t.comp();
    const auto p = plot();
    const float side = jlimit (70.0f, 150.0f, p.getHeight() * 0.52f);
    const Rectangle<float> panel (p.getX() + 6.0f, p.getY() + 6.0f, side + 64.0f, side + 46.0f);
    g.setColour (Colour (0xff0c0c0f).withAlpha (0.88f));
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (cc.withAlpha (0.35f));
    g.drawRoundedRectangle (panel, 8.0f, 1.0f);

    const float thr = rawv (ParamIDs::glueThresh);
    const float ratio = Tables::glueRatioValues[jlimit (0, 2, roundToInt (rawv (ParamIDs::glueRatio)))];
    g.setColour (cc);
    g.setFont (uiFont (11.0f, true, 0.1f));
    g.drawText ("GLUE  " + Tables::glueRatios[jlimit (0, 2, roundToInt (rawv (ParamIDs::glueRatio)))],
                panel.withHeight (22.0f).withTrimmedLeft (10.0f), Justification::centredLeft);

    // Graph: -48 .. 0 dB on both axes
    const Rectangle<float> gr (panel.getX() + 10.0f, panel.getY() + 24.0f, side, side);
    constexpr float lo = -48.0f;
    auto gx = [&] (float db) { return gr.getX() + (jlimit (lo, 0.0f, db) - lo) / -lo * gr.getWidth(); };
    auto gy = [&] (float db) { return gr.getBottom() - (jlimit (lo, 0.0f, db) - lo) / -lo * gr.getHeight(); };
    g.setColour (Colours::white.withAlpha (0.05f));
    g.fillRect (gr);
    g.setColour (Colours::white.withAlpha (0.08f));
    for (float d = -12.0f; d > lo; d -= 12.0f)
    {
        g.drawHorizontalLine ((int) gy (d), gr.getX(), gr.getRight());
        g.drawVerticalLine ((int) gx (d), gr.getY(), gr.getBottom());
    }
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawLine (gx (lo), gy (lo), gx (0.0f), gy (0.0f), 1.0f);
    auto curve = [&] (float in)
    {
        constexpr float knee = 6.0f;
        const float over = in - thr, slope = 1.0f / ratio - 1.0f;
        float grDb = 0.0f;
        if (over >= knee * 0.5f)       grDb = over * slope;
        else if (over > -knee * 0.5f)  grDb = (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee) * slope;
        return in + grDb;
    };
    Path c;
    for (float in = lo; in <= 0.01f; in += 0.5f)
    {
        const Point<float> pt (gx (in), gy (curve (in)));
        if (in == lo) c.startNewSubPath (pt); else c.lineTo (pt);
    }
    g.setColour (cc);
    g.strokePath (c, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    const float dashes[] = { 3.0f, 3.0f };
    Path tl;
    tl.startNewSubPath (gx (thr), gr.getBottom());
    tl.lineTo (gx (thr), gy (curve (thr)));
    Path dashed;
    PathStrokeType (1.0f).createDashedStroke (dashed, tl, dashes, 2);
    g.setColour (cc.withAlpha (0.5f));
    g.fillPath (dashed);

    // The live dot: where the signal is now
    if (glueLevelShown > lo)
    {
        const Point<float> dot (gx (glueLevelShown), gy (glueLevelShown + grShown));
        g.setColour (cc.withAlpha (0.3f));
        g.fillEllipse (dot.x - 7.0f, dot.y - 7.0f, 14.0f, 14.0f);
        g.setColour (Colours::white);
        g.fillEllipse (dot.x - 3.5f, dot.y - 3.5f, 7.0f, 7.0f);
    }

    // Gain reduction meter
    const Rectangle<float> meter (gr.getRight() + 14.0f, gr.getY(), 12.0f, gr.getHeight());
    g.setColour (Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (meter, 3.0f);
    const float amount = jlimit (0.0f, 1.0f, -grShown / 24.0f);
    g.setColour (cc);
    g.fillRoundedRectangle (meter.withHeight (meter.getHeight() * amount), 3.0f);
    g.setColour (Colour ((uint8) 200, 200, 210, 0.6f));
    g.setFont (uiFont (9.0f));
    g.drawText ("GR", meter.withY (meter.getY() - 14.0f).withHeight (12.0f).expanded (8.0f, 0.0f), Justification::centred);
    g.setColour (cc);
    g.setFont (uiFont (11.0f, true));
    g.drawText (String (grShown, 1), Rectangle<float> (meter.getCentreX() - 22.0f, meter.getBottom() + 2.0f, 44.0f, 14.0f), Justification::centred);
}

void ScopeView::paintEq (Graphics& g, const Theme& t)
{
    using namespace ParamIDs;
    const auto p = plot();
    const double sr = proc.currentSampleRate.load();
    const bool showOttBands = ottFocus && ottOn();

    // ---- Grid
    g.setFont (uiFont (10.0f));
    for (const float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f);
        Path l;
        l.startNewSubPath (bulge (x, p.getY() - 6.0f));
        l.lineTo (bulge (x, p.getCentreY()));
        l.lineTo (bulge (x, p.getBottom() + 4.0f));
        g.setColour (Colours::white.withAlpha (f == 1000.0f ? 0.08f : 0.05f));
        g.strokePath (l, PathStrokeType (1.0f));
        g.setColour (Colour ((uint8) 200, 200, 210, 0.5f));
        const auto pos = bulge (x, p.getBottom() + 12.0f);
        g.drawText (f >= 1000.0f ? String ((int) (f / 1000.0f)) + "k" : String ((int) f), Rectangle<float> (pos.x - 20.0f, pos.y - 7.0f, 40.0f, 14.0f), Justification::centred);
    }
    for (const float db : { 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f })
    {
        if (showOttBands)
            break;
        const float y = yForDb (db);
        Path l;
        for (int k = 0; k <= 12; ++k)
        {
            const auto pt = bulge (p.getX() + p.getWidth() * (float) k / 12.0f, y);
            if (k == 0) l.startNewSubPath (pt); else l.lineTo (pt);
        }
        g.setColour (Colours::white.withAlpha (db == 0.0f ? 0.10f : 0.045f));
        g.strokePath (l, PathStrokeType (1.0f));
        g.setColour (Colour ((uint8) 200, 200, 210, 0.5f));
        const auto pos = bulge (p.getX() - 26.0f, y);
        g.drawText ((db > 0 ? "+" : "") + String ((int) db), Rectangle<float> (pos.x - 20.0f, pos.y - 7.0f, 40.0f, 14.0f), Justification::centred);
    }

    Graphics::ScopedSaveState clipState (g);
    g.reduceClipRegion (p.expanded (14.0f, 4.0f).toNearestInt());

    // ---- Spectrum (input for PRE, output for POST)
    {
        Path spec;
        const float bottom = p.getBottom() + 4.0f;
        spec.startNewSubPath (bulge (p.getX(), bottom));
        float prevY = bottom;
        for (float x = p.getX(); x <= p.getRight(); x += 2.0f)
        {
            const float f0 = freqForX (x - 1.0f), f1 = freqForX (x + 1.0f);
            const float k0 = f0 / (float) sr * (float) fftSize, k1 = f1 / (float) sr * (float) fftSize;
            float db = -120.0f;
            if (k1 - k0 < 1.0f)
            {
                const float k = 0.5f * (k0 + k1);
                const int i = jlimit (0, fftSize / 2 - 2, (int) k);
                const float fr = jlimit (0.0f, 1.0f, k - (float) i);
                db = specDb[(size_t) i] + (specDb[(size_t) i + 1] - specDb[(size_t) i]) * fr;
            }
            else
            {
                for (int i = jlimit (0, fftSize / 2 - 1, (int) k0); i <= jlimit (0, fftSize / 2 - 1, (int) k1); ++i)
                    db = jmax (db, specDb[(size_t) i]);
            }
            db += 4.5f * std::log2 (jmax (20.0f, freqForX (x)) / 1000.0f);   // tilt so pink noise looks flat
            float y = jlimit (p.getY(), bottom, p.getBottom() - (db + 84.0f) / 78.0f * p.getHeight() * 0.9f);
            y = 0.5f * (y + prevY);
            prevY = y;
            spec.lineTo (bulge (x, y));
        }
        spec.lineTo (bulge (p.getRight(), bottom));
        spec.closeSubPath();
        ColourGradient sg (Colours::white.withAlpha (showOttBands ? 0.12f : 0.22f), 0.0f, p.getY(), Colours::white.withAlpha (0.02f), 0.0f, bottom, false);
        g.setGradientFill (sg);
        g.fillPath (spec);
    }

    if (showOttBands)
    {
        paintOtt (g, t);
        return;
    }

    // ---- Filter curves: the other filter faint, the selected one bright
    auto curvePath = [&] (bool post, bool closed)
    {
        auto v = [&] (const char* n) { return proc.apvts.getRawParameterValue (filterId (post, n))->load(); };
        Path path;
        bool started = false;
        for (float x = p.getX() - 14.0f; x <= p.getRight() + 14.0f; x += 2.0f)
        {
            const double m = cdsp::FilterBank::magnitude (sr, freqForX (x), v (fLc), v (fHc), roundToInt (v (fLcSlope)),
                                                          roundToInt (v (fHcSlope)), v (fPkFreq), v (fPkGain), v (fPkQ));
            const auto pt = bulge (x, yForDb (jlimit (-30.0f, 24.0f, cdsp::gainToDb ((float) m))));
            if (! started) { path.startNewSubPath (pt); started = true; } else path.lineTo (pt);
        }
        if (closed)
        {
            path.lineTo (bulge (p.getRight() + 14.0f, yForDb (-30.0f)));
            path.lineTo (bulge (p.getX() - 14.0f, yForDb (-30.0f)));
            path.closeSubPath();
        }
        return path;
    };
    const bool otherOn = proc.apvts.getRawParameterValue (filterId (! postSelected, fOn))->load() > 0.5f;
    const bool selOn = proc.apvts.getRawParameterValue (filterId (postSelected, fOn))->load() > 0.5f;
    if (otherOn)
    {
        g.setColour (Colours::white.withAlpha (0.28f));
        g.strokePath (curvePath (! postSelected, false), PathStrokeType (1.3f));
    }
    const float a = selOn ? 1.0f : 0.4f;
    g.setColour (t.accent.withAlpha (0.13f * a));
    g.fillPath (curvePath (postSelected, true));
    g.setColour (t.accentHi.withAlpha (a));
    g.strokePath (curvePath (postSelected, false), PathStrokeType (2.4f, PathStrokeType::curved, PathStrokeType::rounded));

    // ---- Points
    auto drawPoint = [&] (Grab gp, const String& label)
    {
        const auto pos = bulge (pointPos (gp).x, pointPos (gp).y);
        const bool hot = hover == gp || grab == gp;
        g.setColour (t.accent.withAlpha ((hot ? 0.35f : 0.22f) * a));
        g.fillEllipse (pos.x - 13.0f, pos.y - 13.0f, 26.0f, 26.0f);
        g.setColour (Colour (0xff141417));
        g.fillEllipse (pos.x - 8.0f, pos.y - 8.0f, 16.0f, 16.0f);
        g.setColour (t.accent.withAlpha (a));
        g.drawEllipse (pos.x - 8.0f, pos.y - 8.0f, 16.0f, 16.0f, 2.5f);
        const Rectangle<float> tag (pos.x - 44.0f, pos.y - 38.0f, 88.0f, 20.0f);
        g.setColour (Colour (0xff141417).withAlpha (0.9f));
        g.fillRoundedRectangle (tag, 5.0f);
        g.setColour (t.accent.withAlpha (0.6f * a));
        g.drawRoundedRectangle (tag, 5.0f, 1.0f);
        g.setColour (t.accentHi.withAlpha (jmax (0.6f, a)));
        g.setFont (uiFont (11.0f));
        g.drawText (label, tag, Justification::centred);
    };
    auto hz = [] (float f) { return f < 1000.0f ? String (roundToInt (f)) + " Hz" : String (f / 1000.0f, 1) + " kHz"; };
    const char* slopes[3] = { "12", "24", "48" };
    const float lc = fv (fLc), hc = fv (fHc);
    drawPoint (Grab::lc, lc <= 20.05f ? String ("LC off") : "LC " + hz (lc) + " /" + slopes[jlimit (0, 2, roundToInt (fv (fLcSlope)))]);
    drawPoint (Grab::hc, hc >= 19990.0f ? String ("HC off") : "HC " + hz (hc) + " /" + slopes[jlimit (0, 2, roundToInt (fv (fHcSlope)))]);
    const float pg = fv (fPkGain);
    drawPoint (Grab::pk, hz (fv (fPkFreq)) + " " + (pg > 0.05f ? "+" : "") + String (pg, 1) + " dB");

    if (! selOn)
    {
        g.setColour (Colour ((uint8) 220, 220, 228, 0.75f));
        g.setFont (uiFont (13.0f));
        g.drawText (String (postSelected ? "POST" : "PRE") + " filter is off. Click its light to turn it on.",
                    p.withTrimmedTop (p.getHeight() * 0.62f).withHeight (24.0f), Justification::centred);
    }
}

// Three OTT bands: bars from the bottom = how much the quiet parts are lifted,
// bars from the top = how much the loud parts are pushed down; the thin strip is the band level
void ScopeView::paintOtt (Graphics& g, const Theme& t)
{
    const auto p = plot();
    const auto cc = t.comp();
    const float edges[4] = { 20.0f, 250.0f, 2500.0f, 20000.0f };
    const char* names[3] = { "LOW", "MID", "HIGH" };
    const float half = p.getHeight() * 0.5f;
    constexpr float range = 24.0f;    // dB shown by a half-height bar
    constexpr float levelLo = -72.0f;

    for (int b = 0; b < 3; ++b)
    {
        const float x0 = xForFreq (edges[b]) + (b == 0 ? 0.0f : 5.0f);
        const float x1 = xForFreq (edges[b + 1]) - (b == 2 ? 0.0f : 5.0f);
        const Rectangle<float> col (x0, p.getY(), x1 - x0, p.getHeight());
        const auto colPath = bulgedRect (col);
        g.setColour (Colours::white.withAlpha (0.035f));
        g.fillPath (colPath);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.strokePath (colPath, PathStrokeType (1.0f));

        const float gainDb = ottGainShown[b];
        const float bw = col.getWidth() * 0.62f;
        const float bx = col.getCentreX() - bw * 0.5f;
        // Up: from the bottom
        if (gainDb > 0.05f)
        {
            const float h = jmin (1.0f, gainDb / range) * half;
            const Rectangle<float> bar (bx, col.getBottom() - h, bw, h);
            ColourGradient ug (t.accentHi.withAlpha (0.85f), 0.0f, bar.getY(), t.accentDim.withAlpha (0.35f), 0.0f, col.getBottom(), false);
            g.setGradientFill (ug);
            g.fillPath (bulgedRect (bar));
            g.setColour (t.accentHi);
            g.strokePath (bulgedRect (bar.withHeight (2.0f)), PathStrokeType (1.0f));
        }
        // Down: from the top
        if (gainDb < -0.05f)
        {
            const float h = jmin (1.0f, -gainDb / range) * half;
            const Rectangle<float> bar (bx, col.getY(), bw, h);
            ColourGradient dg (cc.withAlpha (0.35f), 0.0f, col.getY(), cc.withAlpha (0.85f), 0.0f, bar.getBottom(), false);
            g.setGradientFill (dg);
            g.fillPath (bulgedRect (bar));
            g.setColour (cc);
            g.strokePath (bulgedRect (bar.withTrimmedTop (h - 2.0f)), PathStrokeType (1.0f));
        }

        // Band level strip with the two thresholds of OTT (up -30 dB, down -18 dB)
        const Rectangle<float> strip (col.getRight() - 16.0f, col.getY() + 8.0f, 6.0f, col.getHeight() - 16.0f);
        auto ly = [&] (float db) { return strip.getBottom() - (jlimit (levelLo, 0.0f, db) - levelLo) / -levelLo * strip.getHeight(); };
        g.setColour (Colours::white.withAlpha (0.07f));
        g.fillPath (bulgedRect (strip));
        if (ottLevelShown[b] > levelLo)
        {
            g.setColour (Colours::white.withAlpha (0.55f));
            g.fillPath (bulgedRect (strip.withTop (ly (ottLevelShown[b]))));
        }
        g.setColour (t.accentHi);
        g.strokePath (bulgedRect ({ strip.getX() - 4.0f, ly (-30.0f) - 1.0f, strip.getWidth() + 8.0f, 2.0f }), PathStrokeType (1.0f));
        g.setColour (cc);
        g.strokePath (bulgedRect ({ strip.getX() - 4.0f, ly (-18.0f) - 1.0f, strip.getWidth() + 8.0f, 2.0f }), PathStrokeType (1.0f));

        // Numbers and names
        g.setFont (uiFont (12.0f, true));
        if (gainDb > 0.1f)
        {
            g.setColour (t.accentHi);
            const auto pos = bulge (col.getCentreX(), col.getBottom() - jmin (1.0f, gainDb / range) * half - 12.0f);
            g.drawText ("+" + String (gainDb, 1), Rectangle<float> (pos.x - 30.0f, pos.y - 8.0f, 60.0f, 16.0f), Justification::centred);
        }
        if (gainDb < -0.1f)
        {
            g.setColour (cc);
            const auto pos = bulge (col.getCentreX(), col.getY() + jmin (1.0f, -gainDb / range) * half + 12.0f);
            g.drawText (String (gainDb, 1), Rectangle<float> (pos.x - 30.0f, pos.y - 8.0f, 60.0f, 16.0f), Justification::centred);
        }
        g.setColour (Colour ((uint8) 220, 220, 228, 0.75f));
        g.setFont (uiFont (12.0f, true, 0.15f));
        const auto np = bulge (col.getCentreX(), p.getCentreY());
        g.drawText (names[b], Rectangle<float> (np.x - 40.0f, np.y - 9.0f, 80.0f, 18.0f), Justification::centred);
    }

    // Centre line and a little legend
    Path mid;
    for (int k = 0; k <= 12; ++k)
    {
        const auto pt = bulge (p.getX() + p.getWidth() * (float) k / 12.0f, p.getCentreY());
        if (k == 0) mid.startNewSubPath (pt); else mid.lineTo (pt);
    }
    g.setColour (Colours::white.withAlpha (0.06f));
    g.strokePath (mid, PathStrokeType (1.0f));
}

//==============================================================================
MeterColumn::MeterColumn (const String& t, AudioProcessorValueTreeState& apvts, const char* gainParam, const String& knobName)
    : title (t)
{
    knob.init (*this, apvts, knobName, gainParam, false);
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

Rectangle<int> PresetBar::nameBox() const { return { 30, 0, getWidth() - 96, getHeight() }; }

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
    const auto card = getLocalBounds().toFloat().withSizeKeepingCentre (380.0f, 320.0f);
    g.setColour (Colour (0xff141417));
    g.fillRoundedRectangle (card, 14.0f);
    g.setColour (t.accent.withAlpha (0.6f));
    g.drawRoundedRectangle (card.reduced (0.5f), 14.0f, 1.2f);
    Logo::draw (g, card.withHeight (130.0f).withY (card.getY() + 24.0f).reduced (100.0f, 0.0f), FillType (t.lightBg ? t.accent : t.logo));
    g.setColour (Colour (0xffececf0));
    g.setFont (uiFont (30.0f, true, 0.08f));
    g.drawText ("CRUMBLE", card.withY (card.getY() + 166.0f).withHeight (36.0f), Justification::centred);
    g.setColour (t.accent);
    g.setFont (uiFont (14.0f));
    g.drawText ("Version " + String (JucePlugin_VersionString), card.withY (card.getY() + 204.0f).withHeight (20.0f), Justification::centred);
    g.setColour (Colour (0xffa5a5ad));
    g.setFont (uiFont (12.0f, true, 0.3f));
    g.drawText ("PRIMAL RAGE LAB", card.withY (card.getY() + 238.0f).withHeight (18.0f), Justification::centred);
    g.setFont (uiFont (11.0f));
    g.setColour (Colour (0xff6a6a72));
    g.drawText ("Click anywhere to close", card.withY (card.getY() + 284.0f).withHeight (18.0f), Justification::centred);
}

//==============================================================================
TourOverlay::TourOverlay()
{
    for (auto* b : { &back, &next, &skip })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    back.onClick = [this] { showStep (step - 1); };
    next.onClick = [this]
    {
        if (step + 1 >= Help::tourLength)
        {
            setVisible (false);
            if (onFinish) onFinish();
        }
        else
        {
            showStep (step + 1);
        }
    };
    skip.onClick = [this] { setVisible (false); if (onFinish) onFinish(); };
}

void TourOverlay::start (Help::Lang l)
{
    lang = l;
    setVisible (true);
    toFront (false);
    showStep (0);
}

void TourOverlay::showStep (int s)
{
    step = jlimit (0, Help::tourLength - 1, s);
    const auto& t = themeOf (*this);
    back.setButtonText (Help::ui ("Back", "Назад", lang));
    next.setButtonText (step + 1 >= Help::tourLength ? Help::ui ("Done", "Готово", lang) : Help::ui ("Next", "Дальше", lang));
    skip.setButtonText (Help::ui ("Skip", "Пропустить", lang));
    next.setColour (TextButton::buttonColourId, t.accent);
    next.setColour (TextButton::textColourOffId, Colour (0xff111114));
    back.setVisible (step > 0);
    skip.setVisible (step + 1 < Help::tourLength);
    if (onStep)
        onStep (Help::tour[step].target);
    resized();
    repaint();
}

Rectangle<float> TourOverlay::calloutArea() const
{
    const auto& st = Help::tour[step];
    const auto target = resolveTarget ? resolveTarget (st.target) : Rectangle<float>();
    const float w = 380.0f;
    AttributedString s;
    s.append (Help::ui (st.textEn, st.textRu, lang), uiFont (14.0f), Colours::white);
    TextLayout tl;
    tl.createLayout (s, w - 36.0f);
    const float h = 54.0f + tl.getHeight() + 70.0f;
    const auto bounds = getLocalBounds().toFloat().reduced (16.0f);
    if (target.isEmpty())
        return bounds.withSizeKeepingCentre (w, h);
    float x = jlimit (bounds.getX(), bounds.getRight() - w, target.getCentreX() - w * 0.5f);
    float y = target.getBottom() + 22.0f;
    if (y + h > bounds.getBottom())
        y = target.getY() - 22.0f - h;
    if (y < bounds.getY())
    {
        // Not enough room above or below: put it to the side
        y = jlimit (bounds.getY(), bounds.getBottom() - h, target.getCentreY() - h * 0.5f);
        x = target.getRight() + 22.0f + w < bounds.getRight() ? target.getRight() + 22.0f : target.getX() - 22.0f - w;
    }
    return { x, y, w, h };
}

void TourOverlay::resized()
{
    const auto c = calloutArea().toNearestInt();
    next.setBounds (c.getRight() - 16 - 96, c.getBottom() - 16 - 30, 96, 30);
    back.setBounds (next.getX() - 8 - 86, next.getY(), 86, 30);
    skip.setBounds (c.getX() + 14, next.getY(), 104, 30);
}

void TourOverlay::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto& st = Help::tour[step];
    const auto target = resolveTarget ? resolveTarget (st.target) : Rectangle<float>();

    Path dim;
    dim.addRectangle (getLocalBounds().toFloat());
    if (! target.isEmpty())
    {
        dim.addRoundedRectangle (target.expanded (6.0f), 12.0f);
        dim.setUsingNonZeroWinding (false);
    }
    g.setColour (Colours::black.withAlpha (0.68f));
    g.fillPath (dim);

    const auto c = calloutArea();
    if (! target.isEmpty())
    {
        g.setColour (t.accent.withAlpha (0.35f));
        g.drawRoundedRectangle (target.expanded (9.0f), 14.0f, 5.0f);
        g.setColour (t.accent);
        g.drawRoundedRectangle (target.expanded (6.0f), 12.0f, 2.0f);

        // Arrow from the card to the highlighted part
        Point<float> from, to;
        if (c.getBottom() <= target.getY())       { from = { c.getCentreX(), c.getBottom() }; to = { jlimit (target.getX() + 10.0f, target.getRight() - 10.0f, c.getCentreX()), target.getY() - 8.0f }; }
        else if (c.getY() >= target.getBottom())  { from = { c.getCentreX(), c.getY() };      to = { jlimit (target.getX() + 10.0f, target.getRight() - 10.0f, c.getCentreX()), target.getBottom() + 8.0f }; }
        else if (c.getX() >= target.getRight())   { from = { c.getX(), c.getCentreY() };      to = { target.getRight() + 8.0f, jlimit (target.getY() + 10.0f, target.getBottom() - 10.0f, c.getCentreY()) }; }
        else                                      { from = { c.getRight(), c.getCentreY() };  to = { target.getX() - 8.0f, jlimit (target.getY() + 10.0f, target.getBottom() - 10.0f, c.getCentreY()) }; }
        Path arrow;
        arrow.addArrow (Line<float> (from, to), 2.5f, 12.0f, 10.0f);
        g.setColour (t.accent);
        g.fillPath (arrow);
    }

    g.setColour (Colour (0xff141417));
    g.fillRoundedRectangle (c, 12.0f);
    g.setColour (t.accent.withAlpha (0.7f));
    g.drawRoundedRectangle (c.reduced (0.5f), 12.0f, 1.2f);

    g.setColour (t.accentHi);
    g.setFont (uiFont (17.0f, true));
    g.drawText (Help::ui (st.titleEn, st.titleRu, lang), c.reduced (18.0f, 0.0f).withY (c.getY() + 14.0f).withHeight (24.0f), Justification::centredLeft);
    g.setColour (Colour (0xff8a8a94));
    g.setFont (uiFont (12.0f));
    g.drawText (String (step + 1) + " / " + String (Help::tourLength), c.reduced (18.0f, 0.0f).withY (c.getY() + 14.0f).withHeight (24.0f), Justification::centredRight);

    AttributedString s;
    s.append (Help::ui (st.textEn, st.textRu, lang), uiFont (14.0f), Colour (0xffe2e2e8));
    s.setLineSpacing (3.0f);
    TextLayout tl;
    tl.createLayout (s, c.getWidth() - 36.0f);
    tl.draw (g, Rectangle<float> (c.getX() + 18.0f, c.getY() + 46.0f, c.getWidth() - 36.0f, tl.getHeight()));
}

//==============================================================================
WelcomePrompt::WelcomePrompt()
{
    for (auto* b : { &yes, &notNow, &never, &en, &ru })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    ru.setButtonText (String::fromUTF8 ("Русский"));
    yes.onClick = [this] { if (onChoice) onChoice (0); };
    notNow.onClick = [this] { if (onChoice) onChoice (1); };
    never.onClick = [this] { if (onChoice) onChoice (2); };
    en.onClick = [this] { setLang (Help::Lang::en); if (onLanguage) onLanguage (lang); };
    ru.onClick = [this] { setLang (Help::Lang::ru); if (onLanguage) onLanguage (lang); };
    setLang (Help::Lang::en);
}

void WelcomePrompt::setLang (Help::Lang l)
{
    lang = l;
    const auto& t = themeOf (*this);
    yes.setButtonText (Help::ui ("Yes, show me", "Да, покажи", lang));
    notNow.setButtonText (Help::ui ("Not now", "Не сейчас", lang));
    never.setButtonText (Help::ui ("Don't ask again", "Больше не спрашивать", lang));
    yes.setColour (TextButton::buttonColourId, t.accent);
    yes.setColour (TextButton::textColourOffId, Colour (0xff111114));
    for (auto* b : { &en, &ru })
    {
        const bool sel = (b == &ru) == (lang == Help::Lang::ru);
        b->setColour (TextButton::buttonColourId, sel ? t.accent : Colour (0xff26262b));
        b->setColour (TextButton::textColourOffId, sel ? Colour (0xff111114) : Colour (0xffececf0));
    }
    repaint();
}

Rectangle<float> WelcomePrompt::card() const
{
    return getLocalBounds().toFloat().withSizeKeepingCentre (500.0f, 270.0f);
}

void WelcomePrompt::resized()
{
    const auto c = card().toNearestInt();
    en.setBounds (c.getX() + 170, c.getY() + 128, 100, 28);
    ru.setBounds (en.getRight() + 8, en.getY(), 100, 28);
    yes.setBounds (c.getRight() - 24 - 150, c.getBottom() - 24 - 34, 150, 34);
    notNow.setBounds (yes.getX() - 10 - 120, yes.getY(), 120, 34);
    never.setBounds (c.getX() + 24, yes.getY(), 170, 34);
}

void WelcomePrompt::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    g.fillAll (Colours::black.withAlpha (0.62f));
    const auto c = card();
    g.setColour (Colour (0xff141417));
    g.fillRoundedRectangle (c, 14.0f);
    g.setColour (t.accent.withAlpha (0.7f));
    g.drawRoundedRectangle (c.reduced (0.5f), 14.0f, 1.2f);

    Logo::draw (g, Rectangle<float> (c.getX() + 24.0f, c.getY() + 22.0f, 54.0f, 56.0f), FillType (t.lightBg ? t.accent : t.logo));
    g.setColour (Colour (0xffececf0));
    g.setFont (uiFont (19.0f, true));
    g.drawText (Help::ui ("Hi! Want a quick tour of Crumble?", "Привет! Показать, как работает Crumble?", lang),
                c.withTrimmedLeft (94.0f).withTrimmedRight (20.0f).withY (c.getY() + 28.0f).withHeight (26.0f), Justification::centredLeft);
    g.setColour (Colour (0xffa5a5ad));
    g.setFont (uiFont (13.0f));
    g.drawText (Help::ui ("It takes a minute and shows what every knob does.", "Займёт минуту: покажу, что делает каждая ручка.", lang),
                c.withTrimmedLeft (94.0f).withTrimmedRight (20.0f).withY (c.getY() + 56.0f).withHeight (20.0f), Justification::centredLeft);
    g.setColour (Colour (0xffececf0));
    g.setFont (uiFont (13.0f, true));
    g.drawText (Help::ui ("Help language", "Язык подсказок", lang), Rectangle<float> (c.getX() + 24.0f, c.getY() + 128.0f, 140.0f, 28.0f), Justification::centredLeft);
}
