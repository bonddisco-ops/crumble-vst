#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout CrumbleProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                StringArray { "Sine", "Noise", "Wide Noise" }, 1));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "freq", 1 }, "Frequency",
                NormalisableRange<float> (20.0f, 18000.0f, 0.0f, 0.3f), 3000.0f,
                AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width",
                NormalisableRange<float> (0.0f, 100.0f), 40.0f,
                AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
                NormalisableRange<float> (0.0f, 100.0f), 30.0f,
                AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                NormalisableRange<float> (0.0f, 100.0f), 100.0f,
                AudioParameterFloatAttributes().withLabel ("%")));

    return layout;
}

CrumbleProcessor::CrumbleProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pMode   = apvts.getRawParameterValue ("mode");
    pFreq   = apvts.getRawParameterValue ("freq");
    pWidth  = apvts.getRawParameterValue ("width");
    pAmount = apvts.getRawParameterValue ("amount");
    pMix    = apvts.getRawParameterValue ("mix");
}

bool CrumbleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CrumbleProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& d : delays)
        d.prepare ((int) (maxDepthSeconds * sr) + 16);
    for (auto& f : noiseFilt)
        f.reset();
    phase = 0.0;

    depthSmooth.reset (sr, 0.05);
    mixSmooth.reset (sr, 0.05);
    const float amount = pAmount->load() / 100.0f;
    depthSmooth.setCurrentAndTargetValue (amount * amount * (float) (maxDepthSeconds * sr));
    mixSmooth.setCurrentAndTargetValue (pMix->load() / 100.0f);
}

void CrumbleProcessor::updateNoiseFilters (float freq, float width)
{
    const float f0 = juce::jlimit (20.0f, (float) (0.45 * sr), freq);
    const float q = 20.0f * std::pow (0.015f, width);   // width 0 -> Q 20 (narrow), width 1 -> Q 0.3 (wide)
    const float w0 = juce::MathConstants<float>::twoPi * f0 / (float) sr;
    const float alpha = std::sin (w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;

    for (auto& f : noiseFilt)
    {
        f.b0 = alpha / a0;
        f.b1 = 0.0f;
        f.b2 = -alpha / a0;
        f.a1 = -2.0f * std::cos (w0) / a0;
        f.a2 = (1.0f - alpha) / a0;
    }

    // Keep the modulation level roughly constant whatever the bandwidth is
    noiseGain = juce::jlimit (1.0f, 40.0f, std::sqrt (0.318f * (float) sr * q / f0));
}

float CrumbleProcessor::nextNoise (int ch)
{
    const float x = rng.nextFloat() * 2.0f - 1.0f;
    return std::tanh (noiseFilt[ch].process (x) * noiseGain);
}

void CrumbleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    if (numCh == 0 || numSamples == 0)
        return;

    const int mode = juce::roundToInt (pMode->load());
    const float freq = pFreq->load();
    const float width = pWidth->load() / 100.0f;
    const float amount = pAmount->load() / 100.0f;

    updateNoiseFilters (freq, width);
    depthSmooth.setTargetValue (amount * amount * (float) (maxDepthSeconds * sr));
    mixSmooth.setTargetValue (pMix->load() / 100.0f);

    const double twoPi = juce::MathConstants<double>::twoPi;
    const double phaseInc = twoPi * (double) freq / sr;

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < numSamples; ++i)
    {
        const float depth = depthSmooth.getNextValue();
        const float mix = mixSmooth.getNextValue();

        float mod[2] = { 0.0f, 0.0f };
        if (mode == 0)
        {
            mod[0] = mod[1] = (float) std::sin (phase);
            phase += phaseInc;
            if (phase >= twoPi)
                phase -= twoPi;
        }
        else
        {
            mod[0] = nextNoise (0);
            mod[1] = (mode == 2 && numCh > 1) ? nextNoise (1) : mod[0];
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = data[ch][i];
            delays[ch].push (x);
            const float wet = delays[ch].read (depth * (0.5f + 0.5f * mod[ch]));
            data[ch][i] = x + (wet - x) * mix;
        }
    }
}

void CrumbleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CrumbleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CrumbleProcessor::createEditor()
{
    return new CrumbleEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrumbleProcessor();
}
