#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using cdsp::dbToGain;
using cdsp::gainToDb;

namespace
{
    String hzText (float v)
    {
        return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz";
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
    String msText (float v)
    {
        return v < 1000.0f ? String (roundToInt (v)) + " ms" : String (v / 1000.0f, 2) + " s";
    }
}

AudioProcessorValueTreeState::ParameterLayout CrumbleProcessor::createLayout()
{
    using namespace ParamIDs;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f), def,
                   AudioParameterFloatAttributes().withLabel ("%")
                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; }));
    };
    auto hzParam = [] (const String& id, const String& name, float lo, float hi, float def, float offLow, float offHigh)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.0f, 0.25f), def,
                   AudioParameterFloatAttributes().withLabel ("Hz")
                       .withStringFromValueFunction ([offLow, offHigh] (float v, int)
                       {
                           if ((offLow > 0.0f && v <= offLow + 0.05f) || (offHigh > 0.0f && v >= offHigh - 10.0f))
                               return String ("Off");
                           return hzText (v);
                       })
                       .withValueFromStringFunction ([offLow, offHigh] (const String& t)
                       {
                           if (t.trim().equalsIgnoreCase ("off"))
                               return offLow > 0.0f ? offLow : offHigh;
                           return hzFromText (t);
                       }));
    };
    auto dbParam = [] (const String& id, const String& name, float lo, float hi, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def,
                   AudioParameterFloatAttributes().withLabel ("dB")
                       .withStringFromValueFunction ([] (float v, int) { return dbText (v); }));
    };
    auto msParam = [] (const String& id, const String& name, float lo, float hi, float def, float skew)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.0f, skew), def,
                   AudioParameterFloatAttributes().withLabel ("ms")
                       .withStringFromValueFunction ([] (float v, int) { return msText (v); })
                       .withValueFromStringFunction ([] (const String& t)
                       {
                           const auto s = t.trim().toLowerCase();
                           const float v = s.getFloatValue();
                           return s.endsWith ("s") && ! s.endsWith ("ms") ? v * 1000.0f : v;
                       }));
    };
    auto choice = [] (const String& id, const String& name, const StringArray& items, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def);
    };
    auto toggle = [] (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    };

    // Global
    layout.add (dbParam (input, "Input", -24.0f, 24.0f, 0.0f));
    layout.add (dbParam (output, "Output", -24.0f, 12.0f, 0.0f));
    layout.add (pct (mix, "Mix", 100.0f));
    layout.add (toggle (autoGain, "Auto Gain", true));
    layout.add (toggle (order, "Bitcrush First", false));
    layout.add (choice (chain, "Block Order", Tables::chainNames, 0));

    // Crumble
    layout.add (toggle (crOn, "Crumble On", true));
    layout.add (choice (mode, "Mode", Tables::modes, 2));
    layout.add (hzParam (freq, "Frequency", 20.0f, 18000.0f, 3000.0f, 0.0f, 0.0f));
    layout.add (choice (rate, "Sync Rate", Tables::noteNames, 9));
    layout.add (pct (texture, "Texture", 40.0f));
    layout.add (pct (amount, "Amount", 30.0f));
    layout.add (pct (stereo, "Stereo", 0.0f));
    layout.add (pct (feedback, "Feedback", 0.0f));

    // Bitcrush
    layout.add (toggle (bcOn, "Bitcrush On", false));
    layout.add (hzParam (bcFreq, "Crush Frequency", 200.0f, 48000.0f, 8000.0f, 0.0f, 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { bits, 1 }, "Bits", NormalisableRange<float> (1.0f, 16.0f), 8.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));
    layout.add (pct (quantize, "Quantize", 100.0f));
    layout.add (pct (dither, "Dither", 0.0f));
    layout.add (pct (adcQ, "ADC Q", 50.0f));
    layout.add (pct (dacQ, "DAC Q", 50.0f));

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
    layout.add (pct (colMix, "Color Mix", 100.0f));

    // Comp
    layout.add (toggle (compOn, "Comp On", false));
    layout.add (choice (compTarget, "Comp Target", Tables::targets, 0));
    layout.add (choice (compMode, "Comp Mode", Tables::compModes, 0));
    layout.add (pct (ottDepth, "OTT Depth", 60.0f));
    layout.add (pct (ottTime, "OTT Time", 50.0f));
    layout.add (pct (ottUp, "OTT Up", 100.0f));
    layout.add (pct (ottDown, "OTT Down", 100.0f));
    layout.add (dbParam (glueThresh, "Glue Threshold", -40.0f, 0.0f, -12.0f));
    layout.add (choice (glueRatio, "Glue Ratio", Tables::glueRatios, 1));
    layout.add (msParam (glueAttack, "Glue Attack", 0.1f, 30.0f, 3.0f, 0.35f));
    layout.add (msParam (glueRelease, "Glue Release", 50.0f, 1200.0f, 300.0f, 0.5f));
    layout.add (pct (compMix, "Comp Mix", 100.0f));

    // Tremolo
    layout.add (toggle (trOn, "Tremolo On", false));
    layout.add (choice (trTarget, "Tremolo Target", Tables::targets, 0));
    layout.add (choice (trShape, "Tremolo Shape", Tables::trShapes, 0));
    layout.add (pct (trDepth, "Tremolo Depth", 100.0f));
    layout.add (choice (trRateMode, "Tremolo Rate Mode", Tables::rateModes, 0));
    layout.add (choice (trRate, "Tremolo Rate", Tables::noteNames, 6));
    layout.add (msParam (trMs, "Tremolo Time", 10.0f, 4000.0f, 250.0f, 0.3f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { trPan, 1 }, "Tremolo Stereo", NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
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

    // Filters
    for (const bool post : { false, true })
    {
        const String n = post ? "Post " : "Pre ";
        layout.add (toggle (filterId (post, fOn), n + "Filter On", false));
        layout.add (choice (filterId (post, fTarget), n + "Filter Target", Tables::fxTargets, 0));
        layout.add (hzParam (filterId (post, fLc), n + "Low Cut", 20.0f, 20000.0f, 20.0f, 20.0f, 0.0f));
        layout.add (hzParam (filterId (post, fHc), n + "High Cut", 20.0f, 20000.0f, 20000.0f, 0.0f, 20000.0f));
        layout.add (choice (filterId (post, fLcSlope), n + "Low Cut Slope", Tables::slopes, 1));
        layout.add (choice (filterId (post, fHcSlope), n + "High Cut Slope", Tables::slopes, 1));
        layout.add (hzParam (filterId (post, fPkFreq), n + "Peak Frequency", 20.0f, 20000.0f, 1000.0f, 0.0f, 0.0f));
        layout.add (dbParam (filterId (post, fPkGain), n + "Peak Gain", -18.0f, 18.0f, 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { filterId (post, fPkQ), 1 }, n + "Peak Width",
                    NormalisableRange<float> (0.3f, 10.0f, 0.0f, 0.4f), 1.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "Q " + String (v, 2); })));
    }

    return layout;
}

