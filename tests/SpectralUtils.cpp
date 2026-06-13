#include "SpectralUtils.h"

#include <cmath>
#include <cstdlib>

namespace morphium::test
{
namespace
{
    juce::File testsDir()
    {
        return juce::File (MORPHIUM_TESTS_DIR);
    }

    float toDb (float linear) noexcept
    {
        return juce::Decibels::gainToDecibels (linear, -300.0f);
    }

    // Parabolic interpolation around bin k using dB magnitudes. Returns the
    // fractional bin offset in [-0.5, 0.5].
    float parabolicOffset (const std::vector<float>& mag, int k)
    {
        if (k <= 0 || k >= (int) mag.size() - 1)
            return 0.0f;

        const float a = toDb (mag[(size_t) k - 1]);
        const float b = toDb (mag[(size_t) k]);
        const float c = toDb (mag[(size_t) k + 1]);
        const float denom = a - 2.0f * b + c;

        if (std::abs (denom) < 1.0e-9f)
            return 0.0f;

        return juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / denom);
    }
}

// ---------------------------------------------------------------------- render

juce::AudioBuffer<float> renderProcessor (juce::AudioProcessor& processor,
                                          const std::vector<MidiEvent>& events,
                                          int totalSamples, double sampleRate, int blockSize)
{
    processor.setNonRealtime (true);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> out (2, totalSamples);
    out.clear();

    juce::AudioBuffer<float> chunk (2, blockSize);
    juce::MidiBuffer midi;

    for (int pos = 0; pos < totalSamples; pos += blockSize)
    {
        midi.clear();
        for (const auto& event : events)
            if (event.sampleIndex >= pos && event.sampleIndex < pos + blockSize)
                midi.addEvent (event.message, event.sampleIndex - pos);

        chunk.clear();
        processor.processBlock (chunk, midi);

        const int n = juce::jmin (blockSize, totalSamples - pos);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, chunk, ch, 0, n);
    }

    processor.releaseResources();
    return out;
}

juce::AudioBuffer<float> renderSingleNote (juce::AudioProcessor& processor,
                                           int midiNote, float velocity,
                                           int noteSamples, int totalSamples,
                                           double sampleRate, int blockSize)
{
    const std::vector<MidiEvent> events {
        { 0,           juce::MidiMessage::noteOn  (1, midiNote, velocity) },
        { noteSamples, juce::MidiMessage::noteOff (1, midiNote) },
    };
    return renderProcessor (processor, events, totalSamples, sampleRate, blockSize);
}

// -------------------------------------------------------------------- spectrum

Spectrum analyse (const float* samples, int numSamples, double sampleRate, int fftOrder)
{
    const int fftSize = 1 << fftOrder;

    std::vector<float> data ((size_t) fftSize * 2, 0.0f);
    const int n = juce::jmin (numSamples, fftSize);
    std::copy (samples, samples + n, data.begin());

    juce::dsp::WindowingFunction<float> window ((size_t) n,
        juce::dsp::WindowingFunction<float>::hann);
    window.multiplyWithWindowingTable (data.data(), (size_t) n);

    juce::dsp::FFT fft (fftOrder);
    fft.performFrequencyOnlyForwardTransform (data.data());

    Spectrum spectrum;
    spectrum.sampleRate = sampleRate;
    spectrum.fftSize = fftSize;
    spectrum.magnitude.assign (data.begin(), data.begin() + fftSize / 2);
    return spectrum;
}

Spectrum analyseBuffer (const juce::AudioBuffer<float>& buffer, int channel,
                        int startSample, int numSamples, double sampleRate, int fftOrder)
{
    jassert (startSample + numSamples <= buffer.getNumSamples());
    return analyse (buffer.getReadPointer (channel) + startSample, numSamples,
                    sampleRate, fftOrder);
}

