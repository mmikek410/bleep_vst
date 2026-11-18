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
- [x] First green GitHub Actions run (macOS arm64 VST3 + AU, tests, pluginval)
- [x] Loaded and tested in Ableton Live 12 (confirmed by user)
- Rollback: branch `release/phase1` (commit 379315c). The proxy used for pushing rejects tags, so it's a branch.

## Phase 2 (spec: generative hypnotic techno synth)

### Milestone 1 — Internal sequencer
- [x] JUCE-free `Sequencer`: host PPQ sync, mid-song start, loop and jump handling, tempo changes, stop releases notes, free-run without a host transport
- [x] Sample-accurate steps (exact to half a sample at every block size and rate tested), note-offs before note-ons
- [x] 32-step pattern model with velocity/probability/gate/ratchet/lock fields stored (UI and behaviour in later milestones)
- [x] INTERNAL / EXTERNAL modes; Phase 1 MIDI path unchanged
- [x] Params: seq_mode, seq_rate, seq_length, seq_root, seq_gate (Phase 1 IDs unchanged)
- [x] Lock-free-on-audio pattern sharing (PatternStore)
- [x] State v2 with PATTERN node; Phase 1 states load (tested)
- [x] Step grid UI with playhead, paint-drag, pitch drag
- [x] Tests: sequencer suite, plugin suite with simulated playhead, backward compatibility
- [ ] **Cloud build green**
- [ ] **User test in Ableton** (sync, loop, tempo change, save/reopen)

### Milestone 2 — RANDOM, density, pitch range, probability, MUTATE, lock, undo
### Milestone 3 — FM, pitch envelope, filter
### Milestone 4 — velocity, gate, ratchet, swing, snapshots A–D, scales
### Milestone 5 — LFO, delay, saturation, randomize sound
### Milestone 6 — GUI refinement, presets, optimisation

## Decisions to confirm
- Company name and the plugin/manufacturer codes (`Blpa` / `Bl01`) in CMakeLists.txt are placeholders. Change them before using the plugin in real projects.
