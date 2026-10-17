#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    float gainToDb (float g)  { return 20.0f * std::log10 (g + 1.0e-9f); }

    // Squash (OTT-style) settings
    constexpr float kDownThreshold = -18.0f, kDownRatio = 4.0f;
    constexpr float kUpThreshold   = -30.0f, kUpRatio   = 3.0f;
    constexpr float kUpMaxDb = 24.0f, kFloorDb = -72.0f, kSquashMakeupDb = 6.0f;

    String hzText (float v)
    {
        return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 1) + " kHz";
    }
    float hzFromText (const String& text)
    {
        auto t = text.trim().toLowerCase();
        const float v = t.getFloatValue();
        return t.containsChar ('k') ? v * 1000.0f : v;
    }
    String dbText (float v)
    {
        return (v > 0.05f ? "+" : "") + String (v, 1) + " dB";
    }
}

AudioProcessorValueTreeState::ParameterLayout CrumbleProcessor::createLayout()
{
    using namespace ParamIDs;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f), def,
                   AudioParameterFloatAttributes().withLabel ("%")
                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; }));
    };
    auto hzParam = [] (const char* id, const char* name, float lo, float hi, float def, float offAt)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.0f, 0.3f), def,
                   AudioParameterFloatAttributes().withLabel ("Hz")
                       .withStringFromValueFunction ([offAt] (float v, int)
                       {
                           if (offAt > 0.0f && std::abs (v - offAt) < offAt * 0.002f)
                               return String ("Off");
                           return hzText (v);
                       })
                       .withValueFromStringFunction ([offAt] (const String& t)
                       {
                           return t.trim().equalsIgnoreCase ("off") ? offAt : hzFromText (t);
                       }));
    };
    auto dbParam = [] (const char* id, const char* name, float lo, float hi, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def,
                   AudioParameterFloatAttributes().withLabel ("dB")
                       .withStringFromValueFunction ([] (float v, int) { return dbText (v); }));
    };
    auto choice = [] (const char* id, const char* name, const StringArray& items, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def);
    };
    auto toggle = [] (const char* id, const char* name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    };

    // Global
    layout.add (dbParam (input, "Input", -24.0f, 24.0f, 0.0f));
    layout.add (dbParam (output, "Output", -24.0f, 12.0f, 0.0f));
    layout.add (pct (mix, "Mix", 100.0f));
    layout.add (toggle (autoGain, "Auto Gain", true));
    layout.add (toggle (order, "Bitcrush First", false));

    // Crumble
    layout.add (toggle (crOn, "Crumble On", true));
    layout.add (choice (mode, "Mode", Tables::modes, 2));
    layout.add (hzParam (freq, "Frequency", 20.0f, 18000.0f, 3000.0f, 0.0f));
    layout.add (choice (rate, "Sync Rate", Tables::noteNames, 9));
    layout.add (pct (texture, "Texture", 40.0f));
    layout.add (pct (amount, "Amount", 30.0f));
    layout.add (pct (stereo, "Stereo", 0.0f));
    layout.add (pct (feedback, "Feedback", 0.0f));

    // Bitcrush
    layout.add (toggle (bcOn, "Bitcrush On", false));
    layout.add (hzParam (bcFreq, "Crush Frequency", 200.0f, 48000.0f, 8000.0f, 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { bits, 1 }, "Bits", NormalisableRange<float> (1.0f, 16.0f), 8.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));
    layout.add (pct (quantize, "Quantize", 100.0f));
    layout.add (pct (dither, "Dither", 0.0f));
    layout.add (pct (adcQ, "ADC Q", 50.0f));
    layout.add (pct (dacQ, "DAC Q", 50.0f));

    // Wet filter
    layout.add (toggle (fltOn, "Filter On", true));
    layout.add (choice (fltTarget, "Filter Target", Tables::targets, 0));
    layout.add (hzParam (lowCut, "Low Cut", 20.0f, 2000.0f, 20.0f, 20.0f));
    layout.add (hzParam (highCut, "High Cut", 1000.0f, 20000.0f, 20000.0f, 20000.0f));

    // Color
    layout.add (toggle (colOn, "Color On", true));
    layout.add (choice (colTarget, "Color Target", Tables::targets, 0));
    layout.add (choice (driveType, "Drive Type", Tables::driveTypes, 0));
    layout.add (pct (drive, "Drive", 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { tone, 1 }, "Tone", NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                {
                    const int i = roundToInt (v);
                    return i == 0 ? String ("Flat") : (i < 0 ? "Dark " + String (-i) : "Bright " + String (i));
                })));
    layout.add (pct (squash, "Squash", 0.0f));

    // Tremolo
    layout.add (toggle (trOn, "Tremolo On", false));
    layout.add (choice (trTarget, "Tremolo Target", Tables::targets, 0));
    layout.add (choice (trShape, "Tremolo Shape", Tables::trShapes, 0));
    layout.add (pct (trDepth, "Tremolo Depth", 100.0f));
    layout.add (choice (trRate, "Tremolo Rate", Tables::noteNames, 6));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { trPan, 1 }, "Tremolo Pan", NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                {
                    const int i = roundToInt (v);
                    return i > 0 ? "+" + String (i) : String (i);
                })));

    // Follow
    layout.add (toggle (folOn, "Follow On", false));
    layout.add (dbParam (folThresh, "Follow Threshold", -60.0f, 0.0f, -18.0f));
    layout.add (choice (folMode, "Follow Mode", Tables::folModes, 0));

    // Clip
    layout.add (toggle (clipOn, "Clip On", true));
    layout.add (choice (clipMode, "Clip Mode", Tables::clipModes, 0));
    layout.add (dbParam (ceiling, "Ceiling", -12.0f, 0.0f, 0.0f));

    return layout;
}

