#pragma once
#include "PluginProcessor.h"
#include <vector>

// Knobs, toggles and lists in the Primal Rage Lab style
class CrumbleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CrumbleLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// Live waveform that gets brighter and more broken with Amount and Drive
class WaveDisplay : public juce::Component,
                    private juce::Timer
{
public:
    explicit WaveDisplay (CrumbleProcessor&);
    void paint (juce::Graphics&) override;

private:
    struct Particle { float x, y, vx, vy, life; };

    void timerCallback() override;

    CrumbleProcessor& proc;
    std::atomic<float>* pAmount;
    std::atomic<float>* pDrive;
    std::atomic<float>* pSquash;
    std::atomic<float>* pStereo;
    std::atomic<float>* pMode;
    std::vector<Particle> particles;
    juce::Random rng;
    float time = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveDisplay)
};

class CrumbleEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit CrumbleEditor (CrumbleProcessor&);
    ~CrumbleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Group
    {
        juce::String title;
        juce::Rectangle<int> area;
    };

    void timerCallback() override;
    void initKnob (Knob&, const juce::String& name, const juce::String& paramId);
    void prepareButton (juce::Button&);
    void prepareCombo (juce::ComboBox&);
    void buildBackground();

    CrumbleProcessor& proc;
    CrumbleLookAndFeel laf;   // declared first so it outlives every child component

    juce::Image background, logo;
    WaveDisplay wave;

    juce::ComboBox modeBox, rateBox;
    juce::ToggleButton syncButton { "Sync" }, autoGainButton { "Auto gain" };
    std::unique_ptr<ComboBoxAttachment> modeAttachment, rateAttachment;
    std::unique_ptr<ButtonAttachment> syncAttachment, autoGainAttachment;

    Knob freq, texture, amount, stereo, lowCut, highCut, drive, squash, mix, output;
    std::vector<Group> groups;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrumbleEditor)
};
