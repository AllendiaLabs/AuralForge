# Phase 0 Research: Embedded Builder & Interactive Graph

## Decision 1: Keep the graph editor entirely inside the existing ImGui/JUCE host

- **Decision**: Extend the current in-plugin ImGui-based graph surface instead of introducing a separate editor shell or external tool.
- **Rationale**: The constitution requires the VST to be the sole interface. Reusing the existing host also keeps input routing, rendering lifecycle, and plugin-state integration centralized.
- **Alternatives considered**:
  - Separate standalone editor: rejected because it violates the single-interface rule.
  - Rebuild the editor in a different UI stack: rejected because it increases integration complexity without solving a constitutional requirement.

## Decision 2: Use ML Forge interaction patterns as the UX reference, not as a second runtime

- **Decision**: Mirror ML Forge’s node interactions, palette behavior, map view, and inline property layout while keeping AuralForge’s existing runtime ownership and plugin-specific rules.
- **Rationale**: The spec explicitly asks for ML Forge-like graph editing, but the plugin still needs AuralForge-specific Blue/Gold execution behavior, freeze flow, and real-time audio safety.
- **Alternatives considered**:
  - Pixel-perfect ML Forge duplication: rejected because AuralForge has different runtime semantics and constraints.
  - Minimal graph tweaks without matching ML Forge patterns: rejected because it misses the user’s requested workflow target.

## Decision 3: Represent TCN as a single editable graph element with internal sub-layer ownership

- **Decision**: Keep `TCN` as one graph element in the editor while treating its internal layers as owned implementation detail for parameter editing and randomization.
- **Rationale**: The spec wants TCN editable like other nodes while also allowing all TCN parameters and internal mutable parameters to be controlled from graph view.
- **Alternatives considered**:
  - Expand TCN into many visible nodes: rejected because it conflicts with the requirement that TCN be one element.
  - Keep TCN parameters in a separate menu: rejected because the live TCN menu is being removed.

## Decision 4: Freeze via local JSON IPC contract and atomic runtime swap

- **Decision**: Serialize a selected valid subgraph into a local JSON payload, send it to the Python worker, receive a compiled `.pt` artifact, preload it off-thread, and atomically replace the live subgraph with one Gold BlackBox node.
- **Rationale**: This follows the constitution’s mandated data flow and preserves real-time safety by doing all expensive work outside the audio thread.
- **Alternatives considered**:
  - In-audio-thread compilation/loading: rejected due to hard real-time violations.
  - Freeze entire graph only: rejected because the constitution specifies manual granular freeze.

## Decision 5: Seeded randomization is per-element, persisted, and deterministic

- **Decision**: Each weighted element owns a persisted signed 32-bit seed and a randomize action that reinitializes all mutable parameters of that element only.
- **Rationale**: The user clarified deterministic save/load behavior and scope boundaries, and the constitution already endorses live random weight manipulation for creative workflows.
- **Alternatives considered**:
  - Global randomization controls: rejected because the TCN menu is removed and the workflow should live on each element.
  - Session-only seeds: rejected because it breaks reproducible recall.
  - Randomize weights but not biases/internal sub-layers: rejected because the user explicitly wants all mutable parameters of the selected element.

## Decision 6: Auto-initialize uninitialized weighted elements before randomization

- **Decision**: If a weighted element has not initialized its mutable parameters yet, the same user action first initializes and then randomizes them.
- **Rationale**: This avoids dead-end UI states and gives users a consistent one-click creative action.
- **Alternatives considered**:
  - Block with error: rejected because it adds friction to a frequent exploratory workflow.
  - Ask for confirmation first: rejected because it interrupts rapid sound iteration.

## Decision 7: Trackpad pan/zoom and map view are first-class navigation features

- **Decision**: Support two-finger pan, pinch zoom, and overview map navigation directly in graph view.
- **Rationale**: Large graphs are explicitly in scope, and navigation quality is part of the user-visible success criteria.
- **Alternatives considered**:
  - Scrollbars only: rejected because it does not match the requested ML Forge workflow.
  - Map view as optional future work: rejected because it is explicitly requested in the feature spec.

## Decision 8: Integrate a DC blocker as a standard post-graph safety stage

- **Decision**: Treat DC offset protection as a standard output safety stage applied to graph audio output rather than exposing it as optional user-configured graph topology in Phase 2.
- **Rationale**: The spec frames this as a standard safety protection, and keeping it standard reduces user error risk.
- **Alternatives considered**:
  - User-addable filter node only: rejected because the request emphasizes standard protection.
  - No DC blocker until later: rejected because speaker-safety protection is explicitly requested now.

## Decision 9: Performance metrics belong to frozen-node inspection and validation workflows

- **Decision**: Display live performance metrics for frozen nodes in or near the node and use them as part of quickstart validation rather than as a broad observability subsystem.
- **Rationale**: The spec asks for live performance metrics in the freeze workflow, but Phase 2 does not require a full telemetry stack.
- **Alternatives considered**:
  - Full tracing/log aggregation: rejected as over-scoped for this phase.
  - No visible metrics, tests only: rejected because the user explicitly requested live metrics.

## Decision 10: Compile the editable document into immutable live graph runtimes

- **Decision**: Add a dedicated live graph compiler and publisher between the
  editable `NodeGraph` and the audio callback. The compiler validates the DAG,
  infers channels, constructs element-local weights, prepares mutable history
  off-thread, and atomically publishes a complete runtime.
- **Rationale**: The original task list only synchronized inline TCN controls
  and would have left newly placed Linear, Conv1D, and Activation nodes as
  visual metadata. A real modular runtime is required for FR-002, FR-004,
  FR-007, FR-022, and the constitution's Live Blue engine.
- **Alternatives considered**:
  - Keep the Phase 1 TCN as the only audio runtime: rejected because custom
    graphs would not affect sound.
  - Mutate a live module in place: rejected because it races the audio thread
    and breaks deterministic element-local randomization.
