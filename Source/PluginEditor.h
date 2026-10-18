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
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b0b0d)); }

    static constexpr int baseW = 1160, baseH = 850;

    static juce::String loadSetting (const juce::String& key, const juce::String& fallback);
    static void saveSetting (const juce::String& key, const juce::String& value);

    // For tests and screenshots
    void setThemeByName (const juce::String& name) { applyTheme (Theme::byName (name), false); }
    ScopeView& getScope() { return scope; }
    void startTour (int step) { tour.start (lang); tour.showStep (step); }
    void showWelcome() { welcome.setVisible (true); welcome.toFront (false); }
    void touchCeiling() { scope.touchCeiling (true); }

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
    void applyHelp();
    void setScale (float scale, bool remember);
    void computeLayout();
    void layoutInside();
    void showComp();
    void mouseDown (const juce::MouseEvent&) override;
    void showSettingsMenu();
    void buildBackground();
    void addCombo (juce::Component& parent, juce::ComboBox&, const char* paramId, const juce::StringArray& items,
                   std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&, bool compact);
    juce::Rectangle<float> tourTarget (const juce::String& name);

    // Block order and dragging
    int widthOf (const Block*) const;
    Block* movable (int index);   // 0 Color, 1 Comp, 2 Tremolo
    void layoutRow1 (bool bitFirst, bool animate, const Block* skip);
    void layoutRow2 (const int order[3], bool animate, const Block* skip);
    int row2SlotX (const int order[3], int index) const;
    void dragStart (Block&);
    void dragMove (Block&, int dx);
    void dragEnd (Block&);
    void setChoice (const char* id, int index);

    CrumbleProcessor& proc;
    CrumbleLookAndFeel laf;           // first, so it outlives every component
    const Theme* theme = nullptr;
    Content content { *this };
    juce::Image background;
    float scale = 1.0f;

    // Layout in content coordinates; the content is scaled to fit the window and stretches to its shape
    int lw = baseW, lh = baseH;
    int x0 = 96, x1 = 1064, r1y = 449, r2y = 661;
    int wCrumble = 428, wBit = 508, wColor = 196, wComp = 312, wTrem = 196, wMix = 102, wClip = 102, gap2 = 15;
    bool constructed = false, bgDirty = false;
    double bgDirtyMs = 0.0, openedMs = 0.0;
    int bgW = 0, bgH = 0;
    int lastCompOn = -1, lastCompMode = -1;
    bool helpOn = true;
    Help::Lang lang = Help::Lang::en;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    PresetBar presetBar { proc };
    IconButton gear { IconButton::Icon::gear };
    ScopeView scope { proc };
    MeterColumn inCol { "IN", proc.apvts, ParamIDs::input, "Input" };
    MeterColumn outCol { "OUT", proc.apvts, ParamIDs::output, "Output" };

    Block crumbleBlock { "CRUMBLE", proc.apvts, ParamIDs::crOn, true };
    Block bitBlock     { "BITCRUSH", proc.apvts, ParamIDs::bcOn, true };
    Block colorBlock   { "COLOR", proc.apvts, ParamIDs::colOn, true };
    Block compBlock    { "COMP", proc.apvts, ParamIDs::compOn, true };
    Block tremBlock    { "TREMOLO", proc.apvts, ParamIDs::trOn, true };
    Block mixBlock     { "MIX", proc.apvts, nullptr, false };
    Block clipBlock    { "CLIP", proc.apvts, ParamIDs::clipOn, false };

    juce::ComboBox modeBox, driveTypeBox, compModeBox, trShapeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt, driveTypeAtt, compModeAtt, trShapeAtt;
    std::unique_ptr<Segmented> colTarget, compTarget, trTarget, clipMode;
    juce::ToggleButton autoGainButton { "Auto gain" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAtt;

    Knob freq, rate, texture, amount, stereo, feedback;
    Knob bcFreq, bits, quantize, dither, adcQ, dacQ;
    Knob drive, tone, colMix;
    Knob ottDepth, ottTime, ottUp, ottDown, glueThresh, glueRatio, glueAttack, glueRelease, compMix;
    Knob trDepth, trRate, trMs, trStereo;
    Knob mix, ceiling;

    AboutOverlay about;
    TourOverlay tour;
    WelcomePrompt welcome;

    bool shownBitFirst = false;
    int shownChain[3] { 0, 1, 2 };
    Block* dragging = nullptr;
    int dragOriginX = 0;
    bool tentativeBitFirst = false;
    int tentativeChain[3] { 0, 1, 2 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrumbleEditor)
};
