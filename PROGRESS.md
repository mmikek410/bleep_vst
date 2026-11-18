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

### Preservation: BLEEP 001 Classic alongside BLEEP 001
- [x] Phase 1 recovered from git `379315c` into `classic/` (13 files byte-identical; only PRODUCT_NAME changed)
- [x] Classic keeps the original IDs (Blpa/Bl01, com.bleepaudio.bleep001)
- [x] BLEEP 001 moved to new IDs (Blpa/Bl02, com.bleepaudio.bleep001.v2)
- [x] CI builds both (matrix), uploads two artifacts, and runs a side-by-side install check (bundle IDs, VST3 class IDs, AU registration, pluginval on both)
- [ ] Cloud build green. The first attempt never started (GitHub had no macOS arm64 runners); it was rebuilt with Milestone 2.
- [ ] User test: both appear in Live; a Phase 1 set opens with Classic

### Milestone 2 — Generative (RANDOM / MUTATE)
- [x] JUCE-free generator: motif-based RANDOM (density exact, range, locks, velocity, probability), MUTATE (amount-scaled, groove-preserving), deterministic seeds
- [x] Undo/redo history (64) covering RANDOM, MUTATE, CLEAR, grid gestures and randomized sound
- [x] Params gen_density, gen_range, gen_mutate, gen_rand_sound (state v3); IDs unchanged elsewhere
- [x] RANDOMIZE SOUND limited to existing params (attack/decay/release); FM params later
- [x] Step LOCK lane in the grid; velocity and probability shown on steps
- [x] Visual identity: Michroma wordmark (direction C, chosen by user), Barlow Condensed UI, flat thin-line look-and-feel, fonts embedded with OFL notices
- [x] Tests: generator suite; plugin tests for generating during playback, determinism, undo/redo, state save/reload, v2 compatibility
- [x] Offscreen UI snapshot tool (visual review; CI artifact)
- [ ] **Cloud build green**
- [ ] **User test in Ableton**
### Milestone 3 — FM, pitch envelope, filter
### Milestone 4 — velocity, gate, ratchet, swing, snapshots A–D, scales
### Milestone 5 — LFO, delay, saturation, randomize sound
### Milestone 6 — GUI refinement, presets, optimisation

## Decisions to confirm
- Resolved: plugin codes are final. Bl01 = Classic, Bl02 = BLEEP 001. Never change them.
