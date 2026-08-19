# Feature Specification: Signal Analysis & Expressive Input Controls (Phase 2.2)

**Feature Branch**: `003-signal-analysis-controls`

**Created**: 2026-08-19

**Status**: Draft

**Input**: User description: "Phase 2.2 — Signal Analysis & Expressive Input Controls: per-element visualization graphs showing cumulative sound transformation (transfer function, frequency response, related analysis views; left and right channels on same plots); gain control on activation function and TCN elements controlling nonlinearity slope; knob inputs for continuous parameters; XY trackpad for two-axis simultaneous control of paired parameters."

## Clarifications

### Session 2026-08-19

- Q: What should drive the per-element analysis plots when there is no suitable live input signal available? → A: Use live audio if available, or a standard signal such as white noise if not.
- Q: Should frozen Gold BlackBox nodes support the same per-element analysis views as live Blue nodes? → A: Yes, Gold nodes support the same cumulative analysis views as Blue nodes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Inspect Cumulative Signal Transformation at Any Element (Priority: P1)

A user building an audio processing graph selects any element and opens its analysis panel. The panel displays how the signal has been transformed cumulatively from the graph input through every upstream element to the selected element's output. The user can switch between analysis views — transfer function (input vs. output amplitude), frequency response, and phase response — to understand how the network is shaping sound at that point in the chain. Left and right channels are overlaid on the same plot so the user can compare stereo behavior at a glance.

**Why this priority**: Per-element analysis is the defining capability of Phase 2.2. It gives users immediate visual feedback on how each stage alters the signal, which is essential for intentional sound design and debugging.

**Independent Test**: Can be fully tested by building a simple graph (Audio In → Activation → Audio Out), selecting the Activation element, opening its analysis panel, and verifying that transfer-function and frequency-response plots reflect the cumulative transformation up to that element with both channels visible on the same axes.

**Acceptance Scenarios**:

1. **Given** a connected graph with audio flowing, **When** the user selects any processing element, **Then** an analysis panel is available showing cumulative transformation up to that element's output
2. **Given** the analysis panel is open on an element, **When** the user switches to the transfer-function view, **Then** input vs. output amplitude curves for left and right channels are displayed on the same plot with distinguishable channel styling
3. **Given** the analysis panel is open, **When** the user switches to the frequency-response view, **Then** magnitude (and optionally phase) curves for left and right channels are displayed on the same plot
4. **Given** the user selects a downstream element in a multi-element chain, **When** the analysis panel updates, **Then** the displayed curves reflect the cumulative effect of all upstream elements, not just the selected element in isolation
5. **Given** stereo audio is being processed, **When** the user views any analysis plot, **Then** both left and right channel traces appear on the same plot for direct comparison
6. **Given** the user selects a frozen Gold BlackBox node, **When** the analysis panel is opened, **Then** the same cumulative analysis views available on Blue live nodes are shown for the compiled behavior of that node

---

### User Story 2 - Shape Nonlinearity Steepness with Gain on Activation and TCN Elements (Priority: P1)

A user working with an Activation Function element (ReLU, Sigmoid, Tanh, LeakyReLU) or a TCN element adjusts a dedicated **Gain** parameter to control the slope of the nonlinearity. Increasing gain steepens the transfer characteristic; decreasing gain flattens it. The change takes effect in live audio processing immediately, and the updated transfer curve is reflected in the element's analysis view.

**Why this priority**: Gain-as-slope control is a constitution-mandated capability that directly extends the expressiveness of two key element types. It pairs naturally with the analysis views introduced in User Story 1.

**Independent Test**: Can be fully tested by placing an Activation element, adjusting its Gain parameter from minimum to maximum, and verifying both the audible output and the transfer-function plot change to reflect a steeper or flatter nonlinearity.

**Acceptance Scenarios**:

1. **Given** an Activation Function element on the canvas, **When** the user views its inline properties, **Then** a **Gain** parameter is listed alongside existing properties
2. **Given** a TCN element on the canvas, **When** the user views its inline properties, **Then** a **Gain** parameter is listed alongside existing properties
3. **Given** an Activation or TCN element with Gain set to a higher value, **When** audio is processed, **Then** the nonlinearity slope is steeper compared to a lower Gain value (observable in both audio character and transfer-function plot)
4. **Given** the user adjusts Gain during live playback, **When** the value changes, **Then** the audio output and analysis plots update without interruption

---

### User Story 3 - Edit Continuous Parameters with Rotary Knobs (Priority: P2)

