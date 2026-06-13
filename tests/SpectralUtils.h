#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <vector>

/**
    Offline render + spectral analysis helpers shared by every test suite.

    All analysis works on a Spectrum produced by analyse(): a Hann-windowed
    16384-point magnitude FFT. Peak frequencies use parabolic interpolation,
    so they are accurate to a fraction of a bin (~0.3 Hz), which is what lets
    the modal-ratio tests assert in Hz rather than bins.
*/
namespace morphium::test
{
    // ------------------------------------------------------------------ render

    struct MidiEvent
    {
        int sampleIndex = 0;
        juce::MidiMessage message;
    };

    /** Prepares the processor and renders totalSamples offline, injecting the
        given MIDI events at their absolute sample positions. */
    juce::AudioBuffer<float> renderProcessor (juce::AudioProcessor& processor,
                                              const std::vector<MidiEvent>& events,
                                              int totalSamples,
                                              double sampleRate = 44100.0,
                                              int blockSize = 512);

    /** Convenience: one note-on at t=0 and a note-off after noteSamples. */
    juce::AudioBuffer<float> renderSingleNote (juce::AudioProcessor& processor,
                                               int midiNote, float velocity,
                                               int noteSamples, int totalSamples,
                                               double sampleRate = 44100.0,
                                               int blockSize = 512);

    // ---------------------------------------------------------------- spectrum

    struct Spectrum
    {
        std::vector<float> magnitude;   // linear, size fftSize/2
        double sampleRate = 44100.0;
        int fftSize = 0;

        float hzPerBin() const noexcept { return (float) (sampleRate / fftSize); }
    };

    struct SpectralPeak
    {
        float freqHz = 0.0f;
        float magnitudeDbc = 0.0f;      // dB relative to the strongest peak
    };

    /** Hann-windowed magnitude FFT of numSamples (truncated/zero-padded to fftOrder). */
    Spectrum analyse (const float* samples, int numSamples,
                      double sampleRate, int fftOrder = 14);

    Spectrum analyseBuffer (const juce::AudioBuffer<float>& buffer, int channel,
                            int startSample, int numSamples, double sampleRate,
                            int fftOrder = 14);

    /** Local maxima at or above thresholdDbc (relative to the strongest bin),
        parabolic-interpolated, sorted by frequency. */
    std::vector<SpectralPeak> findPeaks (const Spectrum& spectrum,
                                         float thresholdDbc = -60.0f,
                                         float minHz = 20.0f, float maxHz = 20000.0f);

    /** Frequency of the strongest bin inside [minHz, maxHz], parabolic-interpolated. */
    float detectF0 (const Spectrum& spectrum, float minHz, float maxHz);

    /** True if a peak exists within tolCents of hz and at or above minDbc. */
    bool hasPeakNear (const Spectrum& spectrum, float hz,
                      float tolCents = 25.0f, float minDbc = -40.0f);

    /** Energy ratio (dB) of non-harmonic bins vs harmonic bins of f0, scanning
        from 0.5*f0 to 0.98*Nyquist. A bin is harmonic when it falls within
        max(2 bins, k*f0*relTolerance) of the k-th harmonic. The aliasing metric. */
    float nonHarmonicRatioDb (const Spectrum& spectrum, float f0,
                              float relTolerance = 0.01f);

    /** 10*log10 of the summed energy inside [loHz, hiHz]. Relative use only. */
    float bandEnergyDb (const Spectrum& spectrum, float loHz, float hiHz);

    float spectralCentroidHz (const Spectrum& spectrum);

    // --------------------------------------------------------------- waveform

    bool  containsNonFinite (const juce::AudioBuffer<float>& buffer);
    float peakLinear (const juce::AudioBuffer<float>& buffer);
    float rmsDb (const juce::AudioBuffer<float>& buffer, int startSample = 0, int numSamples = -1);
    float dcOffset (const juce::AudioBuffer<float>& buffer, int channel = 0);

    /** Seconds from the envelope peak until the 256-sample RMS envelope last
        sits at or above peak + floorDb. Returns 0 when the signal is silent. */
    float decayTimeSeconds (const float* samples, int numSamples,
                            double sampleRate, float floorDb = -60.0f);

    // -------------------------------------------------------------- artifacts

    /** Writes a 24-bit WAV into tests/artifacts/<name>.wav for human listening. */
    void writeWavArtifact (const juce::AudioBuffer<float>& buffer,
                           double sampleRate, const juce::String& name);

    /** Compares peaks against tests/golden/<name>.json. When the file is
        missing (or MORPHIUM_GENERATE_GOLDEN is set) it writes the snapshot and
        passes. Every golden peak must have a measured match within tolerances. */
    bool matchesGolden (const std::vector<SpectralPeak>& peaks,
                        const juce::String& name,
                        float freqToleranceHz = 5.0f,
                        float magToleranceDb = 4.0f,
                        juce::String* failureDetails = nullptr);
}
