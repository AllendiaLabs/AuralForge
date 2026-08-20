# Data Model: Steerable Discovery & Training

## Overview

Phase 3 extends the graph document and runtime with steerable TCN/Activation features, per-element training arm state and Weights provenance, dual-instance capture pairing, a master-owned capture set and train job, and Gold auto-load/Unfreeze that preserves trained weights.

## Entities

### Graph Node (extended)

**New / extended fields**
- `armedForTraining`: bool — only on nodes with trainable parameters; default `true`
- `weightsProvenance`: `random` | `file`
- `weightsSeed`: int — when `random`
- `weightsPath`: string — when `file` or post-train (absolute or app-relative path)
- `residual`: bool — `tcn` only; default `false` (set `true` for steerable-nafx parity)
- `dilationGrowth`: int ≥ 1 — `tcn`; layer *n* dilation = growth^n (RONN/steerable schedule)
- Activation enum includes `prelu` on `activation` and `tcn`

**TCN ports (extended)**
- Existing audio `in` / `out`
- New conditioning input pin `film` (FiLM) — `signalKind: conditioning`

**Validation**
- Nodes without trainable parameters MUST NOT persist `armedForTraining` as user-toggleable train inclusion
- `film` pin accepts conditioning sources (Knob, XY outputs, Merge conditioning out)
- Incompatible weight file load MUST leave prior provenance unchanged

### Training Arm State

Logical view of which trainable nodes enter the next train snapshot.

**Fields**
- `elementId`
- `armed`: bool (default true for trainable nodes)

**Rules**
- Train requires ≥1 armed trainable element
- Disarmed trainable nodes remain Blue outside auto-loaded Gold
- Control sources are implicitly excluded

### Element Weights

**Fields**
- `mode`: `seed` | `path`
- `seed` or `path`
- Browse target: user-selected file path

**Relationships**
- Belongs to one weight-bearing Graph Node or Gold BlackBox
- Unfreeze copies path-mode weights onto restored Blue children

### Plugin Instance Pairing

**Fields**
- `sessionId`
- `role`: `master` | `slave` (master = capture initiator)
- `peerInstanceId` / endpoint
- `syncState`: `unpaired` | `discovering` | `paired` | `recording` | `error`
- `captureRole`: `clean` | `processed` | `unassigned`
- `captureBypass`: bool (default `true` while capture session active)

**Rules**
- Exactly one Clean and one Processed before Record
- Slave UI omits full Train / capture-set ownership

### Sample Pair (x, y)

**Fields**
- `pairId`
- `xPath`, `yPath` — lossless or float PCM files of instance **inputs**
- `durationSeconds`
- `sampleRate`, `numChannels`
- `capturedAt`
- `bypassUsed` (metadata)

**Validation**
- Non-empty aligned duration; failed/partial takes discarded
- No maximum duration field constraint

### Training Library Entry

**Fields**
- `id`, `displayName`, `createdAt`
- `source`: `capture` | `import`
- `durationSeconds`, `sampleRate`, `channels`
- `xPath`, `yPath`, `byteSize`
- `notes` (optional), `tags[]` (optional)
- `selectedForTrain`: bool

**Rules**
- Persist in user data across sessions
- Delete removes owned audio files after confirm
- Mixed sample rates across selected set → Train blocked with message (v1)

### Training Library

**Fields**
- `ownerInstanceId` (master)
- `entries[]` — Training Library Entry
- `selectedPairIds[]` — subset used for the next Train
- Local directory under plugin user data; disk usage aggregatable

**Rules**
- Record and import both **append** entries
- Train consumes **selected** entries snapshot at Run
- ≥1 selected pair required to enable Train (with copyright ack)
- UI contract: `contracts/training-library-ui-contract.md`

### Copyright Acknowledgment

**Fields**
- `acknowledged`: bool
- `acknowledgedAt`
- `textVersion` (disclaimer revision)

**Storage**: local user-data only (never network)

### Training Job

**Fields**
- `jobId` / `request_id`
- `state`: `idle` | `running` | `paused` | `stopping` | `succeeded` | `failed` | `stopped`
- `step`, `totalSteps` (default ~2500)
- `loss` (latest)
- `learningRate`
- `armedElementIds[]`
- `captureSetSnapshotRef`
- `artifactPath` (on success)
- `errorMessage` (on failure)

**Transitions**
- `idle` → `running` (Run, gates passed)
- `running` ↔ `paused` (Pause/Resume)
- `running`|`paused` → `stopped` (Stop; no model swap)
- `running` → `succeeded` (export + auto-load path)
- `running`|`paused` → `failed`
- While `running`|`paused`, audio uses **prior** model

### Steerable Gold BlackBox (extended)

**Fields** (existing BlackBox +)
- `origin`: `manual_freeze` | `train_autoload`
- `weightsPath` (trained artifact)
- `sourceSubgraph` including weight provenance for Unfreeze
- Conditioning-capable compiled forward `g(x, c)` when FiLM was in armed graph

**Rules**
- Auto-load replaces armed trainable chain only
- Unfreeze restores Blue graph **with trained weights retained**
- Weights remain until randomize or retrain/reload

### TCN (steerable)

**Properties**
- Existing: depth, kernel_size, channels, dilation, activation, gain
- New: `residual`, activation includes `prelu`
- Port: `film` conditioning in

### Activation (extended)

**Properties**
- activation includes `prelu`
- Existing gain retained

## Relationships (summary)

```text
Master Instance
  ├── owns CaptureSet (SamplePairs)
  ├── owns TrainingJob
  ├── owns Graph (armed trainable nodes + control sources)
  └── pairs-with Slave Instance (capture role + bypass)

TrainingJob --uses--> CaptureSet + Armed Subgraph
TrainingJob --produces--> Steerable Gold BlackBox
Gold BlackBox --unfreeze--> Blue nodes + Element Weights (path retained)
```
