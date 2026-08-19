# Data Model: Signal Analysis & Expressive Input Controls

## Overview

Phase 2.2 extends the graph document with analysis state, Gain on Activation/TCN, new conditioning source node types, Merge conditioning lanes, and N-dimensional analysis snapshots — preserving the existing node/link/runtime split.

## Entities

### Graph Node

Represents one editable or frozen graph element in the document.

**Existing fields used**
- `id`, `type`, `state`, `label`, `position`, `size`, `colour`
- `inputs`, `outputs` (pin vectors)
- `properties`, `hasWeights`, `seed`, `explicitSeed`, `useExplicitSeed`
- `metrics`, `artifactPath`, `sourceSubgraph` (BlackBox)

**New / extended `type` values**
- `knobInput` — 1D conditioning source
- `xyTrackpad` — 2D conditioning source (X/Y outputs)

**New Phase 2.2 fields (where applicable)**
- `selectedAnalysisView`: `transfer` | `frequency` | `phase` (per-node UI preference)
- `conditioningValue`: scalar output for `knobInput` (persisted knob position/value)
- `conditioningX`, `conditioningY`: scalar outputs for `xyTrackpad`

**Validation rules**
- All `activation` and `tcn` nodes MUST expose a `gain` property.
- `knobInput` / `xyTrackpad` nodes MUST NOT expose weight randomization controls.
- `blackBox` (`frozenGold`) MUST allow analysis access at compiled boundary.
- `audioInput` / `audioOutput` use boundary-node analysis semantics only.

### Pin

Endpoint on a graph node.

**Extended metadata**
- `signalKind`: `audio` | `conditioning` — distinguishes tensor audio paths from scalar conditioning paths
- `shape` (existing): channel count for audio; scalar semantics for conditioning

**Validation rules**
- `knobInput`: one `conditioning` output
- `xyTrackpad`: two `conditioning` outputs (`x`, `y`)
- Processing element inputs remain Phase 2 layout; connection validator accepts compatible `audio`, `conditioning`, or Merge output per rules in graph-control contract
- Merge inputs accept both `audio` and `conditioning`; Merge output signal kind inferred from connected inputs

### Node Property

Canonical architectural parameter (unchanged ownership model).

**Phase 2.2 extensions**
- `gain` on `activation` and `tcn`: continuous, default `1.0`, range `[0.1, 10.0]`

**Validation rules**
- Gain changes MUST NOT alter Knob/XY conditioning values.
- Invalid Gain text input MUST clamp or reject with user feedback (implementation choice; must not corrupt runtime).

### Knob Input Element (node)

Graph source for 1D conditioning **c**.

**Fields**
- `conditioningValue`: current scalar output
- `minimum`, `maximum`: optional UI bounds (defaults TBD in implementation, e.g. −10..10 per steerable demos)
- Position, label, colour (standard node fields)

**Relationships**
- Output connects to Merge conditioning inputs or directly to processing element inputs
- Does NOT bind to `NodeProperty` keys on other nodes

### XY Trackpad Element (node)

Graph source for 2D conditioning (c0, c1).

**Fields**
- `conditioningX`, `conditioningY`: current scalar outputs
- Normalized pad position (persisted for restore)
- Two conditioning output pins

**Validation rules**
- X and Y are independent scalars; each may wire separately

### Merge Element (extended)

Existing merge node with conditioning lane support.

**Fields (existing)**
- `mode`: add | multiply | concatenate
- Multiple inputs, one output

**Phase 2.2 behavior**
- Accepts audio and/or conditioning inputs
- When no conditioning inputs connected: implicit **c = 0** contribution
- `concatenate` applies to audio channel expansion only, not scalar conditioning combination

### Analysis Panel State

UI state for per-element analysis.

**Fields**
- `nodeId`: selected node
- `view`: `transfer` | `frequency` | `phase`
- `signalSourceMode`: `livePreferredWithProbeFallback`
- `transferMarkerVisible`: true only during active playback
- `status`: `live` | `probeFallback` | `disconnected` | `unavailable`

### Analysis Snapshot

Immutable result consumed by InfoPanel.

**Fields**
- `nodeId`, `runtimeState` (`liveBlue` | `frozenGold`), `sourceMode` (`live` | `probe`), `view`
- `channelCount`: number of feature dimensions at analysis point
- `chainSeries`: array of `channelCount` curve series (each series: sampled x/y points)
- `elementOnlySeries`: array of `channelCount` curve series
- `transferMarker` (optional): `{ inputLevel, outputLevel, channelIndex }` — chain curve only, playback only
- `generatedAtRevision`, `isStale`

**Series layout by view**
- Transfer: x = input amplitude, y = output amplitude (per dimension)
- Frequency: x = frequency (Hz, log axis), y = magnitude (dB)
- Phase: x = frequency (Hz, log axis), y = phase (degrees)

**Validation rules**
- `chainSeries` and `elementOnlySeries` MUST each have length `channelCount`
- Snapshot generation MUST NOT mutate audio-thread state
- Gold snapshots reflect compiled boundary behavior

## Relationships

- Graph has many nodes and links
- `knobInput` / `xyTrackpad` are source nodes (like `audioInput`)
- Merge aggregates many inputs → one output
- One Analysis Panel State targets one node at a time
- One Analysis Snapshot corresponds to one (node, view, revision) request

## State Transitions

### Analysis source
`unavailable` → `probeFallback` → `live` (and reverse when signal suitability changes)

### Transfer marker
`hidden` (stopped) ↔ `visible` (playing, on chain curve)

### Runtime mode
`liveBlue` ↔ `frozenGold` (freeze/unfreeze); analysis semantics preserved

## Persistence Notes

Serialize in graph `ValueTree`:
- Knob/XY node types, positions, conditioning values
- Gain property values
- Per-node `selectedAnalysisView`
- Full cable topology (direct and via Merge)

Do NOT persist ephemeral snapshot buffers or transfer marker positions.
