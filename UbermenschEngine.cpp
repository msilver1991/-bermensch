#include "UbermenschEngine.h"

#include <algorithm>
#include <cmath>

namespace ubermensch
{
namespace
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
}

// ------------------------------------------------------------------------------------------
void Engine::prepare (double sampleRate)
{
    sr      = sampleRate;
    fftSize = (sr >= 88200.0) ? 16384 : 8192;
    hop     = fftSize / 8;

    ring.assign ((size_t) fftSize, 0.0f);
    frame.assign ((size_t) fftSize, 0.0f);
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) fftSize);

    spec.assign ((size_t) fftSize, {});
    mag.assign ((size_t) fftSize / 2 + 1, 0.0f);

    twiddle.resize ((size_t) fftSize / 2);
    for (int i = 0; i < fftSize / 2; ++i)
    {
        const double a = -2.0 * 3.14159265358979323846 * (double) i / (double) fftSize;
        twiddle[(size_t) i] = { (float) std::cos (a), (float) std::sin (a) };
    }

    bitrev.resize ((size_t) fftSize);
    int bits = 0;
    while ((1 << bits) < fftSize) ++bits;
    for (int i = 0; i < fftSize; ++i)
    {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        bitrev[(size_t) i] = r;
    }

    tmpL.assign (kSubBlock, 0.0f);
    tmpR.assign (kSubBlock, 0.0f);

    // fixed per-slot pan positions (golden-ratio sequence, -1..1)
    for (int i = 0; i < kNumOsc; ++i)
    {
        float f = (float) i * 0.6180339887f;
        f -= std::floor (f);
        osc[(size_t) i].gL = f * 2.0f - 1.0f;   // temporarily holds base pan, converted in process()
    }
    reset();
}

void Engine::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    ringPos = hopCounter = 0;
    numLatest = 0;
    haveAnalysis = false;
    grabPending = false;
    engaged = false;
    env = 0.0f;
    lpL = lpR = 0.0f;
    lastHopRms = 0.0f;
    for (auto& o : osc)
    {
        o.freq = o.targetFreq = 0.0f;
        o.amp = o.targetAmp = 0.0f;
        for (auto& z : o.z) z = { 1.0f, 0.0f };
    }
    lastWidth = -1.0f;
}

void Engine::setEngaged (bool shouldEngage)
{
    if (shouldEngage && ! engaged)
    {
        grabPending = true;
        freshOnGrab = (env < 0.002f);
    }
    engaged = shouldEngage;
}

int Engine::getNumActivePartials() const noexcept
{
    int n = 0;
    for (auto& o : osc) if (o.targetAmp > 0.0f) ++n;
    return n;
}

// ------------------------------------------------------------------------------------------
void Engine::fft (std::vector<std::complex<float>>& d) const
{
    const int n = fftSize;
    for (int i = 0; i < n; ++i)
    {
        const int j = bitrev[(size_t) i];
        if (j > i) std::swap (d[(size_t) i], d[(size_t) j]);
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len >> 1, step = n / len;
        for (int i = 0; i < n; i += len)
            for (int k = 0; k < half; ++k)
            {
                const auto t = d[(size_t) (i + k + half)] * twiddle[(size_t) (k * step)];
                const auto u = d[(size_t) (i + k)];
                d[(size_t) (i + k)]        = u + t;
                d[(size_t) (i + k + half)] = u - t;
            }
    }
}

void Engine::analyse()
{
    // oldest sample is at ringPos
    for (int i = 0; i < fftSize; ++i)
        spec[(size_t) i] = { ring[(size_t) ((ringPos + i) % fftSize)] * window[(size_t) i], 0.0f };
    fft (spec);

    const int half = fftSize / 2;
    float maxMag = 0.0f;
    for (int k = 0; k <= half; ++k)
    {
        mag[(size_t) k] = std::abs (spec[(size_t) k]);
        maxMag = std::max (maxMag, mag[(size_t) k]);
    }

    // mean-square of the newest hop (used as a gate for tracking modes)
    double acc = 0.0;
    for (int i = 0; i < hop; ++i)
    {
        const float s = ring[(size_t) ((ringPos - 1 - i + fftSize) % fftSize)];
        acc += (double) s * s;
    }
    lastHopRms = (float) std::sqrt (acc / (double) hop);

    // absolute floor: ~ -90 dBFS sinusoid; relative floor: -54 dB under the strongest bin
    const float floorMag = 3.0e-5f * (float) fftSize * 0.25f;
    const float thr = std::max (maxMag * 0.002f, floorMag);

    struct Cand { int k; float m; };
    std::vector<Cand> cands;
    cands.reserve (512);
    const int lo = std::max (2, (int) std::ceil (20.0 * fftSize / sr));
    const int hi = std::min (half - 3, (int) std::floor (0.45 * fftSize));
    for (int k = lo; k <= hi; ++k)
    {
        const float m = mag[(size_t) k];
        if (m > thr && m > mag[(size_t) k - 1] && m >= mag[(size_t) k + 1])
            cands.push_back ({ k, m });
    }
    std::sort (cands.begin(), cands.end(), [] (const Cand& a, const Cand& b) { return a.m > b.m; });
    if (cands.size() > 400) cands.resize (400);

    std::vector<Cand> accepted;
    accepted.reserve (kMaxPartials);
    for (const auto& c : cands)
    {
        bool reject = false;
        for (const auto& a : accepted)
            if (std::abs (a.k - c.k) <= 8 && c.m < 0.04f * a.m) { reject = true; break; } // window sidelobes
        if (reject) continue;
        accepted.push_back (c);
        if ((int) accepted.size() >= kMaxPartials) break;
    }

    numLatest = 0;
    for (const auto& c : accepted)
    {
        const float a = std::log (mag[(size_t) c.k - 1] + 1.0e-12f);
        const float b = std::log (mag[(size_t) c.k]     + 1.0e-12f);
        const float g = std::log (mag[(size_t) c.k + 1] + 1.0e-12f);
        const float denom = a - 2.0f * b + g;
        float delta = (std::abs (denom) > 1.0e-9f) ? 0.5f * (a - g) / denom : 0.0f;
        delta = std::clamp (delta, -0.5f, 0.5f);
        const float peak = std::exp (b - 0.25f * (a - g) * delta);

        Peak p;
        p.freq = ((float) c.k + delta) * (float) (sr / fftSize);
        p.amp  = 4.0f * peak / (float) fftSize;
        latest[(size_t) numLatest++] = p;
    }
    haveAnalysis = true;
}

