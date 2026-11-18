# BLEEP / 001

Generative FM synthesizer with an integrated step sequencer, made for hypnotic, raw, minimal and groove techno.

**Current status: Phase 1.** A sine voice with an ADSR envelope, 8-voice polyphony, MIDI input, parameters you can automate, and state saved with your Ableton set. See `PROGRESS.md`.

## Requirements (macOS)

- macOS 12 or newer, Apple Silicon or Intel
- Xcode Command Line Tools: `xcode-select --install`
- CMake 3.22 or newer: `brew install cmake`
- About 2 GB of free disk space
- You don't need to install JUCE yourself. `build.sh` downloads JUCE 9.0.5 into `external/JUCE`.

## Cloud build (no Xcode needed)

Every push to `main` runs `.github/workflows/build-macos.yml` on GitHub's Apple Silicon runner. It builds the VST3 (and the AU), runs all the tests, ad-hoc signs the bundles, validates the VST3 with pluginval and uploads a ZIP. To download it, open the repo's **Actions** tab, click the latest green run, and download **BLEEP001-macOS-arm64** under *Artifacts*. You can also start a run by hand: Actions > Build macOS (arm64) > *Run workflow*.

The ZIP contains `BLEEP 001.vst3`, `BLEEP 001.component`, `install.sh` and `INSTALL.txt`. Unzip it, then run `bash install.sh` in Terminal. The script copies the plug-ins into `~/Library/Audio/Plug-Ins`, removes the download quarantine and re-signs them for local use.

## Local build and install

```bash
cd ~/"devices ableton/BLEEP001"
bash build.sh
```

The script checks your tools first. Then it builds the VST3, AU and Standalone versions and runs the DSP tests. It copies the plug-ins to `~/Library/Audio/Plug-Ins/{VST3,Components}`, signs them for Apple Silicon and runs `auval`. Everything it prints is also saved to `build.log`.

Options: `CLEAN=1 bash build.sh` rebuilds from scratch. `UNIVERSAL=1 bash build.sh` builds arm64 and x86_64 together, which you need if Ableton runs under Rosetta.

## Loading it in Ableton Live 12

1. Open Settings > Plug-Ins. Turn on **Use VST3 Plug-in System Folders**, and also **Use Audio Units v2** if you want the AU.
2. Click **Rescan**.
3. In the browser, go to Plug-Ins > BLEEP Audio > **BLEEP 001** and drag it onto a MIDI track.

## Project layout

```
CMakeLists.txt        build definition; product codes are set at the top
build.sh              one-command macOS build
Source/dsp/           JUCE-free DSP engine (shared with tests and future products)
  Envelope.h          percussive ADSR (linear attack, exponential decay/release)
  Voice.h             one voice: sine oscillator + ADSR (FM and filter arrive in Phase 2)
  SynthEngine.*       8-voice allocation and stealing, output gain smoothing
  Smoother.h          one-pole parameter smoother
Source/plugin/        JUCE wrapper
  Parameters.*        parameter IDs and ranges (IDs must never be renamed)
  PluginProcessor.*   MIDI handling with sample-accurate block splitting, state save/load
  PluginEditor.*      temporary functional UI
tests/EngineTests.cpp offline render tests (sample rates, block sizes, clicks, NaN, leaks)
tests/PluginTests.cpp headless processor tests (MIDI to audio, state round trip); -DBLEEP_BUILD_PLUGIN_TESTS=ON
```

Design rules: no allocation, locks or I/O on the audio thread. The engine receives note events, so the Phase 3 sequencer will feed the same engine through the same path. State is saved as XML with a `stateVersion` tag so older sets can be upgraded later.

## Developing without the plugin

```bash
cmake -S . -B build-tests -DBLEEP_BUILD_PLUGIN=OFF && cmake --build build-tests
./build-tests/bleep_tests --wav test.wav   # also renders a 2-bar bleep pattern
```
