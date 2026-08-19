# Quickstart: Signal Analysis & Expressive Input Controls

## Purpose

Validate Phase 2.2 end to end in the AuralForge plug-in editor.

## Prerequisites

- CMake build with JUCE, LibTorch, Dear ImGui, Python 3
- Phase 2 graph editor functional (element menu, Merge, inline properties, freeze)

## Build & Test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Validation Scenarios

### 1. Dual chain/element analysis (stereo)

1. Build `Audio In → Activation → Audio Out`.
2. Select Activation; open analysis panel.
3. Switch transfer, frequency, and phase views.

**Expected**
- Each view shows **chain** and **element-only** curve families.
- Two traces per family (stereo L/R) on shared axes with legend.
- Transfer curves visible while stopped.
- During playback: marker on **chain** transfer curve, lying on the line.

### 2. Multi-channel / latent path

1. Build a graph with Conv1D or TCN using channels > 2 (e.g., 8 or 32).
2. Open analysis on that node.

**Expected**
- One trace per channel/feature dimension per family (not hard-coded to stereo).
- Legend identifies dimensions; plot remains readable.

### 3. Probe fallback

1. Open analysis with silent or disconnected upstream path.

**Expected**
- Static curves still render (probe-driven).
- UI indicates probe fallback status.

### 4. Gain on Activation and TCN

1. Edit **Gain** inline on Activation, then TCN, during playback.
2. Observe transfer view (element-only and chain).

**Expected**
- Immediate audible change; transfer curves update.
- No glitches or editor freeze.

### 5. Knob Input conditioning

1. Add **Knob Input** from element menu.
2. Wire to Merge alongside Audio In → Merge → TCN → Audio Out (also test direct Knob → element).
3. Rotate knob during playback.

**Expected**
- Audio character changes; inline TCN parameters unchanged.
- Value readout updates on Knob node.

### 6. XY Trackpad conditioning

1. Add **XY Trackpad**; wire X/Y to Merge or directly to inputs.
2. Move pointer through pad extremes.

**Expected**
- Both X/Y readouts update; audio responds in real time.
- Inline architectural params on targets unchanged.

### 7. Merge audio + conditioning

1. Audio In + Knob + XY → Merge → TCN → Audio Out.
2. Adjust knob and trackpad while playing.

**Expected**
- Combined conditioning affects output; Merge modes (add/multiply) behave per spec.
- With no conditioning cables: neutral c = 0 baseline.

### 8. Gold-node analysis parity

1. Freeze a valid chain to Gold BlackBox.
2. Open analysis on Gold node; cycle all views.

**Expected**
- Same chain/element-only views as Blue nodes at compiled boundary.

### 9. State recall

1. Build graph with Knob, XY, Merge routing, Gain edits, analysis view preference.
2. Save and reload plug-in state.

**Expected**
- Topology, conditioning values, Gain, and cable routing restore correctly.

## Related Artifacts

- `data-model.md`
- `contracts/analysis-runtime-contract.md`
- `contracts/graph-control-ui-contract.md`
- `plan.md`, `tasks.md`