// ------------------------------------------------------------------------------------------
void Engine::droopAll()
{
    // held notes in Sustain/Synth ring out over `decayMs` (-60 dB)
    const float hopSec = (float) hop / (float) sr;
    const float droop = std::exp (-6.9078f * hopSec / std::max (0.05f, params.decayMs * 0.001f));
    for (auto& o : osc)
    {
        if (o.targetAmp <= 0.0f) continue;
        o.targetAmp *= droop;
        if (o.targetAmp < 1.0e-6f) o.targetAmp = 0.0f;
    }
}

void Engine::applyTargets (bool fresh, bool tracking)
{
    const float hopSec = (float) hop / (float) sr;
    const float droop  = std::exp (-6.9078f * hopSec / std::max (0.05f, params.decayMs * 0.001f));
    if (fresh)
    {
        for (auto& o : osc) { o.amp = o.targetAmp = 0.0f; o.freq = o.targetFreq = 0.0f; }
        for (int i = 0; i < numLatest && i < kNumOsc; ++i)
        {
            auto& o = osc[(size_t) i];
            o.freq = o.targetFreq = latest[(size_t) i].freq;
            o.targetAmp = latest[(size_t) i].amp;
        }
        return;
    }

    const float glissNorm = std::min (1.0f, params.glissMs / 1500.0f);
    const float maxSemis  = (params.mode == Mode::Freeze) ? 0.5f + 23.5f * glissNorm
                                                          : 0.75f + 11.0f * glissNorm;
    std::array<bool, kNumOsc> used {};

    for (int i = 0; i < numLatest; ++i)
    {
        const auto& p = latest[(size_t) i];
        int best = -1;
        float bestDist = maxSemis;
        for (int j = 0; j < kNumOsc; ++j)
        {
            const auto& o = osc[(size_t) j];
            if (used[(size_t) j] || o.targetAmp <= 0.0f || o.targetFreq <= 0.0f) continue;
            const float d = 12.0f * std::abs (std::log2 (p.freq / o.targetFreq));
            if (d < bestDist) { bestDist = d; best = j; }
        }
        if (best >= 0)
        {
            osc[(size_t) best].targetFreq = p.freq;
            osc[(size_t) best].targetAmp  = tracking ? std::max (p.amp, osc[(size_t) best].targetAmp * droop) : p.amp;
            used[(size_t) best] = true;
            continue;
        }
        // need a free (silent) slot, else steal the quietest unused one
        int slot = -1;
        float quietest = 1.0e9f;
        for (int j = 0; j < kNumOsc; ++j)
        {
            if (used[(size_t) j]) continue;
            const auto& o = osc[(size_t) j];
            const float level = o.amp + o.targetAmp;
            if (level < quietest) { quietest = level; slot = j; }
        }
        if (slot < 0) break;
        auto& o = osc[(size_t) slot];
        o.freq = o.targetFreq = p.freq;
        o.amp = 0.0f;
        o.targetAmp = p.amp;
        used[(size_t) slot] = true;
    }
    for (int j = 0; j < kNumOsc; ++j)
    {
        if (used[(size_t) j]) continue;
        auto& o = osc[(size_t) j];
        if (tracking) { o.targetAmp *= droop; if (o.targetAmp < 1.0e-6f) o.targetAmp = 0.0f; }   // old notes keep ringing
        else          o.targetAmp = 0.0f;
    }
}

