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
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "colortarget", 1 }, "Color Target",
                StringArray { "Wet signal", "Whole signal" }, 0));

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
    pColorTarget = apvts.getRawParameterValue ("colortarget");
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

    for (auto* f : { &sq1, &sq2, &sqAp })
    {
        f->prepare (spec);
        f->reset();
    }
    sqAp.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
    const float sqHigh = juce::jmin (2500.0f, (float) (0.4 * sr));
    sq1.setCutoffFrequency (250.0f);
    sq2.setCutoffFrequency (sqHigh);
    sqAp.setCutoffFrequency (sqHigh);

    // 4th-order Butterworth = two 2nd-order stages with these Q values
    const float q1 = 0.5412f, q2 = 1.3066f;
    for (auto* f : { &hp1, &hp2, &lp1, &lp2 })
    {
        f->prepare (spec);
        f->reset();
    }
    hp1.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hp2.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lp1.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lp2.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    hp1.setResonance (q1); hp2.setResonance (q2);
    lp1.setResonance (q1); lp2.setResonance (q2);
    currentLowCut = currentHighCut = -1.0f;
    updateFilters (pLowCut->load(), pHighCut->load());

    const int delaySize = (int) (maxDepthSeconds * sr) + 16;
    for (int ch = 0; ch < 2; ++ch)
    {
        crumbleDelay[ch].prepare (delaySize);
        alignDry[ch].prepare (delaySize);
    }

    for (auto& f : noiseFilt)
        f.reset();
    phase = 0.0;
    tremPhase = 0.0;

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

void CrumbleProcessor::updateFilters (float lowCut, float highCut)
{
    const float nyqLimit = (float) (0.45 * sr);
    lowCut = juce::jlimit (20.0f, nyqLimit, lowCut);
    highCut = juce::jlimit (20.0f, nyqLimit, highCut);

    if (std::abs (lowCut - currentLowCut) > 0.01f)
    {
        hp1.setCutoffFrequency (lowCut);
        hp2.setCutoffFrequency (lowCut);
        currentLowCut = lowCut;
    }
    if (std::abs (highCut - currentHighCut) > 0.01f)
    {
        lp1.setCutoffFrequency (highCut);
        lp2.setCutoffFrequency (highCut);
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

// Drive -> Squash (OTT-style up/down compression in 3 sub-bands), measured for auto gain
float CrumbleProcessor::colorStage (int ch, float x, float driveGain, float driveBlend, float squash, float makeup,
                                    float attCoef, float relCoef, float rmsCoef)
{
    float y = x + (std::tanh (driveGain * x) - x) * driveBlend;

    float s0, r, s1, s2;
    sq1.processSample (ch, y, s0, r);
    sq2.processSample (ch, r, s1, s2);
    s0 = sqAp.processSample (ch, s0);

    // Written as "signal + band * (gain - 1)", so with Squash at 0 the signal is exactly unchanged
    const float bands[3] = { s0, s1, s2 };
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

    agIn[ch]  += rmsCoef * (x * x - agIn[ch]);
    agOut[ch] += rmsCoef * (y * y - agOut[ch]);
    return y * agGain;
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
    const bool colorOnAll = juce::roundToInt (pColorTarget->load()) == 1;

    updateNoiseFilters (freq, pTexture->load() / 100.0f);
    const float lowCutValue = pLowCut->load(), highCutValue = pHighCut->load();
    updateFilters (lowCutValue, highCutValue);
    const bool lowOn  = lowCutValue > 20.05f;
    const bool highOn = highCutValue < 19990.0f;

    depthSmooth.setTargetValue (amount * amount * (float) (maxDepthSeconds * sr));
    mixSmooth.setTargetValue (pMix->load() / 100.0f);
    driveSmooth.setTargetValue (pDrive->load() / 100.0f);
    squashSmooth.setTargetValue (pSquash->load() / 100.0f);
    outSmooth.setTargetValue (dbToGain (pOutput->load()));
    stereoSmooth.setTargetValue (pStereo->load() / 100.0f);

    const double twoPi = juce::MathConstants<double>::twoPi;
    const double phaseInc = twoPi * (double) freq / sr;

    // Sync: the effect fades in and out (tremolo) at a tempo-locked rate
    const bool sync = pSync->load() > 0.5f;
    double tremInc = 0.0;
    if (sync)
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
                        tremPhase = cycles - std::floor (cycles);
                    }
            }
        }
        tremInc = bpm / 60.0 / beatsPerCycle / sr;
    }

    const float attCoef = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
    const float relCoef = 1.0f - std::exp (-1.0f / (0.060f * (float) sr));
    const float rmsCoef = 1.0f - std::exp (-1.0f / (0.300f * (float) sr));
    const float agDown  = 1.0f - std::exp (-1.0f / (0.005f * (float) sr));   // turn down fast
    const float agUp    = 1.0f - std::exp (-1.0f / (0.150f * (float) sr));   // turn up slowly

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    float lastLevel = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix    = mixSmooth.getNextValue();
        const float drive  = driveSmooth.getNextValue();
        const float squash = squashSmooth.getNextValue();
        const float outG   = outSmooth.getNextValue();
        const float st     = stereoSmooth.getNextValue();

        // Tremolo: full effect on the beat, dips to nothing halfway through the cycle
        float level = 1.0f;
        if (sync)
        {
            level = (float) (0.5 + 0.5 * std::cos (twoPi * tremPhase));
            tremPhase += tremInc;
            if (tremPhase >= 1.0)
                tremPhase -= 1.0;
        }
        lastLevel = level;
        const float depth = depthSmooth.getNextValue() * level;

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
        const float makeup = dbToGain (kSquashMakeupDb * squash);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = data[ch][i];

            // 1. Crumble
            crumbleDelay[ch].push (x);
            float wet = crumbleDelay[ch].read (depth * (0.5f + 0.5f * mod[ch]));

            // Keep the dry signal in time with the average crumble delay
            alignDry[ch].push (x);
            const float dry = alignDry[ch].read (depth * 0.5f);

            // 2. Color on the wet signal
            if (! colorOnAll)
                wet = colorStage (ch, wet, driveGain, driveBlend, squash, makeup, attCoef, relCoef, rmsCoef);

            // 3. Wet filters (always running, so turning them on is smooth)
            const float hp = hp2.processSample (ch, hp1.processSample (ch, wet));
            if (lowOn)
                wet = hp;
            const float lp = lp2.processSample (ch, lp1.processSample (ch, wet));
            if (highOn)
                wet = lp;

            // 4. Mix, then color on the whole signal if chosen
            float out = dry + (wet - dry) * mix;
            if (colorOnAll)
                out = colorStage (ch, out, driveGain, driveBlend, squash, makeup, attCoef, relCoef, rmsCoef);

            data[ch][i] = out * outG;
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

    effectLevel.store (lastLevel, std::memory_order_relaxed);
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
