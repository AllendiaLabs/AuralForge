# Phase 0 Research: Signal Analysis & Expressive Input Controls

## Decision 1: Compute analysis from background-refreshed graph snapshots, not on the audio thread

**Decision**: Generate per-element analysis from immutable runtime snapshots on the message/background thread. The audio thread publishes lightweight capture state (live input level for transfer marker, revision tokens) only.

**Rationale**: Constitution forbids blocking/allocation on the audio thread. Existing `LiveGraphPublisher` and freeze coordinator patterns already separate editor from runtime.

**Alternatives considered**:
- Analysis inside `processBlock()`: rejected (real-time violation).
- Recompute from mutable editor document every frame: rejected (drift from runtime, poor scaling).

## Decision 2: Live audio preferred; white-noise probe fallback

**Decision**: Static curve computation uses live audio when suitable; otherwise falls back to an internal probe (white noise for frequency/phase; ramp or noise-derived levels for transfer).

**Rationale**: Matches spec clarification. Keeps plots useful when stopped, silent, or partially disconnected.

**Alternatives considered**:
- Live only: rejected (empty plots during editing).
- Probe only: rejected (disconnects from heard signal during performance).

## Decision 3: Dual curve families — chain and element-only

**Decision**: Each analysis view returns two curve sets for the selected node: **chain** (cumulative upstream through node output) and **element-only** (selected node isolated with probe injected at its input boundary). Both include all channel/feature dimensions at the analysis point.

**Rationale**: User clarification separates cumulative chain behavior from single-stage contribution. Element-only is computed by compiling/running an isolated subgraph fragment for the selected node while chain uses the full upstream path.

**Alternatives considered**:
- Chain only: rejected (cannot inspect single-stage contribution).
- Separate panels: rejected (spec requires same view with two families).

## Decision 4: N-channel/feature-dimension overlay (not stereo-only)

**Decision**: Analysis series expose one trace per channel/feature dimension at the analysis tensor shape. UI renders all traces on shared axes with cyclic distinguishable colors and a compact legend (index or L/R labels when stereo). No artificial cap on dimension count in v1; if readability suffers at very high N, use thinner lines and scrollable legend — not data omission.

**Rationale**: Constitution v1.2.0 and spec require latent multi-channel support, not L/R only.

**Alternatives considered**:
- Stereo pair fields only (`left`/`right`): rejected (cannot represent 32/64-channel latent paths).
- Hard cap at 8 traces: rejected (contradicts spec).

## Decision 5: Transfer live marker on chain curve only; hidden when not playing

**Decision**: During active playback, sample the current input→output operating point from published live capture and draw a marker constrained to the **chain** transfer curve. When playback stops, hide the marker (static curves remain).

**Rationale**: Spec requires on-curve marker during playback; static curves always visible. Hiding when stopped avoids misleading frozen positions (open edge case resolved for planning).

**Alternatives considered**:
- Hold last marker position: rejected (implies false operating point while stopped).
- Marker on element-only curve: rejected (spec targets chain curve).

## Decision 6: Gold BlackBox analysis parity at compiled boundary

**Decision**: Gold nodes expose the same chain/element-only views as Blue nodes, reflecting compiled BlackBox I/O behavior without decomposing internal modular detail.

**Rationale**: Spec and constitution require parity; dual-engine model treats Gold as alternate execution of same workflow.

**Alternatives considered**:
- Blue-only analysis: rejected.
- Reduced Gold views: rejected.

## Decision 7: Knob Input and XY Trackpad as graph source elements (not inline control modes)

**Decision**: Add `knobInput` and `xyTrackpad` `NodeType` values — source nodes analogous to `audioInput`. They output runtime conditioning scalars (1D and 2D). They persist position, output values, and cable topology in the graph `ValueTree`. They do not read/write inline architectural properties on processing nodes.

**Rationale**: User clarification + steerable NAfx g(x, c) model. Conditioning is network input, not property UI mode.

**Alternatives considered**:
- Per-property knob/XY display modes: rejected (superseded by clarification).
- Dedicated conditioning ports on TCN: rejected (ports unchanged from Phase 2).

## Decision 8: Merge as primary audio+conditioning hub; direct connections allowed

**Decision**: Extend Merge to accept **conditioning** inputs alongside **audio** inputs. Knob/XY may connect to Merge or directly to processing element input ports (not refused). When Merge has no conditioning sources, conditioning contribution defaults to **c = 0**. Add/multiply modes combine conditioning scalars; audio+conditioning merge broadcasts conditioning into compatible tensor ops per mode (planning detail in runtime); concatenate remains audio-dimension expansion only.

**Rationale**: Spec: Merge primary for audio+control; direct wiring valid. Steerable training uses c = 0 baseline.

**Alternatives considered**:
- Mandatory Merge-only routing: rejected (user clarification).
- Separate Merge node type for control: rejected (reuse existing Merge element).

## Decision 9: Gain as first-class node property on Activation and TCN

**Decision**: Add validated continuous `gain` property (default 1.0, range 0.1–10.0) via existing `NodeProperty` system; runtime applies pre-nonlinearity scaling; analysis transfer curves reflect Gain changes.

**Rationale**: Constitution mandate; reuses existing property→processor pipeline.

**Alternatives considered**:
- Editor-only gain: rejected (no persistence/runtime parity).

## Decision 10: Persist conditioning and analysis preferences in graph ValueTree

**Decision**: Serialize Knob/XY node state, Merge mode, gain values, and per-node selected analysis view in existing `NodeGraph::toValueTree()` / `restoreFromValueTree()`.

**Rationale**: Single document for topology + recall; matches seeds/viewport pattern.

**Alternatives considered**:
- Separate settings store: rejected.

## Decision 11: UI contracts instead of external APIs

**Decision**: Phase 1 contracts describe editor/runtime/UI behavior, not HTTP/IPC.

**Rationale**: Internal plug-in feature spanning graph, DSP, and UI subsystems.

**Alternatives considered**:
- No contracts: rejected (multi-subsystem feature benefits from explicit rules).