// ------------------------------------------------------------------------------------------
void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples)
{
    if (params.width != lastWidth)
    {
        lastWidth = params.width;
        for (int i = 0; i < kNumOsc; ++i)
        {
            auto& o = osc[(size_t) i];
            float f = (float) i * 0.6180339887f;
            f -= std::floor (f);
            const float pan = std::clamp ((f * 2.0f - 1.0f) * params.width, -1.0f, 1.0f);
            const float ang = (pan + 1.0f) * kPi * 0.25f;
            o.gL = std::cos (ang);
            o.gR = std::sin (ang);
        }
    }

    const float levelLin = std::pow (10.0f, params.levelDb / 20.0f);
    const float toneCoef = 1.0f - std::exp (-kTwoPi * std::min (params.toneHz, (float) (0.45 * sr)) / (float) sr);
    const bool  synth    = (params.mode == Mode::Synth);
    const std::array<float, kLayers> layerGain { 1.0f, synth ? params.subLevel : 0.0f, synth ? params.octLevel : 0.0f };
    const std::array<float, kLayers> layerRatio { 1.0f, 0.5f, 2.0f };

    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min ({ numSamples - done, kSubBlock, hop - hopCounter });

        // ---- feed analysis ring
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) ringPos] = 0.5f * (inL[done + i] + inR[done + i]);
            ringPos = (ringPos + 1) % fftSize;
        }
        hopCounter += n;

        // ---- envelope for this sub-block
        const float envStart = env;
        {
            const float tauMs = std::max (1.0f, engaged ? params.attackMs : params.decayMs);
            const float c = 1.0f - std::exp (-(float) n / ((float) sr * tauMs * 0.001f / 4.6f));
            env += ((engaged ? 1.0f : 0.0f) - env) * c;
            if (! engaged && env < 1.0e-5f) env = 0.0f;
            if (env > 1.0f) env = 1.0f;
        }
        const float envEnd = env;

        if (envStart == 0.0f && envEnd == 0.0f)
        {
            for (int i = 0; i < n; ++i) { outL[done + i] = 0.0f; outR[done + i] = 0.0f; }
        }
        else
        {
            std::fill (tmpL.begin(), tmpL.begin() + n, 0.0f);
            std::fill (tmpR.begin(), tmpR.begin() + n, 0.0f);

            const float dt = (float) n / (float) sr;
            const float glissSec = params.glissMs * 0.001f;
            const float cF = (glissSec < 0.002f) ? 1.0f : 1.0f - std::exp (-dt / (glissSec / 3.0f));
            const float tauA = std::max (0.015f, 0.25f * glissSec);
            const float cA = 1.0f - std::exp (-dt / tauA);

            for (auto& o : osc)
            {
                if (o.amp <= 0.0f && o.targetAmp <= 0.0f) continue;

                if (o.freq > 0.0f && o.targetFreq > 0.0f && o.freq != o.targetFreq)
                {
                    o.freq *= std::pow (o.targetFreq / o.freq, cF);
                    if (std::abs (o.freq - o.targetFreq) < 0.001f * o.targetFreq) o.freq = o.targetFreq;
                }
                const float a0 = o.amp;
                float a1 = a0 + (o.targetAmp - a0) * cA;
                if (o.targetAmp <= 0.0f && a1 < 1.0e-7f) a1 = 0.0f;
                o.amp = a1;
                if (o.freq <= 0.0f) continue;

                for (int L = 0; L < kLayers; ++L)
                {
                    const float lg = layerGain[(size_t) L];
                    const float f  = o.freq * layerRatio[(size_t) L];
                    if (lg <= 0.0f || f < 10.0f || f > 0.45f * (float) sr) continue;

                    const float w = kTwoPi * f / (float) sr;
                    const std::complex<float> rot (std::cos (w), std::sin (w));
                    auto z = o.z[(size_t) L];
                    float amp = a0 * lg;
                    const float damp = (a1 - a0) * lg / (float) n;
                    for (int i = 0; i < n; ++i)
                    {
                        z *= rot;
                        amp += damp;
                        const float s = z.real() * amp;
                        tmpL[(size_t) i] += s * o.gL;
                        tmpR[(size_t) i] += s * o.gR;
                    }
                    const float m2 = std::norm (z);
                    z *= (3.0f - m2) * 0.5f;
                    o.z[(size_t) L] = z;
                }
            }

            for (int i = 0; i < n; ++i)
            {
                const float e = envStart + (envEnd - envStart) * (float) (i + 1) / (float) n;
                const float xl = tmpL[(size_t) i] * e * levelLin;
                const float xr = tmpR[(size_t) i] * e * levelLin;
                lpL += toneCoef * (xl - lpL);
                lpR += toneCoef * (xr - lpR);
                outL[done + i] = std::tanh (lpL);
                outR[done + i] = std::tanh (lpR);
            }
        }

        // ---- hop boundary: new analysis + capture policy
        if (hopCounter >= hop)
        {
            hopCounter = 0;
            analyse();

            const bool gate = lastHopRms > 0.0018f;   // about -55 dBFS
            if (grabPending)
            {
                applyTargets (freshOnGrab);
                grabPending = false;
                freshOnGrab = false;
            }
            else if (params.mode != Mode::Freeze && engaged)
            {
                if (gate && numLatest > 0) applyTargets (false, true);
                else                       droopAll();
            }
        }
        done += n;
    }
}
} // namespace ubermensch
