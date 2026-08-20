# Feature Specification: The Embedded Builder & Interactive Graph (Phase 2)

**Feature Branch**: `002-embedded-builder-graph`

**Created**: 2026-08-19

**Status**: Draft

**Input**: User description: "Phase 2 (Embedded Builder & Manual Freeze) with live performance metrics: select elements and right click to freeze. Add trackpad actions to move and zoom in/out. Graph view should not only be read only — elements should be moveable and connectable with the same logic as ML Forge. Element parameters should be editable from the graph view. TCN should be one element with its parameters editable from the graph view too. Add a menu of elements like in ML Forge, with: audio I/O, linear, conv1D, activation functions. The menu should replace the live TCN menu, because TCN parameters will be editable from the graph view like other elements. Add filter standardly used to avoid DC offset damages if not already here. Add conv dilation in conv and TCN elements. Like in ML Forge, element properties should be listed one by one with new lines — after title, each line should be a text input box followed by the name of the property. Add map view where you can click to move on grid like in ML Forge. Each element with weights should have the randomize weight button and seed (because the TCN menu will disappear with it)."

## Clarifications

### Session 2026-08-19

- Q: What seed format should weighted elements accept for deterministic randomization? → A: Signed 32-bit integer (`-2147483648` to `2147483647`)
- Q: When a user clicks `Randomize Weights` on a weighted element, which parameters should be randomized? → A: All parameters of the targeted element (e.g., weights and biases for Linear; all layer parameters for TCN)
- Q: If a weighted element has not initialized its parameters yet, what should happen when the user clicks `Randomize Weights`? → A: Auto-initialize parameters, then randomize
- Q: Should each element's seed value persist when the project/plugin state is saved and reloaded? → A: Yes, persist seed per element across save/load

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Build a Custom Neural Network from the Element Menu (Priority: P1)

A user opens OpenYourBox, opens the element menu (replacing the previous live TCN menu), and browses available elements: Audio Input, Audio Output, Linear, Conv1D, Activation Functions (ReLU, Sigmoid, Tanh, LeakyReLU), and TCN. The user drags elements onto the graph canvas, positions them freely, and connects them by drawing cables between compatible ports — following the same interaction logic as ML Forge. The resulting graph defines a custom neural network architecture that processes audio in real time.

**Why this priority**: The interactive graph editor is the foundational interaction model for Phase 2. Without the ability to place, move, connect, and configure elements, no other feature can function.

**Independent Test**: Can be fully tested by opening the element menu, dragging three elements (Audio In → Conv1D → Audio Out), connecting them, and verifying audio passes through the graph.

**Acceptance Scenarios**:

1. **Given** the graph view is open, **When** the user opens the element menu, **Then** all available element types are listed (Audio In, Audio Out, Linear, Conv1D, Activation Functions, TCN)
2. **Given** an element is in the menu, **When** the user drags it onto the canvas, **Then** a new node appears at the drop location with its ports visible
3. **Given** two elements on the canvas, **When** the user draws a cable from an output port to a compatible input port, **Then** the connection is established and visually rendered
4. **Given** two elements on the canvas, **When** the user draws a cable between incompatible ports (shape mismatch), **Then** the cable turns red and the connection is refused with a tooltip explaining the dimension mismatch

---

### User Story 2 - Edit Element Parameters Inline from the Graph View (Priority: P1)

A user clicks on any element in the graph (including TCN, Conv1D, Linear, or Activation) and sees its properties listed directly on the node — one property per line, each displayed as a text input box followed by the property name, matching ML Forge's layout. The user edits values (e.g., kernel size, dilation, number of channels) and the changes take effect on the live audio processing immediately.

**Why this priority**: Inline parameter editing is essential for the interactive workflow. Without it, users cannot configure their networks.

**Independent Test**: Can be fully tested by placing a Conv1D element, editing its kernel size and dilation parameters via the inline text inputs, and verifying the audio output changes.

**Acceptance Scenarios**:

1. **Given** a Conv1D element on the canvas, **When** the user views the element, **Then** properties are listed one per line: a text input box followed by the property name (e.g., "3" followed by "Kernel Size")
2. **Given** a TCN element on the canvas, **When** the user views the element, **Then** all TCN parameters (depth, kernel size, channels, dilation) are displayed as editable text input fields
3. **Given** an element with a parameter text input, **When** the user types a new value and confirms, **Then** the parameter updates and the live audio processing reflects the change
4. **Given** the old live TCN menu existed, **When** Phase 2 is active, **Then** the live TCN menu is no longer present — TCN parameters are exclusively edited from the graph view

