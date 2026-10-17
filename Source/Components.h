#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include <functional>

// ---------------------------------------------------------------------------
// Look and feel driven by the current theme
class CrumbleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CrumbleLookAndFeel();
    void setTheme (const Theme&);
    const Theme& theme() const { return *current; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

private:
    const Theme* current = nullptr;
};

juce::Font uiFont (float height, bool bold = false, float kerning = 0.0f);

// ---------------------------------------------------------------------------
// A knob with its name above it
struct Knob
{
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    void init (juce::Component& parent, juce::AudioProcessorValueTreeState&, const juce::String& name,
               const juce::String& paramId, bool reverseArc = false);
    void setBounds (int x, int y, int w);
    void setDimmed (bool dimmed);
};

// ---------------------------------------------------------------------------
// Two or more buttons bound to a choice parameter (Wet / All, Above / Below, Soft / Hard)
class Segmented : public juce::Component
{
public:
    Segmented (juce::RangedAudioParameter&, const juce::StringArray& items);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int getIdealWidth() const;

private:
    juce::StringArray items;
    int selected = 0;
    juce::ParameterAttachment attachment;
};

// ---------------------------------------------------------------------------
class PowerButton : public juce::Button
{
public:
    PowerButton() : juce::Button ("Power") { setClickingTogglesState (true); setWantsKeyboardFocus (false); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class IconButton : public juce::Button
{
public:
    enum class Icon { prev, next, save, gear, swap };
    explicit IconButton (Icon i) : juce::Button ("Icon"), icon (i) { setWantsKeyboardFocus (false); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon icon;
};

// ---------------------------------------------------------------------------
// A panel with a title and an optional power switch
class Block : public juce::Component
{
public:
    Block (const juce::String& title, juce::AudioProcessorValueTreeState&, const char* powerParam);
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isOn() const;

    std::function<void (bool)> onPowerChanged;

private:
    juce::String title;
    juce::AudioProcessorValueTreeState& apvts;
    std::unique_ptr<PowerButton> power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
};

// ---------------------------------------------------------------------------
// The CRT screen: live waveform, input ghost, Follow threshold lines, zoom
class ScopeView : public juce::Component,
                  private juce::Timer
{
public:
    explicit ScopeView (CrumbleProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    float ampZoom = 1.0f, timeWindow = 0.6f;

private:
    struct Particle { float x, y, vx, vy, life; };
    void timerCallback() override;
    juce::Rectangle<float> screen() const;
    juce::Point<float> bulge (float x, float y) const;
    float thresholdY (bool upper) const;
    bool nearThreshold (float y) const;

    CrumbleProcessor& proc;
    juce::ToggleButton followButton { "Follow" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
    std::unique_ptr<Segmented> followMode;
    std::vector<Particle> particles;
    juce::Random rng;
    float time = 0.0f;
    bool draggingThreshold = false;
    juce::RangedAudioParameter* thresholdParam = nullptr;
};

// ---------------------------------------------------------------------------
// IN / OUT column: peak readout, stereo meter, gain knob
class MeterColumn : public juce::Component
{
public:
    MeterColumn (const juce::String& title, juce::AudioProcessorValueTreeState&, const char* gainParam, const juce::String& knobName);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void push (float left, float right);   // called by the editor ~30 times a second

private:
    juce::String title;
    Knob knob;
    float level[2] {}, peakHold = 0.0f;
};

// ---------------------------------------------------------------------------
class PresetBar : public juce::Component,
                  private juce::Timer
{
public:
    explicit PresetBar (CrumbleProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void showMenu();
    void askForName (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> onOk);
    juce::Rectangle<int> nameBox() const;

    CrumbleProcessor& proc;
    IconButton prev { IconButton::Icon::prev }, next { IconButton::Icon::next }, save { IconButton::Icon::save };
};

// ---------------------------------------------------------------------------
class AboutOverlay : public juce::Component
{
public:
    AboutOverlay() { setInterceptsMouseClicks (true, false); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
    juce::Image logo;
};