A user editing continuous numeric parameters (e.g., Gain, kernel size, dilation, channel count where applicable) can switch from text input to a rotary knob control. The user drags vertically or horizontally on the knob to increase or decrease the value, with the current value displayed numerically. Knob editing is available for any continuous parameter that already supports text input editing from Phase 2.

**Why this priority**: Knob inputs improve tactile, real-time parameter exploration during performance and sound design. They depend on the inline parameter model from Phase 2 but do not block analysis or gain features.

**Independent Test**: Can be fully tested by selecting a Conv1D element, switching its Dilation parameter to knob mode, rotating the knob to change the value, and verifying the audio output updates accordingly.

**Acceptance Scenarios**:

1. **Given** an element with a continuous numeric parameter, **When** the user views that parameter's control, **Then** they can choose between text input and rotary knob input modes
2. **Given** a parameter in knob mode, **When** the user drags on the knob, **Then** the value changes smoothly within the parameter's valid range and the live audio reflects the change
3. **Given** a parameter in knob mode, **When** the user views the control, **Then** the current numeric value is displayed alongside the knob
4. **Given** a discrete or integer-only parameter with strict step constraints, **When** the user switches to knob mode, **Then** the knob snaps to valid step values

---

### User Story 4 - Control Two Parameters Simultaneously with an XY Trackpad (Priority: P2)

A user assigns two continuous parameters of the same element to an XY trackpad control. Moving a pointer within the trackpad area adjusts both parameters at once — horizontal position maps to one parameter, vertical position to the other. This enables expressive, two-dimensional exploration of parameter space (e.g., Gain vs. dilation, or kernel size vs. channels) during live performance.

**Why this priority**: The XY trackpad unlocks performative, gestural control over paired parameters. It complements knob inputs and is most valuable once continuous parameter editing (Phase 2) and gain control (User Story 2) exist.

**Independent Test**: Can be fully tested by assigning Gain (X-axis) and Dilation (Y-axis) on a TCN element to an XY trackpad, moving the pointer, and verifying both parameters and the audio output change simultaneously.

**Acceptance Scenarios**:

1. **Given** an element with at least two continuous parameters, **When** the user opens parameter input options, **Then** they can assign any two continuous parameters to the X and Y axes of an XY trackpad
2. **Given** an XY trackpad is configured for an element, **When** the user moves the pointer within the trackpad, **Then** both assigned parameters update in real time and audio processing reflects the changes
3. **Given** an XY trackpad is active, **When** the user views the control, **Then** the current values of both assigned parameters are displayed
4. **Given** the user reassigns axis bindings, **When** they select new parameters for X and Y, **Then** the trackpad immediately maps to the newly assigned parameters without requiring a graph restart

---

### Edge Cases

- What happens when the user opens analysis views on an element that has no upstream audio connection (disconnected or silent input)?
- How do analysis plots behave when the selected element is a frozen (Gold) BlackBox node — do cumulative views still reflect the compiled subgraph's behavior?
- What happens when Gain is set to its minimum or maximum boundary value on Activation or TCN elements?
- How does the system handle invalid Gain input (negative, zero, non-numeric) entered via text while knob mode is also available?
- What happens when the user switches between analysis view types while parameters are being actively adjusted?
- How do analysis plots update when the graph topology changes (element added, removed, or reconnected upstream of the selected element)?
- What happens when a parameter assigned to an XY trackpad axis is switched to knob-only or text-only mode — is the trackpad binding cleared?
- How does knob input behave when the parameter's valid range is very small vs. very large?
- What happens when left and right channels carry identical mono-derived signals — do analysis plots still render both traces without visual overlap confusion?
- How does the system behave when analysis is requested on Audio Input or Audio Output elements (boundary nodes with no internal processing)?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide a per-element analysis panel accessible from any element on the graph canvas
- **FR-002**: Analysis views MUST show the cumulative signal transformation from graph input through all upstream elements to the selected element's output
- **FR-003**: Analysis panel MUST include at minimum: transfer-function view (input vs. output amplitude), frequency-response view (magnitude), and phase-response view
- **FR-004**: All analysis plots MUST display left and right channel traces on the same plot with visually distinguishable styling
- **FR-005**: Analysis plots MUST update when upstream graph topology, parameters, or weights change
- **FR-005a**: Analysis views MUST use live audio as the driving signal when a suitable live input is available, and MUST fall back to a standard internal probe signal when it is not
- **FR-005b**: Frozen Gold BlackBox nodes MUST support the same cumulative analysis views as Blue live nodes, based on their compiled behavior
- **FR-006**: Activation Function elements MUST expose a **Gain** parameter that controls the slope of the nonlinearity
- **FR-007**: TCN elements MUST expose a **Gain** parameter that controls the slope of the nonlinearity applied within the TCN block
- **FR-008**: Gain parameter changes MUST take effect in live audio processing without interrupting playback
- **FR-009**: Adjusting Gain MUST produce observable changes in the element's transfer-function analysis view
- **FR-010**: System MUST allow users to edit continuous numeric parameters using rotary knob controls as an alternative to text input
- **FR-011**: Knob controls MUST display the current numeric value alongside the rotary control
- **FR-012**: Knob input MUST respect each parameter's valid range and step constraints
- **FR-013**: System MUST provide an XY trackpad control allowing simultaneous adjustment of two continuous parameters on the same element
- **FR-014**: Users MUST be able to assign which parameter maps to the X axis and which maps to the Y axis
- **FR-015**: XY trackpad MUST update both assigned parameters and live audio in real time during pointer movement
- **FR-016**: XY trackpad MUST display current values for both assigned parameters
- **FR-017**: Input mode preferences (text, knob, or XY trackpad assignment) MUST persist when the project/plugin state is saved and reloaded
- **FR-018**: Analysis, knob, and XY trackpad interactions MUST NOT interrupt audio processing — updates are applied without audible glitches
- **FR-019**: Analysis views MUST remain usable at the constitution's UI responsiveness target (60 FPS) while audio is actively processing

