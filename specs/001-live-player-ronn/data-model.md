# Data Model: The Live Player & RONN (Phase 1)

**Branch**: `001-live-player-ronn` | **Date**: 2026-08-19

## Entities

### TCNModel

A runtime-constructed `torch::nn::Module` subclass representing the full TCN signal chain.

| Field | Type | Constraints | Notes |
|-------|------|-------------|-------|
| depth | int | ≥1, no upper cap | Number of Conv1d layers |
| kernelSize | int | ≥2 | Convolution kernel width |
| channels | int | ≥1 | Internal channel count per layer |
| activationType | enum {ReLU, Sigmoid, Tanh, LeakyReLU} | — | Nonlinearity after each Conv1d |
| dilationBase | int | Fixed at 2 (Phase 1) | Dilation = dilationBase^layerIndex |
| inputChannels | int | 1 or 2 | Determined by host bus config |
| outputChannels | int | 1 or 2 | Matches inputChannels in Phase 1 |
| layers | vector\<Conv1dWithActivation\> | length == depth | Ordered layer stack |
| inputProjection | Conv1d | — | Projects inputChannels → channels |
| outputProjection | Conv1d | — | Projects channels → outputChannels |
| receptiveField | int (computed) | — | `1 + (kernelSize-1) * (2^depth - 1)` samples |
| parameterCount | int (computed) | — | Total trainable parameters |

**State transitions**:
- `Uninitialized` → `Ready`: On construction with valid parameters
- `Ready` → `Ready`: On atomic swap (new model replaces old)

**Relationships**: Owned by `PluginProcessor` via atomic shared pointer. Read by audio thread, written by GUI thread.

### Conv1dWithActivation

A single layer in the TCN stack.

| Field | Type | Constraints | Notes |
|-------|------|-------------|-------|
| conv | torch::nn::Conv1d | — | Causal 1D convolution |
| activation | torch::nn::AnyModule | — | One of the four supported activations |
| dilation | int | 2^layerIndex | Exponentially increasing |
| causalPadding | int (computed) | (kernelSize-1) * dilation | Left-pad amount |

### LookbackBuffer

Circular buffer for causal block-based processing.

| Field | Type | Constraints | Notes |
|-------|------|-------------|-------|
| buffer | torch::Tensor | Shape: [numChannels, receptiveField-1] | Pre-allocated |
| numChannels | int | Matches model I/O | — |
| size | int | receptiveField - 1 | — |
| writePos | int | 0..size-1 | Circular write head |

**State transitions**:
- Cleared (zeroed) on model swap or `prepareToPlay`
- Updated each `processBlock` with trailing input samples

### ParameterState

APVTS-managed parameter set persisted with DAW project.

| Parameter ID | Type | Range | Default | DAW Automatable |
|-------------|------|-------|---------|-----------------|
| `depth` | int | 1–999 | 4 | Yes |
| `kernel_size` | int | 2–65 | 3 | Yes |
| `channels` | int | 1–512 | 16 | Yes |
| `activation` | choice | ReLU/Sigmoid/Tanh/LeakyReLU | ReLU | Yes |
| `randomize` | bool | — | false | Yes (rising edge triggers) |
| `global_seed` | int | 0–2147483647 | 42 | Yes |
| `dry_wet` | float | 0.0–1.0 | 1.0 | Yes |

### GraphNode (UI)

Visual representation of a TCN layer in the node graph.

| Field | Type | Notes |
|-------|------|-------|
| id | int | Unique within graph |
| label | string | e.g. "Conv1d [d=4]", "ReLU" |
| type | enum {AudioInput, Conv1d, Activation, AudioOutput} | — |
| color | RGBA | Blue (constitution) |
| position | Vec2 | Auto-laid-out, not user-editable in Phase 1 |
| inputPins | vector\<Pin\> | — |
| outputPins | vector\<Pin\> | — |

### GraphLink (UI)

Connection between two GraphNodes.

| Field | Type | Notes |
|-------|------|-------|
| id | int | Unique |
| sourceNodeId | int | — |
| sourcePinIndex | int | — |
| destNodeId | int | — |
| destPinIndex | int | — |

### WeightBlob (Serialization)

Binary representation of model weights for DAW state persistence.

| Field | Type | Notes |
|-------|------|-------|
| data | vector\<uint8_t\> | Output of `torch::save` |
| modelHash | uint64 | Hash of architecture params — used to validate compatibility on load |

## Validation Rules

1. `depth >= 1` — enforced by APVTS parameter range
2. `kernelSize >= 2` — enforced by APVTS parameter range
3. `channels >= 1` — enforced by APVTS parameter range
4. On state restore: if `modelHash` doesn't match rebuilt architecture, discard weights and re-randomize with stored seed
5. Receptive field display must update within 200 ms of parameter change (SC-008)
