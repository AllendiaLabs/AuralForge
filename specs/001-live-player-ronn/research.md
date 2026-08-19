# Research: The Live Player & RONN (Phase 1)

**Branch**: `001-live-player-ronn` | **Date**: 2026-08-19

## R1: LibTorch C++ Integration with JUCE

**Decision**: Use LibTorch (C++ API) pre-built binaries linked via CMake. Use `torch::nn::Module` subclasses for the TCN — no TorchScript needed in Phase 1.

**Rationale**: LibTorch C++ API provides full `torch::nn` module construction at runtime (dynamic depth, kernel size, channels). This is the Live Engine path per the constitution. TorchScript compilation is only needed for the Frozen Engine (Phase 2).

**Alternatives considered**:
- ONNX Runtime: Good inference performance but no dynamic architecture modification at runtime. Would require re-exporting on every parameter change.
- Custom C++ convolution kernels (no framework): Maximum control, but enormous effort for a first iteration. Can be revisited if LibTorch overhead proves too high.

**Key findings**:
- LibTorch `libtorch-macos-arm64` and `libtorch-macos-x86_64` are available as pre-built packages. Universal binary may require fat library merging or separate builds.
- `torch::nn::Conv1d` with `dilation` option directly supports the TCN architecture.
- `torch::NoGradGuard` should be active during inference to avoid unnecessary graph tracking overhead.
- Thread safety: `torch::nn::Module` is not thread-safe for concurrent read/write. Solution: build new model on GUI thread, swap to audio thread via atomic pointer.

## R2: Atomic Model Swap Pattern (Zero Audio-Thread Allocation)

**Decision**: Double-buffered model swap using `std::atomic<std::shared_ptr<TCNModel>>`. GUI thread constructs the new model, stores it in a shared_ptr, and atomically publishes it. Audio thread loads the current pointer each `processBlock` call.

**Rationale**: `std::atomic<std::shared_ptr<>>` (C++20, or use `juce::ReferenceCountedObjectPtr` with atomic exchange for C++17) provides lock-free pointer swap. The audio thread never allocates — it only reads the current model pointer.

**Alternatives considered**:
- Lock-free queue of model pointers: More complex, no real benefit over atomic shared_ptr for single-producer single-consumer.
- Spinlock on audio thread: Violates zero-allocation/zero-blocking mandate.
- `std::atomic<TCNModel*>` with manual ref counting: Error-prone lifetime management.

**Key findings**:
- C++17 does not guarantee `std::atomic<std::shared_ptr<>>` is lock-free. Fallback: use JUCE's `ReferenceCountedObjectPtr` with `std::atomic<ReferenceCountedObjectPtr<>>` or a simple atomic raw pointer + ref counting via `juce::ReferenceCountedObject`.
- Weight randomization follows the same path: randomize on GUI thread → build new model → atomic swap.
- Architecture parameter changes (depth, kernel size, channels) also trigger full model rebuild + swap (not in-place modification).

## R3: Causal TCN Architecture (RONN Pattern)

**Decision**: Stack of `Conv1d` layers with exponentially increasing dilation factors (`dilation = 2^i` for layer `i`), each followed by a configurable activation. Causal padding ensures output length equals input length.

**Rationale**: Matches the RONN paper design. Exponential dilation gives receptive field of `(kernel_size - 1) * sum(2^i for i in 0..depth-1) + 1` samples. Causal padding = `(kernel_size - 1) * dilation` (left-pad only).

**Alternatives considered**:
- WaveNet-style with gated activations and skip connections: More complex, deferred to future iteration.
- Residual connections between layers: Useful but adds complexity. Can be added later as a parameter toggle.

**Key findings**:
- Receptive field formula: `RF = 1 + (kernel_size - 1) * (2^depth - 1)` (for dilation doubling from 1).
- For default config (depth=4, kernel_size=3): RF = 1 + 2 * 15 = 31 samples = ~0.7 ms at 44.1 kHz. Very manageable.
- At extreme configs (depth=20, kernel_size=7): RF = 1 + 6 * 1,048,575 ≈ 6.3M samples = ~143 seconds. UI must warn.
- Causal padding in LibTorch: `torch::nn::Conv1dOptions` does not have a "causal" mode. Must manually left-pad input by `(kernel_size - 1) * dilation` zeros (or look-back buffer samples).

## R4: Look-back Buffer for Block-Based Processing

**Decision**: Maintain a circular buffer per channel storing the last `receptive_field - 1` input samples. On each `processBlock`, prepend look-back samples to the current block, run the TCN, and output only the last `blockSize` samples of the result.

**Rationale**: The TCN is causal and requires `RF - 1` past samples to produce valid output for the first sample of each block. Without the look-back buffer, the first samples of each block would produce transient artifacts.

**Alternatives considered**:
- Report latency equal to receptive field and use host-provided latency compensation: Would add latency, unacceptable for real-time performance.
- Process sample-by-sample: Extremely inefficient with Conv1d; block processing is orders of magnitude faster.

