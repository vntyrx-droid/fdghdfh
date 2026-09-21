# Chorus

A three-voice modulated-delay chorus, built as **VST3**, **CLAP** and a **standalone**
application from one codebase.

The DSP core is plain C++17 with no framework dependency, so it can be unit
tested on its own or dropped into a different plugin shell.

## Why this repository exists

This project started from a `Chorus_x64.dll` that "wouldn't load in anything".
It turned out not to be a broken plugin but a *different kind* of plugin — see
[`docs/uploaded-dll-analysis.md`](docs/uploaded-dll-analysis.md) for the full
breakdown. The short version:

| Property | The uploaded DLL | This project |
| --- | --- | --- |
| Format | FL Studio native (`FruityPlug`) | VST3 + CLAP + standalone |
| Entry point | `CreatePlugInstance` | `GetPluginFactory` / `clap_entry` |
| Loads in | FL Studio only | Any VST3 or CLAP host |
| Source | Closed, compiled from Delphi | This repository |

No code was taken from that binary. This is an original implementation written
from the signal flow up, so the licensing is yours to decide.

## Building

Requires CMake 3.22+ and a C++17 compiler. JUCE and the CLAP wrapper are
fetched automatically, so a fresh clone needs nothing else.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Artefacts land in `build/Chorus_artefacts/Release/`:

```
VST3/Chorus.vst3
CLAP/Chorus.clap
Standalone/Chorus
```

On Linux you will also need the usual JUCE development packages:

```bash
sudo apt install libasound2-dev libxinerama-dev libxcursor-dev libxrandr-dev \
                 libfreetype-dev libfontconfig1-dev libgl1-mesa-dev
```

### Options

| Option | Default | Effect |
| --- | --- | --- |
| `CHORUS_BUILD_CLAP` | `ON` | Also build the CLAP version |
| `CHORUS_BUILD_TESTS` | `ON` | Build the test executables |

### Prebuilt downloads

Every push builds Windows, macOS and Linux versions in CI. To download one
without installing a compiler: open the repository's **Actions** tab, click the
most recent **Build** run, and grab the artifact for your platform
(`Chorus-Windows`, `Chorus-macOS`, `Chorus-Linux`) from the Artifacts section
at the bottom. Unzip it and install as below.

GitHub only keeps artifacts for 90 days, and you must be signed in to download
them.

### Installing

Copy the built bundle into your host's plugin folder, then rescan:

| Platform | VST3 | CLAP |
| --- | --- | --- |
| Windows | `C:\Program Files\Common Files\VST3\` | `C:\Program Files\Common Files\CLAP\` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/CLAP/` |
| Linux | `~/.vst3/` | `~/.clap/` |

`Chorus.vst3` is a **folder**, not a single file. Copy the whole thing — a host
cannot load it if its internal structure is broken up.

### Ableton Live

Live uses the **VST3** build. It does not support CLAP, and it has not needed a
`.dll` since VST2 — `Chorus.vst3` is the file you want.

1. Copy `Chorus.vst3` into `C:\Program Files\Common Files\VST3\`
   (macOS: `~/Library/Audio/Plug-Ins/VST3/`).
2. In Live: **Options → Preferences → Plug-Ins**, turn on **Use VST3 Plug-In
   System Folders**, then click **Rescan**.
3. The plugin appears under **Plug-Ins → Independent → Chorus**.

If it does not show up, the usual causes are an architecture mismatch (Live is
64-bit and needs the 64-bit build) or a partially copied bundle. Live's
**Plug-Ins** preference pane lists plugins it rejected, which is the quickest
way to tell the two apart.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

Two suites, neither needing a framework or an audio device:

- `chorus_dsp_test` — delay-line interpolation, LFO shape and phase stability,
  and engine behaviour (dry passthrough, stereo decorrelation, silence in /
  silence out, parameter clamping, smoothing under abrupt changes).
- `chorus_processor_test` — instantiates the real `AudioProcessor` and checks
  parameter wiring, mono and stereo processing, and state round-tripping.

## Controls

| Control | Range | What it does |
| --- | --- | --- |
| **LFO 1–3 Rate** | 0–10 Hz | Sweep speed of each voice |
| **LFO 1–3 On** | — | Enables each voice; voices are averaged, so the level holds steady |
| **Delay** | 0.5–40 ms | Base delay the sweep starts from |
| **Depth** | 0–20 ms | How far the sweep travels above the base delay |
| **Stereo** | 0–100 % | Phase offset between left and right, up to 180° |
| **Shape** | Sine / Triangle | LFO waveform |
| **Mix** | 0–100 % | Dry/wet blend |
| **Wet only** | — | Outputs the wet path alone, for send/bus use |

Defaults are two active voices at 0.35 Hz and 0.83 Hz, 12 ms base delay and
6 ms depth — a slow, wide chorus rather than a null setting.

## How it works

```
input ──┬────────────────────────────────── dry ──┐
        │                                         ├── mix ── output
        └── delay line ── 3 modulated taps ── wet ─┘
                 ▲
             LFO 1,2,3  (phase-offset per voice, and again per channel)
```

Points worth knowing if you extend it:

- **Cubic interpolation.** The read pointer moves continuously, and linear
  interpolation would impose a low-pass whose cutoff tracks the fractional
  delay — audible as a dull warble. `DelayLine` uses 4-point Catmull-Rom.
- **One oscillator, many taps.** Voices and channels read the same LFOs at
  different phase offsets, so they stay locked in relative phase instead of
  drifting apart.
- **Double-precision phase.** A `float` phase accumulator loses about 4e-3 of a
  cycle per second at 48 kHz, enough to pull the voices out of alignment over a
  session. The accumulator is `double`; only the output is `float`.
- **Upward-only sweep.** The modulation adds to the base delay rather than
  swinging around it, so the read pointer can never cross the write head.
- **No feedback path.** Feedback turns a chorus into a flanger. If you want
  that, feed the wet tap back into `DelayLine::write` — with a clamp well below
  1.0 on the feedback gain.

## Layout

```
source/
  dsp/
    DelayLine.h        fractional delay, Catmull-Rom interpolation
    Lfo.h              phase accumulator, sine and triangle shapes
    ChorusEngine.h/cpp voice mixing, smoothing, dry/wet
  ParameterIDs.h       parameter layout and IDs
  PluginProcessor.*    JUCE AudioProcessor
  PluginEditor.*       GUI
tests/
  dsp_test.cpp         DSP core checks
  processor_test.cpp   plugin wrapper checks
```

`source/dsp/` has no JUCE include anywhere in it — that boundary is worth
keeping if you add features.
