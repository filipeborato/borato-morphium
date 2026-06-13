#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <vector>

namespace morphium
{
    /**
        A real, band-limited, mip-mapped single-cycle wavetable bank.

        Built once and shared read-only by every voice. It is sample-rate
        independent: each "frame" stores one cycle of a waveform, and the mip
        level caps the harmonic count so playback stays alias-free regardless of
        the played note (the higher the pitch, the fewer harmonics survive).

        A "shape" is a set of `numFrames` morph frames; scanning `pos01` across
        them interpolates between waveforms — this is the wavetable scan that the
        WAVE knob drives. There are several distinct shapes, all electronic
        members of the SYNTH source family.

        Real-time contract: the bank allocates and computes its tables once on
        first use (`get()`); `read()` is allocation- and lock-free.
    */
    class WavetableBank
    {
    public:
        enum class Shape { Harmonic, Pwm, Formant, Metallic, EPiano, Digital, NumShapes };

        static constexpr int tableSize   = 2048;          // power of two (mask wrap)
        static constexpr int numFrames   = 4;             // morph frames per shape
        static constexpr int maxHarmonic = 64;            // richest mip's harmonic cap
        static constexpr int numMips     = 7;             // caps 64,32,16,8,4,2,1

        static const WavetableBank& get()
        {
            static const WavetableBank instance;
            return instance;
        }

        /** One band-limited sample. `pos01` scans the morph frames, `phase01` is
            the cycle phase in [0,1), `mip` is the harmonic-cap level for the pitch. */
        float read (Shape shape, float pos01, double phase01, int mip) const noexcept
        {
            const int s = juce::jlimit (0, (int) Shape::NumShapes - 1, (int) shape);
            const int m = juce::jlimit (0, numMips - 1, mip);

            const float fp = juce::jlimit (0.0f, (float) (numFrames - 1),
                                           pos01 * (float) (numFrames - 1));
            const int   f0 = (int) fp;
            const int   f1 = juce::jmin (f0 + 1, numFrames - 1);
            const float ff = fp - (float) f0;

            const double x  = phase01 * tableSize;
            const int    i0 = ((int) x) & (tableSize - 1);
            const int    i1 = (i0 + 1)  & (tableSize - 1);
            const float  xf = (float) (x - std::floor (x));

            const float* t0 = frameData (s, f0, m);
            const float* t1 = frameData (s, f1, m);

            const float a = t0[i0] + (t0[i1] - t0[i0]) * xf;
            const float b = t1[i0] + (t1[i1] - t1[i0]) * xf;
            return a + (b - a) * ff;
        }

        /** Highest mip (fewest harmonics) needed to stay alias-free at this pitch.
            `phaseInc` is cycles-per-sample (= frequency / sampleRate). */
        static int mipForIncrement (double phaseInc) noexcept
        {
            const int allowed = (phaseInc > 1.0e-9) ? (int) (0.5 / phaseInc) : maxHarmonic;
            int cap = maxHarmonic, mip = 0;
            while (mip < numMips - 1 && cap > juce::jmax (1, allowed))
            {
                cap >>= 1;
                ++mip;
            }
            return mip;
        }

    private:
        std::vector<float> data;   // [shape][frame][mip][tableSize], flattened

        const float* frameData (int shape, int frame, int mip) const noexcept
        {
            const size_t idx = (((size_t) shape * numFrames + frame) * numMips + mip)
                             * (size_t) tableSize;
            return data.data() + idx;
        }

        float* frameDataMut (int shape, int frame, int mip) noexcept
        {
            const size_t idx = (((size_t) shape * numFrames + frame) * numMips + mip)
                             * (size_t) tableSize;
            return data.data() + idx;
        }

        WavetableBank()
        {
            data.assign ((size_t) (int) Shape::NumShapes * numFrames * numMips * tableSize, 0.0f);

            for (int s = 0; s < (int) Shape::NumShapes; ++s)
                for (int f = 0; f < numFrames; ++f)
                    buildFrame ((Shape) s, f);
        }