CrumbleProcessor::CrumbleProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    using namespace ParamIDs;
    pInput = raw (input);       pOutput = raw (output);     pMix = raw (mix);
    pAutoGain = raw (autoGain); pOrder = raw (order);
    pCrOn = raw (crOn);         pMode = raw (mode);         pFreq = raw (freq);       pRate = raw (rate);
    pTexture = raw (texture);   pAmount = raw (amount);     pStereo = raw (stereo);   pFeedback = raw (feedback);
    pBcOn = raw (bcOn);         pBcFreq = raw (bcFreq);     pBits = raw (bits);       pQuantize = raw (quantize);
    pDither = raw (dither);     pAdcQ = raw (adcQ);         pDacQ = raw (dacQ);
    pFltOn = raw (fltOn);       pFltTarget = raw (fltTarget); pLowCut = raw (lowCut); pHighCut = raw (highCut);
    pColOn = raw (colOn);       pColTarget = raw (colTarget); pDriveType = raw (driveType);
    pDrive = raw (drive);       pTone = raw (tone);         pSquash = raw (squash);
    pTrOn = raw (trOn);         pTrTarget = raw (trTarget); pTrShape = raw (trShape);
    pTrDepth = raw (trDepth);   pTrRate = raw (trRate);     pTrPan = raw (trPan);
    pFolOn = raw (folOn);       pFolThresh = raw (folThresh); pFolMode = raw (folMode);
    pClipOn = raw (clipOn);     pClipMode = raw (clipMode); pCeiling = raw (ceiling);
}

