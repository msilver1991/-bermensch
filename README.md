# Übermensch

A VST3 / AU / Standalone freeze, sustain and synth plugin inspired by the Electro-Harmonix
Superego+. It is an original implementation, not a copy of the pedal's algorithm, and it is not
affiliated with Electro-Harmonix. Build it from source with JUCE 8.

## How it sounds / works

The engine listens to the input (8192-point STFT), picks the strongest spectral peaks (up to 96
partials, each with an accurate frequency and amplitude) and re-creates them with a bank of sine
oscillators. Because the held sound is *synthesised*, it can sit there for as long as you like,
glide to new pitches, and be layered with octaves.

| Control | What it does |
|---|---|
| **ENGAGE** | Footswitch. Also follows MIDI sustain pedal (CC64), so a pedal or MIDI foot controller works. |
| **GRAB** | Take a new capture while engaged. Partials glide to the new notes over **Gliss**. |
| **Mode: Freeze** | Captures what you were playing when you engage, holds it until release. |
| **Mode: Sustain** | Keeps following what you play; notes you played keep ringing (for **Decay**) while you add more. |
| **Mode: Synth** | Like Sustain, plus the **Sub** (−1 oct) and **Octave** (+1 oct) layers. |
| **Attack** | Fade-in time of the held sound. |
| **Decay** | Fade-out after release; in Sustain/Synth also how long held notes ring. |
| **Gliss** | Pitch glide (and cross-fade) time between captures. 0 = instant. |
| **Tone** | Low-pass on the held sound. |
| **Width** | Stereo spread of the partials. |
| **Blend** | Dry ↔ wet (centre = both at full level). |
| **Level** | Wet output gain. |

## Build

Needs CMake 3.22+, a C++17 compiler and git.

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --parallel

The first configure downloads JUCE 8.0.8. To use your own copy: `-DUBERMENSCH_JUCE_DIR=/path/to/JUCE`.
The plugin lands in `build/Ubermensch_artefacts/Release/VST3/Ubermensch.vst3`; copy it to your
VST3 folder (Windows `C:\Program Files\Common Files\VST3`, macOS `~/Library/Audio/Plug-Ins/VST3`,
Linux `~/.vst3`). The file name is ASCII; hosts show the name "Übermensch".

Linux needs the usual JUCE packages: `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev
libxcursor-dev libxext-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev`.

No local toolchain? Push this folder to a GitHub repo; `.github/workflows/build.yml` builds
Windows, macOS and Linux and attaches the plugins as downloadable artifacts. (macOS builds are not
code-signed, so you may need to allow them in Privacy & Security.)

## Tests

`cmake -B build -DUBERMENSCH_BUILD_TESTS=ON` builds `ubermensch_engine_test`, an offline test of
the DSP (pitch accuracy, hold, gliss, sustain tracking, octave layers, odd block sizes, 44.1/48/96k)
that also writes demo WAVs. The engine (`Source/UbermenschEngine.*`) has no JUCE dependency.

## Tuning ideas (all in `UbermenschEngine.cpp`)

- Gate for tracking modes: `lastHopRms > 0.0018f` (~ −55 dBFS).
- Partial count: `kMaxPartials` / `kNumOsc`.
- Analysis size: `fftSize` in `prepare()` (bigger = better bass pitch, slower tracking).