---

### User Story 3 - Navigate the Graph with Trackpad and Map View (Priority: P2)

A user working on a large graph uses trackpad gestures to pan (two-finger drag) and zoom in/out (pinch gesture). For very large graphs, the user opens a miniature map view that shows the entire graph layout. Clicking a location on the map view instantly moves the main viewport to that area, matching ML Forge's navigation model.

**Why this priority**: Navigation is critical for usability on large graphs but the graph must be buildable first (P1 stories).

**Independent Test**: Can be fully tested by placing 10+ elements spread across a large canvas, using trackpad to pan/zoom, and clicking on the map view to jump to different areas.

**Acceptance Scenarios**:

1. **Given** a graph with multiple elements, **When** the user performs a two-finger drag on the trackpad, **Then** the canvas pans in the corresponding direction
2. **Given** a graph view, **When** the user performs a pinch gesture on the trackpad, **Then** the canvas zooms in or out smoothly
3. **Given** a large graph, **When** the user opens the map view, **Then** a miniature overview of the entire graph is displayed with a viewport rectangle showing the current visible area
4. **Given** the map view is open, **When** the user clicks a location on the map, **Then** the main graph viewport scrolls to center on that location

---

### User Story 4 - Freeze and Unfreeze Selected Elements (Priority: P2)

A user selects one or more elements on the graph (click or marquee selection), right-clicks, and chooses "Freeze Selection." The VST compiles the selected subgraph into a TorchScript `.pt` file via the Python backend. During compilation, a progress indicator is visible. On completion, the selected nodes are atomically replaced by a single Gold BlackBox node with a lock icon. The user can later right-click the BlackBox and choose "Unfreeze" to restore the original modular nodes. Live performance metrics (latency, inference time) are displayed for frozen nodes.

**Why this priority**: Freeze/unfreeze is the core Phase 2 capability from the constitution, but it depends on the graph editor being functional first.

**Independent Test**: Can be fully tested by placing three connected elements, selecting them, choosing "Freeze Selection," verifying the Gold BlackBox appears, checking performance metrics are displayed, and then unfreezing back to modular nodes.

**Acceptance Scenarios**:

1. **Given** a selection of connected Blue nodes, **When** the user right-clicks and selects "Freeze Selection," **Then** a progress indicator appears showing compilation status
2. **Given** compilation completes successfully, **When** the frozen node replaces the selection, **Then** it appears as a single Gold node with a lock icon and audio continues without interruption
3. **Given** a frozen Gold BlackBox node, **When** the user right-clicks and selects "Unfreeze," **Then** the original Blue modular nodes are restored in their previous positions and connections
4. **Given** a frozen Gold BlackBox node, **When** the user inspects it, **Then** live performance metrics (inference latency per buffer) are displayed on or near the node

---

### User Story 5 - Per-Element Weight Randomization and Seed (Priority: P1)

A user working in graph view selects any element that owns trainable weights and can randomize that element's weights directly from the element's inline controls — without needing the removed TCN menu. Each weighted element also exposes a seed input so the user can reproduce a specific randomized state by reusing the same seed value.

**Why this priority**: Weight randomization (RONN-style glitch) is a core creative workflow inherited from Phase 1. Since the TCN menu is being removed, randomization and seed controls must move to per-element inline properties to preserve this capability.

**Independent Test**: Can be tested by placing two weighted elements, randomizing one with a specific seed, verifying only that element's output changes, then reapplying the same seed and confirming the result is identical.

**Acceptance Scenarios**:

1. **Given** a weighted element is visible in graph view, **When** the user views its controls, **Then** a "Randomize Weights" button and a seed input field are displayed
2. **Given** two weighted elements are in the graph, **When** the user triggers randomization on one element, **Then** only the targeted element's parameters are randomized
3. **Given** a non-weighted element (e.g., Audio In, Activation), **When** the user views its controls, **Then** no randomization or seed controls are shown
4. **Given** a weighted element was randomized with seed N, **When** the user reapplies seed N and randomizes again, **Then** the resulting randomized state is reproducible
5. **Given** a frozen (Gold) element, **When** the user attempts to randomize it, **Then** the randomization control is disabled or hidden until the element is unfrozen
6. **Given** a weighted element has not initialized parameters yet, **When** the user clicks "Randomize Weights", **Then** the system auto-initializes parameters for that element and immediately applies randomization
7. **Given** a weighted element has a seed value set, **When** the project/plugin state is saved and later reloaded, **Then** that element's seed value is restored unchanged

---

### User Story 6 - DC Offset Protection via High-Pass Filter (Priority: P3)

