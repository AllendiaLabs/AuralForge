# Phase 0 Research: Signal Analysis & Expressive Input Controls

## Decision 1: Compute analysis views from background-refreshed graph snapshots, not directly on the audio thread

**Decision**: Generate per-element cumulative analysis data from immutable runtime snapshots owned by the editor or a helper worker on the message/background side. The audio thread only publishes lightweight state needed to identify the current live/frozen graph and never performs full plot generation, FFT-style analysis, or buffer allocation for Phase 2.2 views.

**Rationale**: The constitution forbids blocking or allocating on the audio thread. Existing architecture already separates message-thread graph editing from runtime processing and uses asynchronous publication patterns (`LiveGraphPublisher`, freeze coordinator). Reusing that pattern for analysis snapshots keeps UI inspection responsive without risking audio dropouts.

**Alternatives considered**:
- Run all analysis directly inside `processBlock()`: rejected because it risks violating zero-allocation and real-time guarantees.
- Recompute full graph behavior from the mutable editor document on every frame: rejected because it can drift from actual runtime state and scales poorly as graphs grow.
- Require manual offline analysis refresh: rejected because it conflicts with the feature goal of live, cumulative inspection.

## Decision 2: Use live audio when available, otherwise fall back to a standard internal probe signal such as white noise

**Decision**: Analysis views consume live audio whenever a suitable active signal is present. If the input is silent, disconnected, or otherwise unsuitable for stable visualization, the analysis path falls back to a standard internal probe signal, with white noise as the default example called out in the spec clarification.

**Rationale**: This preserves a meaningful connection to what the user is hearing during normal operation while keeping plots available in otherwise empty states. It also supports deterministic validation of analysis behavior without requiring real-world audio input at all times.

**Alternatives considered**:
- Live audio only: rejected because plots disappear during silence and disconnected editing, reducing usability.
- Probe signal only: rejected because it disconnects the analysis view from the currently heard signal during performance.

## Decision 3: Gold BlackBox nodes expose the same analysis views as Blue live nodes

**Decision**: Frozen Gold nodes must provide the same per-element cumulative analysis views as Blue nodes. Their analysis reflects compiled behavior at the BlackBox boundary rather than reconstructed internal modular detail.

**Rationale**: The clarification explicitly chose parity. This keeps freeze/unfreeze comparisons understandable, avoids a mode-dependent UX gap, and aligns with the constitution's dual-engine model where Blue and Gold are different execution forms of the same user workflow.

**Alternatives considered**:
- Blue-only analysis: rejected because it breaks parity and makes freeze behavior harder to inspect.
- Reduced analysis for Gold nodes only: rejected because it introduces inconsistent semantics without strong benefit.

## Decision 4: Persist control and analysis preferences in the existing graph document

**Decision**: Persist knob mode selections, XY parameter bindings, XY pointer defaults, chosen analysis view, and gain property values in the graph `ValueTree` alongside existing node properties, viewport state, and seeds.

**Rationale**: The repository already serializes graph topology, node properties, viewport state, and per-node randomization seeds through `NodeGraph::toValueTree()` / `restoreFromValueTree()`. Extending that document avoids creating parallel persistence systems and lets state recall remain testable through the existing processor integration path.

**Alternatives considered**:
- Store all new UI state only in editor memory: rejected because it breaks save/reload requirements.
- Add a separate settings file: rejected because the constitution favors a single plug-in interface and this would complicate recall and portability.

## Decision 5: Model gain as a first-class continuous node property on Activation and TCN nodes

**Decision**: Add gain to the existing node-property system as a validated continuous property for Activation and TCN nodes, with runtime application happening through the same editor-to-processor configuration flow used by other node properties.

**Rationale**: The current graph model already owns per-node properties and propagates edits through `propertyChanged` callbacks into the processor/runtime. Extending this path minimizes architectural churn and keeps gain visible to text, knob, XY, persistence, and analysis workflows through one canonical property definition.

**Alternatives considered**:
- Add gain as an editor-only transient modifier: rejected because it would not persist or participate cleanly in runtime compilation/analysis.
- Encode gain outside node properties in processor-only state: rejected because it would create duplicate configuration paths.

## Decision 6: Treat knob and XY controls as alternate views over canonical parameter values

**Decision**: Text input, knob input, and XY trackpad bindings are presentation/control modes over the same underlying validated node properties. The graph document remains the source of truth for current parameter values.

**Rationale**: This avoids duplicate parameter state, preserves current inline editing behavior, and keeps validation centralized. It also makes it easier to test save/restore and runtime updates because all control modalities converge on the same committed property values.

**Alternatives considered**:
- Separate knob-owned and XY-owned value stores: rejected because it invites divergence and complicates synchronization.
- Global control mode for the entire graph: rejected because the feature spec is per-parameter/per-element.

## Decision 7: Document UI contracts instead of external service APIs

**Decision**: Phase 1 contracts for this feature are UI/runtime contracts describing graph-editor interactions, analysis behavior, and persistence semantics rather than HTTP or IPC APIs.

**Rationale**: Phase 2.2 adds internal editor/runtime behaviors, not a new external service. The most useful contracts here are stable behavior descriptions that tasks and tests can implement against.

**Alternatives considered**:
- No contracts: rejected because this feature spans multiple subsystems and benefits from explicit interaction rules.
- Over-specified implementation pseudo-code: rejected because plan artifacts should remain design-focused.
