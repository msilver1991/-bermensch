// Offline test for the Übermensch engine: renders test signals, checks spectra, writes demo WAVs.
#include "../Source/UbermenschEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <complex>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <random>
using namespace ubermensch;
static const double SR = 48000.0;
static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("  FAIL: %s\n", msg); ++failures; } else std::printf("  ok:   %s\n", msg); } while (0)

// plucked-string-ish tone: harmonics with exponential decay
static void addPluck (std::vector<float>& x, double f, double t0, double amp, double decay = 1.2, int harmonics = 8)
{
    for (size_t i = (size_t)(t0 * SR); i < x.size(); ++i)
    {
        double t = (double) i / SR - t0, v = 0;
        for (int h = 1; h <= harmonics; ++h) { if (f * h > 0.45 * SR) break; v += std::sin (2 * M_PI * f * h * t) / (h * 1.3) * std::exp (-t * (decay + 0.4 * h)); }
        x[i] += (float) (amp * v);
    }
}
static std::vector<std::complex<double>> dft (const std::vector<float>& x, size_t start, size_t n)
{ // naive-free: use radix-2 FFT in double
    std::vector<std::complex<double>> a (n);
    for (size_t i = 0; i < n; ++i) a[i] = x[start + i] * (0.5 - 0.5 * std::cos (2 * M_PI * i / n));
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1) { double ang = -2 * M_PI / len; std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len) { std::complex<double> w (1); for (size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; } } }
    return a;
}
// strongest spectral peak frequency + amplitude in [lo,hi] Hz
static void peakIn (const std::vector<float>& x, size_t start, double lo, double hi, double& f, double& amp)
{
    const size_t n = 32768; auto a = dft (x, start, n);
    size_t bk = 0; double bm = 0;
    for (size_t k = (size_t)(lo * n / SR); k <= (size_t)(hi * n / SR); ++k) if (std::abs (a[k]) > bm) { bm = std::abs (a[k]); bk = k; }
    f = bk * SR / n; amp = 4 * bm / n;
}
static double rms (const std::vector<float>& x, size_t s, size_t e) { double a = 0; for (size_t i = s; i < e; ++i) a += (double) x[i] * x[i]; return std::sqrt (a / (e - s)); }
static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R)
{
    FILE* f = std::fopen (path, "wb"); uint32_t n = (uint32_t) L.size(), dataBytes = n * 4, riff = 36 + dataBytes, sr = (uint32_t) SR, br = sr * 4, fmt = 16;
    uint16_t pcm = 1, ch = 2, ba = 4, bps = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f); std::fwrite (&fmt, 4, 1, f);
    std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f); std::fwrite (&sr, 4, 1, f); std::fwrite (&br, 4, 1, f); std::fwrite (&ba, 2, 1, f); std::fwrite (&bps, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) for (float v : { L[i], R[i] }) { int16_t s = (int16_t) std::lrint (std::clamp (v, -1.0f, 1.0f) * 32767.0f); std::fwrite (&s, 2, 1, f); }
    std::fclose (f);
}
struct Event { double t; int what; double value; }; // 0 engage, 1 grab
static void render (Engine& e, const std::vector<float>& in, std::vector<float>& L, std::vector<float>& R, std::vector<Event> ev, int block = 512)
{
    L.assign (in.size(), 0); R.assign (in.size(), 0);
    std::sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.t < b.t; });
    size_t ei = 0; bool engaged = false;
    for (size_t pos = 0; pos < in.size(); pos += block)
    {
        int n = (int) std::min<size_t> (block, in.size() - pos);
        while (ei < ev.size() && ev[ei].t * SR <= pos) { if (ev[ei].what == 0) engaged = ev[ei].value > 0; else e.grab(); ++ei; }
        e.setEngaged (engaged);
        e.process (&in[pos], &in[pos], &L[pos], &R[pos], n);
    }
}
static bool finite (const std::vector<float>& x) { for (float v : x) if (!std::isfinite (v)) return false; return true; }