**Key findings**:
- Buffer size must be dynamically resized when architecture parameters change (new receptive field). Resize happens on GUI thread; new buffer is swapped atomically alongside the new model.
- Pre-allocate the padded tensor on `prepareToPlay` and reuse across `processBlock` calls to avoid audio-thread allocations.
- When the model changes mid-stream, the look-back buffer content from the old model is invalid for the new model. Clear the buffer on swap (brief transient is acceptable and matches user expectation on parameter change).

## R5: Dear ImGui Integration in JUCE

**Decision**: Use JUCE's OpenGL renderer to host a Dear ImGui context. The JUCE editor component creates an `OpenGLContext`, initializes ImGui with the OpenGL backend, and renders the node graph each frame.

**Rationale**: Dear ImGui + imgui-node-editor is mandated by the constitution. JUCE's `OpenGLContext` provides a GL surface inside the plugin window.

**Alternatives considered**:
- JUCE native components for the node graph: Would not match the ML Forge UI paradigm and lacks imgui-node-editor's layout capabilities.
- Separate window for ImGui: Violates "VST is the sole interface" principle.

**Key findings**:
- `juce_opengl` module must be enabled in the JUCE project.
- ImGui input events (mouse, keyboard) must be forwarded from JUCE's event system to ImGui's IO struct.
- imgui-node-editor provides `BeginNode`/`EndNode`, `BeginPin`/`EndPin`, `Link` APIs — maps well to the read-only TCN visualization.
- In Phase 1, the node graph is read-only: no drag, no user-created links. Nodes are auto-laid out based on the TCN architecture.
- Performance: ImGui renders are lightweight; 60 FPS is trivially achievable for a <100 node graph.

## R6: JUCE Parameter System (APVTS)

**Decision**: Use `juce::AudioProcessorValueTreeState` (APVTS) for all DAW-automatable parameters: depth, kernel_size, channels, activation_function, randomize_trigger, global_seed, dry_wet_mix.

**Rationale**: APVTS is the standard JUCE mechanism for DAW-automatable, save/restore-able parameters. Provides thread-safe parameter reads on the audio thread.

**Alternatives considered**:
- Raw `AudioParameterFloat`/`Int` without APVTS: Less organized, harder to serialize.

**Key findings**:
- `depth` as `AudioParameterInt` (range 1–999, default 4). Large upper range to honor "no cap" requirement; UI warns at high values.
- `kernel_size` as `AudioParameterInt` (range 2–65, default 3).
- `channels` as `AudioParameterInt` (range 1–512, default 16).
- `activation_function` as `AudioParameterChoice` with choices ["ReLU", "Sigmoid", "Tanh", "LeakyReLU"].
- `randomize_trigger` as `AudioParameterBool` — rising edge triggers randomization. DAW-automatable.
- `global_seed` as `AudioParameterInt` (range 0–2^31, default 42).
- Parameter changes detected via listeners on the GUI thread → trigger model rebuild → atomic swap.

## R7: Weight Serialization for DAW State

**Decision**: Serialize model weights as a flat byte blob (via `torch::save` to a `std::ostringstream`) appended to the APVTS XML state in `getStateInformation`. Deserialize in `setStateInformation` by rebuilding the model from parameters and loading the weight blob.

**Rationale**: DAW projects must restore the exact sonic state including the randomized weights. APVTS handles parameter state; weights need a separate serialization path.

**Alternatives considered**:
- Save only the seed + parameters (deterministic rebuild): Attractive but randomization may use system entropy, making exact reproduction impossible without saving weights.
- Save weights as a temp file referenced by path: Fragile — files can be moved/deleted.

**Key findings**:
- `torch::save({model->parameters()}, stream)` and `torch::load` provide the mechanism.
- Weight blob size for default config (depth=4, kernel_size=3, channels=16): ~few KB. Negligible in a DAW project.
- For extreme configs (depth=100, channels=512): could be several MB. Acceptable for DAW project files.

## R8: ML Forge Codebase Adaptation Strategy

**Decision**: Use ML Forge as a reference for architectural patterns (block definitions, graph data model, node rendering, section/category organization) but rewrite in C++ rather than attempting to port Python directly.

**Rationale**: ML Forge is Python/DearPyGui. AuralForge is C++/JUCE/Dear ImGui. The languages and UI frameworks differ too much for direct porting, but the design patterns (SECTIONS dict for block definitions, GraphNode dataclass, validation system) translate cleanly to C++ equivalents.

**Alternatives considered**:
- Automated Python-to-C++ transpilation: Impractical for UI code.
- Ignore ML Forge entirely: Misses the opportunity to reuse proven graph architecture patterns.

**Key findings**:
- ML Forge `blocks.py` SECTIONS dictionary → C++ `BlockDefinition` structs in a registry.
- ML Forge `graph.py` `GraphNode` dataclass → C++ `GraphNode` struct.
- ML Forge color coding (per-category) → Phase 1 uses uniform Blue (per constitution), but the color infrastructure should support per-node colors for Phase 2.
- ML Forge's undo/redo system is not needed in Phase 1 (read-only graph) but the pattern should be noted for Phase 2.
