# Quickstart: Signal Analysis & Expressive Input Controls

## Purpose

Validate Phase 2.2 end to end in the existing AuralForge plug-in/editor workflow.

## Prerequisites

- Project dependencies installed via CMake, including JUCE, LibTorch, Dear ImGui, and Python 3
- A build directory configured for the current platform
- Existing Phase 2 graph editor functional

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Validation Scenarios

### 1. Live-node cumulative analysis

1. Launch the plug-in editor.
2. Build a simple graph such as `Audio In -> Activation -> Audio Out`.
3. Select the Activation node and open its analysis panel.
4. Confirm the transfer, frequency, and phase views are available.
5. Confirm left and right channels are shown on the same plot.

Expected outcome:
- Analysis opens within the selected node workflow.
- Cumulative plots reflect the selected node's upstream signal path.
- Stereo traces are visually distinct on shared axes.

### 2. Probe fallback during silence or disconnected input

1. Leave the graph connected but provide no suitable live input, or select a node with no usable upstream signal.
2. Open the analysis panel.

Expected outcome:
- Analysis still renders using the fallback probe path.
- The UI indicates probe-driven behavior rather than appearing broken or empty.

### 3. Gain control on Activation and TCN

1. Add an Activation node and a TCN node in separate validation runs.
2. Edit the `Gain` property on each while audio is running.
3. Re-open or watch the transfer-function view as Gain changes.

Expected outcome:
- Audio character changes immediately.
- Transfer curves visibly steepen or flatten with Gain changes.
- No audible glitch or editor freeze occurs.

### 4. Knob-mode parameter editing

1. Choose a node with continuous editable properties.
2. Switch one supported property from text input to knob mode.
3. Drag the knob through multiple values, including boundary values.

Expected outcome:
- The current numeric value remains visible.
- Property values respect bounds and step constraints.
- Runtime audio updates track the knob without interruption.

### 5. XY trackpad paired control

1. Configure an XY binding on one node using two supported continuous parameters.
2. Move the XY pointer through corners and center positions.
3. Observe the bound parameter values and audio output.

Expected outcome:
- Both parameters change together in real time.
- The current X- and Y-bound values are visible.
- The binding remains attached to the chosen node and parameters.

### 6. Gold-node analysis parity

1. Freeze a valid Blue-node chain into a Gold BlackBox.
2. Select the resulting Gold node.
3. Open the analysis panel and cycle through all analysis views.

Expected outcome:
- Gold nodes expose the same analysis entry points as Blue nodes.
- Cumulative plots reflect compiled behavior at the BlackBox boundary.
- No unfreeze is required to inspect the frozen result.

### 7. State recall

1. Configure gain, knob mode, XY binding, and analysis view preferences.
2. Save plug-in state and reload it.

Expected outcome:
- Graph topology and existing randomization state still restore correctly.
- Gain values, knob modes, XY bindings, and selected analysis views restore correctly.

## Related Artifacts

- Data model: `specs/003-signal-analysis-controls/data-model.md`
- Analysis/runtime contract: `specs/003-signal-analysis-controls/contracts/analysis-runtime-contract.md`
- UI interaction contract: `specs/003-signal-analysis-controls/contracts/graph-control-ui-contract.md`
