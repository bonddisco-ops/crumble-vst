#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// Crumble by Primal Rage Lab
//
// Signal flow:
//   wet = Crumble (modulated micro delay, optional tempo tremolo)
//         -> [Color: Drive -> Squash -> auto gain, when set to "Wet signal"]
//         -> Low Cut / High Cut filters
//   out = Mix (dry + wet) -> [Color, when set to "Whole signal"] -> Output
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

    // Waveform for the display: one value per bucket (the sample with the largest magnitude).
    struct Scope
    {
        static constexpr int size = 512;
        float data[2][size] {};
        std::atomic<int> writeCount { 0 };
    };
    Scope scope;

    static const juce::StringArray rateNames;

    // Current tremolo level of the effect (0..1), for the display
    std::atomic<float> effectLevel { 1.0f };
    static constexpr double maxDepthSeconds = 0.002;   // 2 ms at Amount 100 %

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
        // d = delay in samples (0 = the newest sample)
        float read (float d) const
        {
            const float pos = (float) (write - 1) - d;
            const int i0 = (int) std::floor (pos);
            const float frac = pos - (float) i0;
            const float a = buf[(size_t) (i0 & mask)];
            const float b = buf[(size_t) ((i0 + 1) & mask)];
            return a + (b - a) * frac;
        }
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateNoiseFilters (float freq, float texture);
    float filteredNoise (int source);
    void updateFilters (float lowCut, float highCut);
    float colorStage (int ch, float x, float driveGain, float driveBlend, float squash, float makeup,
                      float attCoef, float relCoef, float rmsCoef);
    void pushToScope (float left, float right);

    // Parameters
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pFreq = nullptr;
    std::atomic<float>* pSync = nullptr;
    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pTexture = nullptr;
    std::atomic<float>* pAmount = nullptr;
    std::atomic<float>* pStereo = nullptr;
    std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pHighCut = nullptr;
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pSquash = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pAutoGain = nullptr;
    std::atomic<float>* pColorTarget = nullptr;

    double sr = 44100.0;

    // Wet filters: two cascaded state-variable filters each = 24 dB/oct Butterworth
    juce::dsp::StateVariableTPTFilter<float> hp1, hp2, lp1, lp2;
    float currentLowCut = -1.0f, currentHighCut = -1.0f;
    double tremPhase = 0.0;

    // Squash: three sub-bands inside the mid band
    juce::dsp::LinkwitzRileyFilter<float> sq1, sq2, sqAp;
    float sqEnv[2][3] {};
    float sqGainDb[2][3] {};

    // Crumble
    DelayLine crumbleDelay[2];
    DelayLine alignDry[2];
    Biquad noiseFilt[3];               // 0 = shared, 1 = left only, 2 = right only
    float noiseGain = 1.0f;
    juce::Random rng;
    double phase = 0.0;

    // Auto gain for Drive + Squash
    float agIn[2] {}, agOut[2] {};
    float agGain = 1.0f;

    juce::SmoothedValue<float> depthSmooth, mixSmooth, driveSmooth, squashSmooth, outSmooth, stereoSmooth;

    // Scope decimation
    int bucketLen = 32, bucketPos = 0;
    float bucketPeak[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrumbleProcessor)
};