The audio processing pipeline includes a standard high-pass filter (DC blocker) to prevent DC offset accumulation that can damage speakers and cause clipping. This filter is applied automatically in the signal chain.

**Why this priority**: DC offset protection is a safety feature. It is important but does not block core graph editing or freeze functionality.

**Independent Test**: Can be fully tested by routing a signal through a graph that is likely to produce DC offset (e.g., certain activation functions or weight configurations), and verifying the output signal has no DC component.

**Acceptance Scenarios**:

1. **Given** audio is processed through the graph, **When** the output signal is analyzed, **Then** DC offset is below an inaudible threshold (< 0.001 amplitude)
2. **Given** a network configuration that would normally produce DC offset, **When** audio passes through the pipeline, **Then** the DC blocker removes the offset without audibly affecting the desired signal

---

### User Story 7 - Convolution Dilation Support (Priority: P2)

Conv1D and TCN elements both expose a "dilation" parameter in their inline property editors. Users can set dilation values to increase the receptive field of convolutional layers without increasing parameter count, enabling more expressive audio processing networks.

**Why this priority**: Dilation is a standard and powerful feature for temporal convolutions in audio processing. It directly enhances the expressiveness of user-built networks.

**Independent Test**: Can be fully tested by placing a Conv1D element, setting its dilation parameter to a value > 1, and verifying the receptive field increases (observable through changed audio characteristics on a test signal).

**Acceptance Scenarios**:

1. **Given** a Conv1D element on the canvas, **When** the user views its properties, **Then** a "Dilation" property is listed with a text input box
2. **Given** a TCN element on the canvas, **When** the user views its properties, **Then** a "Dilation" property is listed with a text input box
3. **Given** a Conv1D element with dilation set to 2, **When** audio is processed, **Then** the convolution operates with dilation factor 2 (receptive field doubles compared to dilation 1)

---

### Edge Cases

- What happens when the user attempts to freeze a single disconnected node with no input/output connections?
- How does the system handle freezing a subgraph that contains shape mismatches (illegal connections that bypassed validation)?
- What happens when the Python backend is unavailable or crashes during a freeze compilation?
- How does the graph behave when the user edits a parameter to an invalid value (e.g., kernel size of 0 or negative dilation)?
- What happens when the user attempts to connect a node's output back to its own input (cycle detection)?
- How does the map view handle an empty graph with no elements?
- What happens when the user zooms out beyond the minimum zoom level or zooms in beyond the maximum?
- If a user clicks "Randomize Weights" on an element whose parameters are not initialized, the system auto-initializes then randomizes without interrupting audio.
- How does the system handle invalid seed input (empty, non-numeric, out-of-range outside signed 32-bit integer)?
- What happens when the user randomizes an element while audio is actively processing?
- How does the system behave when a frozen (Gold) element is selected and randomization is requested?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide an element menu containing: Audio Input, Audio Output, Linear, Conv1D, Activation Functions (ReLU, Sigmoid, Tanh, LeakyReLU), and TCN
- **FR-002**: System MUST allow users to drag elements from the menu onto the graph canvas, placing them at the drop location
- **FR-003**: System MUST allow users to move elements freely on the canvas by clicking and dragging
- **FR-004**: System MUST allow users to connect elements by drawing cables between compatible output and input ports, using the same connection logic as ML Forge
- **FR-005**: System MUST enforce shape compatibility at connection time — incompatible connections are refused with a red cable and tooltip
- **FR-006**: System MUST display element properties inline on each node, one property per line, as a text input box followed by the property name (matching ML Forge layout)
- **FR-007**: System MUST allow real-time editing of element parameters via the inline text input boxes, with changes reflected in live audio processing
- **FR-008**: TCN MUST be represented as a single element with all its parameters (depth, kernel size, channels, dilation) editable from the graph view
- **FR-009**: The previous live TCN menu MUST be removed and replaced by the element menu and inline graph editing
- **FR-010**: System MUST support trackpad two-finger drag for panning the graph canvas
- **FR-011**: System MUST support trackpad pinch gesture for zooming the graph canvas in and out
- **FR-012**: System MUST provide a map view (miniature overview) of the entire graph, where clicking a location moves the main viewport to that area
- **FR-013**: System MUST allow users to select one or more elements (click or marquee), right-click, and choose "Freeze Selection" to compile the subgraph into a TorchScript `.pt` file
- **FR-014**: During freeze compilation, a progress indicator MUST be displayed
- **FR-015**: On successful freeze, selected nodes MUST be atomically replaced by a single Gold BlackBox node with a lock icon, without interrupting audio
- **FR-016**: Users MUST be able to right-click a Gold BlackBox node and choose "Unfreeze" to restore the original Blue modular nodes
- **FR-017**: Frozen Gold BlackBox nodes MUST display live performance metrics (inference latency per buffer)
- **FR-018**: Conv1D and TCN elements MUST expose a "Dilation" parameter in their inline property editors
- **FR-019**: The audio processing pipeline MUST include a DC blocking high-pass filter to prevent DC offset accumulation
- **FR-020**: The graph MUST prevent cyclic connections (output connected back to input of the same node or forming a loop)
- **FR-021**: Every element that owns weights MUST display a "Randomize Weights" button and a seed input field in its inline properties
- **FR-022**: Randomization MUST apply only to the targeted weighted element without altering other elements
- **FR-023**: When a seed value is provided, randomization MUST be deterministic and reproducible for the same element and seed
- **FR-024**: Elements without weights (Audio I/O, Activation Functions) MUST NOT display randomization or seed controls
- **FR-025**: Randomization MUST NOT interrupt audio processing — weight swap is prepared on the GUI thread and applied atomically
- **FR-026**: Seed input MUST be validated as a signed 32-bit integer (`-2147483648` to `2147483647`) with user-facing feedback for invalid values
- **FR-027**: Randomization MUST reinitialize all mutable parameters of the targeted element (for example: weights and biases for Linear; all mutable layer parameters inside TCN)
- **FR-028**: If the targeted weighted element has uninitialized parameters, the system MUST auto-initialize those parameters and then perform randomization in the same user action
- **FR-029**: Each weighted element's seed value MUST be saved and restored with the project/plugin state

