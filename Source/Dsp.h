#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <complex>
#include <vector>

// Small DSP building blocks used by the processor (and the display curves)
namespace cdsp
{
    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)  { return 20.0f * std::log10 (g + 1.0e-9f); }
    inline float coefFor (double seconds, double sr) { return 1.0f - (float) std::exp (-1.0 / (seconds * sr)); }

    //==========================================================================
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1[2] {}, z2[2] {};

        float process (int ch, float x)
        {
            const float y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

        // RBJ peaking EQ
        static void peakCoefs (double sr, double f0, double q, double gainDb, double c[5])
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, 0.49 * sr, f0) / sr;
            const double alpha = std::sin (w0) / (2.0 * q);
            const double a0 = 1.0 + alpha / A;
            c[0] = (1.0 + alpha * A) / a0;
            c[1] = (-2.0 * std::cos (w0)) / a0;
            c[2] = (1.0 - alpha * A) / a0;
            c[3] = (-2.0 * std::cos (w0)) / a0;
            c[4] = (1.0 - alpha / A) / a0;
        }
        void setPeak (double sr, double f0, double q, double gainDb)
        {
            double c[5];
            peakCoefs (sr, f0, q, gainDb, c);
            b0 = (float) c[0]; b1 = (float) c[1]; b2 = (float) c[2]; a1 = (float) c[3]; a2 = (float) c[4];
        }
        static double peakMagnitude (double sr, double f, double f0, double q, double gainDb)
        {
            double c[5];
            peakCoefs (sr, f0, q, gainDb, c);
            const double w = juce::MathConstants<double>::twoPi * f / sr;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
            return std::abs ((c[0] + c[1] * z1 + c[2] * z2) / (1.0 + c[3] * z1 + c[4] * z2));
        }
    };

    //==========================================================================
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

    //==========================================================================
    // Butterworth high-pass or low-pass, 12 / 24 / 48 dB per octave
    struct CutFilter
    {
        juce::dsp::StateVariableTPTFilter<float> st[4];
        int stages = 2;
        float cutoff = -1.0f;
        int slope = -1;

        void prepare (const juce::dsp::ProcessSpec& spec, juce::dsp::StateVariableTPTFilterType type)
        {
            for (auto& s : st)
            {
                s.prepare (spec);
                s.setType (type);
                s.reset();
            }
            cutoff = -1.0f;
            slope = -1;
        }
        void set (float hz, int slopeIndex)
        {
            slopeIndex = juce::jlimit (0, 2, slopeIndex);
            if (slopeIndex != slope)
            {
                static const float qs[3][4] = { { 0.70711f, 0, 0, 0 }, { 0.54120f, 1.30656f, 0, 0 },
                                                { 0.50980f, 0.60134f, 0.89998f, 2.56292f } };
                stages = slopeIndex == 0 ? 1 : (slopeIndex == 1 ? 2 : 4);
                for (int i = 0; i < stages; ++i)
                    st[i].setResonance (qs[slopeIndex][i]);
                slope = slopeIndex;
            }
            if (std::abs (hz - cutoff) > 0.001f)
            {
                for (auto& s : st)
                    s.setCutoffFrequency (hz);
                cutoff = hz;
            }
        }
        float process (int ch, float x)
        {
            for (int i = 0; i < stages; ++i)
                x = st[i].processSample (ch, x);
            return x;
        }

        // Ideal magnitude, used for the display
        static double magnitude (double f, double fc, int slopeIndex, bool high)
        {
            const int n = slopeIndex == 0 ? 2 : (slopeIndex == 1 ? 4 : 8);
            const double r = high ? fc / f : f / fc;
            return 1.0 / std::sqrt (1.0 + std::pow (r, 2.0 * n));
        }
    };

    //==========================================================================
    // Low cut + high cut + one peak band, with smoothed settings
    struct FilterBank
    {
        CutFilter hp, lp;
        Biquad peak;
        double sr = 44100.0;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lcS, hcS, pkFS;
        juce::SmoothedValue<float> pkGS, pkQS;
        int lcSlope = 1, hcSlope = 1, counter = 0;
        bool lcOn = false, hcOn = false;

        void prepare (const juce::dsp::ProcessSpec& spec, float lc, float hc, float pkF, float pkG, float pkQ)
        {
            sr = spec.sampleRate;
            hp.prepare (spec, juce::dsp::StateVariableTPTFilterType::highpass);
            lp.prepare (spec, juce::dsp::StateVariableTPTFilterType::lowpass);
            peak.reset();
            for (auto* s : { &lcS, &hcS, &pkFS })
                s->reset (sr, 0.04);
            pkGS.reset (sr, 0.04);
            pkQS.reset (sr, 0.04);
            lcS.setCurrentAndTargetValue (clampHz (lc));
            hcS.setCurrentAndTargetValue (clampHz (hc));
            pkFS.setCurrentAndTargetValue (clampHz (pkF));
            pkGS.setCurrentAndTargetValue (pkG);
            pkQS.setCurrentAndTargetValue (pkQ);
            counter = 0;
            updateCoefs();
        }
        float clampHz (float hz) const { return juce::jlimit (20.0f, (float) (0.45 * sr), hz); }

        void setTargets (float lc, float hc, int lcSl, int hcSl, float pkF, float pkG, float pkQ)
        {
            lcOn = lc > 20.05f;
            hcOn = hc < 19990.0f;
            lcSlope = lcSl;
            hcSlope = hcSl;
            lcS.setTargetValue (clampHz (lc));
            hcS.setTargetValue (clampHz (hc));
            pkFS.setTargetValue (clampHz (pkF));
            pkGS.setTargetValue (pkG);
            pkQS.setTargetValue (pkQ);
        }
        void updateCoefs()
        {
            hp.set (lcS.getCurrentValue(), lcSlope);
            lp.set (hcS.getCurrentValue(), hcSlope);
            peak.setPeak (sr, pkFS.getCurrentValue(), pkQS.getCurrentValue(), pkGS.getCurrentValue());
        }
        void tick()   // once per sample frame
        {
            lcS.getNextValue(); hcS.getNextValue(); pkFS.getNextValue(); pkGS.getNextValue(); pkQS.getNextValue();
            if (++counter >= 16)
            {
                counter = 0;
                updateCoefs();
            }
        }
        float process (int ch, float x)
        {
            const float h = hp.process (ch, x);
            if (lcOn) x = h;
            const float l = lp.process (ch, x);
            if (hcOn) x = l;
            return peak.process (ch, x);
        }

        // Total magnitude for the display
        static double magnitude (double sr, double f, float lc, float hc, int lcSl, int hcSl, float pkF, float pkG, float pkQ)
        {
            double m = 1.0;
            if (lc > 20.05f) m *= CutFilter::magnitude (f, lc, lcSl, true);
            if (hc < 19990.0f) m *= CutFilter::magnitude (f, hc, hcSl, false);
            if (std::abs (pkG) > 0.01f) m *= Biquad::peakMagnitude (sr, f, pkF, pkQ, pkG);
            return m;
        }
    };

    //==========================================================================
    // Level matching between a block's input and output
    struct AutoGain
    {
        float in[2] {}, out[2] {};
        float gain = 1.0f;
        float rms = 0.001f, down = 0.01f, up = 0.001f, maxGain = 4.0f;

        void prepare (double sr, double rmsSeconds, double downSeconds, double upSeconds, float maxBoost)
        {
            maxGain = maxBoost;
            rms = coefFor (rmsSeconds, sr);
            down = coefFor (downSeconds, sr);
            up = coefFor (upSeconds, sr);
            in[0] = in[1] = out[0] = out[1] = 0.0f;
            gain = 1.0f;
        }
        void measure (int ch, float x, float y)
        {
            in[ch]  += rms * (x * x - in[ch]);
            out[ch] += rms * (y * y - out[ch]);
        }
        void update (bool active, int numCh)
        {
            float target = 1.0f;
            if (active)
            {
                const float a = in[0] + (numCh > 1 ? in[1] : 0.0f);
                const float b = out[0] + (numCh > 1 ? out[1] : 0.0f);
                target = (a > 1.0e-10f && b > 1.0e-12f) ? juce::jlimit (0.063f, maxGain, std::sqrt (a / b)) : gain;
            }
            gain += (target < gain ? down : up) * (target - gain);
        }
    };

    //==========================================================================
    // Saturation with six curves, tone tilt and DC blocking
    struct Color
    {
        float dcX[2] {}, dcY[2] {}, tapeLp[2] {}, toneLp[2] {};
        float toneCoef = 0.1f, tapeCoef = 0.9f;

        void reset() { for (int c = 0; c < 2; ++c) dcX[c] = dcY[c] = tapeLp[c] = toneLp[c] = 0.0f; }
        void setup (double sr, float drive)
        {
            toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 1000.0f / (float) sr);
            tapeCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin ((float) (0.45 * sr), 18000.0f - drive * 12000.0f) / (float) sr);
        }
        float process (int ch, float x, int type, float drive, float tone)
        {
            const float blend = juce::jmin (1.0f, drive * 10.0f);
            const float g = dbToGain (drive * 30.0f);
            float s = x;
            switch (type)
            {
                case 0: s = std::tanh (g * x); break;                                                    // Soft
                case 1: s = std::tanh (g * x * 0.8f) * 1.1f; tapeLp[ch] += tapeCoef * (s - tapeLp[ch]); s = tapeLp[ch]; break; // Tape
                case 2: s = std::tanh (g * x + 0.35f) - std::tanh (0.35f); break;                         // Tube
                case 3: { const float a = g * x * 1.5f;                                                   // Fuzz
                          s = a > 0.0f ? 1.0f - std::exp (-a) : -0.7f * (1.0f - std::exp (a / 0.7f)); break; }
                case 4: s = juce::jlimit (-1.0f, 1.0f, g * x); break;                                     // Hard
                case 5: s = std::sin (juce::MathConstants<float>::halfPi * x * (1.0f + drive * 8.0f)); break; // Fold
                default: break;
            }
            float y = x + (s - x) * blend;

            const float dc = y - dcX[ch] + 0.995f * dcY[ch];
            dcX[ch] = y;
            dcY[ch] = dc;
            if (type == 2 || type == 3)
                y += (dc - y) * blend;

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
            return y;
        }
    };

    //==========================================================================
    // OTT-style three-band upward + downward compression
    struct Ott
    {
        juce::dsp::LinkwitzRileyFilter<float> x1, x2, ap;
        float env[2][3] {}, gdb[2][3] {};
        float att = 0.01f, rel = 0.001f;

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            for (auto* f : { &x1, &x2, &ap })
            {
                f->prepare (spec);
                f->reset();
            }
            ap.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
            const float hi = juce::jmin (2500.0f, (float) (0.4 * spec.sampleRate));
            x1.setCutoffFrequency (250.0f);
            x2.setCutoffFrequency (hi);
            ap.setCutoffFrequency (hi);
            for (int c = 0; c < 2; ++c)
                for (int b = 0; b < 3; ++b)
                    env[c][b] = gdb[c][b] = 0.0f;
        }
        void setTime (double sr, float time01)
        {
            const double f = std::pow (2.0, (time01 - 0.5) * 4.0);   // 0.25x .. 4x
            att = coefFor (0.002 * f, sr);
            rel = coefFor (0.060 * f, sr);
        }
        float process (int ch, float x, float depth, float up, float down)
        {
            constexpr float dThr = -18.0f, dRatio = 4.0f, uThr = -30.0f, uRatio = 3.0f, upMax = 24.0f, floorDb = -72.0f;
            float s0, r, s1, s2;
            x1.processSample (ch, x, s0, r);
            x2.processSample (ch, r, s1, s2);
            s0 = ap.processSample (ch, s0);
            const float bands[3] = { s0, s1, s2 };
            const float makeup = dbToGain (6.0f * depth);
            float out = x;
            for (int b = 0; b < 3; ++b)
            {
                const float level = std::abs (bands[b]);
                float& e = env[ch][b];
                e += (level > e ? att : rel) * (level - e);
                const float eDb = gainToDb (e);
                float target = 0.0f;
                if (eDb > dThr)
                    target = ((dThr + (eDb - dThr) / dRatio) - eDb) * down;
                else if (eDb < uThr && eDb > floorDb)
                    target = juce::jmin (upMax, (uThr + (eDb - uThr) / uRatio) - eDb) * up;
                float& g = gdb[ch][b];
                g += 0.01f * (target - g);
                out += bands[b] * (dbToGain (g * depth) * makeup - 1.0f);
            }
            return out;
        }
    };

    //==========================================================================
    // Stereo-linked bus compressor with a soft knee
    struct Glue
    {
        float env = 0.0f, att = 0.01f, rel = 0.001f, grDb = 0.0f;

        void reset() { env = 0.0f; grDb = 0.0f; }
        void setup (double sr, float attackMs, float releaseMs)
        {
            att = coefFor (attackMs * 0.001, sr);
            rel = coefFor (releaseMs * 0.001, sr);
        }
        void process (float* v, int numCh, float threshDb, float ratio)
        {
            float level = std::abs (v[0]);
            if (numCh > 1) level = juce::jmax (level, std::abs (v[1]));
            env += (level > env ? att : rel) * (level - env);
            const float over = gainToDb (env) - threshDb;
            constexpr float knee = 6.0f;
            const float slope = 1.0f / ratio - 1.0f;
            if (over <= -knee * 0.5f)      grDb = 0.0f;
            else if (over >= knee * 0.5f)  grDb = over * slope;
            else                           grDb = (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee) * slope;
            const float g = dbToGain (grDb);
            for (int ch = 0; ch < numCh; ++ch)
                v[ch] *= g;
        }
    };
}
