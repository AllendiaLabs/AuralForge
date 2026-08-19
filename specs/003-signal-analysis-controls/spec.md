# Feature Specification: Signal Analysis & Expressive Input Controls (Phase 2.2)

**Feature Branch**: `003-signal-analysis-controls`

**Created**: 2026-08-19

**Status**: Draft

**Input**: User description: "Phase 2.2 — Signal Analysis & Expressive Input Controls: per-element visualization graphs showing cumulative sound transformation (transfer function, frequency response, related analysis views; left and right channels on same plots); gain control on activation function and TCN elements controlling nonlinearity slope; knob inputs for continuous parameters; XY trackpad for two-axis simultaneous control of paired parameters."

## Clarifications

### Session 2026-08-19

- Q: What should drive the per-element analysis plots when there is no suitable live input signal available? → A: Use live audio if available, or a standard signal such as white noise if not.
- Q: Should frozen Gold BlackBox nodes support the same per-element analysis views as live Blue nodes? → A: Yes, Gold nodes support the same cumulative analysis views as Blue nodes.
- Q: Are Knob and XY Trackpad inline input-mode toggles on existing element properties, or graph elements? → A: They are **graph elements** added from the element menu, placed on the canvas, and wired to other elements. Multiple control sources (including other Knob or XY Trackpad elements) may be combined using the existing **Merge** element before reaching conditional processing elements.
- Q: How should Knob and XY Trackpad relate to processing elements — parameter modulators, per-element value ports, or network inputs? → A: Knob Input and XY Trackpad are **network input sources** for control, analogous to **Audio Input** for audio. They supply a global **conditioning signal** **c** (1D from Knob, 2D from XY) consumed by the network following the steerable neural audio effects model g(x, c). They do not connect to or modify inline text parameters (kernel size, depth, etc.), which remain architectural configuration separate from runtime conditioning.
- Q: Should processing elements (including TCN) gain dedicated conditioning input ports, and may Knob/XY connect directly to them? → A: **No new ports.** All processing elements keep their existing Phase 2 input ports unchanged. Knob Input and XY Trackpad **may** connect directly to those input ports **or** through Merge. Merge is the **primary** pattern when combining conditioning with audio — not a hard requirement; direct connections are allowed and not refused.
- Q: How should transfer-function, frequency-response, and phase-response analysis views behave for each selected element? → A: **Every analysis view** shows **two static response curves**: (1) **chain** — cumulative response through all upstream elements to the selected element's output; (2) **element-only** — response of the selected element in isolation. **Transfer view** always renders both curves even when audio is not playing; during playback it overlays a **live marker** on the chain curve at the current operating point (input level → output level), constrained to lie **on** the curve. **Frequency view** shows static magnitude (dB) vs frequency for both curve families. **Phase view** shows static phase (degrees) vs frequency for both curve families (standard Bode-style phase plot). **All channels or feature dimensions** at the analysis point are overlaid on the same plot with distinguishable styling (stereo L/R is one case; latent multi-channel feature spaces are supported).
- Q: Must analysis plots be limited to stereo left/right channels? → A: **No.** Plots MUST support **any number of channels or feature dimensions** at the analysis point — stereo L/R when processing stereo audio, but also wider latent/feature spaces (e.g., 32- or 64-channel tensors) depending on graph topology. Each dimension gets a distinguishable trace on shared axes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Inspect Chain vs Element Response at Any Graph Point (Priority: P1)

A user building an audio processing graph selects any element and opens its analysis panel. For each analysis view, the panel displays **two static response curves**: a **chain** curve (cumulative transformation from graph input through all upstream elements to the selected element's output) and an **element-only** curve (the selected element's response in isolation). The user switches between **transfer function** (input vs. output amplitude), **frequency response** (magnitude in dB vs. frequency), and **phase response** (phase in degrees vs. frequency). Curves are visible even when audio is not playing. During playback, the transfer view overlays a live marker on the chain curve at the current operating point — the marker must lie on the curve. **All channels or feature dimensions** at the analysis point are overlaid on the same plot with distinguishable styling — stereo L/R when applicable, or more dimensions in latent/feature space as determined by graph topology.

**Why this priority**: Per-element analysis is the defining capability of Phase 2.2. Dual chain/element curves let users separate cumulative chain behavior from the contribution of one stage.

