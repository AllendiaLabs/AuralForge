# Quickstart: Steerable Discovery & Training

End-to-end validation for Phase 3. Assumes OpenYourBox builds with Phase 2 + 2.2 baselines (freeze, Knob/XY, Merge, TCN/Activation).

## Prerequisites

- AU/VST3 OpenYourBox in a DAW
- Optional: two insert slots (dry + after reference FX) for capture
- Optional: aligned clean/processed wav pair for file-only train
- Python env packaged like freeze worker (PyTorch + MR-STFT dependency matching specified sizes)

## Scenario 1 — Steerable TCN graph (no train)

1. Add TCN + Activation; enable residual; PReLU; set **Dilation growth** (try preset 8 or 10); confirm dilation/RF readout.
2. Connect XY (or two Knobs via Merge) to TCN **FiLM** pin.
3. Confirm arm on TCN (default on); absent on Knob/XY; Weights shows `seed N`.

**Expect**: Shape-legal FiLM; Gⁿ dilations; control sources not armable.

## Scenario 2 — Training library: import + capture

1. Open Library; **Import** clean + processed files; select the pair; preview x and y.
2. Optionally: dual-instance Capture (master/slave, Clean/Processed, bypass on) → Record → Stop; confirm pair **added** to same library with Capture badge.
3. Multi-select / deselect; confirm Train summary shows selected count.

**Expect**: File-only path works; capture is additive; selection gates Train.

## Scenario 3 — Copyright gate

1. With ≥1 selected pair, open Train before acknowledgment.
2. Complete blocking modal; confirm Train enables (with armed elements).

**Expect**: Ack persists across relaunch.

## Scenario 4 — Non-blocking train + Stop

1. Run Train; play through prior model; watch loss/step and `Train window ≈ N s` / RF info.
2. Pause → Resume → Stop.

**Expect**: No model swap on Stop; UI usable; no train-induced audio dropouts.

## Scenario 5 — Success → Gold + free c

1. Steerable-nafx-equivalent graph (FiLM + residual + PReLU + growth 8/10 + XY).
2. Run to completion (~2500 steps or shortened test build).
3. Confirm armed chain → Gold BlackBox; Knob/XY remain Blue and morph timbre; Weights shows artifact path.

**Expect**: No separate Freeze; MR-STFT recipe as specified.

## Scenario 6 — Unfreeze keeps weights

1. Unfreeze train Gold BlackBox.
2. Confirm Blue graph + trained Weights path retained.
3. Randomize one element → seed provenance replaces path for that element.

**Expect**: Weights survive Unfreeze until randomize/retrain/reload.

## Scenario 7 — Disarm exclusion

1. Disarm one trainable node; Train successfully.
2. Confirm it remains Blue outside Gold.

**Expect**: Only armed trainable nodes absorbed.

## Scenario 8 — Mixed sample rate blocked

1. Select two library pairs with different sample rates.
2. Attempt Train.

**Expect**: Blocked with clear message (v1).

## References

- `spec.md` — US1–US5, FR-*, SC-*
- `contracts/training-library-ui-contract.md`
- `contracts/train-worker-ipc.md`
- `contracts/steerable-graph-ui-contract.md`
- `contracts/instance-pairing-capture-contract.md`
- `data-model.md`, `research.md`, `plan.md`