bool CrumbleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CrumbleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const dsp::ProcessSpec spec { sr, (uint32) jmax (1, samplesPerBlock), 2 };

    for (auto* f : { &sq1, &sq2, &sqAp })
    {
        f->prepare (spec);
        f->reset();
    }
    sqAp.setType (dsp::LinkwitzRileyFilterType::allpass);
    const float sqHigh = jmin (2500.0f, (float) (0.4 * sr));
    sq1.setCutoffFrequency (250.0f);
    sq2.setCutoffFrequency (sqHigh);
    sqAp.setCutoffFrequency (sqHigh);

    hp.prepare (spec, dsp::StateVariableTPTFilterType::highpass);
    lp.prepare (spec, dsp::StateVariableTPTFilterType::lowpass);
    adc.prepare (spec, dsp::StateVariableTPTFilterType::lowpass);
    dac.prepare (spec, dsp::StateVariableTPTFilterType::lowpass);
    hp.cutoff = lp.cutoff = adc.cutoff = dac.cutoff = -1.0f;

    const int delaySize = (int) ((maxDepthSeconds + 0.002) * sr) + 16;
    for (int ch = 0; ch < 2; ++ch)
    {
        crumbleDelay[ch].prepare (delaySize);
        alignDry[ch].prepare (delaySize);
        fbState[ch] = fbHp[ch] = hold[ch] = 0.0f;
        dcX[ch] = dcY[ch] = tapeLp[ch] = toneLp[ch] = 0.0f;
        agIn[ch] = agOut[ch] = 0.0f;
        trGain[ch] = 1.0f;
        for (int b = 0; b < 3; ++b)
            sqEnv[ch][b] = sqGainDb[ch][b] = 0.0f;
    }
    for (auto& f : noiseFilt)
        f.reset();
    lfoPhase = holdPhase = trPhase = 0.0;
    agGain = 1.0f;
    folEnv = 0.0f;
    folGain = 1.0f;
    appliedOrder = pOrder->load() > 0.5f;
    swapGain = 1.0f;

    auto init = [this] (SmoothedValue<float>& s, float seconds, float value)
    {
        s.reset (sr, seconds);
        s.setCurrentAndTargetValue (value);
    };
    const float amount = pAmount->load() / 100.0f;
    init (inGainS, 0.05f, dbToGain (pInput->load()));
    init (outGainS, 0.05f, dbToGain (pOutput->load()));
    init (mixS, 0.05f, pMix->load() / 100.0f);
    init (depthS, 0.05f, amount * amount * (float) (maxDepthSeconds * sr));
    init (stereoS, 0.05f, pStereo->load() / 100.0f);
    init (feedbackS, 0.05f, pFeedback->load() / 100.0f);
    init (driveS, 0.05f, pDrive->load() / 100.0f);
    init (toneS, 0.05f, pTone->load() / 100.0f);
    init (squashS, 0.05f, pSquash->load() / 100.0f);
    init (trDepthS, 0.05f, pTrDepth->load() / 100.0f);
    init (crFade, 0.02f, pCrOn->load() > 0.5f ? 1.0f : 0.0f);
    init (bcFade, 0.02f, pBcOn->load() > 0.5f ? 1.0f : 0.0f);
    init (fltFade, 0.02f, pFltOn->load() > 0.5f ? 1.0f : 0.0f);
    init (colFade, 0.02f, pColOn->load() > 0.5f ? 1.0f : 0.0f);
    init (trFade, 0.02f, pTrOn->load() > 0.5f ? 1.0f : 0.0f);
    init (folFade, 0.02f, pFolOn->load() > 0.5f ? 1.0f : 0.0f);
    init (clipFade, 0.02f, pClipOn->load() > 0.5f ? 1.0f : 0.0f);

    sqAtt = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
    sqRel = 1.0f - std::exp (-1.0f / (0.060f * (float) sr));
    rmsCoef = 1.0f - std::exp (-1.0f / (0.3f * (float) sr));
    toneCoef = 1.0f - std::exp (-MathConstants<float>::twoPi * 1000.0f / (float) sr);

    bucketLen = jmax (1, roundToInt (sr * 0.0005));
    bucketPos = 0;
    bucketOut[0] = bucketOut[1] = bucketIn = 0.0f;
}

void CrumbleProcessor::updateNoiseFilters (float freq, float texture)
{
    const float f0 = jlimit (20.0f, (float) (0.45 * sr), freq);
    const float q = 20.0f * std::pow (0.015f, texture);
    const float w0 = MathConstants<float>::twoPi * f0 / (float) sr;
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
    noiseGain = jlimit (1.0f, 40.0f, std::sqrt (0.318f * (float) sr * q / f0));
}