void CrumbleProcessor::bindFilter (FilterParams& f, bool post)
{
    using namespace ParamIDs;
    f.on = raw (filterId (post, fOn));         f.target = raw (filterId (post, fTarget));
    f.lc = raw (filterId (post, fLc));         f.hc = raw (filterId (post, fHc));
    f.lcSlope = raw (filterId (post, fLcSlope)); f.hcSlope = raw (filterId (post, fHcSlope));
    f.pkF = raw (filterId (post, fPkFreq));    f.pkG = raw (filterId (post, fPkGain));
    f.pkQ = raw (filterId (post, fPkQ));
}

CrumbleProcessor::CrumbleProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    using namespace ParamIDs;
    pInput = raw (input);       pOutput = raw (output);     pMix = raw (mix);
    pAutoGain = raw (autoGain); pOrder = raw (order);       pChain = raw (chain);
    pCrOn = raw (crOn);         pMode = raw (mode);         pFreq = raw (freq);       pRate = raw (rate);
    pTexture = raw (texture);   pAmount = raw (amount);     pStereo = raw (stereo);   pFeedback = raw (feedback);
    pBcOn = raw (bcOn);         pBcFreq = raw (bcFreq);     pBits = raw (bits);       pQuantize = raw (quantize);
    pDither = raw (dither);     pAdcQ = raw (adcQ);         pDacQ = raw (dacQ);
    pColOn = raw (colOn);       pColTarget = raw (colTarget); pDriveType = raw (driveType);
    pDrive = raw (drive);       pTone = raw (tone);         pColMix = raw (colMix);
    pCompOn = raw (compOn);     pCompTarget = raw (compTarget); pCompMode = raw (compMode);
    pOttDepth = raw (ottDepth); pOttTime = raw (ottTime);   pOttUp = raw (ottUp);     pOttDown = raw (ottDown);
    pGlueThresh = raw (glueThresh); pGlueRatio = raw (glueRatio); pGlueAttack = raw (glueAttack);
    pGlueRelease = raw (glueRelease); pCompMix = raw (compMix);
    pTrOn = raw (trOn);         pTrTarget = raw (trTarget); pTrShape = raw (trShape); pTrDepth = raw (trDepth);
    pTrRateMode = raw (trRateMode); pTrRate = raw (trRate); pTrMs = raw (trMs);       pTrPan = raw (trPan);
    pFolOn = raw (folOn);       pFolThresh = raw (folThresh); pFolMode = raw (folMode);
    pClipOn = raw (clipOn);     pClipMode = raw (clipMode); pCeiling = raw (ceiling);
    bindFilter (preP, false);
    bindFilter (postP, true);
}

