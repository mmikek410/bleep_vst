# BLEEP / 001

Generative FM synthesizer with an integrated step sequencer, made for hypnotic, raw, minimal and groove techno.

**Current status: Phase 2, Milestone 2, generative.** A host-synced 16-step sequencer with RANDOM, MUTATE, DENSITY, PITCH RANGE, MUTATION, RANDOMIZE SOUND, step LOCK and UNDO/REDO, in a new interface with embedded fonts. It plays the Phase 1 sine/ADSR voice; FM comes in Milestone 3. See `PROGRESS.md`.

## Two plug-ins, installed side by side

| Plug-in | What it is | Plugin code | Bundle ID | AU | Artifact |
|---|---|---|---|---|---|
| **BLEEP 001** | current development version (sequencer, …) | `Blpa` / `Bl02` | `com.bleepaudio.bleep001.v2` | `aumu Bl02 Blpa` | `BLEEP001-macOS-arm64` |
| **BLEEP 001 Classic** | frozen Phase 1, in `classic/` | `Blpa` / `Bl01` | `com.bleepaudio.bleep001` | `aumu Bl01 Blpa` | `BLEEP001-Classic-macOS-arm64` |

Ableton finds a plug-in by its codes, so **these must never change**. Sets saved with Phase 1, or with the Milestone 1 test build, which used `Bl01` too, open with Classic. Sets saved with BLEEP 001 from the `Bl02` builds onward open with BLEEP 001. Every CI run installs both on a clean Mac and checks that their bundle IDs, VST3 class IDs and AU codes all differ.

## Typography

The wordmark and section titles use **Michroma**. Controls and values use **Barlow Condensed**. Both are SIL Open Font License fonts embedded in the plug-in, so nothing needs to be installed. See `Assets/Fonts/README.md`.

## Using the sequencer

- **SEQ: EXTERNAL MIDI / INTERNAL** (header button). In External mode the synth plays incoming MIDI, exactly like Phase 1. In Internal mode the built-in sequencer plays whenever Live's transport runs. Incoming MIDI still plays on top.
- **Step grid.** Click the top square of a step to switch it on or off, or drag across steps to paint them. Drag up or down in the lower area to set the pitch, from -24 to +24 semitones from ROOT. Double-click the lower area to reset the pitch.
- **Rate** (1/4, 1/8, 1/16, 1/32), **LENGTH** (1–16 steps; odd lengths give polymetric loops against the bar), **ROOT** and **GATE** (note length as a percentage of a step).
- **CLEAR** turns off every step that isn't locked.

## Generating patterns

- **RANDOM** writes a new 16-step pattern: active steps, pitch, velocity (shown as the brightness of each step) and probability (shown as a % in the step). It is built for hypnotic techno: a short motif of 3, 4, 6, 8 or 16 steps repeats across the bar, off-beats and anticipations are favoured over downbeats, and the root and small intervals dominate. Pitches come from minor-pentatonic degrees until the SCALE control arrives in Milestone 4.
- **DENSITY** sets exactly how many of the 16 steps are on. Locked steps count toward it.
- **RANGE** limits generated pitches to ± that many semitones around ROOT.
- **MUTATE** varies the current pattern by **MUTATION** amount. At around 10–20 % it changes one note, velocity or probability. At higher amounts it also moves hits to neighbouring steps, which shifts the groove but keeps it recognisable.
- **LOCK** is the thin bar under each step's on/off square. RANDOM, MUTATE and CLEAR never touch locked steps.
- **RANDOMIZE SOUND**: when it's on, RANDOM also sets attack, decay and release within short, safe ranges. It never touches volume or tuning. FM and pitch-envelope settings join once they exist.
- **UNDO / REDO** step back through RANDOM, MUTATE, CLEAR and grid edits, including sound changes, up to 64 steps. The history lasts until the plug-in is closed. The pattern itself is saved with the Live set.
- Everything works while Live is playing. A new pattern takes effect from the next step with no dropout, because the audio thread never waits for the UI.
- Note names follow Ableton's convention: MIDI 60 = C3.
- The sequencer follows Live's song position. It stays in sync when you start mid-song, loop, jump or change tempo, and goes silent when Live stops.

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
classic/              BLEEP 001 Classic: frozen Phase 1 source with its own CMakeLists (see classic/README.md)
build.sh              one-command macOS build
Source/dsp/           JUCE-free DSP engine (shared with tests and future products)
  Envelope.h          percussive ADSR (linear attack, exponential decay/release)
  Voice.h             one voice: sine oscillator + ADSR (FM and filter arrive in Phase 2)
  SynthEngine.*       8-voice allocation and stealing, output gain smoothing
  Smoother.h          one-pole parameter smoother
Source/seq/           JUCE-free sequencer
  Pattern.*           32-step pattern (on, pitch, velocity, probability, gate, ratchet, lock) + text encoding
  Sequencer.*         host-synced scheduler: step k starts at k * stepPpq, so jumps, loops and tempo changes stay in sync
  Generator.*         RANDOM / MUTATE (seeded, deterministic) + undo History
Source/plugin/        JUCE wrapper
  Parameters.*        parameter IDs and ranges (IDs must never be renamed)
  PluginProcessor.*   MIDI handling with sample-accurate block splitting, state save/load
  PatternStore.h      pattern shared between UI and audio (try-lock on the audio thread, never blocks)
  StepGrid.*          16-step grid component
  PluginEditor.*      editor layout
  Theme.h             colours and spacing
  BleepLookAndFeel.*  embedded fonts, flat buttons, thin-arc knobs
  Wordmark.h          BLEEP / 001 wordmark (all proportions in one Style struct)
Assets/Fonts/         Michroma + Barlow Condensed (SIL OFL 1.1, licences included)
tests/EngineTests.cpp offline render tests (sample rates, block sizes, clicks, NaN, leaks)
tests/GeneratorTests.cpp RANDOM density/range/locks/repetition, MUTATE strength, undo history
tests/UiSnapshot.cpp  renders the editor to PNG offscreen (CI uploads it)
tests/SequencerTests.cpp sequencer timing, mid-song start, loop, tempo change, stop, free-run, length, probability, encoding
tests/PluginTests.cpp headless processor tests (MIDI to audio, state round trip); -DBLEEP_BUILD_PLUGIN_TESTS=ON
```

Design rules: no allocation, blocking locks or I/O on the audio thread. The engine receives note events, so the Phase 3 sequencer will feed the same engine through the same path. State is saved as XML with a `stateVersion` tag: 1 = Phase 1, 2 = adds the sequencer parameters and a `PATTERN` node, 3 = adds the generator parameters. Phase 1 sets load with the sequencer off and the default pattern. Parameter IDs are never renamed. The Phase 1 rollback point is the `release/phase1` branch, and it also ships as BLEEP 001 Classic.

## Developing without the plugin

```bash
cmake -S . -B build-tests -DBLEEP_BUILD_PLUGIN=OFF && cmake --build build-tests
./build-tests/bleep_tests --wav test.wav   # also renders a 2-bar bleep pattern
```
