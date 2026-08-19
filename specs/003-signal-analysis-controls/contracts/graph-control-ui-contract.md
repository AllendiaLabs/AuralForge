# Contract: Graph Control UI

## Purpose

Define the user-facing interaction rules for gain editing, knob controls, XY bindings, and analysis access in the Phase 2.2 graph editor.

## Node-Level Requirements

1. Any node with Phase 2.2 analysis support MUST expose an entry point to open its analysis panel.
2. Activation and TCN nodes MUST expose a `Gain` property in the same inline property system as other node parameters.
3. Gold BlackBox nodes MUST expose the same analysis access path as Blue nodes.

## Parameter Editing Rules

1. Canonical parameter values remain node-property values; text, knob, and XY are alternate control surfaces for those values.
2. A parameter that supports knob mode MUST show both its control surface and current numeric value.
3. Knob editing MUST honor the parameter's existing range and stepping rules.
4. Parameters that do not support knob or XY control MUST remain text-only.

## XY Binding Rules

1. An XY control binds exactly two distinct supported parameters on the same node.
2. The user MUST be able to choose which property maps to X and which maps to Y.
3. Moving the XY pointer MUST update both bound parameters in real time.
4. The UI MUST display the current values of both bound parameters while the XY control is active.
5. If one bound property becomes invalid or unsupported, the XY binding MUST be cleared or marked invalid on restore rather than silently binding to a different property.

## Analysis Panel Rules

1. The panel MUST offer transfer, frequency, and phase analysis views.
2. Left and right channels MUST be shown on the same plot using distinguishable visual styling.
3. The panel MUST continue to function for Gold nodes and for situations where live input is unavailable.
4. When probe fallback is active, the UI MUST indicate that analysis is probe-driven rather than live-driven.

## Persistence Rules

1. Gain values MUST persist with normal graph state recall.
2. Per-parameter knob/text mode selections MUST persist with graph state recall.
3. XY axis bindings MUST persist with graph state recall.
4. Selected analysis-view preferences MAY persist per node if the implementation chooses node-local preferences; if not, the implementation must define one consistent session-level default.
