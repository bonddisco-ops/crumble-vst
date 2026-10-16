#pragma once
#include "PluginProcessor.h"

class CrumbleEditor : public juce::AudioProcessorEditor
{
public:
    explicit CrumbleEditor (CrumbleProcessor&);
    ~CrumbleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void initKnob (Knob& k, const juce::String& name, const juce::String& paramId, const juce::String& suffix);

    CrumbleProcessor& proc;
    juce::LookAndFeel_V4 laf;   // declared first so it outlives every child component

    juce::Label modeLabel;
    juce::ComboBox modeBox;
    std::unique_ptr<ComboBoxAttachment> modeAttachment;

    Knob freq, width, amount, mix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrumbleEditor)
};