std::vector<SpectralPeak> findPeaks (const Spectrum& spectrum, float thresholdDbc,
                                     float minHz, float maxHz)
{
    const auto& mag = spectrum.magnitude;
    const float hzPerBin = spectrum.hzPerBin();

    float maxMag = 0.0f;
    for (float m : mag)
        maxMag = juce::jmax (maxMag, m);

    std::vector<SpectralPeak> peaks;
    if (maxMag <= 0.0f)
        return peaks;

    const float threshold = maxMag * juce::Decibels::decibelsToGain (thresholdDbc);
    const int loBin = juce::jmax (2, (int) (minHz / hzPerBin));
    const int hiBin = juce::jmin ((int) mag.size() - 3, (int) (maxHz / hzPerBin));

    for (int k = loBin; k <= hiBin; ++k)
    {
        const float m = mag[(size_t) k];
        if (m < threshold)
            continue;

        // Local maximum over a +-2 bin neighbourhood.
        if (m >= mag[(size_t) k - 1] && m > mag[(size_t) k + 1]
            && m >= mag[(size_t) k - 2] && m > mag[(size_t) k + 2])
        {
            const float offset = parabolicOffset (mag, k);
            peaks.push_back ({ ((float) k + offset) * hzPerBin,
                               toDb (m) - toDb (maxMag) });
        }
    }

    return peaks;
}

float detectF0 (const Spectrum& spectrum, float minHz, float maxHz)
{
    const auto& mag = spectrum.magnitude;
    const float hzPerBin = spectrum.hzPerBin();
    const int loBin = juce::jmax (1, (int) (minHz / hzPerBin));
    const int hiBin = juce::jmin ((int) mag.size() - 2, (int) (maxHz / hzPerBin));

    int best = loBin;
    for (int k = loBin; k <= hiBin; ++k)
        if (mag[(size_t) k] > mag[(size_t) best])
            best = k;

    return ((float) best + parabolicOffset (mag, best)) * hzPerBin;
}

bool hasPeakNear (const Spectrum& spectrum, float hz, float tolCents, float minDbc)
{
    const float lo = hz * std::pow (2.0f, -tolCents / 1200.0f);
    const float hi = hz * std::pow (2.0f,  tolCents / 1200.0f);

    for (const auto& peak : findPeaks (spectrum, minDbc, lo * 0.8f, hi * 1.25f))
        if (peak.freqHz >= lo && peak.freqHz <= hi && peak.magnitudeDbc >= minDbc)
            return true;

    return false;
}

float nonHarmonicRatioDb (const Spectrum& spectrum, float f0, float relTolerance)
{
    const auto& mag = spectrum.magnitude;
    const float hzPerBin = spectrum.hzPerBin();
    const float nyquist = (float) spectrum.sampleRate * 0.5f;

    const int loBin = (int) (f0 * 0.5f / hzPerBin);
    const int hiBin = juce::jmin ((int) mag.size() - 1,
                                  (int) (nyquist * 0.98f / hzPerBin));

    double harmonicEnergy = 0.0, otherEnergy = 0.0;

    for (int bin = loBin; bin <= hiBin; ++bin)
    {
        const float hz = (float) bin * hzPerBin;
        const int k = juce::jmax (1, (int) std::lround (hz / f0));
        const float window = juce::jmax (2.0f * hzPerBin, (float) k * f0 * relTolerance);
        const double energy = (double) mag[(size_t) bin] * mag[(size_t) bin];

        if (std::abs (hz - (float) k * f0) <= window)
            harmonicEnergy += energy;
        else
            otherEnergy += energy;
    }

    if (harmonicEnergy <= 0.0)
        return 0.0f;

    return 10.0f * (float) std::log10 (juce::jmax (otherEnergy, 1.0e-30) / harmonicEnergy);
}

float bandEnergyDb (const Spectrum& spectrum, float loHz, float hiHz)
{
    const float hzPerBin = spectrum.hzPerBin();
    const int loBin = juce::jmax (0, (int) (loHz / hzPerBin));
    const int hiBin = juce::jmin ((int) spectrum.magnitude.size() - 1, (int) (hiHz / hzPerBin));

    double energy = 0.0;
    for (int bin = loBin; bin <= hiBin; ++bin)
        energy += (double) spectrum.magnitude[(size_t) bin] * spectrum.magnitude[(size_t) bin];

    return 10.0f * (float) std::log10 (juce::jmax (energy, 1.0e-30));
}

float spectralCentroidHz (const Spectrum& spectrum)
{
    const float hzPerBin = spectrum.hzPerBin();
    double weighted = 0.0, total = 0.0;

    for (size_t bin = 1; bin < spectrum.magnitude.size(); ++bin)
    {
        const double m = spectrum.magnitude[bin];
        weighted += m * bin * hzPerBin;
        total += m;
    }

    return total > 0.0 ? (float) (weighted / total) : 0.0f;
}

// -------------------------------------------------------------------- waveform

