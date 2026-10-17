#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <vector>
#include "Params.h"
#include "PresetManager.h"

// Crumble 3 by Primal Rage Lab
//
//  IN -> [Crumble <-> Bitcrush] -> Color(wet) -> Filter(wet) -> Tremolo(wet) -> x Follow -> Mix
//     -> Color(all) -> Filter(all) -> Tremolo(all) -> Output -> Clip -> OUT
//
// "Wet" stages work on the effect only (the difference between the processed and the
// clean signal), so the original stays intact even at Mix 100 %.
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
        static constexpr int size = 4096;        // buckets of 0.5 ms -> about 2 s of history
        float out[2][size] {};
        float in[size] {};
        std::atomic<int> writeCount { 0 };
    };
    Scope scope;
    std::atomic<float> inPeak[2] {}, outPeak[2] {};
    std::atomic<float> followGainNow { 1.0f };
    std::atomic<float> tremoloNow { 1.0f };

    static constexpr double maxDepthSeconds = 0.002;   // crumble depth at Amount 100 %

private:
    struct Biquad
    {
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1 = z2 = 0; }
    };

    struct DelayLine
    {
        std::vector<float> buf;
        int mask = 0, write = 0;

        void prepare (int minSize)
        {
            const int n = juce::nextPowerOfTwo (minSize);
            buf.assign ((size_t) n, 0.0f);
            mask = n - 1;
            write = 0;
        }
        void push (float x)
        {
            buf[(size_t) write] = x;
            write = (write + 1) & mask;
        }
        float read (float d) const   // d = delay in samples, 0 = newest
        {
            const float pos = (float) (write - 1) - d;
            const int i0 = (int) std::floor (pos);
            const float frac = pos - (float) i0;
            const float a = buf[(size_t) (i0 & mask)];
            const float b = buf[(size_t) ((i0 + 1) & mask)];
            return a + (b - a) * frac;
        }
    };

    struct Butter4   // 4th-order Butterworth from two state-variable stages
    {
        juce::dsp::StateVariableTPTFilter<float> s1, s2;
        void prepare (const juce::dsp::ProcessSpec& spec, juce::dsp::StateVariableTPTFilterType type)
        {
            for (auto* s : { &s1, &s2 })
            {
                s->prepare (spec);
                s->setType (type);
                s->reset();
            }
            s1.setResonance (0.5412f);
            s2.setResonance (1.3066f);
        }
        void setCutoff (float hz)
        {
            if (std::abs (hz - cutoff) > 0.01f)
            {
                s1.setCutoffFrequency (hz);
                s2.setCutoffFrequency (hz);
                cutoff = hz;
            }
        }
        float process (int ch, float x) { return s2.processSample (ch, s1.processSample (ch, x)); }
        float cutoff = -1.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::atomic<float>* raw (const char* id) { return apvts.getRawParameterValue (id); }

    void updateNoiseFilters (float freq, float texture);
    float filteredNoise (int source);
    float crumbleSample (int ch, float x, float depth, float mod, float feedback);
    float bitcrushSample (int ch, float x, bool newHold, float levels, float quantize, float dither,
                          float adcQ, float dacQ);
    float colorSample (int ch, float x, int type, float drive, float tone, float squash);
    float tremoloShape (int shape, double phase) const;
    float clipSample (float x, bool hard, float ceiling) const;

    // Parameters
    std::atomic<float> *pInput, *pOutput, *pMix, *pAutoGain, *pOrder,
                       *pCrOn, *pMode, *pFreq, *pRate, *pTexture, *pAmount, *pStereo, *pFeedback,
                       *pBcOn, *pBcFreq, *pBits, *pQuantize, *pDither, *pAdcQ, *pDacQ,
                       *pFltOn, *pFltTarget, *pLowCut, *pHighCut,
                       *pColOn, *pColTarget, *pDriveType, *pDrive, *pTone, *pSquash,
                       *pTrOn, *pTrTarget, *pTrShape, *pTrDepth, *pTrRate, *pTrPan,
                       *pFolOn, *pFolThresh, *pFolMode,
                       *pClipOn, *pClipMode, *pCeiling;

    double sr = 44100.0;

    // Crumble
    DelayLine crumbleDelay[2], alignDry[2];
    Biquad noiseFilt[3];          // 0 shared, 1 left only, 2 right only
    float noiseGain = 1.0f;
    juce::Random rng;
    double lfoPhase = 0.0;
    float fbState[2] {}, fbHp[2] {};

    // Bitcrush
    Butter4 adc, dac;
    double holdPhase = 0.0;
    float hold[2] {};

    // Color
    juce::dsp::LinkwitzRileyFilter<float> sq1, sq2, sqAp;
    float sqEnv[2][3] {}, sqGainDb[2][3] {};
    float dcX[2] {}, dcY[2] {}, tapeLp[2] {}, toneLp[2] {};
    float agIn[2] {}, agOut[2] {};
    float agGain = 1.0f;
    float sqAtt = 0.0f, sqRel = 0.0f, rmsCoef = 0.0f, toneCoef = 0.0f, tapeCoef = 0.0f;   // per-sample filter coefficients

    // Filters
    Butter4 hp, lp;

    // Tremolo
    double trPhase = 0.0;
    float trGain[2] { 1.0f, 1.0f };

    // Follow
    float folEnv = 0.0f, folGain = 1.0f;

    // Smoothed values
    juce::SmoothedValue<float> inGainS, outGainS, mixS, depthS, stereoS, feedbackS, driveS, toneS, squashS,
                               trDepthS, crFade, bcFade, fltFade, colFade, trFade, folFade, clipFade;

    // Swapping Crumble and Bitcrush: the effect dips for a few ms, the order flips at the bottom
    bool appliedOrder = false;
    float swapGain = 1.0f;

    // Scope decimation
    int bucketLen = 24, bucketPos = 0;
    float bucketOut[2] {}, bucketIn = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrumbleProcessor)
};