float CrumbleProcessor::filteredNoise (int source)
{
    const float x = rng.nextFloat() * 2.0f - 1.0f;
    return std::tanh (noiseFilt[source].process (x) * noiseGain);
}

// Modulated micro delay with optional feedback (metallic resonance)
float CrumbleProcessor::crumbleSample (int ch, float x, float depth, float mod, float feedback)
{
    // Feedback path: high-passed so the low end does not build up, soft-limited for stability
    const float fbIn = feedback > 0.0005f ? std::tanh (fbHp[ch]) * feedback * 0.75f : 0.0f;
    crumbleDelay[ch].push (x + fbIn);
    const float y = crumbleDelay[ch].read (depth * (0.5f + 0.5f * mod));

    const float fbTap = crumbleDelay[ch].read (depth * 0.5f + (float) (0.0012 * sr));
    fbState[ch] += 0.03f * (fbTap - fbState[ch]);       // one-pole low-pass ~ 200 Hz
    fbHp[ch] = fbTap - fbState[ch];                     // -> high-passed feedback
    return y * (1.0f - 0.35f * feedback);
}

float CrumbleProcessor::bitcrushSample (int ch, float x, bool newHold, float levels, float quantize, float dither,
                                        float adcQ, float dacQ)
{
    // ADC: anti-alias filter quality (low = more aliasing folding into the lows)
    const float pre = x + (adc.process (ch, x) - x) * adcQ;
    if (newHold)
        hold[ch] = pre;
    float y = hold[ch];

    // Bit reduction with optional dither, mixed in by Quantize
    const float d = dither * (rng.nextFloat() - rng.nextFloat()) / levels;
    const float q = std::round ((y + d) * levels) / levels;
    y += (q - y) * quantize;

    // DAC: reconstruction filter quality (low = more aliasing in the highs)
    return y + (dac.process (ch, y) - y) * dacQ;
}

float CrumbleProcessor::colorSample (int ch, float x, int type, float drive, float tone, float squash)
{
    const float blend = jmin (1.0f, drive * 10.0f);
    const float g = dbToGain (drive * 30.0f);
    float s = x;
    switch (type)
    {
        case 0: s = std::tanh (g * x); break;                                                  // Soft
        case 1:                                                                                // Tape
        {
            s = std::tanh (g * x * 0.8f) * 1.1f;
            tapeLp[ch] += tapeCoef * (s - tapeLp[ch]);
            s = tapeLp[ch];
            break;
        }
        case 2: s = std::tanh (g * x + 0.35f) - std::tanh (0.35f); break;                     // Tube (even harmonics)
        case 3:                                                                                // Fuzz (asymmetric)
        {
            const float a = g * x * 1.5f;
            s = a > 0.0f ? 1.0f - std::exp (-a) : -0.7f * (1.0f - std::exp (a / 0.7f));
            break;
        }
        case 4: s = jlimit (-1.0f, 1.0f, g * x); break;                                        // Hard
        case 5: s = std::sin (MathConstants<float>::halfPi * x * (1.0f + drive * 8.0f)); break;  // Fold
        default: break;
    }
    float y = x + (s - x) * blend;

    // DC blocker for the asymmetric curves (Tube, Fuzz), faded in with Drive so nothing changes at 0
    const float dc = y - dcX[ch] + 0.995f * dcY[ch];
    dcX[ch] = y;
    dcY[ch] = dc;
    if (type == 2 || type == 3)
        y += (dc - y) * blend;

    // Tone: tilt around ~1 kHz
    if (std::abs (tone) > 0.001f)
    {
        toneLp[ch] += toneCoef * (y - toneLp[ch]);
        const float hi = y - toneLp[ch];
        y = tone < 0.0f ? toneLp[ch] + hi * (1.0f + tone)
                        : toneLp[ch] * (1.0f - 0.25f * tone) + hi * (1.0f + 1.5f * tone);
    }
    else
    {
        toneLp[ch] = y;
    }

    // Squash: OTT-style up/down compression in 3 sub-bands, as "signal + band * (gain - 1)"
    float s0, r, s1, s2;
    sq1.processSample (ch, y, s0, r);
    sq2.processSample (ch, r, s1, s2);
    s0 = sqAp.processSample (ch, s0);
    const float bands[3] = { s0, s1, s2 };
    const float makeup = dbToGain (kSquashMakeupDb * squash);
    float out = y;
    for (int b = 0; b < 3; ++b)
    {
        const float level = std::abs (bands[b]);
        float& env = sqEnv[ch][b];
        env += (level > env ? sqAtt : sqRel) * (level - env);
        const float envDb = gainToDb (env);
        float target = 0.0f;
        if (envDb > kDownThreshold)
            target = (kDownThreshold + (envDb - kDownThreshold) / kDownRatio) - envDb;
        else if (envDb < kUpThreshold && envDb > kFloorDb)
            target = jmin (kUpMaxDb, (kUpThreshold + (envDb - kUpThreshold) / kUpRatio) - envDb);
        float& gdb = sqGainDb[ch][b];
        gdb += 0.01f * (target - gdb);
        out += bands[b] * (dbToGain (gdb * squash) * makeup - 1.0f);
    }

    // Auto gain measurement
    agIn[ch]  += rmsCoef * (x * x - agIn[ch]);
    agOut[ch] += rmsCoef * (out * out - agOut[ch]);
    return out;
}

