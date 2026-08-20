# Feature Specification: The Live Player (Phase 1)

**Feature Branch**: `001-live-player-ronn`

**Created**: 2026-08-19

**Status**: Draft

**Input**: User description: "Phase 1 — The Live Player & RONN: Working VST that generates sound instantly with the Live Modular Engine using Randomized Overdrive Neural Networks entirely in C++ via torch::nn, with zero training required."

## Clarifications

### Session 2026-08-19

- Q: What should the default and allowable ranges be for the depth parameter (number of layers)? → A: Default 4, minimum 1, no upper cap.
- Q: How should the user trigger weight randomization? → A: UI button, assignable MIDI CC, and DAW-automatable parameter.
- Q: Should the plugin operate as a synthesizer or audio effect? → A: Audio effect (audio in, audio out). RONN/TCN processes incoming audio signals, not MIDI-triggered synthesis.
- Q: Which activation functions should be available? → A: ReLU, Sigmoid, Tanh, LeakyReLU.
- Q: What channel configuration should the plugin support? → A: Determined by the user-defined architecture and its audio I/O modules. The plugin declares flexible bus support and adapts at runtime based on the graph's I/O node configuration.

**Additional clarifications (from user)**:
- The codebase and UI must be model-agnostic. No "RONN" branding — use generic names like "TCN" throughout the UI and code.
- The project should be extensively based on the ML Forge codebase (`.ignore/ml_forge-main/`) for its node graph architecture, block definitions, and UI patterns.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Load Plugin and Process Audio Immediately (Priority: P1)

A music producer opens their DAW, inserts OpenYourBox as a VST3/AU effect plugin on an audio track (or bus). A default TCN configuration is loaded automatically. When audio plays through the track, the plugin immediately processes it through the neural network, producing a distortion/overdrive effect with zero setup, no training, and no configuration.

**Why this priority**: This is the fundamental value proposition of Phase 1 — instant audio processing from the moment the plugin loads. Without this, there is no product.

**Independent Test**: Can be fully tested by loading the plugin on an audio track, playing audio through it, and verifying the output is audibly processed with zero user configuration.

**Acceptance Scenarios**:

1. **Given** a DAW with OpenYourBox inserted on an audio track, **When** audio plays through the track, **Then** the plugin processes the audio through the default TCN and outputs the effected signal within the buffer latency window.
2. **Given** a freshly installed OpenYourBox plugin, **When** the user opens it for the first time, **Then** a default TCN model (depth 4, reasonable kernel size and channel count) is loaded automatically and the plugin is ready to process audio without any manual steps.
3. **Given** the plugin is loaded and processing audio, **When** the DAW is also running other plugins on other tracks, **Then** OpenYourBox does not cause audio dropouts or glitches at a 256-sample buffer size.

---

### User Story 2 - Manipulate TCN Parameters in Real Time (Priority: P1)

The producer sees Blue modular nodes representing the TCN architecture (depth, kernel size, channels, activation function). They adjust parameters — changing the number of layers, kernel sizes, channel counts, or activation type — and hear the tonal character change instantly without interruption.

**Why this priority**: Real-time parameter manipulation is what makes the TCN effect musically expressive. Without it, the plugin is a static effect with no creative value.

**Independent Test**: Can be tested by loading the plugin, adjusting each exposed parameter (depth, kernel size, channels, activation function), and verifying audio output changes correspondingly in real time without dropouts.

**Acceptance Scenarios**:

1. **Given** the plugin is processing audio, **When** the user changes the "depth" parameter (number of layers), **Then** the tonal character changes audibly and immediately without audio interruption.
2. **Given** the plugin is processing audio, **When** the user changes the "kernel size" parameter, **Then** the frequency response and texture change audibly and immediately.
3. **Given** the plugin is processing audio, **When** the user changes the "channels" parameter, **Then** the harmonic richness changes audibly and immediately.
4. **Given** the plugin is processing audio, **When** the user switches the activation function (ReLU, Sigmoid, Tanh, LeakyReLU), **Then** the distortion character changes audibly and immediately.

---

### User Story 3 - Randomize Weights for Glitch/Discovery (Priority: P2)

The producer clicks a "Randomize" button in the UI, triggers it via a mapped MIDI CC, or automates it as a DAW parameter. The neural network weights are scrambled, producing an entirely new and unpredictable timbre. They can keep randomizing to discover interesting sounds, then stop when they find one they like.

**Why this priority**: Weight randomization is the core creative differentiator — it turns a neural network into a sound design discovery tool. It is secondary to basic audio processing and parameter control but essential for the product's identity.

