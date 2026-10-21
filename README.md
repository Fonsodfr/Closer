# Closer — 3-Band Stereo Imager

**Rave Factory** · VST3 / AU · Windows & macOS · built with C++ and JUCE

![Closer interface](docs/closer.png)

Closer narrows (or keeps) the stereo width independently in three frequency bands.
It's a tool for mixing and mastering: fix a phasey mix, tighten a wide low end or
check mono compatibility. It's especially useful for **vinyl cutting**, where wide
low frequencies cause tracking and phase problems: you can make the bass mono
without touching the stereo image of the mids and highs.

## Features

- **3-band Mid/Side width**: Low, Mids and High, each from 0 % (mono) to 100 % (original width)
- **Adjustable crossovers**: low split 40–500 Hz, high split 1–12 kHz
- **Linkwitz-Riley 24 dB/oct crossover** with allpass phase compensation, so the bands sum back flat
- **Mid channel is never processed**: the mono sum of the output is bit-identical to the input
  (verified with a polarity-inverted null test)
- **Real-time spectrum analyser** showing the Mid and the processed Side signal,
  8192-point FFT, +4.5 dB/oct slope, with the crossover points overlaid
- **Phase correlation meter** (−1 to +1), green when mono-safe and orange when out of phase
- **Click-free bypass** (20 ms crossfade), linked to the host's own bypass button
- **Rave Factory interface** drawn entirely in code: custom knobs, LCD readouts, brushed-metal panel
- Lock-free FIFO between the audio thread and the UI (no locks or allocations in `processBlock`)
- Smoothed parameters to avoid zipper noise

## Formats and compatibility

| | Windows | macOS |
| --- | --- | --- |
| Formats | VST3 | VST3, AU |
| Architecture | 64-bit | Universal (Apple Silicon + Intel) |
| Tested in | Reaper | Logic Pro, Ableton Live |

The Audio Unit passes Apple's `auval` validation.

## How it works

1. Left/Right is converted to **Mid** `(L+R)/2` and **Side** `(L−R)/2`.
2. Only the Side signal is split into Low / Mids / High with two LR4 crossovers.
   The Low band goes through an allpass at the high crossover frequency so all
   three bands have the same phase and sum back flat.
3. Each Side band is scaled by its width control and summed back into `Side'`.
4. Output: `L = Mid + Side'`, `R = Mid − Side'`. The Mid is untouched.

## Code layout

```
Source/
  PluginProcessor.h/.cpp   parameters, Mid/Side matrix, width per band, bypass, correlation
  PluginEditor.h/.cpp      the window layout and the chassis
  dsp/ThreeBandSplitter.h  the 3-band LR4 crossover for the Side signal
  dsp/AnalyserFifo.h       lock-free mailbox between the audio and the spectrum display
  ui/                      SpectrumDisplay, CorrelationMeter, RaveFactoryLookAndFeel, Theme
packaging/                 macOS .pkg and Windows Inno Setup installers
```

## Building

One CMake recipe for every platform (JUCE 9.0.3). It uses a `JUCE` folder next to
`CMakeLists.txt` if there is one, otherwise a shared copy a few folders up.

### Windows (Visual Studio 2026)

File › Open › Folder, pick this folder, choose the **Windows Release** preset in the toolbar
and Build › Build All. Then copy `build/windows-release/Closer_artefacts/Release/VST3/Closer.vst3`
to `C:\Program Files\Common Files\VST3\`.

### macOS / command line

```
git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
bash packaging/macos/build-pkg.sh
```

The last step creates `Closer-Installer.pkg`, which installs the VST3 and AU into
`/Library/Audio/Plug-Ins`. GitHub Actions (`.github/workflows/build-macos.yml`) builds the signed and
notarized macOS installer, the Windows installer and a zip with both on every push.

## Roadmap

- [x] Phase 1 — single-band Mid/Side width
- [x] Phase 2 — 3-band Linkwitz-Riley crossover + null test
- [x] Phase 3 — real-time Mid/Side spectrum analyser
- [x] Phase 4 — Rave Factory interface, phase correlation meter and bypass
- [x] macOS build (VST3 + AU, universal) with automated builds on GitHub Actions
- [x] macOS installer package
- [ ] Signed and notarized macOS installer
- [ ] Windows installer

## Author

Made by a producer and mastering engineer with 30 years in the studio
and 200+ releases on digital and vinyl, now building his own tools.