float CrumbleProcessor::tremoloShape (int shape, double p) const
{
    switch (shape)
    {
        case 0: return (float) (0.5 + 0.5 * std::cos (MathConstants<double>::twoPi * p));      // Sine
        case 1: return (float) (1.0 - 2.0 * (p < 0.5 ? p : 1.0 - p));                          // Triangle
        case 2: return p < 0.5 ? 1.0f : 0.0f;                                                   // Square
        case 3: return (float) p;                                                               // Ramp up
        case 4: return (float) (1.0 - p);                                                       // Ramp down
        case 5: return (float) ((1.0 - std::exp (-8.0 * p)) / (1.0 - std::exp (-8.0)));        // Sidechain
        default: return 1.0f;
    }
}

float CrumbleProcessor::clipSample (float x, bool hard, float c) const
{
    if (hard)
        return jlimit (-c, c, x);
    const float k = 0.75f * c;
    const float a = std::abs (x);
    if (a <= k)
        return x;
    const float y = k + (c - k) * std::tanh ((a - k) / (c - k));
    return x < 0.0f ? -y : y;
}

void CrumbleProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numCh = jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    if (numCh == 0 || numSamples == 0)
        return;

    // ---- Read parameters ----
    const int mode = roundToInt (pMode->load());
    const float amount = pAmount->load() / 100.0f;
    const bool autoGain = pAutoGain->load() > 0.5f;
    const bool desiredOrder = pOrder->load() > 0.5f;
    const bool fltAll = roundToInt (pFltTarget->load()) == 1;
    const bool colAll = roundToInt (pColTarget->load()) == 1;
    const bool trAll  = roundToInt (pTrTarget->load()) == 1;
    const int driveType = roundToInt (pDriveType->load());
    const int trShape = roundToInt (pTrShape->load());
    const float trPan = pTrPan->load() / 100.0f;
    const bool folBelow = roundToInt (pFolMode->load()) == 1;
    const float folThresh = pFolThresh->load();
    const bool clipHard = roundToInt (pClipMode->load()) == 1;
    const float ceiling = dbToGain (pCeiling->load());

    updateNoiseFilters (pFreq->load(), pTexture->load() / 100.0f);
    tapeCoef = 1.0f - std::exp (-MathConstants<float>::twoPi * (18000.0f - pDrive->load() * 120.0f) / (float) sr);

    const float lowCutValue = pLowCut->load(), highCutValue = pHighCut->load();
    const bool lowOn = lowCutValue > 20.05f, highOn = highCutValue < 19990.0f;
    hp.setCutoff (jlimit (20.0f, (float) (0.45 * sr), lowCutValue));
    lp.setCutoff (jlimit (20.0f, (float) (0.45 * sr), highCutValue));

    // Bitcrush settings
    const float bcHz = jlimit (50.0f, (float) sr, pBcFreq->load());
    adc.setCutoff (jlimit (20.0f, (float) (0.45 * sr), bcHz * 0.45f));
    dac.setCutoff (jlimit (20.0f, (float) (0.45 * sr), bcHz * 0.45f));
    const double holdInc = bcHz / sr;
    const float levels = std::pow (2.0f, pBits->load() - 1.0f);
    const float quantize = pQuantize->load() / 100.0f;
    const float dither = pDither->load() / 100.0f;
    const float adcQ = pAdcQ->load() / 100.0f;
    const float dacQ = pDacQ->load() / 100.0f;

    // Smoothing targets
    inGainS.setTargetValue (dbToGain (pInput->load()));
    outGainS.setTargetValue (dbToGain (pOutput->load()));
    mixS.setTargetValue (pMix->load() / 100.0f);
    depthS.setTargetValue (amount * amount * (float) (maxDepthSeconds * sr));
    stereoS.setTargetValue (pStereo->load() / 100.0f);
    feedbackS.setTargetValue (pFeedback->load() / 100.0f);
    driveS.setTargetValue (pDrive->load() / 100.0f);
    toneS.setTargetValue (pTone->load() / 100.0f);
    squashS.setTargetValue (pSquash->load() / 100.0f);
    trDepthS.setTargetValue (pTrDepth->load() / 100.0f);
    crFade.setTargetValue (pCrOn->load() > 0.5f ? 1.0f : 0.0f);
    bcFade.setTargetValue (pBcOn->load() > 0.5f ? 1.0f : 0.0f);
    fltFade.setTargetValue (pFltOn->load() > 0.5f ? 1.0f : 0.0f);
    colFade.setTargetValue (pColOn->load() > 0.5f ? 1.0f : 0.0f);
    trFade.setTargetValue (pTrOn->load() > 0.5f ? 1.0f : 0.0f);
    folFade.setTargetValue (pFolOn->load() > 0.5f ? 1.0f : 0.0f);
    clipFade.setTargetValue (pClipOn->load() > 0.5f ? 1.0f : 0.0f);

    // ---- Tempo: Sine Sync and Tremolo lock to the host ----
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                bpm = *b;
            if (auto q = pos->getPpqPosition())
                ppq = *q;
            playing = pos->getIsPlaying();
        }

    const double twoPi = MathConstants<double>::twoPi;
    double lfoInc = twoPi * (double) pFreq->load() / sr;
    if (mode == 1)
    {
        const double beats = Tables::noteBeats[jlimit (0, Tables::noteNames.size() - 1, roundToInt (pRate->load()))];
        lfoInc = twoPi * (bpm / 60.0 / beats) / sr;
        if (playing)
        {
            const double cycles = ppq / beats;
            lfoPhase = twoPi * (cycles - std::floor (cycles));
        }
    }
    const double trBeats = Tables::noteBeats[jlimit (0, Tables::noteNames.size() - 1, roundToInt (pTrRate->load()))];
    const double trInc = bpm / 60.0 / trBeats / sr;
    if (playing)
    {
        const double cycles = ppq / trBeats;
        trPhase = cycles - std::floor (cycles);
    }

    const float folAtt = 1.0f - std::exp (-1.0f / (0.001f * (float) sr));
    const float folRel = 1.0f - std::exp (-1.0f / (0.100f * (float) sr));
    const float folSmooth = 1.0f - std::exp (-1.0f / (0.010f * (float) sr));
    const float trSmooth = 1.0f - std::exp (-1.0f / (0.0015f * (float) sr));
    const float agDown = 1.0f - std::exp (-1.0f / (0.005f * (float) sr));
    const float agUp = 1.0f - std::exp (-1.0f / (0.150f * (float) sr));
    const float swapStep = 1.0f / (0.008f * (float) sr);

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    float peakIn[2] {}, peakOut[2] {};

    for (int i = 0; i < numSamples; ++i)
    {
        const float inG = inGainS.getNextValue(), outG = outGainS.getNextValue(), mix = mixS.getNextValue();
        const float depth = depthS.getNextValue(), st = stereoS.getNextValue(), fb = feedbackS.getNextValue();
        const float drive = driveS.getNextValue(), tone = toneS.getNextValue(), squash = squashS.getNextValue();
        const float trDepth = trDepthS.getNextValue();
        const float crF = crFade.getNextValue(), bcF = bcFade.getNextValue(), fltF = fltFade.getNextValue();
        const float colF = colFade.getNextValue(), trF = trFade.getNextValue(), folF = folFade.getNextValue();
        const float clipF = clipFade.getNextValue();

        // Order swap: dip the effect, flip at the bottom, come back
        if (desiredOrder != appliedOrder)
        {
            swapGain -= swapStep;
            if (swapGain <= 0.0f)
            {
                swapGain = 0.0f;
                appliedOrder = desiredOrder;
            }
        }
        else
        {
            swapGain = jmin (1.0f, swapGain + swapStep);
        }

        // Crumble modulation
        float mod[2] { 0.0f, 0.0f };
        if (mode != 2)
        {
            mod[0] = (float) std::sin (lfoPhase);
            mod[1] = (float) std::sin (lfoPhase + st * MathConstants<double>::pi);
            lfoPhase += lfoInc;
            if (lfoPhase >= twoPi)
                lfoPhase -= twoPi;
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

        // Bitcrush sample-and-hold clock (shared by both channels)
        holdPhase += holdInc;
        const bool newHold = holdPhase >= 1.0;
        if (newHold)
            holdPhase -= std::floor (holdPhase);

        // Tremolo phase
        const double pL = trPhase;
        double pR = trPhase + 0.5 * trPan;
        pR -= std::floor (pR);
        trPhase += trInc;
        if (trPhase >= 1.0)
            trPhase -= 1.0;

        // Follow detector on the input
        float x[2];
        for (int ch = 0; ch < numCh; ++ch)
            x[ch] = data[ch][i] * inG;
        const float level = numCh > 1 ? jmax (std::abs (x[0]), std::abs (x[1])) : std::abs (x[0]);
        folEnv += (level > folEnv ? folAtt : folRel) * (level - folEnv);
        float folTarget = jlimit (0.0f, 1.0f, (gainToDb (folEnv) - (folThresh - 3.0f)) / 6.0f);
        if (folBelow)
            folTarget = 1.0f - folTarget;
        folGain += folSmooth * (folTarget - folGain);
        const float follow = 1.0f + (folGain - 1.0f) * folF;

        const float crDepth = depth * crF;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float in = x[ch];
            if (std::abs (in) > peakIn[ch])
                peakIn[ch] = std::abs (in);

            // Clean reference, kept in time with the average crumble delay
            alignDry[ch].push (in);
            const float dry = alignDry[ch].read (crDepth * 0.5f);

            // 1. Crumble and Bitcrush, in the chosen order
            float w = in;
            auto doCrumble = [&]
            {
                const float y = crumbleSample (ch, w, crDepth, mod[ch], fb);
                w = (crF < 0.999f) ? w + (y - w) * crF : y;
            };
            auto doBitcrush = [&]
            {
                if (bcF > 0.0001f)
                {
                    const float y = bitcrushSample (ch, w, newHold, levels, quantize, dither, adcQ, dacQ);
                    w += (y - w) * bcF;
                }
            };
            if (appliedOrder) { doBitcrush(); doCrumble(); }
            else              { doCrumble(); doBitcrush(); }

            // 2. Color on the wet signal
            if (! colAll && colF > 0.0001f)
            {
                const float y = colorSample (ch, w, driveType, drive, tone, squash) * agGain;
                w += (y - w) * colF;
            }

            // 3. From here on, work on the effect alone: the difference to the clean signal
            float effect = w - dry;

            if (! fltAll)
            {
                float f = effect;
                const float h = hp.process (ch, f);
                if (lowOn) f = h;
                const float l = lp.process (ch, f);
                if (highOn) f = l;
                effect += (f - effect) * fltF;
            }

            // Tremolo gain for this channel (smoothed so square edges do not click)
            const float shape = tremoloShape (trShape, ch == 0 ? pL : pR);
            const float target = 1.0f - trDepth * trF * (1.0f - shape);
            trGain[ch] += trSmooth * (target - trGain[ch]);

            if (! trAll)
                effect *= trGain[ch];

            // 4. Follow and Mix
            float out = dry + effect * follow * mix * swapGain;

            // 5. "All" stages on the whole signal
            if (colAll && colF > 0.0001f)
            {
                const float y = colorSample (ch, out, driveType, drive, tone, squash) * agGain;
                out += (y - out) * colF;
            }
            if (fltAll)
            {
                float f = out;
                const float h = hp.process (ch, f);
                if (lowOn) f = h;
                const float l = lp.process (ch, f);
                if (highOn) f = l;
                out += (f - out) * fltF;
            }
            if (trAll)
                out *= trGain[ch];

            // 6. Output and Clip
            out *= outG;
            if (clipF > 0.0001f)
                out += (clipSample (out, clipHard, ceiling) - out) * clipF;

            data[ch][i] = out;
            if (std::abs (out) > peakOut[ch])
                peakOut[ch] = std::abs (out);
        }

        // Auto gain for Drive and Squash
        float agTarget = 1.0f;
        if (autoGain && colF > 0.0001f && (drive > 0.0005f || squash > 0.0005f))
        {
            const float a = agIn[0] + (numCh > 1 ? agIn[1] : 0.0f);
            const float b = agOut[0] + (numCh > 1 ? agOut[1] : 0.0f);
            agTarget = (a > 1.0e-8f && b > 1.0e-12f) ? jlimit (0.063f, 4.0f, std::sqrt (a / b)) : agGain;
        }
        agGain += (agTarget < agGain ? agDown : agUp) * (agTarget - agGain);

        // Scope
        const float oL = data[0][i], oR = numCh > 1 ? data[1][i] : oL;
        const float inMono = numCh > 1 ? 0.5f * (x[0] + x[1]) : x[0];
        if (std::abs (oL) > std::abs (bucketOut[0])) bucketOut[0] = oL;
        if (std::abs (oR) > std::abs (bucketOut[1])) bucketOut[1] = oR;
        if (std::abs (inMono) > std::abs (bucketIn)) bucketIn = inMono;
        if (++bucketPos >= bucketLen)
        {
            const int count = scope.writeCount.load (std::memory_order_relaxed);
            const int idx = count % Scope::size;
            scope.out[0][idx] = bucketOut[0];
            scope.out[1][idx] = bucketOut[1];
            scope.in[idx] = bucketIn;
            scope.writeCount.store (count + 1, std::memory_order_release);
            bucketPos = 0;
            bucketOut[0] = bucketOut[1] = bucketIn = 0.0f;
        }
    }

    // Mono: copy to any extra channel
    for (int ch = 0; ch < 2; ++ch)
    {
        const float pi = ch < numCh ? peakIn[ch] : peakIn[0];
        const float po = ch < numCh ? peakOut[ch] : peakOut[0];
        if (pi > inPeak[ch].load (std::memory_order_relaxed))  inPeak[ch].store (pi, std::memory_order_relaxed);
        if (po > outPeak[ch].load (std::memory_order_relaxed)) outPeak[ch].store (po, std::memory_order_relaxed);
    }
    followGainNow.store (1.0f + (folGain - 1.0f) * folFade.getCurrentValue(), std::memory_order_relaxed);
    tremoloNow.store (trGain[0], std::memory_order_relaxed);
}

void CrumbleProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CrumbleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            presets.setCurrentNameFromState (tree.getProperty ("presetName").toString());
            apvts.replaceState (tree);
        }
}

AudioProcessorEditor* CrumbleProcessor::createEditor()
{
    return new CrumbleEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrumbleProcessor();
}