        // Builds every mip of one (shape, frame) by summing its sine harmonics
        // up to each mip's cap, then normalising so frames match in level.
        void buildFrame (Shape shape, int frame)
        {
            std::array<float, maxHarmonic + 1> amp {};
            harmonicSpectrum (shape, frame, amp);

            int cap = maxHarmonic;
            for (int mip = 0; mip < numMips; ++mip, cap >>= 1)
            {
                float* t = frameDataMut ((int) shape, frame, mip);
                float peak = 1.0e-9f;

                for (int n = 0; n < tableSize; ++n)
                {
                    const double ph = (double) n / (double) tableSize;
                    double v = 0.0;
                    for (int h = 1; h <= cap; ++h)
                        if (amp[(size_t) h] != 0.0f)
                            v += (double) amp[(size_t) h]
                                 * std::sin (juce::MathConstants<double>::twoPi * h * ph);

                    t[n] = (float) v;
                    peak = juce::jmax (peak, std::abs ((float) v));
                }

                const float g = 0.9f / peak;
                for (int n = 0; n < tableSize; ++n)
                    t[n] *= g;
            }
        }

        // The personality of each wavetable: the sine-series amplitude of every
        // harmonic for the given morph frame (0..numFrames-1).
        static void harmonicSpectrum (Shape shape, int frame,
                                      std::array<float, maxHarmonic + 1>& amp) noexcept
        {
            const float t = (float) frame / (float) (numFrames - 1);   // 0..1 morph

            switch (shape)
            {
                case Shape::Harmonic:
                    // sine -> triangle -> saw -> square: a classic richness sweep.
                    switch (frame)
                    {
                        case 0: amp[1] = 1.0f; break;
                        case 1: for (int h = 1; h <= maxHarmonic; h += 2)         // triangle
                                    amp[(size_t) h] = ((h % 4 == 1) ? 1.0f : -1.0f) / (float) (h * h);
                                break;
                        case 2: for (int h = 1; h <= maxHarmonic; ++h)            // saw
                                    amp[(size_t) h] = 1.0f / (float) h;
                                break;
                        default: for (int h = 1; h <= maxHarmonic; h += 2)        // square
                                    amp[(size_t) h] = 1.0f / (float) h;
                                break;
                    }
                    break;

                case Shape::Pwm:
                {
                    // Pulse with a duty cycle that narrows across the frames.
                    const float duty = 0.5f - 0.42f * t;   // 0.50 -> 0.08
                    for (int h = 1; h <= maxHarmonic; ++h)
                        amp[(size_t) h] = std::sin (juce::MathConstants<float>::pi * h * duty)
                                          / (float) h;
                    break;
                }

                case Shape::Formant:
                {
                    // A saw windowed by a resonant peak that climbs the harmonic
                    // series — a vowel-like sweep (A -> E -> I -> O).
                    const float centre = 2.0f + t * 9.0f;  // harmonic 2 -> 11
                    const float width  = 2.5f;
                    for (int h = 1; h <= maxHarmonic; ++h)
                    {
                        const float d = ((float) h - centre) / width;
                        amp[(size_t) h] = (1.0f / (float) h) * (0.25f + 1.6f * std::exp (-0.5f * d * d));
                    }
                    break;
                }

                case Shape::Metallic:
                {
                    // Odd-heavy and progressively brighter: a clangy digital lead.
                    const float bright = 0.3f + 0.7f * t;
                    for (int h = 1; h <= maxHarmonic; ++h)
                    {
                        const float base = (h % 2 == 1) ? 1.0f / std::pow ((float) h, 0.7f)
                                                        : 0.15f / (float) h;
                        amp[(size_t) h] = base * (h <= 4 ? 1.0f : bright);
                    }
                    break;
                }

                case Shape::EPiano:
                {
                    // Fundamental + a soft "tine" cluster that rises across frames.
                    amp[1] = 1.0f;
                    amp[2] = 0.5f;
                    amp[3] = 0.15f;
                    const float tine = 6.0f + t * 9.0f;    // harmonic 6 -> 15
                    for (int h = 1; h <= maxHarmonic; ++h)
                    {
                        const float d = ((float) h - tine) / 1.5f;
                        amp[(size_t) h] += 0.4f * std::exp (-0.5f * d * d);
                    }
                    break;
                }

                case Shape::Digital:
                default:
                {
                    // Fixed pseudo-random harmonic phases per frame: a complex,
                    // PPG-style digital wave that morphs frame to frame.
                    for (int h = 1; h <= maxHarmonic; ++h)
                    {
                        const float w = 0.5f + 0.5f * std::sin ((float) h * 2.39996f
                                                                + (float) frame * 1.7f);
                        amp[(size_t) h] = (1.0f / (float) h) * w;
                    }
                    break;
                }
            }
        }
    };
}