bool containsNonFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (data[i]))
                return true;
    }
    return false;
}

float peakLinear (const juce::AudioBuffer<float>& buffer)
{
    return buffer.getMagnitude (0, buffer.getNumSamples());
}

float rmsDb (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples < 0)
        numSamples = buffer.getNumSamples() - startSample;

    double sum = 0.0;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float rms = buffer.getRMSLevel (ch, startSample, numSamples);
        sum += (double) rms * rms;
    }

    const float rms = (float) std::sqrt (sum / buffer.getNumChannels());
    return toDb (rms);
}

float dcOffset (const juce::AudioBuffer<float>& buffer, int channel)
{
    const float* data = buffer.getReadPointer (channel);
    double sum = 0.0;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        sum += data[i];
    return (float) (sum / buffer.getNumSamples());
}

float decayTimeSeconds (const float* samples, int numSamples, double sampleRate, float floorDb)
{
    constexpr int hop = 256;
    const int numBlocks = numSamples / hop;
    if (numBlocks < 2)
        return 0.0f;

    std::vector<float> envelope ((size_t) numBlocks, 0.0f);
    for (int b = 0; b < numBlocks; ++b)
    {
        double sum = 0.0;
        for (int i = 0; i < hop; ++i)
        {
            const float s = samples[b * hop + i];
            sum += (double) s * s;
        }
        envelope[(size_t) b] = (float) std::sqrt (sum / hop);
    }

    int peakBlock = 0;
    for (int b = 1; b < numBlocks; ++b)
        if (envelope[(size_t) b] > envelope[(size_t) peakBlock])
            peakBlock = b;

    const float threshold = envelope[(size_t) peakBlock]
                            * juce::Decibels::decibelsToGain (floorDb);
    if (threshold <= 0.0f)
        return 0.0f;

    int lastAbove = peakBlock;
    for (int b = peakBlock; b < numBlocks; ++b)
        if (envelope[(size_t) b] >= threshold)
            lastAbove = b;

    return (float) (lastAbove - peakBlock) * hop / (float) sampleRate;
}

// ------------------------------------------------------------------- artifacts

void writeWavArtifact (const juce::AudioBuffer<float>& buffer,
                       double sampleRate, const juce::String& name)
{
    auto dir = testsDir().getChildFile ("artifacts");
    dir.createDirectory();

    auto file = dir.getChildFile (name + ".wav");
    file.deleteFile();

    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    if (stream == nullptr)
        return;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.get(), sampleRate,
                                (unsigned int) buffer.getNumChannels(), 24, {}, 0));
    if (writer == nullptr)
        return;

    stream.release(); // writer owns the stream now
    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

bool matchesGolden (const std::vector<SpectralPeak>& peaks, const juce::String& name,
                    float freqToleranceHz, float magToleranceDb, juce::String* failureDetails)
{
    auto dir = testsDir().getChildFile ("golden");
    dir.createDirectory();
    auto file = dir.getChildFile (name + ".json");

    const bool regenerate = std::getenv ("MORPHIUM_GENERATE_GOLDEN") != nullptr;

    if (regenerate || ! file.existsAsFile())
    {
        juce::Array<juce::var> list;
        for (const auto& peak : peaks)
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("hz", peak.freqHz);
            obj->setProperty ("dbc", peak.magnitudeDbc);
            list.add (juce::var (obj));
        }
        file.replaceWithText (juce::JSON::toString (juce::var (list)));
        return true;
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    const auto* golden = parsed.getArray();
    if (golden == nullptr)
    {
        if (failureDetails != nullptr)
            *failureDetails = "golden file is not a JSON array: " + file.getFullPathName();
        return false;
    }

    juce::String failures;
    for (const auto& entry : *golden)
    {
        const float hz  = (float) (double) entry.getProperty ("hz", 0.0);
        const float dbc = (float) (double) entry.getProperty ("dbc", 0.0);

        bool matched = false;
        for (const auto& peak : peaks)
        {
            if (std::abs (peak.freqHz - hz) <= freqToleranceHz
                && std::abs (peak.magnitudeDbc - dbc) <= magToleranceDb)
            {
                matched = true;
                break;
            }
        }

        if (! matched)
            failures << "no measured peak matches golden { " << hz << " Hz, "
                     << dbc << " dBc }\n";
    }

    if (failureDetails != nullptr)
        *failureDetails = failures;

    return failures.isEmpty();
}
}
