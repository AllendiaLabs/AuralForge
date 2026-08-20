# Contract: Graph Control & Conditioning UI

## Purpose

Define interaction rules for Gain, Knob Input, XY Trackpad, Merge routing, and analysis access in Phase 2.2.

## Element Menu

1. Menu MUST include **Knob Input** and **XY Trackpad** as draggable source elements alongside Audio Input and processing elements.
2. Knob/XY are added to canvas like any other element (drag from menu).

## Knob Input Element

1. Node MUST expose a rotary control and numeric readout of current conditioning output.
2. One conditioning output pin; connects to Merge inputs and/or processing element input ports.
3. Adjusting the knob MUST update runtime conditioning and downstream audio without glitches.
4. Knob MUST NOT read or write inline architectural properties on other nodes.

## XY Trackpad Element

1. Node MUST expose a 2D pad with X and Y numeric readouts.
2. Two conditioning output pins (X, Y); each may wire independently.
3. Pointer movement MUST update both outputs in real time when connected.
4. MUST NOT modify inline architectural properties on other nodes.

## Merge Element

1. MUST accept audio and conditioning inputs on existing input pins (extended validation).
2. Primary pattern: Audio In + Knob/XY → Merge → downstream processing.
3. Direct Knob/XY → element connections MUST remain valid (not refused).
4. With no conditioning inputs, implicit conditioning contribution is **c = 0**.
5. Operating modes: add/multiply combine conditioning scalars; concatenate is audio-only.

## Processing Elements

1. Phase 2 input port layout unchanged (no new port types).
2. Ports accept compatible audio, conditioning, or Merge outputs per `signalKind` rules.
3. Activation and TCN MUST expose inline **Gain** property (architectural/runtime slope control).
4. Gold BlackBox MUST expose same analysis entry as Blue nodes.

## Port Compatibility (high level)

| Source kind | Valid destinations |
|-------------|-------------------|
| audio | audio inputs, Merge (audio lane) |
| conditioning | Merge (conditioning lane), processing inputs accepting conditioning |
| Merge output | processing element inputs matching merged signal kind |

Incompatible connections MUST show red cable + tooltip (existing shape-integrity pattern).

## Analysis Panel

1. Views: transfer, frequency, phase.
2. Each view shows **chain** and **element-only** curve families.
3. One trace per channel/feature dimension on shared axes with distinguishable styling + legend.
4. Transfer: static curves always; live marker on chain curve during playback only.
5. Probe fallback MUST be indicated in UI when not live-driven.

## Persistence

1. Knob/XY positions, conditioning values, and cable topology MUST persist in graph state.
2. Gain values MUST persist with node properties.
3. Per-node selected analysis view MAY persist (recommended).

## Excluded from Freeze Subgraphs

Knob Input and XY Trackpad are UI/control sources and MUST NOT be included in TorchScript freeze compilation in Phase 2.2.

## Implementation Anchors

- Types + pins: `OpenYourBox/Source/graph/GraphTypes.h`
- Validation + persistence: `OpenYourBox/Source/graph/NodeGraph.cpp`
- Rendering: `OpenYourBox/Source/graph/NodeRenderer.cpp`
- Editor orchestration: `OpenYourBox/Source/PluginEditor.cpp`

### Knob / XY / Merge conditioning notes (T003)

- `NodeType::knobInput` and `NodeType::xyTrackpad` are source elements (`SignalKind::conditioning` outputs). They persist `conditioningValue` / `conditioningX`+`conditioningY` in the graph `ValueTree`.
- Connection validation accepts conditioning → Merge and conditioning → existing processing inputs; channel mismatch is resolved by broadcast, not refusal.
- Merge classifies each connected source as audio or conditioning. Add/multiply combine conditioning scalars and broadcast onto audio; concatenate ignores conditioning lanes. Missing conditioning is `c = 0` (audio path unchanged).
- Knob/XY value edits publish `RuntimeControlState` without a full graph recompile. Topology changes still recompile.
- Knob Input and XY Trackpad are excluded from freeze subgraph compilation.

## Implementation Status

Implemented in Phase 2.2 (`tasks.md` T004–T051).

- Knob Input and XY Trackpad appear in the element menu, persist in `ValueTree`, and publish `RuntimeControlState` without a full recompile.
- Merge accepts audio + conditioning; add/multiply broadcast scalars onto audio; missing conditioning is `c = 0`.
- Gain is a real inline property on Activation and TCN (0.1–10.0, default 1.0).
