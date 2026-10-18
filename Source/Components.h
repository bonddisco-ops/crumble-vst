#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include "Help.h"
#include <functional>

// ---------------------------------------------------------------------------
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
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

private:
    const Theme* current = nullptr;
};

juce::Font uiFont (float height, bool bold = false, float kerning = 0.0f);
const Theme& themeOf (const juce::Component&);

// The metal look shared by every control: glass jewel lamps, raised keys, recesses milled into the panel
namespace Metal
{
    void jewel (juce::Graphics&, juce::Point<float> centre, float radius, bool on, juce::Colour colour);
    void key (juce::Graphics&, juce::Rectangle<float>, bool on, const Theme&, float corner = 6.0f, bool hover = false);
    void milled (juce::Graphics&, juce::Rectangle<float>, float corner, const Theme&);
    void screw (juce::Graphics&, juce::Point<float> centre, float angle);
}

// ---------------------------------------------------------------------------
class ClickLabel : public juce::Label
{
public:
    std::function<void()> onClick;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
};

// A knob with its name above it; 'compact' knobs are used in the bottom row
struct Knob
{
    juce::Slider slider;
    ClickLabel label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    bool compact = false;

    void init (juce::Component& parent, juce::AudioProcessorValueTreeState&, const juce::String& name,
               const juce::String& paramId, bool isCompact, bool reverseArc = false);
    void setBounds (int x, int y, int w);
    void setVisible (bool);
    void setDimmed (bool);
    void setTooltip (const juce::String& t) { slider.setTooltip (t); label.setTooltip (t); }
    juce::Rectangle<int> getArea() const { return label.getBounds().getUnion (slider.getBounds()); }
};

// ---------------------------------------------------------------------------
// Two or more buttons bound to a choice parameter (Wet / All, FX / All, Above / Below...)
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::RangedAudioParameter&, const juce::StringArray& items, bool compact = false);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int getIdealWidth() const;

    // Locked: shows 'forcedIndex' greyed out and ignores clicks (the parameter keeps its value)
    void setLocked (bool shouldLock, int forcedIndex);

private:
    float fontSize() const { return compact ? 12.0f : 13.0f; }
    juce::StringArray items;
    int selected = 0;
    bool compact = false, locked = false;
    int forced = 1;
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
    enum class Icon { prev, next, save, gear };
    explicit IconButton (Icon i) : juce::Button ("Icon"), icon (i) { setWantsKeyboardFocus (false); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon icon;
};

// ---------------------------------------------------------------------------
// A panel with a title, an optional power switch, and (if movable) a drag handle on the title row
class Block : public juce::Component, public juce::SettableTooltipClient
{
public:
    Block (const juce::String& title, juce::AudioProcessorValueTreeState&, const char* powerParam, bool movable);
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isOn() const;
    bool isMovable() const { return movable; }
    juce::Button* getPowerButton() { return power.get(); }

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void (Block&)> onDragStart, onDragEnd;
    std::function<void (Block&, int)> onDragMove;   // distance moved, in the parent's coordinates

private:
    bool inHandle (const juce::MouseEvent&) const;
    juce::String title;
    bool movable = false, dragging = false;
    int dragStartParentX = 0;
    std::unique_ptr<PowerButton> power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
};

// ---------------------------------------------------------------------------
// [ ● PRE ]  pill: the light switches the filter, the name selects it for editing
class FilterPill : public juce::Component, public juce::SettableTooltipClient
{
public:
    FilterPill (juce::AudioProcessorValueTreeState&, bool post);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isOn() const { return on; }

    bool selected = false;
    std::function<void()> onSelect;      // the name was clicked
    std::function<void()> onTurnedOn;    // the light was switched on by the user

private:
    bool post = false, on = false;
    juce::ParameterAttachment attachment;
};

class ViewSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int view = 0;                         // 0 WAVE, 1 EQ
    std::function<void (int)> onChange;
};

// ---------------------------------------------------------------------------
// The CRT screen: WAVE (live waveform, Follow lines, clip lines) and EQ (spectrum and filter curves)
class ScopeView : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::Timer
{
public:
    enum class View { wave, eq };

    explicit ScopeView (CrumbleProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void setView (View);
    View getView() const { return view; }
    void selectFilter (bool post);
    void touchCeiling (bool dragging);
    // Compressor pictures (1 Glue on the wave, 2 OTT bands) are shown only while its knobs are set:
    // 5 s after the last touch, or as soon as any other control is grabbed, the screen goes back
    void touchComp (int kind, bool held);
    void dismissComp();
    bool isCompShown() const { return compKind != 0; }
    void setHelp (bool enabled, Help::Lang);
    void setFxAvailable (bool available);
    void setBarsIndex (int i);
    int getBarsIndex() const { return barsIndex; }
    std::function<void (int)> onBarsChanged;
    void advanceFrames (int n) { for (int i = 0; i < n; ++i) timerCallback(); }   // for tests

    // Areas for the tour, in this component's coordinates
    juce::Rectangle<int> followArea() const;
    juce::Rectangle<int> pillsArea() const;
    juce::Rectangle<int> viewSwitchArea() const { return viewSwitch.getBounds(); }

    static constexpr float barValues[6] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };

private:
    struct Particle { float x, y, vx, vy, life; };
    enum class Grab { none, thresh, glue, lc, pk, hc };

