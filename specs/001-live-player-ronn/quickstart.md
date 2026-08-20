# Quickstart Validation Guide: The Live Player & RONN (Phase 1)

**Branch**: `001-live-player-ronn` | **Date**: 2026-08-19

## Prerequisites

- macOS 12+ (Monterey or later)
- Xcode 14+ with command-line tools
- CMake 3.25+
- LibTorch (C++ distribution) for macOS — download from pytorch.org
- A DAW that supports VST3 or AU (e.g., Reaper, Logic Pro, Ableton Live)
- Audio file or live input for testing

## Build

```bash
# From repository root
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/libtorch -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

The build produces:
- `OpenYourBox.vst3` in the VST3 output directory
- `OpenYourBox.component` in the AU output directory

## Installation

```bash
# VST3
cp -r build/OpenYourBox_artefacts/Release/VST3/OpenYourBox.vst3 ~/Library/Audio/Plug-Ins/VST3/

# AU
cp -r build/OpenYourBox_artefacts/Release/AU/OpenYourBox.component ~/Library/Audio/Plug-Ins/Components/
```

## Validation Scenarios

### V1: Plugin Loads and Processes Audio (SC-001, FR-001, FR-002)

1. Open DAW, insert OpenYourBox on an audio track
2. Route audio (file or live input) through the track
3. Press play

**Expected**: Audio is audibly processed through the default TCN (depth=4) immediately. No setup required. Effect is audible within 1 second of playback.

### V2: Parameter Changes in Real Time (SC-002, FR-003, FR-004)

1. With audio playing through the plugin:
2. Change **Depth** from 4 → 8
3. Change **Kernel Size** from 3 → 7
4. Change **Channels** from 16 → 64
5. Switch **Activation** from ReLU → Tanh

**Expected**: Each change produces an audible difference in the effect character. No audio dropouts, clicks, or pops during changes. Latency stays below 7 ms (verify via DAW latency meter).

### V3: Weight Randomization (SC-003, FR-005, FR-006)

1. With audio playing, click the **Randomize** button in the UI
2. Click it again repeatedly (5+ times in quick succession)
3. Automate the `randomize` parameter in the DAW timeline

**Expected**: Each randomize action produces a distinctly different sound. No dropouts during rapid randomization. DAW automation triggers randomization at the correct timeline position.

### V4: Node Graph Visualization (FR-007, FR-008, FR-009)

1. Open the plugin UI
2. Verify Blue nodes are visible showing the TCN architecture
3. Change the **Depth** parameter from 4 → 6

**Expected**: Graph shows Audio In → [Conv1d + Activation] × depth → Audio Out, all in Blue. When depth changes, nodes are added/removed smoothly. UI stays at 60 FPS (verify visually — no stuttering).

### V5: DAW State Persistence (SC-006, FR-010)

1. Set parameters to non-default values (depth=6, kernel_size=5, channels=32, activation=Tanh)
2. Click Randomize once to get a unique sound
3. Save the DAW project
4. Close and reopen the project

**Expected**: All parameters restored exactly. The sound is identical to before save — same randomized weights.

### V6: Multiple Instances (SC-007, FR-011)

1. Insert 4 instances of OpenYourBox on separate audio tracks
2. Set different parameters on each instance
3. Play all tracks simultaneously

**Expected**: Each instance produces its own distinct effect. No audio dropouts at 256-sample buffer. No state leakage between instances.

### V7: Plugin Validation (SC-004, FR-001)

```bash
# VST3 validation
/path/to/VST3PluginTestHost -t OpenYourBox.vst3

# AU validation
auval -v aufx Afge Afge
```

**Expected**: Both validators pass without errors or warnings.

### V8: Info Display (FR-014, SC-008)

1. Open the plugin UI
2. Check the receptive field display and parameter count
3. Change depth from 4 → 8

**Expected**: Receptive field (in ms) and parameter count are displayed. Values update within 200 ms of the parameter change.

### V9: Edge Cases

| Scenario | Action | Expected |
|----------|--------|----------|
| Silence input | Play silence through plugin | Output is silence |
| Extreme depth | Set depth to 50 | Plugin works; UI warns about large receptive field |
| Sample rate change | Switch DAW sample rate 44.1→96 kHz | Plugin reinitializes without crash |
| Tiny buffer | Set buffer to 32 samples | Audio processes correctly (may have higher CPU) |

## Implementation Validation Results (2026-08-19)

- **V1 — automated core path PASS**: the processor integration test constructs the
  default stereo TCN, processes a 256-sample block, and verifies non-silent output
  that differs from the dry input. DAW insertion remains a manual release check.
- **V2 — implementation complete, manual DAW check pending**: architecture parameters
  coalesce onto the message thread and publish immutable model/runtime snapshots.
- **V3 — deterministic core PASS, manual automation check pending**: equal
  seed/counter/architecture values produce byte-identical tensors; UI, MIDI CC, and
  host parameter trigger paths are implemented.
- **V4 — implementation complete, visual host check pending**: the Dear ImGui node
  editor renders the ML Forge-style node/link model and updates from APVTS values.
- **V5 — automated PASS**: processor state round-trip restores parameters and serialized
  model weights with sample-exact output from a reset history.
- **V6 — per-instance audit PASS, four-instance DAW load pending**: processor, model,
  randomization counter, history, and UI contexts are instance-owned.
- **V7 — build/signing PASS, external validators pending**: Debug VST3 and AU bundles
  compile and pass strict macOS code-signature verification. Steinberg validator and
  installed-component `auval` remain release-environment checks.
- **V8 — implementation complete, visual timing check pending**: metrics are read from
  the published model and rendered every frame.
- **V9 — automated silence PASS**: digital silence remains digital silence. Sample-rate,
  extreme-depth warning, and tiny-buffer paths are implemented; host-driven manual
  checks remain pending.

Automated command:

```bash
cmake --build build --target OpenYourBoxTests OpenYourBoxProcessorTests
ctest --test-dir build --output-on-failure
```