**Independent Test**: Can be fully tested by building Audio In → Activation → Audio Out, selecting Activation, opening analysis, and verifying chain vs element-only curves appear in transfer, frequency, and phase views with all channel/feature traces visible — including static curves while stopped and a transfer marker on the chain curve while playing. Repeat with a multi-channel latent path (e.g., Conv1D with channels > 2) to confirm N-dimensional traces render.

**Acceptance Scenarios**:

1. **Given** any processing element is selected, **When** the user opens the analysis panel, **Then** transfer, frequency, and phase views are available
2. **Given** any analysis view is open, **When** the user inspects the plot, **Then** both a **chain** curve and an **element-only** curve are displayed with visually distinguishable styling
3. **Given** the transfer-function view is open, **When** audio is not playing, **Then** chain and element-only transfer curves are still rendered
4. **Given** the transfer-function view is open and audio is playing, **When** the live signal updates, **Then** a marker on the **chain** curve shows the current operating point and lies on that curve
5. **Given** the frequency-response view is open, **When** the user inspects the plot, **Then** static magnitude (dB) vs frequency curves are shown for both chain and element-only responses
6. **Given** the phase-response view is open, **When** the user inspects the plot, **Then** static phase (degrees) vs frequency curves are shown for both chain and element-only responses
7. **Given** a signal with multiple channels or feature dimensions at the analysis point, **When** the user views any analysis plot, **Then** a distinguishable trace for each channel/feature dimension appears on the same plot for both chain and element-only families
8. **Given** a downstream element in a multi-element chain is selected, **When** the analysis panel updates, **Then** the chain curve reflects all upstream elements while the element-only curve reflects only the selected element
9. **Given** a frozen Gold BlackBox node is selected, **When** the analysis panel is opened, **Then** the same chain/element-only analysis views available on Blue live nodes are shown for the compiled behavior of that node

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

### User Story 3 - Add Knob Input as a Network Conditioning Source (Priority: P2)

A user opens the element menu and drags a **Knob Input** element onto the graph canvas — a source node for control, analogous to how **Audio Input** is a source node for audio. The user may connect the Knob's conditioning output directly to a processing element's existing input port, or — the primary pattern when combining with audio — route it through a **Merge** element together with an audio source before feeding downstream elements. Adjusting the rotary knob changes the conditioning signal **c** entering the network; live audio reflects the change. Inline text parameters on processing elements remain architectural configuration and are unaffected.

**Why this priority**: Knob Input establishes the steerable control-input pattern (g(x, c)) in the graph editor. It depends on Phase 2's element menu and connection model but does not block analysis or gain features.

**Independent Test**: Can be fully tested by building Audio In → Merge → TCN → Audio Out with a Knob Input also feeding the Merge, rotating the knob, and verifying the audio character changes while inline TCN parameters remain unchanged.

**Acceptance Scenarios**:

1. **Given** the element menu is open, **When** the user browses available elements, **Then** **Knob Input** appears as a draggable source element alongside Audio Input and processing elements
2. **Given** a Knob Input element, **When** the user connects its output to a Merge element input, **Then** the connection is accepted and visually rendered
3. **Given** a Knob Input element and a processing element, **When** the user connects the Knob output directly to the processing element's existing input port, **Then** the connection is accepted and visually rendered
4. **Given** a Knob Input feeding a Merge whose output connects to a TCN, **When** the user drags the rotary knob, **Then** the downstream audio output changes in real time
5. **Given** a Knob Input connected directly or through Merge to a processing chain, **When** the user edits inline text parameters on downstream elements, **Then** the Knob conditioning value and inline architectural parameters remain independent
6. **Given** a Knob Input element, **When** the user views the node, **Then** the current conditioning output value is displayed numerically alongside the rotary control
7. **Given** two Knob Input elements and a Merge element in add mode, **When** both Knob outputs connect to Merge inputs and Merge output feeds a processing element, **Then** the merged result reflects both conditioning sources according to the Merge mode

---

### User Story 4 - Add XY Trackpad as a Dual Conditioning Source (Priority: P2)

A user drags an **XY Trackpad** element from the element menu onto the canvas — a two-dimensional conditioning source analogous to a pair of control knobs (c0, c1 in steerable neural audio effects). The user may connect X and Y outputs directly to processing element input ports, or — the primary pattern when combining with audio — route them through **Merge** together with audio and/or Knob sources. Moving the pointer within the trackpad adjusts both conditioning dimensions simultaneously, enabling expressive two-axis steering during live performance.