int main()
{
    const char* outDir = std::getenv ("OUT_DIR") ? std::getenv ("OUT_DIR") : ".";
    char path[512];
    const size_t N = (size_t)(8.0 * SR);

    std::printf ("Test 1: Freeze holds a captured chord after input stops\n");
    {
        std::vector<float> in (N, 0.0f);
        addPluck (in, 220.0, 0.2, 0.3); addPluck (in, 277.18, 0.2, 0.3); addPluck (in, 329.63, 0.2, 0.3);
        Engine e; e.prepare (SR); Params p; p.mode = Mode::Freeze; p.attackMs = 30; p.decayMs = 800; p.glissMs = 0; p.toneHz = 18000; e.setParams (p);
        std::vector<float> L, R; render (e, in, L, R, { { 0.6, 0, 1 }, { 5.0, 0, 0 } });
        CHECK (finite (L) && finite (R), "output finite");
        CHECK (rms (in, (size_t)(4.0 * SR), (size_t)(4.5 * SR)) < 0.005, "input has died away by 4s");
        CHECK (rms (L, (size_t)(4.0 * SR), (size_t)(4.5 * SR)) > 0.05, "wet still sounding at 4s (frozen)");
        double f, a;
        for (double target : { 220.0, 277.18, 329.63 }) { peakIn (L, (size_t)(3.0 * SR), target - 8, target + 8, f, a);
            std::printf ("     peak near %.2f -> %.2f Hz amp %.3f\n", target, f, a);
            CHECK (std::abs (f - target) < 0.5 && a > 0.01, "frozen partial at the right pitch"); }
        CHECK (rms (L, (size_t)(7.0 * SR), (size_t)(8.0 * SR)) < 1e-3, "decays to silence after release");
        CHECK (rms (L, (size_t)(1.0 * SR), (size_t)(1.2 * SR)) < 0.5, "level sane (no blow-up)");
        std::snprintf (path, sizeof path, "%s/ubermensch_demo_freeze.wav", outDir);
        std::vector<float> mixL = L, mixR = R; for (size_t i = 0; i < N; ++i) { mixL[i] = 0.7f * in[i] + 0.7f * L[i]; mixR[i] = 0.7f * in[i] + 0.7f * R[i]; }
        writeWav (path, mixL, mixR);
    }

    std::printf ("Test 2: Gliss glides to a second capture\n");
    {
        std::vector<float> in (N, 0.0f);
        addPluck (in, 220.0, 0.2, 0.4, 0.8, 1);
        addPluck (in, 330.0, 3.0, 0.4, 0.8, 1);
        Engine e; e.prepare (SR); Params p; p.mode = Mode::Freeze; p.attackMs = 30; p.decayMs = 500; p.glissMs = 1500; p.toneHz = 18000; e.setParams (p);
        std::vector<float> L, R; render (e, in, L, R, { { 0.5, 0, 1 }, { 3.5, 1, 0 }, { 7.0, 0, 0 } });
        double f, a;
        peakIn (L, (size_t)(2.0 * SR), 150, 500, f, a); std::printf ("     before grab: %.2f Hz\n", f);
        CHECK (std::abs (f - 220.0) < 1.0, "holds 220 Hz before grab");
        peakIn (L, (size_t)(4.0 * SR), 150, 500, f, a); std::printf ("     mid-glide:   %.2f Hz\n", f);
        CHECK (f > 225 && f < 330, "pitch is between old and new mid-glide");
        peakIn (L, (size_t)(6.3 * SR), 150, 500, f, a); std::printf ("     after glide: %.2f Hz\n", f);
        CHECK (std::abs (f - 330.0) < 1.5, "lands on 330 Hz");
        std::snprintf (path, sizeof path, "%s/ubermensch_demo_gliss.wav", outDir);
        std::vector<float> mixL = L, mixR = R; for (size_t i = 0; i < N; ++i) { mixL[i] = 0.5f * in[i] + 0.7f * L[i]; mixR[i] = 0.5f * in[i] + 0.7f * R[i]; }
        writeWav (path, mixL, mixR);
    }

    std::printf ("Test 3: Sustain follows new notes, then holds the last one\n");
    {
        std::vector<float> in (N, 0.0f);
        addPluck (in, 196.0, 0.2, 0.4, 1.5); addPluck (in, 294.0, 2.5, 0.4, 1.5);
        Engine e; e.prepare (SR); Params p; p.mode = Mode::Sustain; p.attackMs = 30; p.decayMs = 12000; p.glissMs = 0; p.toneHz = 18000; e.setParams (p);
        std::vector<float> L, R; render (e, in, L, R, { { 0.1, 0, 1 }, { 6.5, 0, 0 } });
        double f, a;
        peakIn (L, (size_t)(1.5 * SR), 150, 500, f, a); CHECK (std::abs (f - 196.0) < 0.7, "tracks first note");
        peakIn (L, (size_t)(5.0 * SR), 150, 500, f, a); std::printf ("     %.2f Hz amp %.3f\n", f, a); CHECK (std::abs (f - 294.0) < 0.7, "holds second note after input died");
        CHECK (rms (L, (size_t)(5.0 * SR), (size_t)(5.5 * SR)) > 0.02, "still sounding");
        std::snprintf (path, sizeof path, "%s/ubermensch_demo_sustain.wav", outDir);
        std::vector<float> mixL = L, mixR = R; for (size_t i = 0; i < N; ++i) { mixL[i] = 0.7f * in[i] + 0.7f * L[i]; mixR[i] = 0.7f * in[i] + 0.7f * R[i]; }
        writeWav (path, mixL, mixR);
    }

    std::printf ("Test 4: Synth mode adds octave layers\n");
    {
        std::vector<float> in (N, 0.0f);
        addPluck (in, 220.0, 0.2, 0.3, 1.0);
        Engine e; e.prepare (SR); Params p; p.mode = Mode::Synth; p.attackMs = 30; p.decayMs = 12000; p.glissMs = 0; p.subLevel = 0.8f; p.octLevel = 0.8f; p.toneHz = 18000; e.setParams (p);
        std::vector<float> L, R; render (e, in, L, R, { { 0.1, 0, 1 }, { 6.0, 0, 0 } });
        double f, a, a1, a2;
        peakIn (L, (size_t)(4.0 * SR), 100, 120, f, a); std::printf ("     sub: %.2f Hz amp %.3f\n", f, a); a1 = a;
        CHECK (std::abs (f - 110.0) < 0.8 && a > 0.003, "sub-octave at 110 Hz");
        peakIn (L, (size_t)(4.0 * SR), 430, 450, f, a); std::printf ("     oct: %.2f Hz amp %.3f\n", f, a); a2 = a;
        CHECK (std::abs (f - 440.0) < 0.8 && a > 0.003, "octave-up at 440 Hz");
        std::snprintf (path, sizeof path, "%s/ubermensch_demo_synth.wav", outDir);
        std::vector<float> mixL = L, mixR = R; for (size_t i = 0; i < N; ++i) { mixL[i] = 0.5f * in[i] + 0.7f * L[i]; mixR[i] = 0.5f * in[i] + 0.7f * R[i]; }
        writeWav (path, mixL, mixR); (void) a1; (void) a2;
    }

    std::printf ("Test 5: robustness (odd block sizes, noise, 44.1k/96k, silence)\n");
    {
        std::mt19937 rng (1); std::normal_distribution<float> nd (0, 0.2f);
        for (double sr : { 44100.0, 96000.0 })
            for (int block : { 1, 7, 64, 1000 })
            {
                std::vector<float> in ((size_t)(3 * sr)); for (auto& v : in) v = nd (rng);
                Engine e; e.prepare (sr); Params p; p.mode = Mode::Synth; p.subLevel = p.octLevel = 1; p.glissMs = 300; e.setParams (p);
                std::vector<float> L (in.size()), R (in.size()); bool eng = false; bool ok = true;
                for (size_t pos = 0; pos < in.size(); pos += block) { int n = (int) std::min<size_t> (block, in.size() - pos);
                    if ((pos / (size_t) sr) % 2 == 0) eng = true; else eng = false; e.setEngaged (eng); e.process (&in[pos], &in[pos], &L[pos], &R[pos], n); }
                for (size_t i = 0; i < L.size(); ++i) if (!std::isfinite (L[i]) || !std::isfinite (R[i]) || std::abs (L[i]) > 1.001f) ok = false;
                char msg[100]; std::snprintf (msg, sizeof msg, "sr=%.0f block=%d finite & bounded", sr, block); CHECK (ok, msg);
            }
        Engine e; e.prepare (SR); e.setEngaged (true); std::vector<float> z (48000, 0.0f), L (48000), R (48000); e.process (z.data(), z.data(), L.data(), R.data(), 48000);
        CHECK (rms (L, 0, 48000) == 0.0, "silence in -> silence out");
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
