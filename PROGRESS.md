# BLEEP / 001 — Progress

## Phase 1 — Minimal working plugin
- [x] CMake project, JUCE 9.0.5, VST3 + AU + Standalone targets
- [x] macOS build script (toolchain checks, JUCE fetch, tests, install, ad-hoc signing, auval)
- [x] JUCE-free DSP core: sine voice, percussive ADSR, 8-voice allocation with stealing
- [x] Sample-accurate MIDI (block split at each event), all-notes-off
- [x] Parameters: attack, decay, sustain, release, fine tune, output (APVTS)
- [x] State save/restore with version tag
- [x] DSP tests pass in the cloud workspace (44.1/48/96 kHz, block sizes 1–2048)
- [x] Plugin compiles against JUCE 9.0.5 (Linux check build in the cloud workspace)
- [x] Headless processor tests pass: MIDI to audio at 3 sample rates, state save/restore, corrupt state ignored
- [x] GitHub Actions workflow for macOS arm64 (VST3 + AU, tests, ad-hoc signing, pluginval, ZIP artifact)
- [ ] **First green GitHub Actions run** (needs the repo on GitHub)
- [ ] Build on the Mac with ./build.sh (optional; blocked by free disk space)
- [ ] **Load in Ableton Live 12, play notes, hear bleeps**
- [ ] Save the set, reopen it, confirm the knob values came back

## Phase 2 — Bleep synthesis (next)
- FM (sine/triangle carrier, ratio, index), pitch envelope (±, decay), low-pass filter (cutoff, resonance, env amount)

## Phase 3 — Sequencer
## Phase 4 — Generate / Mutate / Lock
## Phase 5 — Interface and presets
## Phase 6 — Saturation, delay, factory presets, optimisation

## Decisions to confirm
- Company name and the plugin/manufacturer codes (`Blpa` / `Bl01`) in CMakeLists.txt are placeholders. Change them before using the plugin in real projects.