**Why this priority**: XY Trackpad provides the primary 2D conditioning interface matching the c ∈ R² control space used in steerable effect discovery. It complements Knob Input and Merge-based audio+control routing.

**Independent Test**: Can be fully tested by placing an XY Trackpad with X and Y feeding a Merge that also receives Audio In, connecting Merge output to TCN, moving the trackpad pointer, and verifying audio changes while inline TCN parameters stay independent.

**Acceptance Scenarios**:

1. **Given** the element menu is open, **When** the user browses available elements, **Then** **XY Trackpad** appears as a draggable source element alongside Audio Input, Knob Input, and processing elements
2. **Given** an XY Trackpad element, **When** the user connects its X and Y outputs to Merge element inputs, **Then** both connections are accepted and visually rendered
3. **Given** an XY Trackpad element and processing elements, **When** the user connects X and/or Y directly to existing processing element input ports, **Then** the connections are accepted and visually rendered
4. **Given** an XY Trackpad feeding a Merge whose output connects downstream, **When** the user moves the pointer within the trackpad, **Then** audio processing reflects both updated conditioning dimensions in real time
5. **Given** an XY Trackpad connected directly or through Merge to a processing chain, **When** the user edits inline architectural parameters on downstream elements, **Then** trackpad conditioning values and inline parameters remain independent
6. **Given** an XY Trackpad element is active, **When** the user views the node, **Then** the current X and Y conditioning output values are displayed
7. **Given** an XY Trackpad and a Knob Input both feeding the same Merge element, **When** the user adjusts either control, **Then** the Merge output and downstream audio reflect the combined sources according to the Merge mode

---

### Edge Cases

