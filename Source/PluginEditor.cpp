#include "PluginEditor.h"

namespace
{
    const juce::Colour kBg    (0xff171a2e);
    const juce::Colour kPanel (0xff20243d);
    const juce::Colour kInk   (0xffeceaf7);
    const juce::Colour kMute  (0xff9a9dbd);
    const juce::Colour kLine  (0xff363b5e);
    const juce::Colour kAcc   (0xffff5c93);

    juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }
}

void CrumbleEditor::initKnob (Knob& k, const juce::String& name, const juce::String& paramId, const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 22);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setWantsKeyboardFocus (false);
    k.slider.setMouseClickGrabsKeyboardFocus (false);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (font (14.0f, true));
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, kMute);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramId, k.slider);
    // Set after the attachment, which would otherwise override the number of decimals
    k.slider.setNumDecimalPlacesToDisplay (0);
}

CrumbleEditor::CrumbleEditor (CrumbleProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    laf.setColour (juce::ResizableWindow::backgroundColourId, kBg);
    laf.setColour (juce::Slider::rotarySliderFillColourId, kAcc);
    laf.setColour (juce::Slider::rotarySliderOutlineColourId, kLine);
    laf.setColour (juce::Slider::thumbColourId, kInk);
    laf.setColour (juce::Slider::textBoxTextColourId, kInk);
    laf.setColour (juce::Slider::textBoxBackgroundColourId, kBg);
    laf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    laf.setColour (juce::Slider::textBoxHighlightColourId, kAcc.withAlpha (0.4f));
    laf.setColour (juce::TextEditor::backgroundColourId, kPanel);
    laf.setColour (juce::TextEditor::textColourId, kInk);
    laf.setColour (juce::TextEditor::focusedOutlineColourId, kAcc);
    laf.setColour (juce::CaretComponent::caretColourId, kAcc);
    laf.setColour (juce::ComboBox::backgroundColourId, kPanel);
    laf.setColour (juce::ComboBox::textColourId, kInk);
    laf.setColour (juce::ComboBox::outlineColourId, kLine);
    laf.setColour (juce::ComboBox::arrowColourId, kAcc);
    laf.setColour (juce::PopupMenu::backgroundColourId, kPanel);
    laf.setColour (juce::PopupMenu::textColourId, kInk);
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, kAcc);
    laf.setColour (juce::PopupMenu::highlightedTextColourId, kBg);
    setLookAndFeel (&laf);

    modeLabel.setText ("Mode", juce::dontSendNotification);
    modeLabel.setFont (font (14.0f, true));
    modeLabel.setColour (juce::Label::textColourId, kMute);
    addAndMakeVisible (modeLabel);

    modeBox.addItemList (proc.apvts.getParameter ("mode")->getAllValueStrings(), 1);
    modeBox.setWantsKeyboardFocus (false);
    modeBox.setMouseClickGrabsKeyboardFocus (false);
    addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<ComboBoxAttachment> (proc.apvts, "mode", modeBox);

    initKnob (freq,   "Frequency", "freq",   " Hz");
    initKnob (width,  "Width",     "width",  " %");
    initKnob (amount, "Amount",    "amount", " %");
    initKnob (mix,    "Mix",       "mix",    " %");

    setSize (480, 300);
}

CrumbleEditor::~CrumbleEditor()
{
    setLookAndFeel (nullptr);
}

void CrumbleEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (kInk);
    g.setFont (font (24.0f, true));
    g.drawText ("Crumble", 16, 10, 200, 32, juce::Justification::centredLeft);

    g.setColour (kMute);
    g.setFont (font (12.0f));
    g.drawText ("Width only affects the Noise modes", 16, getHeight() - 26, getWidth() - 32, 20,
                juce::Justification::centredLeft);

    g.setColour (kLine);
    g.drawLine (16.0f, 92.0f, (float) getWidth() - 16.0f, 92.0f);
}

void CrumbleEditor::resized()
{
    modeLabel.setBounds (16, 50, 50, 30);
    modeBox.setBounds (68, 50, 170, 30);

    const int colW = (getWidth() - 32) / 4;
    int x = 16;
    for (auto* k : { &freq, &width, &amount, &mix })
    {
        k->label.setBounds (x, 102, colW, 20);
        k->slider.setBounds (x, 124, colW, 140);
        x += colW;
    }
}
