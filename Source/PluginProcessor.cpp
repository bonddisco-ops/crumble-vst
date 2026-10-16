#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Length of one Sine cycle in beats for each sync rate
    const double kRateBeats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625,
                                  2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.0 / 12.0 };

    float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    float gainToDb (float g)  { return 20.0f * std::log10 (g + 1.0e-9f); }

    // Squash (OTT-style) settings
    constexpr float kDownThreshold = -18.0f, kDownRatio = 4.0f;
    constexpr float kUpThreshold   = -30.0f, kUpRatio   = 3.0f;
    constexpr float kUpMaxDb = 24.0f, kFloorDb = -72.0f;
    constexpr float kSquashMakeupDb = 6.0f;
}

const juce::StringArray CrumbleProcessor::rateNames { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
                                                      "1/4T", "1/8T", "1/16T", "1/32T" };

juce::AudioProcessorValueTreeState::ParameterLayout CrumbleProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                   NormalisableRange<float> (0.0f, 100.0f), def,
                   AudioParameterFloatAttributes().withLabel ("%")
                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; }));
    };
    // offAt: value that shows "Off" (a cut filter fully open), or 0 for none
    auto hz = [] (const char* id, const char* name, float lo, float hi, float def, float offAt)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                   NormalisableRange<float> (lo, hi, 0.0f, 0.3f), def,
                   AudioParameterFloatAttributes().withLabel ("Hz")
                       .withStringFromValueFunction ([offAt] (float v, int)
                       {
                           if (offAt > 0.0f && std::abs (v - offAt) < offAt * 0.002f)
                               return String ("Off");
                           return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 1) + " kHz";
                       })
                       .withValueFromStringFunction ([offAt] (const String& text)
                       {
                           auto t = text.trim().toLowerCase();
                           if (t == "off")
                               return offAt;
                           const float v = t.getFloatValue();
                           return t.containsChar ('k') ? v * 1000.0f : v;
                       }));
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Sine", "Noise" }, 1));
    layout.add (hz ("freq", "Frequency", 20.0f, 18000.0f, 3000.0f, 0.0f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Sync", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Sync Rate", rateNames, 4));
    layout.add (pct ("texture", "Texture", 40.0f));
    layout.add (pct ("amount", "Amount", 30.0f));
    layout.add (pct ("stereo", "Stereo", 0.0f));
    layout.add (hz ("lowcut", "Low Cut", 20.0f, 2000.0f, 20.0f, 20.0f));
    layout.add (hz ("highcut", "High Cut", 1000.0f, 20000.0f, 20000.0f, 20000.0f));
    layout.add (pct ("drive", "Drive", 0.0f));
    layout.add (pct ("squash", "Squash", 0.0f));
    layout.add (pct ("mix", "Mix", 100.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
                AudioParameterFloatAttributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "autogain", 1 }, "Auto Gain", true));

    return layout;
}

CrumbleProcessor::CrumbleProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pMode     = apvts.getRawParameterValue ("mode");
    pFreq     = apvts.getRawParameterValue ("freq");
    pSync     = apvts.getRawParameterValue ("sync");
    pRate     = apvts.getRawParameterValue ("rate");
    pTexture  = apvts.getRawParameterValue ("texture");
    pAmount   = apvts.getRawParameterValue ("amount");
    pStereo   = apvts.getRawParameterValue ("stereo");
    pLowCut   = apvts.getRawParameterValue ("lowcut");
    pHighCut  = apvts.getRawParameterValue ("highcut");
    pDrive    = apvts.getRawParameterValue ("drive");
    pSquash   = apvts.getRawParameterValue ("squash");
    pMix      = apvts.getRawParameterValue ("mix");
    pOutput   = apvts.getRawParameterValue ("output");
    pAutoGain = apvts.getRawParameterValue ("autogain");
}

