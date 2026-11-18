# BLEEP 001 Classic

A frozen copy of BLEEP / 001 **Phase 1**: the 8-voice sine synth with ADSR, from git commit `379315c` (branch `release/phase1`).

- Source and tests are byte-identical to that commit. The only change is `PRODUCT_NAME` in `CMakeLists.txt`, so it installs as `BLEEP 001 Classic.vst3` / `.component` next to the current BLEEP 001.
- It keeps the **original identifiers**: manufacturer `Blpa`, plugin code `Bl01`, bundle ID `com.bleepaudio.bleep001`, AU `aumu Bl01 Blpa`. Ableton sets saved with Phase 1, or with the Milestone 1 test build, open with this plug-in.
- It's built by the same GitHub Actions workflow, and the artifact is `BLEEP001-Classic-macOS-arm64`.
- Don't develop here. New work goes in the main project, which has its own identifiers (`Bl02`).