**Independent Test**: Can be tested by clicking randomize repeatedly and verifying each press produces a different sound, with no audio dropouts during the weight swap.

**Acceptance Scenarios**:

1. **Given** the plugin is processing audio, **When** the user triggers weight randomization (via UI button, MIDI CC, or DAW automation), **Then** new weights are applied atomically and the sound changes immediately without any click, pop, or dropout.
2. **Given** the user has randomized to a sound they like, **When** they stop randomizing, **Then** the current weights persist and the sound remains stable.
3. **Given** the plugin is processing audio at high CPU load, **When** the user triggers rapid successive randomizations, **Then** the audio thread is never blocked and playback remains glitch-free.
4. **Given** the user automates the randomization parameter in the DAW timeline, **When** playback reaches the automation point, **Then** randomization triggers and the effect changes as expected.

---

### User Story 4 - Visual Node Graph with Blue Modular Nodes (Priority: P2)

The producer sees a node graph UI inside the plugin window showing the TCN architecture as interconnected Blue nodes. Each node represents a layer (Conv1d, activation, etc.). The graph visually communicates the signal flow from audio input to audio output.

**Why this priority**: The visual node editor is the foundation for Phase 2's full graph editing. In Phase 1 it provides essential visual feedback and establishes the UI paradigm, but the producer cannot yet freely add/remove/rearrange nodes — that comes later.

**Independent Test**: Can be tested by opening the plugin and verifying that Blue nodes are displayed, correctly represent the TCN architecture, and visually update when parameters change (e.g., adding depth adds a visible node).

**Acceptance Scenarios**:

1. **Given** the plugin is open, **When** the user views the node graph, **Then** all TCN layers are displayed as Blue nodes with clear labels and connections showing signal flow from audio input to audio output.
2. **Given** the user changes the depth parameter, **When** the node graph updates, **Then** nodes are added or removed to reflect the new architecture and the UI remains responsive at 60 FPS.
3. **Given** the plugin window is resized, **When** the user views the node graph, **Then** the graph layout adapts appropriately and remains readable.

---

### User Story 5 - Stable DAW Integration (Priority: P1)

The plugin loads, scans, and operates correctly as both VST3 and AU formats across major DAWs (Ableton Live, Logic Pro, Reaper, FL Studio). It handles DAW lifecycle events (save/load project, bypass, multiple instances) without crashes or state corruption.

**Why this priority**: A plugin that crashes DAWs or fails validation will never be used regardless of how innovative the sound engine is. Stability is table stakes.

**Independent Test**: Can be tested by running the plugin through standard DAW validation tools (e.g., Steinberg VST3 validator, AU validation in Logic) and performing lifecycle operations (save, load, bypass, remove, re-add).

**Acceptance Scenarios**:

1. **Given** OpenYourBox is installed, **When** the DAW scans for plugins, **Then** OpenYourBox appears in the plugin list and passes validation without errors.
2. **Given** a DAW project with OpenYourBox and specific parameter settings, **When** the user saves and reopens the project, **Then** all TCN parameters and the current weight state are restored exactly.
3. **Given** multiple instances of OpenYourBox on different tracks, **When** all instances process audio simultaneously, **Then** each instance operates independently without interference or shared state corruption.

---

### Edge Cases