bool CrumbleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CrumbleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };

    for (auto* f : { &xLow, &xHigh, &apLow, &sq1, &sq2, &sqAp })
    {
        f->prepare (spec);
        f->reset();
    }
    apLow.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
    sqAp.setType (juce::dsp::LinkwitzRileyFilterType::allpass);

    const float sqHigh = juce::jmin (2500.0f, (float) (0.4 * sr));
    sq1.setCutoffFrequency (250.0f);
    sq2.setCutoffFrequency (sqHigh);
    sqAp.setCutoffFrequency (sqHigh);

    currentLowCut = currentHighCut = -1.0f;
    updateCrossovers (pLowCut->load(), pHighCut->load());

    const int delaySize = (int) (maxDepthSeconds * sr) + 16;
    for (int ch = 0; ch < 2; ++ch)
        for (auto* d : { &crumbleDelay[ch], &alignLow[ch], &alignHigh[ch], &alignDry[ch] })
            d->prepare (delaySize);

    for (auto& f : noiseFilt)
        f.reset();
    phase = 0.0;

    for (int ch = 0; ch < 2; ++ch)
    {
        agIn[ch] = agOut[ch] = 0.0f;
        for (int b = 0; b < 3; ++b)
            sqEnv[ch][b] = sqGainDb[ch][b] = 0.0f;
    }
    agGain = 1.0f;

    for (auto* s : { &depthSmooth, &mixSmooth, &driveSmooth, &squashSmooth, &outSmooth, &stereoSmooth })
        s->reset (sr, 0.05);

    const float amount = pAmount->load() / 100.0f;
    depthSmooth.setCurrentAndTargetValue (amount * amount * (float) (maxDepthSeconds * sr));
    mixSmooth.setCurrentAndTargetValue (pMix->load() / 100.0f);
    driveSmooth.setCurrentAndTargetValue (pDrive->load() / 100.0f);
    squashSmooth.setCurrentAndTargetValue (pSquash->load() / 100.0f);
    outSmooth.setCurrentAndTargetValue (dbToGain (pOutput->load()));
    stereoSmooth.setCurrentAndTargetValue (pStereo->load() / 100.0f);

    bucketLen = juce::jmax (1, juce::roundToInt (sr * 0.4 / Scope::size));
    bucketPos = 0;
    bucketPeak[0] = bucketPeak[1] = 0.0f;
}

void CrumbleProcessor::updateCrossovers (float lowCut, float highCut)
{
    const float nyqLimit = (float) (0.45 * sr);
    lowCut = juce::jlimit (20.0f, nyqLimit * 0.5f, lowCut);
    highCut = juce::jlimit (lowCut * 1.25f, nyqLimit, highCut);

    if (std::abs (lowCut - currentLowCut) > 0.01f)
    {
        xLow.setCutoffFrequency (lowCut);
        currentLowCut = lowCut;
    }
    if (std::abs (highCut - currentHighCut) > 0.01f)
    {
        xHigh.setCutoffFrequency (highCut);
        apLow.setCutoffFrequency (highCut);
        currentHighCut = highCut;
    }
}