- What happens when the user opens analysis on an element with no upstream audio connection — do static chain/element curves still render using the internal probe signal?
- How do analysis plots behave when the selected element is a frozen (Gold) BlackBox node?
- What happens when Gain is set to its minimum or maximum boundary value on Activation or TCN elements?
- How does the system handle invalid Gain input (negative, zero, non-numeric) entered via inline text on Activation or TCN elements?
- What happens when a Merge element receives only conditioning inputs (Knob/XY) with no audio source connected?
- What happens when a Merge element receives only audio inputs with no Knob or XY conditioning connected — does conditioning default to zero?
- What happens when a Knob or XY Trackpad cable is disconnected mid-performance?
- How does Merge behave when combining audio and conditioning inputs with one source disconnected?
- What happens when many channel/feature dimensions (e.g., 32 or 64) are present — does the plot remain readable with distinguishable styling or apply density limits?
- What happens when mono (1 channel) is processed — does a single trace render without requiring stereo pairs?
- How does the system behave when analysis is requested on Audio Input or Audio Output boundary nodes?
- What happens to the transfer live marker when playback stops — does it hide or hold the last position?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide a per-element analysis panel accessible from any element on the graph canvas
- **FR-002**: Every analysis view MUST display two response curve families for the selected element: **chain** (cumulative through all upstream elements to the selected output) and **element-only** (the selected element in isolation)
- **FR-003**: Analysis panel MUST include transfer-function, frequency-response (magnitude), and phase-response views
- **FR-003a**: Transfer-function view MUST always render chain and element-only curves, even when audio is not playing
- **FR-003b**: During playback, transfer-function view MUST overlay a live operating-point marker on the **chain** curve; the marker MUST lie on the curve at the current input→output coordinates
- **FR-003c**: Frequency-response view MUST show static magnitude (dB) vs frequency for both chain and element-only curves
- **FR-003d**: Phase-response view MUST show static phase (degrees) vs frequency for both chain and element-only curves
- **FR-004**: All analysis plots MUST display a distinguishable trace per channel or feature dimension at the analysis point on shared axes, for both chain and element-only families — supporting any dimension count (mono, stereo L/R, or wider latent/feature spaces)
- **FR-005**: Static analysis curves MUST update when upstream graph topology, parameters, weights, or conditioning values change
- **FR-005a**: Static curve computation MUST use live audio when a suitable input is available, and MUST fall back to a standard internal probe signal (e.g., white noise) when it is not
- **FR-005b**: Frozen Gold BlackBox nodes MUST support the same chain/element-only analysis views as Blue live nodes, based on their compiled behavior
- **FR-006**: Activation Function elements MUST expose a **Gain** parameter that controls the slope of the nonlinearity
- **FR-007**: TCN elements MUST expose a **Gain** parameter that controls the slope of the nonlinearity applied within the TCN block
- **FR-008**: Gain parameter changes MUST take effect in live audio processing without interrupting playback
- **FR-009**: Adjusting Gain MUST produce observable changes in the element's transfer-function analysis view
- **FR-010**: Element menu MUST include **Knob Input** and **XY Trackpad** as draggable source elements, analogous to **Audio Input**
- **FR-011**: Knob Input element MUST expose a rotary control and a single conditioning output connectable to Merge element inputs or directly to processing element input ports
- **FR-012**: Knob Input MUST display the current conditioning output value numerically on the node; its output MUST NOT read from or write to inline architectural parameters on processing elements
- **FR-013**: XY Trackpad element MUST expose a two-axis trackpad control with separate X and Y conditioning outputs connectable to Merge element inputs or directly to processing element input ports, providing a 2D conditioning signal (equivalent to c0 and c1)
- **FR-014**: Merge element MUST be the supported path for combining audio and conditioning sources before downstream processing; direct Knob/XY-to-element connections MUST remain valid when users choose not to merge with audio
- **FR-015**: XY Trackpad MUST update both conditioning outputs in real time during pointer movement; downstream audio MUST reflect changes whether XY outputs connect directly or through Merge
- **FR-016**: XY Trackpad MUST display current X and Y conditioning output values on the node
- **FR-017**: All processing elements (including TCN) MUST retain their existing Phase 2 input ports unchanged — no new port types are added in Phase 2.2; those ports accept audio, conditioning, or Merge output connections per port-type rules
- **FR-018**: Merge element MUST combine audio sources and conditioning sources (Knob Input, XY Trackpad) and route the merged result to downstream processing elements, using its existing operating modes (add, multiply, concatenate) where compatible
- **FR-019**: System MUST enforce port-type compatibility across audio, conditioning, and Merge endpoints without blocking valid direct Knob/XY-to-element connections
- **FR-020**: Knob Input and XY Trackpad elements, their positions, settings, and connection topology (direct or via Merge) MUST persist when the project/plugin state is saved and reloaded
- **FR-021**: Inline architectural parameter editing from Phase 2 MUST remain available on processing elements and MUST remain independent of conditioning connections — editing inline text MUST NOT change Knob/XY conditioning values and vice versa
- **FR-022**: When a Merge element has no conditioning sources connected, the conditioning contribution MUST default to a neutral zero baseline (consistent with steerable discovery training defaults)
- **FR-023**: Analysis, Knob Input, XY Trackpad, and Merge-driven conditioning routing MUST NOT interrupt audio processing — updates are applied without audible glitches
- **FR-024**: Analysis views MUST remain usable at the constitution's UI responsiveness target (60 FPS) while audio is actively processing

### Key Entities

