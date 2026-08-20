# Phase 0 Research: Steerable Discovery & Training

## Decision 1: Localhost master/slave pairing via JUCE InterprocessConnection + discovery registry

**Decision**: Pair two plug-in instances on the same machine using a localhost discovery registry (temp-dir JSON or named localhost port advertisement) plus a bidirectional control channel (`juce::InterprocessConnection` or equivalent TCP loopback). The instance that opens Capture Samples and initiates pairing is **master**; the peer is **slave**. Audio sample payloads for completed takes are transferred over the control channel or via shared temp WAV files referenced by message (prefer file paths for large unbounded captures).

**Rationale**: No cross-instance IPC exists today; constitution forbids external apps. Localhost IPC is DAW-safe, same-session, and keeps capture UI inside each VST window. File-backed clip transfer avoids huge in-memory IPC for unbounded recording length.

**Alternatives considered**:
- Shared memory ring only: harder to version and debug; still need a control channel.
- DAW side-chain / host API pairing: not portable across AU/VST3 hosts.
- Single-instance dual-input capture: rejected by spec (two instances required).

## Decision 2: Capture records host input buffers; default processing bypass

**Decision**: Both Clean and Processed roles record the instance’s **input** audio (pre-graph). A `captureBypass` flag (default **true** during active capture session) forces passthrough of input to output so monitoring is unaffected; user may disable bypass. Record start/stop is user-gated with **no max duration**; master stores the capture set.

**Rationale**: Spec clarifications. Wet reference lives upstream of the Processed instance in the DAW.

**Alternatives considered**:
- Record plugin output: rejected (clarification A).
- Fixed 30s cap: rejected (clarification D).

## Decision 3: Train via ChildProcess Python worker mirroring FreezeCoordinator

**Decision**: Add `TrainCoordinator` + `Backend/train_worker.py` using the same ChildProcess + JSON envelope pattern as freeze. Operations: `train_start`, streaming progress events (`step`, `loss`, `lr`, `status`), `train_pause`, `train_resume`, `train_stop`. On success, worker writes TorchScript `.pt` and returns `artifact_path` + metadata. Plugin prepares load off audio thread and atomically swaps Gold BlackBox for the **armed** subgraph only.

**Rationale**: Constitution decoupled compute; freeze path is proven; ml_forge-style Run/Pause/Stop maps cleanly to IPC commands.

**Alternatives considered**:
- In-process LibTorch training on GUI thread: rejected (blocks UI / risks RT interference).
- Extend `freeze_worker.py` only: possible later; separate `train_worker.py` keeps compile vs optimize concerns clear for v1.

## Decision 4: Fixed steerable NAfx training recipe (specified MR-STFT)

**Decision**: Adam; multiresolution STFT loss matching steerable-nafx / auraloss defaults: FFT sizes and window lengths **{32, 128, 512, 2048}**, hop sizes **{16, 64, 256, 1024}**; steer conditioning **ca = 0**; LR `1e-3 → 1e-4 @ 80% → 1e-5 @ 95%`; default **~2500** steps. Hyperparameters are product constants in the worker (not user-facing in v1).

**Rationale**: Spec FR-008; notebook cell training parameters; closes prior underspecification.

**Alternatives considered**:
- Vague “multiresolution STFT” without sizes: rejected (non-reproducible).
- Full hyperparameter UI: deferred.

## Decision 5b: Dilation growth^G (RONN + steerable-nafx), not power-of-two-only

**Decision**: Expose TCN **dilation growth** G so layer *n* uses dilation **G^n**. UI recommendation for best reproduce + clarity:

1. **Primary control**: integer **slider/stepper** labeled **Dilation growth** (RONN naming), range typically **1–16** (steerable notebook used up to 10).
2. **Live readout** (not a second parameter): resulting dilations `1 → G → G² → …` and **receptive field** in samples and ms at current sample rate.
3. **Optional preset chips** that only *set* G (do not replace the slider): e.g. `2` (RONN/WaveNet-like), `8` (steerable notebook demos), `10` (paper table demos).
4. **Default G = 2** for new TCNs (safe RF / CPU); users aiming at paper parity pick preset 8 or 10 + residual + PReLU + FiLM.

**Rationale**: Same mental model as RONN; presets reduce hunting for paper values; RF readout prevents accidental huge graphs. Avoid per-layer manual dilation editors in v1.

**Alternatives considered**:
- Presets-only (hide G): rejected — blocks arbitrary reproduce.
- Power-of-two-only shift UI: rejected — cannot hit G=8/10.
- Per-layer dilation list: power-user only; defer.

## Decision 4b: Receptive-field-aware cropping + segment length