### Key Entities

- **Analysis Panel**: A per-element UI region displaying cumulative signal transformation plots. Contains view selector (transfer function, frequency response, phase response) and renders left/right channel traces on shared axes.
- **Cumulative Signal Snapshot**: The computed input/output relationship at a selected graph point, aggregating all upstream element effects. Refreshed when topology, parameters, or weights change.
- **Gain Parameter**: A continuous control on Activation Function and TCN elements that scales nonlinearity slope. Distinct from weight randomization controls.
- **Knob Input**: A rotary control bound to a single continuous parameter, offering an alternative to inline text entry.
- **XY Trackpad**: A two-axis control bound to a pair of continuous parameters on the same element, mapping horizontal and vertical pointer position to parameter values.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can open per-element analysis views and identify cumulative transfer-function and frequency-response characteristics within 10 seconds of selecting an element
- **SC-002**: Left and right channel traces are visible on the same plot in 100% of analysis views when stereo audio is active
- **SC-003**: Analysis plots reflect parameter or topology changes within one refresh cycle after the change (< 500 ms perceived latency)
- **SC-004**: Users can adjust Gain on Activation or TCN elements and hear an audible difference in nonlinearity character within one audio buffer cycle
- **SC-005**: Transfer-function plots visibly change when Gain is adjusted, confirming alignment between analysis and audio behavior
- **SC-006**: Users can switch any continuous parameter from text input to knob control and adjust its value in under 3 seconds
- **SC-007**: Users can configure and use an XY trackpad to control two parameters simultaneously, with both values and audio output updating in real time
- **SC-008**: Graph canvas and analysis panel remain responsive at 60 FPS during active audio processing with analysis views open (per constitution UI benchmark)
- **SC-009**: After save and reload, 100% of knob mode selections and XY trackpad axis assignments are restored correctly
- **SC-010**: Zero audible glitches occur when switching analysis view types, adjusting Gain, or moving the XY trackpad pointer during live playback

## Assumptions

- Phase 2 (Embedded Builder & Interactive Graph) is complete: users can build graphs, edit inline text parameters, and process audio through connected elements
- Analysis computations use live audio when a suitable input signal is available; otherwise they fall back to a standard internal probe signal such as white noise rather than requiring offline rendering
- Gain on Activation elements scales the input before the activation function is applied (equivalent to adjusting pre-activation slope); default range is 0.1 to 10.0 with 1.0 as neutral
- Gain on TCN elements applies the same slope-scaling concept to the TCN block's internal nonlinearity stages
- "Continuous parameters" includes all numeric parameters editable via text input in Phase 2 (Gain, dilation, kernel size, channels, depth, etc.) unless explicitly discrete-only
- Knob and XY trackpad controls coexist with text inputs — users choose the input modality per parameter (knob) or per parameter pair (XY trackpad), not globally
- Phase response view is included as a standard analysis type alongside transfer function and frequency response
- Frozen (Gold) BlackBox nodes expose cumulative analysis based on their compiled behavior; internal modular detail is not required to be decomposed in analysis views
- Analysis on boundary nodes (Audio In, Audio Out) shows passthrough or final-output characteristics rather than internal layer transforms