void CrumbleProcessor::updateNoiseFilters (float freq, float texture)
{
    const float f0 = juce::jlimit (20.0f, (float) (0.45 * sr), freq);
    const float q = 20.0f * std::pow (0.015f, texture);   // texture 0 -> Q 20 (tonal), 1 -> Q 0.3 (rough)
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

float CrumbleProcessor::filteredNoise (int source)
{
    const float x = rng.nextFloat() * 2.0f - 1.0f;
    return std::tanh (noiseFilt[source].process (x) * noiseGain);
}

void CrumbleProcessor::pushToScope (float left, float right)
{
    if (std::abs (left) > std::abs (bucketPeak[0]))  bucketPeak[0] = left;
    if (std::abs (right) > std::abs (bucketPeak[1])) bucketPeak[1] = right;

    if (++bucketPos >= bucketLen)
    {
        const int count = scope.writeCount.load (std::memory_order_relaxed);
        const int idx = count % Scope::size;
        scope.data[0][idx] = bucketPeak[0];
        scope.data[1][idx] = bucketPeak[1];
        scope.writeCount.store (count + 1, std::memory_order_release);
        bucketPos = 0;
        bucketPeak[0] = bucketPeak[1] = 0.0f;
    }
}

void CrumbleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    if (numCh == 0 || numSamples == 0)
        return;

    const bool sineMode = juce::roundToInt (pMode->load()) == 0;
    const float freq = pFreq->load();
    const float amount = pAmount->load() / 100.0f;
    const bool autoGain = pAutoGain->load() > 0.5f;

    updateNoiseFilters (freq, pTexture->load() / 100.0f);
    const float lowCutValue = pLowCut->load(), highCutValue = pHighCut->load();
    updateCrossovers (lowCutValue, highCutValue);
    const bool lowOff  = lowCutValue <= 20.05f;
    const bool highOff = highCutValue >= 19990.0f;

    depthSmooth.setTargetValue (amount * amount * (float) (maxDepthSeconds * sr));
    mixSmooth.setTargetValue (pMix->load() / 100.0f);
    driveSmooth.setTargetValue (pDrive->load() / 100.0f);
    squashSmooth.setTargetValue (pSquash->load() / 100.0f);
    outSmooth.setTargetValue (dbToGain (pOutput->load()));
    stereoSmooth.setTargetValue (pStereo->load() / 100.0f);

    // Sine rate: free (Frequency knob) or synced to the host tempo
    const double twoPi = juce::MathConstants<double>::twoPi;
    double sineHz = freq;
    if (sineMode && pSync->load() > 0.5f)
    {
        const int rateIdx = juce::jlimit (0, rateNames.size() - 1, juce::roundToInt (pRate->load()));
        const double beatsPerCycle = kRateBeats[rateIdx];
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm())
                    bpm = *b;
                if (pos->getIsPlaying())
                    if (auto ppq = pos->getPpqPosition())
                    {
                        const double cycles = *ppq / beatsPerCycle;
                        phase = twoPi * (cycles - std::floor (cycles));
                    }
            }
        }
        sineHz = bpm / 60.0 / beatsPerCycle;
    }
    const double phaseInc = twoPi * sineHz / sr;

    const float attCoef = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
    const float relCoef = 1.0f - std::exp (-1.0f / (0.060f * (float) sr));
    const float rmsCoef = 1.0f - std::exp (-1.0f / (0.300f * (float) sr));
    const float agDown  = 1.0f - std::exp (-1.0f / (0.005f * (float) sr));   // turn down fast
    const float agUp    = 1.0f - std::exp (-1.0f / (0.150f * (float) sr));   // turn up slowly

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < numSamples; ++i)
    {
        const float depth  = depthSmooth.getNextValue();
        const float mix    = mixSmooth.getNextValue();
        const float drive  = driveSmooth.getNextValue();
        const float squash = squashSmooth.getNextValue();
        const float outG   = outSmooth.getNextValue();
        const float st     = stereoSmooth.getNextValue();

        // Modulation for left / right
        float mod[2];
        if (sineMode)
        {
            mod[0] = (float) std::sin (phase);
            mod[1] = (float) std::sin (phase + st * juce::MathConstants<double>::pi);
            phase += phaseInc;
            if (phase >= twoPi)
                phase -= twoPi;
        }
        else
        {
            const float shared = filteredNoise (0);
            if (numCh > 1 && st > 0.001f)
            {
                const float norm = 1.0f / std::sqrt ((1.0f - st) * (1.0f - st) + st * st);
                mod[0] = ((1.0f - st) * shared + st * filteredNoise (1)) * norm;
                mod[1] = ((1.0f - st) * shared + st * filteredNoise (2)) * norm;
            }
            else
            {
                mod[0] = mod[1] = shared;
            }
        }

        const float driveGain = dbToGain (drive * 30.0f);
        const float driveBlend = juce::jmin (1.0f, drive * 10.0f);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = data[ch][i];

            // 1. Split into bands
            // A cut at its end stop is "Off": that split is skipped so the sound stays untouched there
            float lo, rest, mid, hi;
            xLow.processSample (ch, x, lo, rest);
            if (lowOff)
            {
                lo = 0.0f;
                rest = x;
            }
            xHigh.processSample (ch, rest, mid, hi);
            if (highOff)
            {
                mid = rest;
                hi = 0.0f;
            }
            const float loAp = apLow.processSample (ch, lo);
            if (! highOff)
                lo = loAp;

            // 2. Crumble the middle band
            crumbleDelay[ch].push (mid);
            const float crumbled = crumbleDelay[ch].read (depth * (0.5f + 0.5f * mod[ch]));

            // Keep the clean parts in time with the average crumble delay
            const float align = depth * 0.5f;
            alignLow[ch].push (lo);
            alignHigh[ch].push (hi);
            alignDry[ch].push (x);
            lo = alignLow[ch].read (align);
            hi = alignHigh[ch].read (align);
            const float dry = alignDry[ch].read (align);

            // 3. Drive
            float y = crumbled + (std::tanh (driveGain * crumbled) - crumbled) * driveBlend;

            // 4. Squash: upward + downward compression in 3 sub-bands
            float s0, r, s1, s2;
            sq1.processSample (ch, y, s0, r);
            sq2.processSample (ch, r, s1, s2);
            s0 = sqAp.processSample (ch, s0);
            // Written as "signal + band * (gain - 1)", so with Squash at 0 the signal is exactly unchanged
            float bands[3] = { s0, s1, s2 };
            const float makeup = dbToGain (kSquashMakeupDb * squash);
            float squashed = y;
            for (int b = 0; b < 3; ++b)
            {
                const float level = std::abs (bands[b]);
                float& env = sqEnv[ch][b];
                env += (level > env ? attCoef : relCoef) * (level - env);

                const float envDb = gainToDb (env);
                float target = 0.0f;
                if (envDb > kDownThreshold)
                    target = (kDownThreshold + (envDb - kDownThreshold) / kDownRatio) - envDb;
                else if (envDb < kUpThreshold && envDb > kFloorDb)
                    target = juce::jmin (kUpMaxDb, (kUpThreshold + (envDb - kUpThreshold) / kUpRatio) - envDb);

                float& g = sqGainDb[ch][b];
                g += 0.01f * (target - g);
                squashed += bands[b] * (dbToGain (g * squash) * makeup - 1.0f);
            }
            y = squashed;

            // 5. Auto gain: match the level of the crumbled band before Drive and Squash
            agIn[ch]  += rmsCoef * (crumbled * crumbled - agIn[ch]);
            agOut[ch] += rmsCoef * (y * y - agOut[ch]);
            y *= agGain;

            // 6. Recombine, mix with dry, output
            const float wet = lo + y + hi;
            data[ch][i] = (dry + (wet - dry) * mix) * outG;
        }

        // Update auto gain once per sample from both channels
        float target = 1.0f;
        if (autoGain && (drive > 0.0005f || squash > 0.0005f))
        {
            const float in  = agIn[0] + (numCh > 1 ? agIn[1] : 0.0f);
            const float out = agOut[0] + (numCh > 1 ? agOut[1] : 0.0f);
            if (in > 1.0e-8f && out > 1.0e-12f)
                target = juce::jlimit (0.063f, 4.0f, std::sqrt (in / out));
            else
                target = agGain;
        }
        agGain += (target < agGain ? agDown : agUp) * (target - agGain);

        pushToScope (data[0][i], numCh > 1 ? data[1][i] : data[0][i]);
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
