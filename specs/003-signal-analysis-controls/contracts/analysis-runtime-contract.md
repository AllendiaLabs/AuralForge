# Contract: Analysis Runtime

## Purpose

Define behavior between graph editor, processor/runtime, and analysis pipeline for Phase 2.2 dual chain/element-only views with N-channel/feature support.

## Participants

- Editor orchestration (`PluginEditor`)
- Graph document (`NodeGraph`)
- Live / frozen runtime (`LiveGraphEngine`, `TorchScriptBlackBox`)
- Analysis snapshot producer (background/message thread)
- Analysis panel consumer (`InfoPanel`)

## Inputs

- Selected node id
- Graph/runtime revision token
- Node runtime state (`liveBlue` | `frozenGold`)
- Analysis view (`transfer` | `frequency` | `phase`)
- Live capture availability (for probe fallback and transfer marker)
- Playback active flag (for transfer marker visibility)

## Required Behavior

1. Selecting an analyzable node MUST allow requesting analysis for that node.
2. Each response MUST include **chain** and **element-only** curve families for the requested view.
3. Each family MUST include one series per channel/feature dimension at the analysis point (`channelCount` ≥ 1).
4. Static curves MUST render without requiring playback.
5. Pipeline MUST prefer suitable live audio; MUST fall back to internal probe (white noise) when not.
6. During playback, transfer view MUST include a marker on the **chain** curve at current input→output coordinates; marker MUST lie on the curve; marker MUST be omitted when not playing.
7. Frequency view: static magnitude (dB) vs log frequency for both families.
8. Phase view: static phase (degrees) vs log frequency for both families.
9. Gold BlackBox nodes MUST return the same view types as Blue nodes at the compiled boundary.
10. Analysis generation MUST NOT block or allocate on the audio thread.
11. Results MUST be tagged with `generatedAtRevision`; stale results MUST NOT present as current after topology/property/weight/conditioning changes.

## Output Shape

```text
AnalysisSnapshot {
  nodeId
  runtimeState
  sourceMode          // live | probe
  view                // transfer | frequency | phase
  channelCount
  chainSeries[]       // length = channelCount
  elementOnlySeries[] // length = channelCount
  transferMarker?     // present only if playing && view == transfer
  generatedAtRevision
  isStale
}
```

Each series entry:
```text
Series {
  channelIndex        // 0 .. channelCount-1
  channelLabel?       // e.g. "L", "R", "ch3" — UI hint
  x[], y[]            // sampled plot points
}
```

## Failure / Degraded Modes

- Unanalyzable node → explicit `unavailable` status (not silent empty success).
- Unsuitable live input → `probeFallback` with curves still rendered.
- Superseded revision → `isStale = true` until refresh completes.

## Implementation Anchors

- Snapshot generation: `AuralForge/Source/dsp/LiveGraphEngine.cpp`
- Revision + live capture: `AuralForge/Source/PluginProcessor.*`, `LiveGraphPublisher.*`
- Editor requests: `AuralForge/Source/PluginEditor.cpp`
- Panel rendering: `AuralForge/Source/ui/InfoPanel.*`

### Dual-curve N-channel notes (T002)

- `AnalysisSnapshot` lives in `LiveGraphEngine.h` with `chainSeries` and `elementOnlySeries` both sized to `channelCount`.
- Chain compile/run taps the selected node inside a full-graph runtime (`processTensorTapped`); element-only runs the compiled operator in isolation (`processIsolated`) with a probe of the node's input width.
- Every series carries `channelIndex` plus an optional `channelLabel` (`L`/`R` for stereo, `chN` otherwise). UI must render all traces; do not cap N.
- Transfer uses a bipolar amplitude sweep; frequency/phase use live capture when suitable and white-noise FFT otherwise.
- `generatedAtRevision` is compared by `PluginEditor` against the processor graph-revision token; stale snapshots stay visible but flagged until refresh.

## Computation Notes

- Chain path: compile/run subgraph from graph inputs through selected node output.
- Element-only path: isolate selected node with probe injected at its input boundary (upstream context excluded).
- MUST NOT invoke audio-thread `processBlock` for analysis; use copied graph + off-thread runtime prepare/process pattern already used for analysis prototyping.
- Gold requests route through frozen BlackBox forward at group boundary.

## Implementation Status

Implemented in Phase 2.2 (`tasks.md` T004–T051).

- Dual chain/element-only snapshots are produced by `LiveGraphEngine::analyse` off the audio thread.
- N-channel series, probe fallback, transfer marker, Gold boundary analysis, and revision tagging are covered by `AuralForgeLiveGraphTests` / `AuralForgeProcessorTests`.
- Editor consumption is `InfoPanel` + `PluginEditor` with 12 Hz refresh throttling.