    void timerCallback() override;
    juce::Rectangle<float> screen() const;
    juce::Rectangle<float> plot() const;
    juce::Point<float> bulge (float x, float y) const;
    void paintWave (juce::Graphics&, const Theme&);
    void paintEq (juce::Graphics&, const Theme&);
    void paintOtt (juce::Graphics&, const Theme&);
    void paintGluePanel (juce::Graphics&, const Theme&);
    bool glueOn() const;
    bool ottOn() const;
    bool glueVisible() const { return compShownKind == 1 && compFade > 0.001f && glueOn(); }
    bool ottVisible() const { return compShownKind == 2 && compFade > 0.001f && ottOn(); }
    juce::Path bulgedRect (juce::Rectangle<float>) const;
    void updateSpectrum();
    void updateTooltip();

    // Wave helpers
    float amp() const { return plot().getHeight() * 0.46f * ampZoom; }
    float thresholdOffset() const;
    float glueOffset() const;
    Grab waveLineAt (float y) const;

    // EQ helpers
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    juce::RangedAudioParameter* fp (const char* name) const;
    float fv (const char* name) const;
    juce::Point<float> pointPos (Grab) const;
    Grab hitPoint (juce::Point<float>) const;
    void setParam (juce::RangedAudioParameter*, float value);

    CrumbleProcessor& proc;
    juce::ToggleButton followButton { "Follow" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
    std::unique_ptr<Segmented> followMode, preTarget, postTarget;
    FilterPill prePill, postPill;
    ViewSwitch viewSwitch;

    View view = View::wave, viewBeforeCeiling = View::wave;
    bool postSelected = false, tempWave = false, ceilingDragging = false, helpOn = true, fxAvailable = true;
    double ceilingTouchMs = -1.0e9;
    Help::Lang lang = Help::Lang::en;

    float ampZoom = 1.0f;
    int barsIndex = 2;
    int lastCount = 0;
    std::vector<Particle> particles;
    juce::Random rng;

    Grab grab = Grab::none, hover = Grab::none;
    juce::RangedAudioParameter* thresholdParam = nullptr;
    juce::RangedAudioParameter* glueThreshParam = nullptr;

    // Compressor display (smoothed for the animation)
    int compKind = 0, compShownKind = 0;     // wanted now / being drawn (stays while fading out)
    bool compHeld = false, compSwitchedView = false;
    double compTouchMs = 0.0;
    float compFade = 0.0f;
    View viewBeforeComp = View::wave;
    float glueGrUi = 0.0f, glueInUi = -100.0f;
    float ottGainUi[3] {}, ottLevelUi[3] { -100.0f, -100.0f, -100.0f };

    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> fftData, window, specDb;
    bool specPost = false;
};

// ---------------------------------------------------------------------------
class MeterColumn : public juce::Component, public juce::SettableTooltipClient
{
public:
    MeterColumn (const juce::String& title, juce::AudioProcessorValueTreeState&, const char* gainParam, const juce::String& knobName);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void push (float left, float right);
    Knob knob;

private:
    juce::String title;
    float level[2] {}, peakHold = 0.0f;
};

// ---------------------------------------------------------------------------
class PresetBar : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PresetBar (CrumbleProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    IconButton prev { IconButton::Icon::prev }, next { IconButton::Icon::next }, save { IconButton::Icon::save };

private:
    void timerCallback() override { repaint(); }
    void showMenu();
    void askForName (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> onOk);
    juce::Rectangle<int> nameBox() const;
    CrumbleProcessor& proc;
};

// ---------------------------------------------------------------------------
class AboutOverlay : public juce::Component
{
public:
    AboutOverlay() { setInterceptsMouseClicks (true, false); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
};

// ---------------------------------------------------------------------------
// Step-by-step tour: dims everything except the part being explained
class TourOverlay : public juce::Component
{
public:
    TourOverlay();
    void start (Help::Lang);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}

    std::function<juce::Rectangle<float> (const juce::String&)> resolveTarget;
    std::function<void (const juce::String&)> onStep;   // called with the target name of each step
    std::function<void()> onFinish;
    int getStep() const { return step; }
    void showStep (int);

private:
    juce::Rectangle<float> calloutArea() const;
    juce::TextButton back, next, skip;
    int step = 0;
    Help::Lang lang = Help::Lang::en;
};

// First-run question: show the tour?
class WelcomePrompt : public juce::Component
{
public:
    WelcomePrompt();
    void paint (juce::Graphics&) override;
    void resized() override;
    void setLang (Help::Lang);

    std::function<void (int)> onChoice;            // 0 yes, 1 not now, 2 never
    std::function<void (Help::Lang)> onLanguage;

private:
    juce::Rectangle<float> card() const;
    juce::TextButton yes, notNow, never, en { "English" }, ru;
    Help::Lang lang = Help::Lang::en;
};