### Key Entities

- **Element**: A node on the graph representing a neural network layer or audio I/O point. Has a type, position, list of parameters, and input/output ports.
- **Connection (Cable)**: A link between an output port of one element and an input port of another. Carries shape metadata for validation.
- **BlackBox (Frozen Node)**: A compiled Gold node that replaces a subgraph of Blue modular nodes. Contains a TorchScript `.pt` reference and displays performance metrics.
- **Element Menu**: The UI panel listing all available element types, replacing the former live TCN menu.
- **Map View**: A miniature overview of the full graph, enabling click-to-navigate on large layouts.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can build a complete audio processing graph (input → processing → output) and hear processed audio within 60 seconds of opening the plugin
- **SC-002**: Parameter edits on any element are reflected in the audio output within one audio buffer cycle (< 10 ms at 256-sample buffer)
- **SC-003**: Freeze compilation for graphs with fewer than 10 layers completes in under 2 seconds
- **SC-004**: Frozen node atomic swap completes in under 100 ms with zero audible glitches
- **SC-005**: Graph canvas panning and zooming via trackpad remains smooth at 60 FPS with 50+ elements on screen
- **SC-006**: Users can locate and navigate to any element in a 50+ element graph using the map view within 3 seconds
- **SC-007**: DC offset in the output signal remains below 0.001 amplitude under all network configurations
- **SC-008**: Frozen Gold BlackBox nodes display inference latency that is lower than the equivalent Live Blue node latency for the same subgraph
- **SC-009**: 100% of weighted element types expose both a randomize control and a seed input in graph view
- **SC-010**: Users can randomize a target weighted element in under 5 seconds from selecting it
- **SC-011**: Reapplying the same seed on the same element reproduces matching output behavior in repeated trials
- **SC-012**: After save and reload, 100% of weighted elements restore their last saved seed values correctly

## Assumptions

- Phase 1 (Live Player & RONN) is complete and the VST loads, processes audio, and renders the graph view
- The ML Forge codebase (`.ignore/ml_forge-main/`) is available as reference for node graph interaction patterns, element menu design, property layout, and map view implementation
- The Python backend for freeze compilation communicates with the VST via local IPC using JSON-serialized architecture descriptions
- LibTorch C++ API is available for both live (`torch::nn`) and frozen (`torch::jit`) execution
- Trackpad gesture support is provided by the OS/windowing layer (JUCE/ImGui handles scroll and pinch events)
- The DC blocking filter uses a standard first-order high-pass design (cutoff ~20 Hz) which is sufficient for audio applications
- "Elements with weights" includes all current and future element types that have mutable trainable parameters (Linear, Conv1D, TCN)
- Frozen (Gold) elements are treated as non-randomizable until explicitly unfrozen
- "All parameters" means all mutable trainable parameters owned by the targeted element and its internal sub-layers.
- Per-element seeds are part of the persisted graph state and reload with the rest of the element configuration.