**Decision**:

- **Cropping**: Always **RF-aware**; **no user toggle** in v1 (correctness). Train panel may show an informational line: `RF-aware crops · RF ≈ X ms`.
- **Segment length**: Product default ≈ steerable-nafx notebook (**~228308 samples**), clamped to `min(default, available_after_rf)`. **Do not** expose raw sample counts in primary UI.
- **Status only in v1**: show `Train window ≈ N s` derived from segment length / sample rate.
- **Long-term**: optional advanced control as **Train window (seconds)**, mapped internally to samples — never ask musicians for 228308.

**Rationale**: Matches notebook training dynamics; hides non-musical units; keeps v1 Train panel focused on Run/Pause/Stop/loss.

**Alternatives considered**:
- Expose sample-length slider like Colab: rejected for primary UX.
- Disable RF context for speed: rejected (incorrect causal training).

## Decision 4c: Training library = captures + file imports; user selects pairs

**Decision**: Maintain a master **training library**. Dual-instance Record **adds** pairs. Users may **import** file-based x/y pairs. Train runs on **selected** library entries only. UI and long-term capabilities are specified in `contracts/training-library-ui-contract.md` (list+detail, preview, tags/collections roadmap, persisted store).

**Rationale**: User clarification; Colab upload path + DAW capture; durable asset UX beyond a flat capture list.

## Decision 5: FiLM conditioning input port on TCN (Phase 3 supersedes 2.2 “no new ports”)

**Decision**: Add a dedicated **FiLM / conditioning** input pin on TCN (`signalKind: conditioning`). Knob, XY, or Merge may connect to it. For steerable-nafx parity, FiLM applies **per TCN block** (scale/shift from global **c**). Missing FiLM input treats conditioning as **0**.

**Rationale**: Spec US4; notebook `TCNBlock` + `FiLM`.

## Decision 6: Residual checkbox + PReLU on Activation and TCN

**Decision**: TCN gains boolean `residual` (checkbox; **on** for steerable-nafx parity). Activation type enums include **PReLU**. Gain-as-slope from Phase 2.2 remains. Other armed graphs remain allowed (FR-008b).

**Rationale**: Spec FR-012 / FR-013 / FR-008b.

## Decision 7: Arm only trainable-parameter elements; control sources never absorb

**Decision**: `armedForTraining` exists only on nodes with trainable parameters (`hasWeights` / parameterized processors). Default **armed**. Knob/XY/Audio In/Out and similar sources are never armable and never enter the Gold BlackBox. Train snapshot = armed trainable subgraph + I/O boundary for audio and FiLM conditioning.

**Rationale**: Spec clarifications (only trainable parameters).

**Alternatives considered**:
- Default-armed everything with user disarm of knobs: rejected (easy to swallow controls into Gold).

## Decision 8: Weights property — seed vs path + browse load

**Decision**: Weight-bearing elements expose **Weights** UI: display `seed N` when random; display filesystem **path** when file/train-backed. Browse opens native file chooser; compatible loads update tensors off audio thread then atomic swap; incompatible files error without changing weights. After train auto-load, Gold BlackBox Weights shows artifact path. After Unfreeze, restored Blue nodes keep trained weight tensors and path provenance until randomize or retrain/reload.

**Rationale**: Spec FR-015/016 and Unfreeze weight-preservation clarification.

**Alternatives considered**:
- Hide provenance: rejected (user request).
- Reset weights on Unfreeze: rejected (user request).

## Decision 9: Copyright acknowledgment — local persistent gate

**Decision**: Blocking modal before first Train; persist acknowledgment in plugin user-data (never uploaded). Train stays disabled until ack + ≥1 valid capture pair + ≥1 armed trainable element.

**Rationale**: Constitution IV.

**Alternatives considered**: Per-session re-ack: rejected (constitution: first training session).

## Decision 10: ml_forge UX patterns without embedding ml_forge

**Decision**: In-plugin ImGui Train panel: Run / Pause / Stop, status text, live loss and step. Capture menu is separate in-window panel. Do not ship or link Dear PyGui ml_forge app.

**Rationale**: Spec assumption + constitution single interface.

**Alternatives considered**:
- Launch ml_forge externally: constitution violation.

## Decision 11: Unfreeze after train keeps weights

**Decision**: Reuse Phase 2 `sourceSubgraph` Unfreeze path; when unfreezing a train-produced BlackBox, restore modular nodes **and** apply stored trained weight tensors / paths to those nodes. Randomize clears to new seed; successful retrain or Weights browse replaces.

**Rationale**: User clarification post-specify.

**Alternatives considered**:
- Unfreeze to random init: rejected.
