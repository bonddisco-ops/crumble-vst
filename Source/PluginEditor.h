#pragma once
#include "PluginProcessor.h"
#include "Components.h"

class CrumbleEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit CrumbleEditor (CrumbleProcessor&);
    ~CrumbleEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override {}

    static constexpr int baseW = 1160, baseH = 744;

    // For tests: switch theme without touching the saved settings
    void setThemeByName (const juce::String& name) { applyTheme (Theme::byName (name), false); }

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (CrumbleEditor& e) : editor (e) {}
        void paint (juce::Graphics& g) override { editor.paintContent (g); }
        void mouseDown (const juce::MouseEvent& e) override;
    private:
        CrumbleEditor& editor;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void applyTheme (const Theme&, bool remember);
    void setScale (float scale, bool remember);
    void layoutRows (bool animate);
    void showSettingsMenu();
    void buildBackground();
    void tintLogo();
    void addCombo (juce::Component& parent, juce::ComboBox&, const char* paramId, const juce::StringArray& items,
                   std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&);

    static juce::String loadSetting (const juce::String& key, const juce::String& fallback);
    static void saveSetting (const juce::String& key, const juce::String& value);

    CrumbleProcessor& proc;
    CrumbleLookAndFeel laf;           // first, so it outlives every component
    const Theme* theme = nullptr;
    Content content { *this };
    juce::Image background, logoRaw, logo;
    float scale = 1.0f;

    PresetBar presetBar { proc };
    IconButton gear { IconButton::Icon::gear }, swapButton { IconButton::Icon::swap };
    ScopeView scope { proc };
    MeterColumn inCol { "IN", proc.apvts, ParamIDs::input, "Input" };
    MeterColumn outCol { "OUT", proc.apvts, ParamIDs::output, "Output" };

    Block crumbleBlock { "CRUMBLE", proc.apvts, ParamIDs::crOn };
    Block bitBlock     { "BITCRUSH", proc.apvts, ParamIDs::bcOn };
    Block filterBlock  { "WET FILTER", proc.apvts, ParamIDs::fltOn };
    Block colorBlock   { "COLOR", proc.apvts, ParamIDs::colOn };
    Block tremBlock    { "TREMOLO", proc.apvts, ParamIDs::trOn };
    Block mixBlock     { "MIX", proc.apvts, nullptr };
    Block clipBlock    { "CLIP", proc.apvts, ParamIDs::clipOn };

    juce::ComboBox modeBox, driveTypeBox, trShapeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt, driveTypeAtt, trShapeAtt;
    std::unique_ptr<Segmented> fltTarget, colTarget, trTarget, clipMode;
    juce::ToggleButton autoGainButton { "Auto gain" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAtt;

    Knob freq, rate, texture, amount, stereo, feedback;
    Knob bcFreq, bits, quantize, dither, adcQ, dacQ;
    Knob lowCut, highCut, drive, tone, squash, trDepth, trRate, trPan, mix, ceiling;
    std::vector<Knob*> allKnobs;

    AboutOverlay about;
    bool shownOrder = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrumbleEditor)
};