- **Analysis Panel**: A per-element UI region with view selector (transfer, frequency, phase). Each view shows **chain** and **element-only** curve families; all channel/feature dimensions on shared axes. Transfer view adds a live on-curve marker during playback.
- **Chain Response Curve**: Static (and transfer live marker) representation of cumulative signal transformation from graph input through all upstream elements to the selected element's output.
- **Element-Only Response Curve**: Static representation of the selected element's response in isolation from upstream chain context.
- **Cumulative Signal Snapshot**: Computed input/output relationships used to derive chain and element-only curves. Refreshed when topology, parameters, weights, or conditioning change.
- **Gain Parameter**: A continuous control on Activation Function and TCN elements that scales nonlinearity slope. Distinct from weight randomization controls.
- **Knob Input Element**: A graph source node (like Audio Input) with a rotary control and one conditioning output. Connects to Merge inputs or directly to processing element input ports. Supplies 1D runtime conditioning **c**. Does not read or write inline architectural parameters.
- **XY Trackpad Element**: A graph source node with a two-axis trackpad UI and separate X/Y conditioning outputs. Connects to Merge inputs or directly to processing element input ports. Supplies 2D runtime conditioning. Does not read or write inline architectural parameters.
- **Merge Element**: Primary routing hub for combining audio **x** and conditioning **c** before downstream processing. Also accepts multiple conditioning or audio sources alone. Extended in Phase 2.2 to accept conditioning inputs alongside audio.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can open analysis views and distinguish chain vs element-only transfer and frequency characteristics within 10 seconds of selecting an element
- **SC-002**: Chain and element-only curve families plus one trace per channel/feature dimension are visible on shared axes in 100% of analysis views for the active dimension count at the analysis point
- **SC-003**: Static analysis curves reflect parameter, topology, or conditioning changes within one refresh cycle after the change (< 500 ms perceived latency)
- **SC-003a**: Transfer live marker tracks the current operating point on the chain curve during playback without leaving the curve
- **SC-004**: Users can adjust Gain on Activation or TCN elements and hear an audible difference in nonlinearity character within one audio buffer cycle
- **SC-005**: Transfer-function plots visibly change when Gain is adjusted, confirming alignment between analysis and audio behavior
- **SC-006**: Users can add a Knob Input element, route it through Merge to a processing chain, and hear an audible change from knob adjustment in under 5 seconds
- **SC-007**: Users can add an XY Trackpad element, route X and Y through Merge to a processing chain, and adjust both dimensions in real time with corresponding audio changes
- **SC-008**: Graph canvas and analysis panel remain responsive at 60 FPS during active audio processing with analysis views open (per constitution UI benchmark)
- **SC-009**: After save and reload, 100% of Knob Input and XY Trackpad element placements, settings, and connection topology (direct or via Merge) are restored correctly
- **SC-010**: Zero audible glitches occur when switching analysis view types, adjusting Gain, rotating a Knob Input element, or moving an XY Trackpad pointer during live playback
- **SC-011**: Users can route Knob and XY conditioning sources through Merge alongside audio, with merged output feeding a processing element and reflecting control changes in real time

## Assumptions

- Phase 2 (Embedded Builder & Interactive Graph) is complete: users can build graphs, edit inline architectural parameters, connect audio paths, and use the Merge element for audio
- Analysis computations use live audio when a suitable input signal is available; otherwise they fall back to a standard internal probe signal such as white noise rather than requiring offline rendering
- Gain on Activation elements scales the input before the activation function is applied (equivalent to adjusting pre-activation slope); default range is 0.1 to 10.0 with 1.0 as neutral
- Gain on TCN elements applies the same slope-scaling concept to the TCN block's internal nonlinearity stages
- The control-input model follows steerable neural audio effects: a conditional network g(x, c) where **x** is audio (from Audio Input) and **c** is a runtime conditioning vector (from Knob Input, XY Trackpad, or Merge). Adjusting **c** steers perceptual attributes of the effect without changing architectural inline parameters
- Knob Input and XY Trackpad are graph source elements like Audio Input — they may connect directly to processing element input ports or through Merge; Merge is the primary pattern when combining conditioning with audio
- All processing elements (including TCN) retain their Phase 2 input port layout unchanged — no new port types are added; existing ports accept compatible audio or conditioning connections
- Typical audio+control topology: Audio In + Knob/XY → Merge → TCN → Audio Out; direct Knob/XY → element wiring is also supported
- Inline text parameters (kernel size, depth, channels, dilation, Gain) are architectural configuration; conditioning signals are runtime steering controls — separate concerns
- When Merge has no conditioning sources connected, conditioning defaults to zero (neutral baseline, consistent with steerable discovery steering at c = 0)
- Merge reuses existing add/multiply/concatenate modes; how each mode applies when mixing audio and conditioning tensors is defined during planning
- Static frequency and phase curves are computed from probe or live measurement — playback is not required for frequency/phase display
- Frequency response uses magnitude in dB vs frequency (log-scaled frequency axis is the conventional display and recommended default)
- Phase response uses phase in degrees vs frequency (standard Bode-style companion to magnitude); shows how much each frequency component is delayed or shifted by the chain vs the element alone
- Analysis plots overlay all channel/feature dimensions at the selected point on shared axes; dimension count follows graph topology (stereo L/R is the common case, latent spaces may have more)
- Transfer-function curves plot output amplitude vs input amplitude; the live playback marker applies only to the chain curve
- Frozen (Gold) BlackBox nodes expose cumulative analysis based on their compiled behavior; internal modular detail is not required to be decomposed in analysis views
- Analysis on boundary nodes (Audio In, Audio Out) shows passthrough or final-output characteristics rather than internal layer transforms
- Knob Input and XY Trackpad elements are UI/control nodes — they do not participate in audio tensor processing paths and are excluded from freeze subgraph compilation unless explicitly included in a future phase
