# Contract: DSP Processing Pipeline

**Scope**: Audio thread processing contract — the invariants that `processBlock` must uphold.

## Processing Flow

```
Audio Input Buffer (blockSize samples × numChannels)
        │
        ▼
┌─────────────────┐
│ Look-back Buffer │──→ Prepend (receptiveField-1) past samples
│ (circular)       │
└─────────────────┘
        │
        ▼
Padded Input Tensor: [numChannels, receptiveField-1 + blockSize]
        │
        ▼
┌─────────────────┐
│  Input Projection │──→ Conv1d: inputChannels → channels
└─────────────────┘
        │
        ▼
┌─────────────────────────────┐
│  Layer 0: Conv1d(dil=1) + Act │
│  Layer 1: Conv1d(dil=2) + Act │
│  ...                           │
│  Layer N: Conv1d(dil=2^N) + Act│
└─────────────────────────────┘
        │
        ▼
┌──────────────────┐
│ Output Projection │──→ Conv1d: channels → outputChannels
└──────────────────┘
        │
        ▼
Take last blockSize samples
        │
        ▼
Dry/Wet Mix: output = dry * input + wet * processed
        │
        ▼
Audio Output Buffer (blockSize samples × numChannels)
```

## Invariants

1. **Zero allocations**: No `new`, `malloc`, `torch::empty`, or `torch::zeros` calls during `processBlock`. All tensors are pre-allocated in `prepareToPlay` or during model swap.
2. **Output length == Input length**: Always. No added latency reported to host (latency = 0 samples).
3. **Silence in → Silence out**: When input RMS < -120 dBFS, output must be < -120 dBFS.
4. **Thread safety**: Audio thread reads model via `atomic_load`. Never writes to model. Never blocks.
5. **Sample rate independence**: Model operates on raw samples. No sample-rate-dependent behavior beyond the look-back buffer size (which is measured in samples, not time).
6. **Buffer size independence**: Works with any block size from 1 to 8192+ samples.
7. **Model swap**: When a new model becomes available via atomic pointer, audio thread picks it up on the next `processBlock` call. Old model is released when its refcount reaches zero (deallocation happens off audio thread).

## Pre-allocated Resources (set in prepareToPlay)

| Resource | Size | Lifetime |
|----------|------|----------|
| Padded input tensor | [maxChannels, maxReceptiveField + maxBlockSize] | Until `releaseResources` |
| Output tensor | [maxChannels, maxBlockSize] | Until `releaseResources` |
| Look-back buffer | [maxChannels, maxReceptiveField] | Until `releaseResources` or model swap |
