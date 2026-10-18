#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "Params.h"
#include "Dsp.h"
#include "PresetManager.h"

// Crumble 3.3 by Primal Rage Lab
//
//  IN -> PRE filter (FX: only what goes into the effect / All: everything)
//     -> [Crumble <-> Bitcrush] -> effect = processed - clean
//     -> blocks set to Wet (Color, Comp, Tremolo in the chosen order) on the effect
//     -> POST filter if set to FX -> x Follow -> Mix with the clean signal
//     -> blocks set to All on everything -> POST filter if set to All -> Output -> Clip -> OUT
//
// With Crumble and Bitcrush both off there is no effect, so every block works on everything.
class CrumbleProcessor : public juce::AudioProcessor
{
public:
    CrumbleProcessor();
    ~CrumbleProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Crumble"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts };

    // ---- Data for the display (written on the audio thread, read by the editor) ----
    struct Scope
    {
        static constexpr int size = 65536;            // 1 ms buckets -> about 65 s of history
        float hi[size] {}, lo[size] {}, in[size] {}, preClip[size] {};
        float gr[size] {};                            // Glue gain reduction in dB (0 or below)
        std::atomic<int> writeCount { 0 };
        // Beat clock: the beat position at bucket number 'beatCount', and the tempo
        std::atomic<int> beatCount { 0 };
        std::atomic<double> beatAt { 0.0 };
        std::atomic<double> bpm { 120.0 };
    };
    Scope scope;

    struct Spectrum
    {
        static constexpr int size = 8192;
        float in[size] {}, out[size] {};
        std::atomic<int> writePos { 0 };
    };
    Spectrum spectrum;

    std::atomic<float> inPeak[2] {}, outPeak[2] {};

    // Compressor meters for the screen
    std::atomic<float> glueGrDb { 0.0f }, glueLevelDb { -100.0f };     // Glue: reduction now, detector level
    std::atomic<float> ottGainDb[3] {}, ottLevelDb[3] {};                  // OTT per band: applied gain (+ up / - down), level
    std::atomic<bool> compActive { false };

    // Editor window size (kept while the plugin lives and saved with the project)
    std::atomic<int> editorW { 0 }, editorH { 0 };
    std::atomic<double> currentSampleRate { 44100.0 };

    static constexpr double maxDepthSeconds = 0.002;   // crumble depth at Amount 100 %

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::atomic<float>* raw (const juce::String& id) { return apvts.getRawParameterValue (id); }

    void updateNoiseFilters (float freq, float texture);
    float filteredNoise (int source);
    float crumbleSample (int ch, float x, float depth, float mod, float feedback);
    float bitcrushSample (int ch, float x, bool newHold, float levels, float quantize, float dither, float adcQ, float dacQ);
    float tremoloShape (int shape, double phase) const;
    static float clipSample (float x, bool hard, float ceiling);
    void runStage (int stage, float* v, int numCh);

    struct FilterParams
    {
        std::atomic<float> *on, *target, *lc, *hc, *lcSlope, *hcSlope, *pkF, *pkG, *pkQ;
    };
    FilterParams preP {}, postP {};
    void bindFilter (FilterParams&, bool post);

    // Parameters
    std::atomic<float> *pInput, *pOutput, *pMix, *pAutoGain, *pOrder, *pChain,
                       *pCrOn, *pMode, *pFreq, *pRate, *pTexture, *pAmount, *pStereo, *pFeedback,
                       *pBcOn, *pBcFreq, *pBits, *pQuantize, *pDither, *pAdcQ, *pDacQ,
                       *pColOn, *pColTarget, *pDriveType, *pDrive, *pTone, *pColMix,
                       *pCompOn, *pCompTarget, *pCompMode, *pOttDepth, *pOttTime, *pOttUp, *pOttDown,
                       *pGlueThresh, *pGlueRatio, *pGlueAttack, *pGlueRelease, *pCompMix,
                       *pTrOn, *pTrTarget, *pTrShape, *pTrDepth, *pTrRateMode, *pTrRate, *pTrMs, *pTrPan,
                       *pFolOn, *pFolThresh, *pFolMode,
                       *pClipOn, *pClipMode, *pCeiling;

    double sr = 44100.0;

    // Crumble
    cdsp::DelayLine crumbleDelay[2], alignDry[2], alignSrc[2];
    cdsp::Biquad noiseFilt[3];          // 0 shared, 1 left only, 2 right only
    float noiseGain = 1.0f;
    juce::Random rng;
    double lfoPhase = 0.0;
    float fbState[2] {}, fbHp[2] {};

    // Bitcrush
    cdsp::CutFilter adc, dac;
    double holdPhase = 0.0;
    float hold[2] {};

    // Blocks
    cdsp::Color color;
    cdsp::Ott ott;
    cdsp::Glue glue;
    cdsp::AutoGain agColor, agComp;
    cdsp::FilterBank preFilter, postFilter;

    // Per-block values for the current sample (set in processBlock, used by runStage)
    struct StageState
    {
        int driveType = 0, compMode = 0;
        float drive = 0, tone = 0, colMix = 1, colF = 0;
        float ottDepth = 0, ottUp = 0, ottDown = 0, glueThresh = 0, glueRatio = 4, compMix = 1, compF = 0;
        float trGain[2] { 1.0f, 1.0f };
        bool autoGain = true;
    } st;

    // Tremolo
    double trPhase = 0.0;
    float trGain[2] { 1.0f, 1.0f };

    // Follow
    float folEnv = 0.0f, folGain = 1.0f;

    // Routing: changes go through a short dip so they never click
    struct Routing
    {
        bool bitFirst = false, colWet = true, compWet = true, trWet = true, preFx = true, postFx = true;
        int chain = 0;
        bool operator!= (const Routing& o) const
        {
            return bitFirst != o.bitFirst || colWet != o.colWet || compWet != o.compWet || trWet != o.trWet
                || preFx != o.preFx || postFx != o.postFx || chain != o.chain;
        }
    };
    Routing applied, pending;
    float routeGain = 1.0f;
    Routing currentRouting();

    // Beat clock for the display
    double beatClock = 0.0;

    // Smoothed values
    juce::SmoothedValue<float> inGainS, outGainS, mixS, depthS, stereoS, feedbackS, driveS, toneS, colMixS, compMixS,
                               ottDepthS, trDepthS, crFade, bcFade, colFade, compFade, trFade, folFade, clipFade,
                               preFade, postFade;

    // Scope decimation
    int bucketLen = 48, bucketPos = 0;
    float bucketHi = 0.0f, bucketLo = 0.0f, bucketIn = 0.0f, bucketPre = 0.0f, bucketGr = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrumbleProcessor)
};
