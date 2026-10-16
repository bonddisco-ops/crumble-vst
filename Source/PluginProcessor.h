#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <cmath>

// Crumble: a very short delay line whose delay time is shaken by noise or a sine.
// The fast modulation smears and "crumbles" the high end of the sound.
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
    void updateNoiseFilters (float freq, float width);
    float nextNoise (int ch);

    std::atomic<float>* pMode   = nullptr;
    std::atomic<float>* pFreq   = nullptr;
    std::atomic<float>* pWidth  = nullptr;
    std::atomic<float>* pAmount = nullptr;
    std::atomic<float>* pMix    = nullptr;

    double sr = 44100.0;
    DelayLine delays[2];
    Biquad noiseFilt[2];
    float noiseGain = 1.0f;
    juce::Random rng;
    double phase = 0.0;
    juce::SmoothedValue<float> depthSmooth, mixSmooth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrumbleProcessor)
};
