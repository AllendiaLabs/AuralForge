# Data Model: Signal Analysis & Expressive Input Controls

## Overview

Phase 2.2 extends the existing graph document with analysis-view state and alternate parameter-input metadata while preserving the current node/link/runtime split.

## Entities

### Graph Node

Represents one editable or frozen graph element already stored in the graph document.

**Existing fields used by Phase 2.2**
- `id`: stable node identifier
- `type`: node kind (`audioInput`, `audioOutput`, `linear`, `convolution`, `activation`, `tcn`, `blackBox`, etc.)
- `state`: runtime mode (`liveBlue` or `frozenGold`)
- `properties`: canonical editable parameter rows
- `hasWeights`, `seed`, `explicitSeed`, `useExplicitSeed`
- `metrics`, `artifactPath`, `sourceSubgraph`

**New Phase 2.2 fields**
- `analysisEnabled`: whether the node can expose analysis views
- `selectedAnalysisView`: current preferred view for this node (`transfer`, `frequency`, `phase`)
- `controlBindings`: collection of input-mode metadata for editable parameters
- `xyBinding`: optional two-parameter XY assignment owned by this node

**Validation rules**
- All `activation` and `tcn` nodes MUST expose a `gain` property.
- `blackBox` nodes in `frozenGold` state MUST still allow analysis access.
- `audioInput` and `audioOutput` nodes may expose analysis, but only with boundary-node semantics.

### Node Property

Canonical parameter definition committed to runtime regardless of whether the user edits it via text, knob, or XY control.

**Fields**
- `key`: stable property identifier
- `label`: user-visible property label
- `value`: current validated value
- `minimum` / `maximum`: inclusive bounds
- `kind`: property kind
- `choices`: allowed labels when the property is enumerated

**Phase 2.2 extensions**
- `supportsKnob`: whether a rotary control is allowed
- `supportsXY`: whether the property may be bound to an XY axis
- `displayMode`: current preferred single-parameter editing mode (`text` or `knob`)

**Validation rules**
- Gain MUST be a continuous property on Activation and TCN nodes.
- Knob mode MUST respect the property's existing bounds and step behavior.
- XY binding MUST reference only properties flagged `supportsXY`.

### XY Binding

Represents a per-node two-axis control assignment.

**Fields**
- `enabled`: whether the binding is active
- `xPropertyKey`: parameter key mapped to horizontal movement
- `yPropertyKey`: parameter key mapped to vertical movement
- `xNormalizedValue`: last persisted X position
- `yNormalizedValue`: last persisted Y position

**Validation rules**
- Both property keys MUST belong to the same node.
- X and Y MUST reference distinct parameter keys.
- If either bound property becomes unavailable, the binding becomes invalid and must be cleared or repaired on restore.

### Analysis Panel State

Represents the current UI state for per-element cumulative analysis.

**Fields**
- `nodeId`: selected node being analyzed
- `view`: selected analysis view (`transfer`, `frequency`, `phase`)
- `signalSourceMode`: `livePreferredWithProbeFallback`
- `channelOverlayMode`: `stereoSharedPlot`
- `status`: `live`, `probeFallback`, `disconnected`, or `unavailable`

**Validation rules**
- The panel MUST always identify whether it is driven by live input or fallback probe behavior.
- Left and right channels MUST be rendered on shared axes when stereo data exists.

### Cumulative Signal Snapshot

Immutable analysis result generated from the current runtime graph and consumed by the editor.

**Fields**
- `nodeId`: node the snapshot corresponds to
- `runtimeState`: `liveBlue` or `frozenGold`
- `sourceMode`: `live` or `probe`
- `sampleRate`
- `leftChannelSeries`: sampled plot data for the active analysis view
- `rightChannelSeries`: sampled plot data for the active analysis view
- `generatedAtRevision`: graph/runtime revision identifier
- `isStale`: whether the graph changed after generation

**Validation rules**
- Snapshot data MUST map to a specific node and runtime revision.
- Snapshot generation MUST not mutate audio-thread state.
- Gold-node snapshots MUST reflect compiled BlackBox behavior at the node boundary.

## Relationships

- One **Graph Node** owns many **Node Properties**.
- One **Graph Node** may own zero or one **XY Binding**.
- One **Graph Node** may be associated with many historical **Cumulative Signal Snapshots**, though only the latest valid snapshot is displayed.
- One **Analysis Panel State** targets one active **Graph Node** at a time.

## State Transitions

### Analysis Source State

`disconnected/unavailable` -> `probeFallback` when no suitable live signal exists  
`probeFallback` -> `live` when suitable live input becomes available  
`live` -> `probeFallback` when live input becomes unsuitable  

### Node Runtime State

`liveBlue` -> `frozenGold` after successful manual freeze  
`frozenGold` -> `liveBlue` after unfreeze  

Phase 2.2 does not add new runtime states, but analysis support must remain valid across both transitions.

### Control Binding State

`text` <-> `knob` for single-parameter display mode  
`xy disabled` -> `xy enabled` when two valid properties are assigned  
`xy enabled` -> `xy disabled` when bindings are cleared or become invalid

## Persistence Notes

- All new node-level control metadata should serialize with the existing graph document rather than a separate store.
- Save/restore tests should verify gain values, knob display modes, XY bindings, and selected analysis views in the same recall path already used for seeds and graph topology.
