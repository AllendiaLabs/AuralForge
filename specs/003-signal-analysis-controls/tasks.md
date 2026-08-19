---
description: "Task list for Signal Analysis & Expressive Input Controls (Phase 2.2)"
---

# Tasks: Signal Analysis & Expressive Input Controls (Phase 2.2)

**Input**: Design documents from `specs/003-signal-analysis-controls/`

**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`

**Tests**: No dedicated test-first tasks — the spec does not request TDD. Regression tasks are included at story checkpoints where they materially reduce risk.

**Organization**: Tasks grouped by user story for independent implementation and validation.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: User story label (`US1`–`US4`) on story-phase tasks only
- Every task includes an exact file path

## Path Conventions

- Plug-in code: `AuralForge/Source/`
- Graph document: `AuralForge/Source/graph/`
- UI: `AuralForge/Source/ui/`
- DSP/runtime: `AuralForge/Source/dsp/`
- Editor/processor: `AuralForge/Source/PluginEditor.*`, `PluginProcessor.*`
- Tests: `Tests/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Confirm design artifacts and implementation anchors before code changes.

- [ ] T001 Verify Phase 2.2 design artifact cross-links in `specs/003-signal-analysis-controls/plan.md`
- [ ] T002 [P] Add implementation anchor notes for dual-curve N-channel analysis in `specs/003-signal-analysis-controls/contracts/analysis-runtime-contract.md`
- [ ] T003 [P] Add implementation anchor notes for Knob/XY/Merge conditioning in `specs/003-signal-analysis-controls/contracts/graph-control-ui-contract.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared graph types, port kinds, Merge conditioning lanes, persistence scaffolding, and editor/runtime hooks required by all user stories.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [ ] T004 Add `knobInput`, `xyTrackpad`, `SignalKind`, and analysis preference types in `AuralForge/Source/graph/GraphTypes.h`
- [ ] T005 [P] Add `AnalysisSnapshot`, dual curve families, and revision helpers in `AuralForge/Source/dsp/LiveGraphEngine.h`
- [ ] T006 Extend connection validation for audio vs conditioning pins in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T007 Extend Merge to accept conditioning inputs and default `c = 0` when absent in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T008 Implement Merge conditioning combine behavior in `AuralForge/Source/dsp/LiveGraphEngine.cpp`
- [ ] T009 Add graph `ValueTree` serialization for conditioning nodes, gain, and analysis prefs in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T010 Update graph document interfaces for Phase 2.2 in `AuralForge/Source/graph/NodeGraph.h`
- [ ] T011 [P] Add editor analysis/conditioning callback hooks in `AuralForge/Source/graph/NodeRenderer.h`
- [ ] T012 [P] Add graph revision tokens and live-capture publication for transfer marker in `AuralForge/Source/PluginProcessor.h`
- [ ] T013 Implement revision invalidation and live-capture handoff in `AuralForge/Source/PluginProcessor.cpp`
- [ ] T014 Wire graph-state invalidation orchestration in `AuralForge/Source/PluginEditor.h`

**Checkpoint**: Shared types, Merge conditioning, persistence, and runtime hooks are ready.

---

## Phase 3: User Story 1 - Inspect Chain vs Element Response at Any Graph Point (Priority: P1) 🎯 MVP

**Goal**: Per-element transfer, frequency, and phase views with **chain** and **element-only** curve families, all channel/feature dimensions, probe fallback, transfer live marker, and Gold parity.

**Independent Test**: Build `Audio In → Activation → Audio Out`; open analysis on Activation; confirm dual families in all three views with N traces (stereo minimum); static curves while stopped; chain transfer marker on-curve during playback; repeat on frozen Gold node.

### Implementation for User Story 1

- [ ] T015 [P] [US1] Implement chain-path analysis compile/run in `AuralForge/Source/dsp/LiveGraphEngine.cpp`
- [ ] T016 [P] [US1] Implement element-only isolated analysis path in `AuralForge/Source/dsp/LiveGraphEngine.cpp`
- [ ] T017 [US1] Implement live-preferred / white-noise probe fallback in `AuralForge/Source/dsp/LiveGraphEngine.cpp`
- [ ] T018 [US1] Implement Gold BlackBox boundary analysis parity in `AuralForge/Source/dsp/LiveGraphEngine.cpp`
- [ ] T019 [P] [US1] Add analysis panel state and snapshot consumption API in `AuralForge/Source/ui/InfoPanel.h`
- [ ] T020 [US1] Render chain vs element-only transfer/frequency/phase plots with N-channel legend in `AuralForge/Source/ui/InfoPanel.cpp`
- [ ] T021 [US1] Render transfer live marker on chain curve during playback only in `AuralForge/Source/ui/InfoPanel.cpp`
- [ ] T022 [US1] Add analysis request orchestration and stale-snapshot handling in `AuralForge/Source/PluginEditor.cpp`
- [ ] T023 [US1] Expose analysis entry points on Blue and Gold nodes in `AuralForge/Source/graph/NodeRenderer.cpp`
- [ ] T024 [US1] Add dual-curve, probe-fallback, and Gold-parity regression coverage in `Tests/ProcessorIntegrationTests.cpp`

**Checkpoint**: User Story 1 independently validates analysis views per `quickstart.md` scenarios 1–3 and 8.

---

## Phase 4: User Story 2 - Shape Nonlinearity Steepness with Gain (Priority: P1)

**Goal**: Activation and TCN expose persistent inline **Gain** (0.1–10.0, default 1.0) affecting runtime nonlinearity slope and analysis transfer curves.

**Independent Test**: Edit Gain on Activation then TCN during playback; confirm audible slope change and transfer curve update without glitches.

### Implementation for User Story 2

- [ ] T025 [P] [US2] Register Gain property on Activation and TCN nodes in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T026 [P] [US2] Render and validate Gain inline property in `AuralForge/Source/graph/NodeRenderer.cpp`
- [ ] T027 [US2] Propagate Gain edits through editor recompile path in `AuralForge/Source/PluginEditor.cpp`
- [ ] T028 [US2] Apply Gain scaling in Activation execution path in `AuralForge/Source/dsp/LiveGraphEngine.cpp`
- [ ] T029 [US2] Apply Gain scaling in TCN execution path in `AuralForge/Source/dsp/TCNModel.cpp`
- [ ] T030 [US2] Invalidate analysis snapshots when Gain changes in `AuralForge/Source/PluginProcessor.cpp`
- [ ] T031 [US2] Add Gain save/restore and audible-change regression in `Tests/ProcessorIntegrationTests.cpp`

**Checkpoint**: User Story 2 independently validates `quickstart.md` scenario 4.

---

## Phase 5: User Story 3 - Add Knob Input as Network Conditioning Source (Priority: P2)

**Goal**: **Knob Input** graph source element with rotary UI, conditioning output, menu placement, direct and Merge routing, persistence, and runtime steering without modifying inline architectural params.

**Independent Test**: Add Knob Input; wire to Merge (with Audio In) or directly to TCN; rotate during playback; confirm audio changes and inline params unchanged; reload state.

### Implementation for User Story 3

- [ ] T032 [P] [US3] Implement Knob Input node factory, pins, and default properties in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T033 [US3] Add Knob Input to element menu and node rendering in `AuralForge/Source/graph/NodeRenderer.cpp`
- [ ] T034 [US3] Accept Knob→Merge and Knob→element connections in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T035 [US3] Publish Knob conditioning values into runtime compile context in `AuralForge/Source/PluginEditor.cpp`
- [ ] T036 [US3] Consume Knob conditioning in live graph forward path in `AuralForge/Source/dsp/LiveGraphEngine.cpp`
- [ ] T037 [US3] Persist and restore Knob node state in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T038 [US3] Add Knob routing and persistence regression in `Tests/LiveGraphEngineTests.cpp`

**Checkpoint**: User Story 3 independently validates `quickstart.md` scenarios 5 and 7 (Knob path).

---

## Phase 6: User Story 4 - Add XY Trackpad as Dual Conditioning Source (Priority: P2)

**Goal**: **XY Trackpad** graph source with X/Y outputs, menu placement, direct and Merge routing, real-time dual-axis steering, persistence.

**Independent Test**: Add XY Trackpad; wire X/Y to Merge or element inputs; move pointer during playback; confirm dual output readouts and audio response; reload state.

### Implementation for User Story 4

- [ ] T039 [P] [US4] Implement XY Trackpad node factory, X/Y pins, and pad state in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T040 [US4] Add XY Trackpad to element menu and pad UI rendering in `AuralForge/Source/graph/NodeRenderer.cpp`
- [ ] T041 [US4] Accept XY→Merge and XY→element connections in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T042 [US4] Publish XY conditioning into runtime compile context in `AuralForge/Source/PluginEditor.cpp`
- [ ] T043 [US4] Consume XY conditioning in live graph forward path in `AuralForge/Source/dsp/LiveGraphEngine.cpp`
- [ ] T044 [US4] Persist and restore XY node state in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T045 [US4] Add XY routing, Merge combo, and persistence regression in `Tests/ProcessorIntegrationTests.cpp`

**Checkpoint**: User Story 4 independently validates `quickstart.md` scenarios 6 and 7 (XY path).

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Cross-story integration, performance, documentation, and release readiness.

- [ ] T046 [P] Validate multi-channel analysis readability (high N legend/styling) in `AuralForge/Source/ui/InfoPanel.cpp`
- [ ] T047 Optimize analysis refresh throttling and 60 FPS guardrails in `AuralForge/Source/PluginEditor.cpp`
- [ ] T048 [P] Run full `quickstart.md` validation and update outcomes in `specs/003-signal-analysis-controls/quickstart.md`
- [ ] T049 [P] Update contract implementation status notes in `specs/003-signal-analysis-controls/contracts/analysis-runtime-contract.md`
- [ ] T050 [P] Update contract implementation status notes in `specs/003-signal-analysis-controls/contracts/graph-control-ui-contract.md`
- [ ] T051 Stabilize Phase 2.2 regression suite in `Tests/ProcessorIntegrationTests.cpp` and `Tests/LiveGraphEngineTests.cpp`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1**: No dependencies — start immediately
- **Phase 2**: Depends on Phase 1 — **blocks all user stories**
- **Phases 3–6**: Depend on Phase 2; may proceed in priority order or parallel after Phase 2
- **Phase 7**: After desired user stories complete

### User Story Dependencies

| Story | Priority | Depends on | Independent test |
|-------|----------|------------|------------------|
| US1 | P1 | Phase 2 only | Dual-curve analysis, N channels, Gold parity |
| US2 | P1 | Phase 2 only (US1 improves validation) | Gain on Activation/TCN |
| US3 | P2 | Phase 2 + Merge conditioning (T007–T008) | Knob Input element |
| US4 | P2 | Phase 2 + Merge conditioning (T007–T008) | XY Trackpad element |

US3 and US4 can run in parallel after Phase 2; US4 does not require US3 completion.

### Parallel Opportunities

- Phase 1: `T002`, `T003`
- Phase 2: `T005`, `T011`, `T012`
- US1: `T015`, `T016`, `T019`
- US2: `T025`, `T026`
- US3: `T032` (after T004–T008)
- US4: `T039` (after T004–T008)
- Polish: `T046`, `T048`, `T049`, `T050`

---

## Parallel Example: User Story 1

```bash
# Snapshot paths in parallel:
Task: "Implement chain-path analysis compile/run in AuralForge/Source/dsp/LiveGraphEngine.cpp"
Task: "Implement element-only isolated analysis path in AuralForge/Source/dsp/LiveGraphEngine.cpp"
Task: "Add analysis panel state and snapshot consumption API in AuralForge/Source/ui/InfoPanel.h"
```

## Parallel Example: User Story 3 + 4 (after Phase 2)

```bash
# Different developers on conditioning source nodes:
Task: "Implement Knob Input node factory ... in AuralForge/Source/graph/NodeGraph.cpp"
Task: "Implement XY Trackpad node factory ... in AuralForge/Source/graph/NodeGraph.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Complete Phase 1 + Phase 2
2. Complete Phase 3 (US1)
3. **Stop and validate** per `quickstart.md` scenarios 1–3, 8
4. Demo cumulative analysis before conditioning controls

### Incremental Delivery

1. US1 → analysis visibility (MVP)
2. US2 → Gain shaping (pairs with US1 transfer plots)
3. US3 → Knob Input conditioning
4. US4 → XY Trackpad conditioning
5. Phase 7 → polish and full quickstart pass

### Parallel Team Strategy

1. Team completes Phase 2 together
2. Then split:
   - Dev A: US1 analysis pipeline + InfoPanel
   - Dev B: US2 Gain
   - Dev C: US3 Knob Input
   - Dev D: US4 XY Trackpad (after or parallel with US3)

---

## Notes

- Supersedes prior tasks.md that described inline knob/XY **property modes** — replaced by graph source elements per clarified spec.
- Knob Input and XY Trackpad are excluded from freeze subgraph compilation (Phase 2.2 scope).
- All tasks use required checklist format with sequential IDs T001–T051.