bool CrumbleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

CrumbleProcessor::Routing CrumbleProcessor::currentRouting()
{
    // Wet / FX only mean something when there is an effect to work on
    const bool fx = pCrOn->load() > 0.5f || pBcOn->load() > 0.5f;
    Routing r;
    r.bitFirst = pOrder->load() > 0.5f;
    r.chain = jlimit (0, 5, roundToInt (pChain->load()));
    r.colWet  = fx && roundToInt (pColTarget->load()) == 0;
    r.compWet = fx && roundToInt (pCompTarget->load()) == 0;
    r.trWet   = fx && roundToInt (pTrTarget->load()) == 0;
    r.preFx   = fx && roundToInt (preP.target->load()) == 0;
    r.postFx  = fx && roundToInt (postP.target->load()) == 0;
    return r;
}

void CrumbleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    currentSampleRate = sampleRate;
    const dsp::ProcessSpec spec { sr, (uint32) jmax (1, samplesPerBlock), 2 };

    adc.prepare (spec, dsp::StateVariableTPTFilterType::lowpass);
    dac.prepare (spec, dsp::StateVariableTPTFilterType::lowpass);
    ott.prepare (spec);
    color.reset();
    glue.reset();
    agColor.prepare (sr, 0.3, 0.005, 0.15, 4.0f);    // saturation: turn down fast when Drive jumps
    agComp.prepare (sr, 0.8, 0.3, 0.3, 10.0f);       // compression pumps, so match slowly and evenly
    preFilter.prepare (spec, preP.lc->load(), preP.hc->load(), preP.pkF->load(), preP.pkG->load(), preP.pkQ->load());
    postFilter.prepare (spec, postP.lc->load(), postP.hc->load(), postP.pkF->load(), postP.pkG->load(), postP.pkQ->load());

    const int delaySize = (int) ((maxDepthSeconds + 0.002) * sr) + 16;
    for (int ch = 0; ch < 2; ++ch)
    {
        crumbleDelay[ch].prepare (delaySize);
        alignDry[ch].prepare (delaySize);
        alignSrc[ch].prepare (delaySize);
        fbState[ch] = fbHp[ch] = hold[ch] = 0.0f;
        trGain[ch] = 1.0f;
    }
    for (auto& f : noiseFilt)
        f.reset();
    lfoPhase = holdPhase = trPhase = 0.0;
    folEnv = 0.0f;
    folGain = 1.0f;
    applied = pending = currentRouting();
    routeGain = 1.0f;


    auto init = [this] (SmoothedValue<float>& s, float seconds, float value)
    {
        s.reset (sr, seconds);
        s.setCurrentAndTargetValue (value);
    };
    auto on = [] (std::atomic<float>* p) { return p->load() > 0.5f ? 1.0f : 0.0f; };
    const float amount = pAmount->load() / 100.0f;
    init (inGainS, 0.05f, dbToGain (pInput->load()));
    init (outGainS, 0.05f, dbToGain (pOutput->load()));
    init (mixS, 0.05f, pMix->load() / 100.0f);
    init (depthS, 0.05f, amount * amount * (float) (maxDepthSeconds * sr));
    init (stereoS, 0.05f, pStereo->load() / 100.0f);
    init (feedbackS, 0.05f, pFeedback->load() / 100.0f);
    init (driveS, 0.05f, pDrive->load() / 100.0f);
    init (toneS, 0.05f, pTone->load() / 100.0f);
    init (colMixS, 0.05f, pColMix->load() / 100.0f);
    init (compMixS, 0.05f, pCompMix->load() / 100.0f);
    init (ottDepthS, 0.05f, pOttDepth->load() / 100.0f);
    init (trDepthS, 0.05f, pTrDepth->load() / 100.0f);
    init (crFade, 0.02f, on (pCrOn));
    init (bcFade, 0.02f, on (pBcOn));
    init (colFade, 0.02f, on (pColOn));
    init (compFade, 0.02f, on (pCompOn));
    init (trFade, 0.02f, on (pTrOn));
    init (folFade, 0.02f, on (pFolOn));
    init (clipFade, 0.02f, on (pClipOn));
    init (preFade, 0.02f, on (preP.on));
    init (postFade, 0.02f, on (postP.on));

    bucketLen = jmax (1, roundToInt (sr * 0.001));
    bucketPos = 0;
    bucketHi = bucketLo = bucketIn = bucketPre = 0.0f;
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
    return std::tanh (noiseFilt[source].process (0, x) * noiseGain);
}

