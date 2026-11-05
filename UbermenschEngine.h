#pragma once
// Übermensch DSP engine - pure C++17, no JUCE dependency (so it can be unit-tested offline).
//
// Concept: the engine continuously analyses the input with an 8192-point STFT, picks the
// strongest spectral peaks (partials) and re-synthesises them with a bank of sine oscillators.
// Freezing = keep the oscillator targets; Sustain/Synth = keep updating them while you play.
// "Gliss" slews every oscillator's pitch (and amplitude) towards its new target.

#include <array>
#include <complex>
#include <cstdint>
#include <vector>

namespace ubermensch
{
enum class Mode { Freeze = 0, Sustain = 1, Synth = 2 };

struct Params
{
    Mode  mode      = Mode::Freeze;
    float attackMs  = 40.0f;     // wet envelope rise when engaged
    float decayMs   = 4000.0f;   // wet fall after release; also how long held notes ring in Sustain/Synth
    float glissMs   = 200.0f;    // pitch/amp slew towards new target
    float subLevel  = 0.0f;      // -1 octave layer (Synth mode only), 0..1
    float octLevel  = 0.0f;      // +1 octave layer (Synth mode only), 0..1
    float toneHz    = 12000.0f;  // wet low-pass
    float width     = 0.5f;      // stereo spread of partials 0..1
    float levelDb   = 0.0f;      // wet output gain
};

class Engine
{
public:
    void prepare (double sampleRate);
    void reset();

    void setParams (const Params& p) { params = p; }
    // Call once per block. A false->true change takes a fresh capture.
    void setEngaged (bool shouldEngage);
    // Take a new capture while already engaged (glides to it if Gliss > 0).
    void grab() { grabPending = true; freshOnGrab = false; }

    // Writes WET signal only (replaces contents of outL/outR). inR may equal inL for mono.
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    float getEnvelope() const noexcept { return env; }
    int   getNumActivePartials() const noexcept;

private:
    static constexpr int kMaxPartials = 96;
    static constexpr int kNumOsc      = 128;
    static constexpr int kSubBlock    = 32;
    static constexpr int kLayers      = 3;   // root, sub (0.5x), octave (2x)

    struct Peak { float freq = 0, amp = 0; };

    struct Osc
    {
        float freq = 0, targetFreq = 0;   // Hz
        float amp = 0, targetAmp = 0;
        float gL = 0.7071f, gR = 0.7071f; // pan gains (fixed per slot)
        std::array<std::complex<float>, kLayers> z { std::complex<float> (1.0f, 0.0f),
                                                      std::complex<float> (1.0f, 0.0f),
                                                      std::complex<float> (1.0f, 0.0f) };
    };

    // --- FFT ---
    void fft (std::vector<std::complex<float>>& data) const;
    void analyse();                   // runs once per hop
    void applyTargets (bool fresh, bool tracking = false);
    void droopAll();

    Params params;
    double sr = 48000.0;
    int fftSize = 8192, hop = 1024, hopCounter = 0;
    int ringPos = 0;
    std::vector<float> ring, window, frame;
    std::vector<std::complex<float>> spec, twiddle;
    std::vector<int> bitrev;
    std::vector<float> mag;

    std::array<Peak, kMaxPartials> latest {};
    int numLatest = 0;
    float lastHopRms = 0.0f;
    bool haveAnalysis = false;

    std::array<Osc, kNumOsc> osc {};

    bool engaged = false, grabPending = false, freshOnGrab = false;
    float lastWidth = -1.0f;
    float env = 0.0f;
    float lpL = 0.0f, lpR = 0.0f;

    std::vector<float> tmpL, tmpR;
};
} // namespace ubermensch
