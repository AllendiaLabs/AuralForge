# Contract: VST3/AU Parameter Interface

**Scope**: DAW-facing parameter contract — how hosts and automation interact with OpenYourBox.

## Parameter List

All parameters are exposed via JUCE `AudioProcessorValueTreeState` and are fully DAW-automatable.

| Parameter ID | Display Name | Type | Range | Default | Units | Notes |
|-------------|-------------|------|-------|---------|-------|-------|
| `depth` | Depth | int | 1–999 | 4 | layers | Architecture rebuild on change |
| `kernel_size` | Kernel Size | int | 2–65 | 3 | samples | Architecture rebuild on change |
| `channels` | Channels | int | 1–512 | 16 | — | Architecture rebuild on change |
| `activation` | Activation | choice | 0–3 | 0 (ReLU) | — | 0=ReLU, 1=Sigmoid, 2=Tanh, 3=LeakyReLU |
| `randomize` | Randomize | bool | 0/1 | 0 | — | Rising edge (0→1) triggers weight randomization |
| `global_seed` | Seed | int | 0–2147483647 | 42 | — | Deterministic seed for randomization |
| `dry_wet` | Dry/Wet | float | 0.0–1.0 | 1.0 | — | 0.0 = fully dry, 1.0 = fully wet |

## Bus Configuration

| Bus | Direction | Channel Configs |
|-----|-----------|----------------|
| Main | Input | Mono (1ch) or Stereo (2ch) |
| Main | Output | Matches input channel count |

The plugin declares flexible bus support. Actual channel count is determined at runtime by the host's bus layout.

## State Persistence

- **Parameters**: Serialized as XML via APVTS `copyState().createXml()`
- **Weights**: Appended as base64-encoded binary blob (output of `torch::save`) to the XML state
- **Architecture hash**: Stored alongside weights to validate compatibility on restore

## Behavioral Contract

1. Parameter changes for `depth`, `kernel_size`, `channels`, `activation` trigger an asynchronous model rebuild on the GUI thread. Audio continues with the previous model until the new one is atomically swapped in.
2. `randomize` parameter: on rising edge (0→1), new random weights are generated using `global_seed` + an incrementing counter. The parameter auto-resets to 0 after triggering.
3. Audio output during model swap: crossfade between old and new model output over ~64 samples to prevent clicks.
4. When no audio input is present (silence), the output must also be silence.