// Modulated micro delay with optional feedback (metallic resonance)
float CrumbleProcessor::crumbleSample (int ch, float x, float depth, float mod, float feedback)
{
    const float fbIn = feedback > 0.0005f ? std::tanh (fbHp[ch]) * feedback * 0.75f : 0.0f;
    crumbleDelay[ch].push (x + fbIn);
    const float y = crumbleDelay[ch].read (depth * (0.5f + 0.5f * mod));
    const float fbTap = crumbleDelay[ch].read (depth * 0.5f + (float) (0.0012 * sr));
    fbState[ch] += 0.03f * (fbTap - fbState[ch]);
    fbHp[ch] = fbTap - fbState[ch];
    return y * (1.0f - 0.35f * feedback);
}

float CrumbleProcessor::bitcrushSample (int ch, float x, bool newHold, float levels, float quantize, float dither,
                                        float adcQ, float dacQ)
{
    const float pre = x + (adc.process (ch, x) - x) * adcQ;
    if (newHold)
        hold[ch] = pre;
    float y = hold[ch];
    const float d = dither * (rng.nextFloat() - rng.nextFloat()) / levels;
    const float q = std::round ((y + d) * levels) / levels;
    y += (q - y) * quantize;
    return y + (dac.process (ch, y) - y) * dacQ;
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

float CrumbleProcessor::clipSample (float x, bool hard, float c)
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

// One of the three movable blocks, on both channels: 0 Color, 1 Comp, 2 Tremolo
void CrumbleProcessor::runStage (int stage, float* v, int numCh)
{
    if (stage == 0)
    {
        if (st.colF <= 0.0001f)
            return;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = v[ch];
            const float y = color.process (ch, x, st.driveType, st.drive, st.tone);
            agColor.measure (ch, x, y);
            v[ch] = x + (y * agColor.gain - x) * st.colMix * st.colF;
        }
        agColor.update (st.autoGain && st.drive > 0.0005f, numCh);
    }
    else if (stage == 1)
    {
        if (st.compF <= 0.0001f)
            return;
        float y[2] { v[0], numCh > 1 ? v[1] : 0.0f };
        if (st.compMode == 0)
        {
            for (int ch = 0; ch < numCh; ++ch)
                y[ch] = ott.process (ch, v[ch], st.ottDepth, st.ottUp, st.ottDown);
        }
        else
        {
            glue.process (y, numCh, st.glueThresh, st.glueRatio);
        }
        for (int ch = 0; ch < numCh; ++ch)
        {
            agComp.measure (ch, v[ch], y[ch]);
            v[ch] += (y[ch] * agComp.gain - v[ch]) * st.compMix * st.compF;
        }
        agComp.update (st.autoGain, numCh);
    }
    else
    {
        for (int ch = 0; ch < numCh; ++ch)
            v[ch] *= st.trGain[ch];
    }
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
    const int trShape = roundToInt (pTrShape->load());
    const float trPan = pTrPan->load() / 100.0f;
    const bool folBelow = roundToInt (pFolMode->load()) == 1;
    const float folThresh = pFolThresh->load();
    const bool clipHard = roundToInt (pClipMode->load()) == 1;
    const float ceiling = dbToGain (pCeiling->load());

    st.driveType = roundToInt (pDriveType->load());
    st.compMode = roundToInt (pCompMode->load());
    st.autoGain = pAutoGain->load() > 0.5f;
    st.ottUp = pOttUp->load() / 100.0f;
    st.ottDown = pOttDown->load() / 100.0f;
    st.glueThresh = pGlueThresh->load();
    st.glueRatio = Tables::glueRatioValues[jlimit (0, 2, roundToInt (pGlueRatio->load()))];
    ott.setTime (sr, pOttTime->load() / 100.0f);
    glue.setup (sr, pGlueAttack->load(), pGlueRelease->load());
    color.setup (sr, pDrive->load() / 100.0f);

    updateNoiseFilters (pFreq->load(), pTexture->load() / 100.0f);

    auto setFilter = [] (cdsp::FilterBank& fb, const FilterParams& p)
    {
        fb.setTargets (p.lc->load(), p.hc->load(), roundToInt (p.lcSlope->load()), roundToInt (p.hcSlope->load()),
                       p.pkF->load(), p.pkG->load(), p.pkQ->load());
    };
    setFilter (preFilter, preP);
    setFilter (postFilter, postP);

    // Bitcrush settings
    const float bcHz = jlimit (50.0f, (float) sr, pBcFreq->load());
    adc.set (jlimit (20.0f, (float) (0.45 * sr), bcHz * 0.45f), 1);
    dac.set (jlimit (20.0f, (float) (0.45 * sr), bcHz * 0.45f), 1);
    const double holdInc = bcHz / sr;
    const float levels = std::pow (2.0f, pBits->load() - 1.0f);
    const float quantize = pQuantize->load() / 100.0f;
    const float dither = pDither->load() / 100.0f;
    const float adcQ = pAdcQ->load() / 100.0f;
    const float dacQ = pDacQ->load() / 100.0f;

    // Smoothing targets
    auto on = [] (std::atomic<float>* p) { return p->load() > 0.5f ? 1.0f : 0.0f; };
    inGainS.setTargetValue (dbToGain (pInput->load()));
    outGainS.setTargetValue (dbToGain (pOutput->load()));
    mixS.setTargetValue (pMix->load() / 100.0f);
    depthS.setTargetValue (amount * amount * (float) (maxDepthSeconds * sr));
    stereoS.setTargetValue (pStereo->load() / 100.0f);
    feedbackS.setTargetValue (pFeedback->load() / 100.0f);
    driveS.setTargetValue (pDrive->load() / 100.0f);
    toneS.setTargetValue (pTone->load() / 100.0f);
    colMixS.setTargetValue (pColMix->load() / 100.0f);
    compMixS.setTargetValue (pCompMix->load() / 100.0f);
    ottDepthS.setTargetValue (pOttDepth->load() / 100.0f);
    trDepthS.setTargetValue (pTrDepth->load() / 100.0f);
    crFade.setTargetValue (on (pCrOn));
    bcFade.setTargetValue (on (pBcOn));
    colFade.setTargetValue (on (pColOn));
    compFade.setTargetValue (on (pCompOn));
    trFade.setTargetValue (on (pTrOn));
    folFade.setTargetValue (on (pFolOn));
    clipFade.setTargetValue (on (pClipOn));
    preFade.setTargetValue (on (preP.on));
    postFade.setTargetValue (on (postP.on));

    pending = currentRouting();

    // ---- Tempo ----
    double bpm = 120.0, ppq = 0.0;
    bool playing = false, hasPpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                bpm = *b;
            if (auto q = pos->getPpqPosition())
            {
                ppq = *q;
                hasPpq = true;
            }
            playing = pos->getIsPlaying();
        }
    bpm = jlimit (20.0, 999.0, bpm);

    // Beat clock for the display: follows the host while playing, runs on its own otherwise
    if (playing && hasPpq)
        beatClock = ppq;
    // The bucket that is being filled now started bucketPos samples ago
    scope.beatCount.store (scope.writeCount.load (std::memory_order_relaxed), std::memory_order_relaxed);
    scope.beatAt.store (beatClock - (double) bucketPos / sr * bpm / 60.0, std::memory_order_relaxed);
    scope.bpm.store (bpm, std::memory_order_relaxed);
    beatClock += (double) numSamples / sr * bpm / 60.0;

    const double twoPi = MathConstants<double>::twoPi;
    double lfoInc = twoPi * (double) pFreq->load() / sr;
    if (mode == 1)
    {
        const double beats = Tables::noteBeats[jlimit (0, Tables::noteNames.size() - 1, roundToInt (pRate->load()))];
        lfoInc = twoPi * (bpm / 60.0 / beats) / sr;
        if (playing && hasPpq)
        {
            const double cycles = ppq / beats;
            lfoPhase = twoPi * (cycles - std::floor (cycles));
        }
    }
    double trInc;
    if (roundToInt (pTrRateMode->load()) == 0)
    {
        const double trBeats = Tables::noteBeats[jlimit (0, Tables::noteNames.size() - 1, roundToInt (pTrRate->load()))];
        trInc = bpm / 60.0 / trBeats / sr;
        if (playing && hasPpq)
        {
            const double cycles = ppq / trBeats;
            trPhase = cycles - std::floor (cycles);
        }
    }
    else
    {
        trInc = 1.0 / (jmax (1.0f, pTrMs->load()) * 0.001 * sr);
    }

    const float folAtt = cdsp::coefFor (0.001, sr);
    const float folRel = cdsp::coefFor (0.100, sr);
    const float folSmooth = cdsp::coefFor (0.010, sr);
    const float trSmooth = cdsp::coefFor (0.0015, sr);
    const float routeStep = 1.0f / (0.005f * (float) sr);

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    float peakIn[2] {}, peakOut[2] {};
    int specPos = spectrum.writePos.load (std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        const float inG = inGainS.getNextValue(), outG = outGainS.getNextValue(), mix = mixS.getNextValue();
        const float depth = depthS.getNextValue(), sw = stereoS.getNextValue(), fb = feedbackS.getNextValue();
        st.drive = driveS.getNextValue();
        st.tone = toneS.getNextValue();
        st.colMix = colMixS.getNextValue();
        st.compMix = compMixS.getNextValue();
        st.ottDepth = ottDepthS.getNextValue();
        const float trDepth = trDepthS.getNextValue();
        const float crF = crFade.getNextValue(), bcF = bcFade.getNextValue();
        st.colF = colFade.getNextValue();
        st.compF = compFade.getNextValue();
        const float trF = trFade.getNextValue(), folF = folFade.getNextValue(), clipF = clipFade.getNextValue();
        const float preF = preFade.getNextValue(), postF = postFade.getNextValue();
        preFilter.tick();
        postFilter.tick();

        // Routing changes: dip the output, switch at the bottom, come back
        if (pending != applied)
        {
            routeGain -= routeStep;
            if (routeGain <= 0.0f)
            {
                routeGain = 0.0f;
                applied = pending;
            }
        }
        else
        {
            routeGain = jmin (1.0f, routeGain + routeStep);
        }

        // Crumble modulation
        float mod[2] { 0.0f, 0.0f };
        if (mode != 2)
        {
            mod[0] = (float) std::sin (lfoPhase);
            mod[1] = (float) std::sin (lfoPhase + sw * MathConstants<double>::pi);
            lfoPhase += lfoInc;
            if (lfoPhase >= twoPi)
                lfoPhase -= twoPi;
        }
        else
        {
            const float shared = filteredNoise (0);
            if (numCh > 1 && sw > 0.001f)
            {
                const float norm = 1.0f / std::sqrt ((1.0f - sw) * (1.0f - sw) + sw * sw);
                mod[0] = ((1.0f - sw) * shared + sw * filteredNoise (1)) * norm;
                mod[1] = ((1.0f - sw) * shared + sw * filteredNoise (2)) * norm;
            }
            else
            {
                mod[0] = mod[1] = shared;
            }
        }

        // Bitcrush sample-and-hold clock
        holdPhase += holdInc;
        const bool newHold = holdPhase >= 1.0;
        if (newHold)
            holdPhase -= std::floor (holdPhase);

        // Tremolo
        {
            double pR = trPhase + 0.5 * trPan;
            pR -= std::floor (pR);
            const double phases[2] = { trPhase, pR };
            for (int ch = 0; ch < 2; ++ch)
            {
                const float target = 1.0f - trDepth * trF * (1.0f - tremoloShape (trShape, phases[ch]));
                trGain[ch] += trSmooth * (target - trGain[ch]);
                st.trGain[ch] = trGain[ch];
            }
            trPhase += trInc;
            if (trPhase >= 1.0)
                trPhase -= std::floor (trPhase);
        }

        // Input, Follow detector
        float x[2] { 0.0f, 0.0f };
        for (int ch = 0; ch < numCh; ++ch)
        {
            x[ch] = data[ch][i] * inG;
            peakIn[ch] = jmax (peakIn[ch], std::abs (x[ch]));
        }
        const float level = numCh > 1 ? jmax (std::abs (x[0]), std::abs (x[1])) : std::abs (x[0]);
        folEnv += (level > folEnv ? folAtt : folRel) * (level - folEnv);
        float folTarget = jlimit (0.0f, 1.0f, (gainToDb (folEnv) - (folThresh - 3.0f)) / 6.0f);
        if (folBelow)
            folTarget = 1.0f - folTarget;
        folGain += folSmooth * (folTarget - folGain);
        const float follow = 1.0f + (folGain - 1.0f) * folF;
        const float monoIn = numCh > 1 ? 0.5f * (x[0] + x[1]) : x[0];

        // PRE filter: on what goes into the effect (FX) or on everything (All)
        float src[2] { x[0], x[1] };
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float f = preFilter.process (ch, x[ch]);
            const float filtered = x[ch] + (f - x[ch]) * preF;
            if (applied.preFx)
                src[ch] = filtered;
            else
                x[ch] = src[ch] = filtered;
        }

        // Crumble and Bitcrush; the effect is the difference to the clean source
        const float crDepth = depth * crF;
        float dry[2] {}, eff[2] {};
        for (int ch = 0; ch < numCh; ++ch)
        {
            alignDry[ch].push (x[ch]);
            alignSrc[ch].push (src[ch]);
            dry[ch] = alignDry[ch].read (crDepth * 0.5f);
            const float ref = alignSrc[ch].read (crDepth * 0.5f);

            float w = src[ch];
            auto doCrumble = [&]
            {
                const float y = crumbleSample (ch, w, crDepth, mod[ch], fb * crF);
                w += (y - w) * crF;
            };
            auto doBitcrush = [&]
            {
                if (bcF > 0.0001f)
                {
                    const float y = bitcrushSample (ch, w, newHold, levels, quantize, dither, adcQ, dacQ);
                    w += (y - w) * bcF;
                }
            };
            if (applied.bitFirst) { doBitcrush(); doCrumble(); }
            else                  { doCrumble(); doBitcrush(); }
            eff[ch] = w - ref;
        }

        // Blocks set to Wet, in the chosen order
        const int* order = Tables::chainOrders[applied.chain];
        const bool wet[3] = { applied.colWet, applied.compWet, applied.trWet };
        for (int k = 0; k < 3; ++k)
            if (wet[order[k]])
                runStage (order[k], eff, numCh);

        // POST filter on the effect
        if (applied.postFx)
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float f = postFilter.process (ch, eff[ch]);
                eff[ch] += (f - eff[ch]) * postF;
            }

        // Follow and Mix
        float out[2] {};
        for (int ch = 0; ch < numCh; ++ch)
            out[ch] = dry[ch] + eff[ch] * follow * mix;

        // Blocks set to All
        for (int k = 0; k < 3; ++k)
            if (! wet[order[k]])
                runStage (order[k], out, numCh);

        // POST filter on everything
        if (! applied.postFx)
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float f = postFilter.process (ch, out[ch]);
                out[ch] += (f - out[ch]) * postF;
            }

        // Output and Clip
        float preClipPeak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float o = out[ch] * outG * routeGain;
            preClipPeak = jmax (preClipPeak, std::abs (o));
            if (clipF > 0.0001f)
                o += (clipSample (o, clipHard, ceiling) - o) * clipF;
            data[ch][i] = o;
            peakOut[ch] = jmax (peakOut[ch], std::abs (o));
        }

        // Display data
        const float grNow = (st.compMode == 1 && st.compF > 0.0001f) ? glue.grDb : 0.0f;
        const float monoOut = numCh > 1 ? 0.5f * (data[0][i] + data[1][i]) : data[0][i];
        spectrum.in[specPos] = monoIn;
        spectrum.out[specPos] = monoOut;
        specPos = (specPos + 1) & (Spectrum::size - 1);

        if (bucketPos == 0)
        {
            bucketHi = bucketLo = monoOut;
            bucketIn = std::abs (monoIn);
            bucketPre = preClipPeak;
            bucketGr = grNow;
        }
        else
        {
            bucketHi = jmax (bucketHi, monoOut);
            bucketLo = jmin (bucketLo, monoOut);
            bucketIn = jmax (bucketIn, std::abs (monoIn));
            bucketPre = jmax (bucketPre, preClipPeak);
            bucketGr = jmin (bucketGr, grNow);
        }
        if (++bucketPos >= bucketLen)
        {
            const int count = scope.writeCount.load (std::memory_order_relaxed);
            const int idx = count & (Scope::size - 1);
            scope.hi[idx] = bucketHi;
            scope.lo[idx] = bucketLo;
            scope.in[idx] = bucketIn;
            scope.preClip[idx] = bucketPre;
            scope.gr[idx] = bucketGr;
            scope.writeCount.store (count + 1, std::memory_order_release);
            bucketPos = 0;
        }
    }
    spectrum.writePos.store (specPos, std::memory_order_release);

    // Compressor meters
    {
        const bool active = st.compF > 0.0001f;
        compActive.store (active, std::memory_order_relaxed);
        const bool isGlue = active && st.compMode == 1, isOtt = active && st.compMode == 0;
        glueGrDb.store (isGlue ? glue.grDb : 0.0f, std::memory_order_relaxed);
        glueLevelDb.store (isGlue ? gainToDb (glue.env) : -100.0f, std::memory_order_relaxed);
        for (int b = 0; b < 3; ++b)
        {
            float g = 0.0f, e = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                g += ott.gdb[ch][b] / (float) numCh;
                e = jmax (e, ott.env[ch][b]);
            }
            ottGainDb[b].store (isOtt ? g * st.ottDepth : 0.0f, std::memory_order_relaxed);
            ottLevelDb[b].store (isOtt ? gainToDb (e) : -100.0f, std::memory_order_relaxed);
        }
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        const float pi = ch < numCh ? peakIn[ch] : peakIn[0];
        const float po = ch < numCh ? peakOut[ch] : peakOut[0];
        if (pi > inPeak[ch].load (std::memory_order_relaxed))  inPeak[ch].store (pi, std::memory_order_relaxed);
        if (po > outPeak[ch].load (std::memory_order_relaxed)) outPeak[ch].store (po, std::memory_order_relaxed);
    }
}

void CrumbleProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    if (editorW.load() > 0)
    {
        state.setProperty ("uiW", editorW.load(), nullptr);
        state.setProperty ("uiH", editorH.load(), nullptr);
    }
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
            if ((int) tree.getProperty ("uiW", 0) > 0)
            {
                editorW.store ((int) tree.getProperty ("uiW"));
                editorH.store ((int) tree.getProperty ("uiH"));
            }
            tree.removeProperty ("uiW", nullptr);
            tree.removeProperty ("uiH", nullptr);
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