- What happens when the audio input is silence? The plugin should output silence (or near-silence) — the TCN should not generate signal from nothing.
- How does the system handle extremely high depth values (e.g., 100+ layers)? The UI should display the receptive field size and warn if the configuration exceeds the latency budget, but not impose an artificial cap.
- What happens when the user changes parameters while audio is not playing? Parameters update silently; the new configuration applies when audio resumes.
- How does the plugin behave when the host sample rate changes mid-session? The plugin should detect the change and reinitialize the look-back buffer and model without crashing.
- What happens if the host provides a buffer size smaller than the model's minimum required input? The plugin should accumulate samples internally via a look-back buffer (as described in the RONN paper) and output at the correct rate, or clearly report the limitation.
- What happens when the receptive field grows very large (multiple seconds)? The plugin should display the receptive field in milliseconds and warn about increased CPU load.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The plugin MUST load as a VST3 and AU effect plugin and pass standard host validation scans.
- **FR-002**: The plugin MUST process audio input through a TCN model with zero user configuration on first load, using a default architecture (depth 4, reasonable kernel size and channel count).
- **FR-003**: The plugin MUST expose parameters for TCN architecture control: depth (number of layers, default 4, minimum 1, no upper cap), kernel size, channel count, and activation function (ReLU, Sigmoid, Tanh, LeakyReLU).
- **FR-004**: Parameter changes MUST take effect in real time without interrupting audio playback.
- **FR-005**: The plugin MUST provide a weight randomization action that atomically swaps all model weights without blocking the audio thread. Randomization MUST be triggerable via UI button, assignable MIDI CC, and DAW-automatable parameter.
- **FR-006**: Weight randomization MUST prepare new weights on the GUI thread and apply them to the audio thread via atomic pointer swap (zero audio-thread allocations).
- **FR-007**: The plugin MUST display the TCN architecture as a node graph with Blue-colored nodes representing each layer.
- **FR-008**: The node graph MUST update visually when architecture parameters (depth, kernel size, channels, activation) change.
- **FR-009**: The plugin MUST render its UI at 60 FPS minimum during all operations.
- **FR-010**: The plugin MUST save and restore all parameter state and current model weights when the DAW project is saved and loaded.
- **FR-011**: Multiple simultaneous instances of the plugin MUST operate independently without shared mutable state.
- **FR-012**: The plugin MUST implement a look-back buffer to handle block-based processing of the causal TCN, ensuring the output block length matches the input block length.
- **FR-013**: The plugin MUST NOT perform any memory allocation on the real-time audio thread.
- **FR-014**: The plugin MUST display the current receptive field size (in milliseconds) and the number of model parameters in the UI.
- **FR-015**: The plugin MUST expose a global seed parameter for effect recallability and presets.
- **FR-016**: The plugin's audio bus configuration (mono/stereo input and output) MUST be determined by the user-defined architecture's audio I/O modules. The plugin declares flexible bus support to the host and adapts at runtime.
- **FR-017**: The UI and codebase MUST use model-agnostic terminology (e.g., "TCN", "Temporal Convolutional Network") and MUST NOT reference "RONN" in any user-facing element or code identifier.

### Key Entities

- **TCN Model**: A temporal convolutional network composed of stacked causal 1D convolution layers with configurable nonlinear activations and exponentially increasing dilation factors. Weights can be randomized to produce novel timbres. Key attributes: depth, kernel size, channel count, activation function, dilation factor, global seed, current weight state.
- **Blue Node**: A visual representation of a single layer in the TCN model within the node graph UI. Displays layer type, parameters, and connections. Color-coded Blue to indicate Live/modular state.
- **Parameter State**: The complete set of user-controllable values (depth, kernel size, channels, activation function, global seed) that define the current TCN configuration. Persisted with DAW project state.
- **Look-back Buffer**: A circular buffer storing past input samples, sized to the TCN's receptive field, enabling block-based causal processing without discontinuities at frame boundaries.
- **Audio I/O Module**: A graph node representing the audio input or output bus. Determines the plugin's channel configuration (mono/stereo) based on user placement in the node graph.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users hear the effect applied to audio within 1 second of playback starting after plugin load — zero setup required.
- **SC-002**: Audio latency for live (Blue) processing stays below 7 ms at a 256-sample buffer on a standard Intel i7 processor.
- **SC-003**: Weight randomization completes and the sound changes within 100 ms of user action, with zero audio dropouts.
- **SC-004**: The plugin passes VST3 and AU validation on macOS without errors or warnings.
- **SC-005**: The UI renders at 60 FPS while audio is processing at full load.
- **SC-006**: DAW project save/load restores the exact sonic state (parameters + weights) with no audible difference.
- **SC-007**: 4 simultaneous plugin instances run without audio dropouts on a standard Intel i7 at 256-sample buffer.
- **SC-008**: The receptive field display updates within 200 ms of any parameter change.

## Assumptions

- Target platforms for Phase 1 are macOS (VST3 and AU). Windows and Linux support may follow in later phases.
- The TCN architecture uses causal 1D convolutions (Conv1d) with exponentially increasing dilation factors, consistent with the RONN paper's design.
- The plugin operates as an audio effect (audio in → audio out). It is not a synthesizer and does not respond to MIDI note input for sound generation (MIDI CC is used only for parameter control).
- The node graph in Phase 1 is read-only/display-only — users cannot drag, connect, or rearrange nodes. Full graph editing is a Phase 2 feature.
- LibTorch (C++ API) is the inference runtime; no Python dependency is required for Phase 1.
- The Dear ImGui / imgui-node-editor stack is used for the node graph UI, embedded within the JUCE plugin window.
- "Standard Intel i7" for performance benchmarks refers to a mid-range desktop processor from 2022 or later (e.g., i7-12700 class).
- The project is extensively based on the ML Forge codebase (node graph architecture, block definitions, UI patterns) adapted from Python/DearPyGui to C++/JUCE/Dear ImGui.
- Depthwise convolutions (as described in the RONN paper for reducing compute with large receptive fields) are deferred to a future iteration unless needed to meet the latency target.
