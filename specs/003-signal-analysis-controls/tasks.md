---

description: "Task list for Signal Analysis & Expressive Input Controls (Phase 2.2)"
---

# Tasks: Signal Analysis & Expressive Input Controls (Phase 2.2)

**Input**: Design documents from `specs/003-signal-analysis-controls/`

**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`

**Tests**: No dedicated test-first tasks generated because the feature spec did not explicitly request TDD. Existing automated coverage and quickstart validation are included where they materially reduce regression risk.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g. `US1`, `US2`)
- Every task includes an exact file path

## Path Conventions

- Plug-in/editor code: `AuralForge/Source/`
- Graph document + editor rendering: `AuralForge/Source/graph/`, `AuralForge/Source/ui/`, `AuralForge/Source/PluginEditor.*`
- Runtime/DSP code: `AuralForge/Source/dsp/`, `AuralForge/Source/PluginProcessor.*`
- Regression tests: `Tests/`

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare Phase 2.2 documentation, shared naming, and implementation anchors before code changes.

- [ ] T001 Update feature execution notes and artifact references in `specs/003-signal-analysis-controls/plan.md`
- [ ] T002 Create implementation placeholders and section anchors for Phase 2.2 follow-on work in `specs/003-signal-analysis-controls/contracts/analysis-runtime-contract.md`
- [ ] T003 [P] Create implementation placeholders and section anchors for control-surface work in `specs/003-signal-analysis-controls/contracts/graph-control-ui-contract.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Extend the shared graph document and editor/runtime plumbing required by all user stories.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [ ] T004 Extend Phase 2.2 graph entities, enums, and persisted metadata in `AuralForge/Source/graph/GraphTypes.h`
- [ ] T005 Implement serialization and restore logic for analysis state, gain, knob modes, and XY bindings in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T006 Update graph-document interfaces for Phase 2.2 property/control access in `AuralForge/Source/graph/NodeGraph.h`
- [ ] T007 [P] Add shared editor-to-runtime callback hooks for analysis requests and alternate control modes in `AuralForge/Source/graph/NodeRenderer.h`
- [ ] T008 [P] Add processor/editor plumbing for Phase 2.2 graph-state publication and invalidation in `AuralForge/Source/PluginEditor.h`
- [ ] T009 Implement processor-facing runtime hooks for analysis snapshot publication and gain-aware graph updates in `AuralForge/Source/PluginProcessor.h`

**Checkpoint**: Shared graph state and runtime plumbing are ready; user stories can now be implemented.

---

## Phase 3: User Story 1 - Inspect Cumulative Signal Transformation at Any Element (Priority: P1) 🎯 MVP

**Goal**: Users can open per-element cumulative analysis views with stereo overlays, probe fallback, and Gold-node parity.

**Independent Test**: Build `Audio In -> Activation -> Audio Out`, open analysis on the Activation node, confirm transfer/frequency/phase views show both channels; then freeze a valid chain and confirm the Gold node exposes the same views.

### Implementation for User Story 1

- [ ] T010 [P] [US1] Add analysis snapshot types and revision-tracking helpers in `AuralForge/Source/dsp/LiveGraphEngine.h`
- [ ] T011 [US1] Implement cumulative analysis snapshot generation with live-input/probe fallback in `AuralForge/Source/dsp/LiveGraphEngine.cpp`
- [ ] T012 [P] [US1] Add analysis-panel rendering data and probe/live status presentation in `AuralForge/Source/ui/InfoPanel.h`
- [ ] T013 [US1] Implement transfer/frequency/phase panel rendering with left/right shared plots in `AuralForge/Source/ui/InfoPanel.cpp`
- [ ] T014 [US1] Add node selection analysis-entry behavior and panel orchestration in `AuralForge/Source/PluginEditor.cpp`
- [ ] T015 [US1] Extend node rendering to expose analysis entry points for Blue and Gold nodes in `AuralForge/Source/graph/NodeRenderer.cpp`
- [ ] T016 [US1] Add analysis parity and fallback regression coverage in `Tests/ProcessorIntegrationTests.cpp`

**Checkpoint**: User Story 1 is independently functional and validates live-node analysis, probe fallback, and Gold-node analysis parity.

---

## Phase 4: User Story 2 - Shape Nonlinearity Steepness with Gain on Activation and TCN Elements (Priority: P1)

**Goal**: Activation and TCN nodes expose a persistent Gain property that updates audio and analysis in real time.

**Independent Test**: Add an Activation node, change `Gain` across its range during playback, confirm audible response changes and the transfer plot updates immediately; repeat with a TCN node.

### Implementation for User Story 2

- [ ] T017 [P] [US2] Add gain-capable node-property definitions for Activation and TCN nodes in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T018 [P] [US2] Extend node-property rendering and validation for the Gain field in `AuralForge/Source/graph/NodeRenderer.cpp`
- [ ] T019 [US2] Implement gain propagation from editor graph state into processor/runtime configuration in `AuralForge/Source/PluginEditor.cpp`
- [ ] T020 [US2] Add gain-aware nonlinearity handling for Activation and TCN execution paths in `AuralForge/Source/dsp/TCNModel.cpp`
- [ ] T021 [US2] Ensure processor/runtime updates invalidate analysis snapshots after Gain changes in `AuralForge/Source/PluginProcessor.cpp`
- [ ] T022 [US2] Add save/restore and audible-change regression coverage for Gain in `Tests/ProcessorIntegrationTests.cpp`

**Checkpoint**: User Story 2 is independently functional and validates persistent Gain editing with synchronized audio and analysis updates.

---

## Phase 5: User Story 3 - Edit Continuous Parameters with Rotary Knobs (Priority: P2)

**Goal**: Users can switch supported continuous parameters from text editing to knob editing without losing validation or persistence.

**Independent Test**: Select a node with a supported continuous property, switch the property to knob mode, sweep through valid values, and confirm runtime updates plus restored mode selection after reload.

### Implementation for User Story 3

- [ ] T023 [P] [US3] Add knob-mode metadata and persistence support for node properties in `AuralForge/Source/graph/GraphTypes.h`
- [ ] T024 [US3] Implement knob-mode storage and restore behavior in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T025 [US3] Implement rotary knob controls with numeric readout for supported properties in `AuralForge/Source/graph/NodeRenderer.cpp`
- [ ] T026 [US3] Integrate knob edits into existing property-change and recompile flows in `AuralForge/Source/PluginEditor.cpp`
- [ ] T027 [US3] Add knob-mode persistence and boundary-behavior regression coverage in `Tests/LiveGraphEngineTests.cpp`

**Checkpoint**: User Story 3 is independently functional and validates knob editing, value clamping, and state recall.

---

## Phase 6: User Story 4 - Control Two Parameters Simultaneously with an XY Trackpad (Priority: P2)

**Goal**: Users can bind two supported parameters on one node to an XY control and update both in real time.

**Independent Test**: Assign two supported parameters on one node to X/Y, move the pointer through the XY surface, and confirm both values and audio output change in real time; reload state and confirm the binding returns.

### Implementation for User Story 4

- [ ] T028 [P] [US4] Add XY binding structures, validation rules, and persistence fields in `AuralForge/Source/graph/GraphTypes.h`
- [ ] T029 [US4] Implement XY binding storage, restore, and invalid-binding cleanup in `AuralForge/Source/graph/NodeGraph.cpp`
- [ ] T030 [US4] Add XY binding configuration UI and two-axis control rendering in `AuralForge/Source/graph/NodeRenderer.cpp`
- [ ] T031 [US4] Integrate XY-driven property updates into runtime recompile and analysis invalidation flows in `AuralForge/Source/PluginEditor.cpp`
- [ ] T032 [US4] Add XY binding recall and simultaneous-parameter update regression coverage in `Tests/ProcessorIntegrationTests.cpp`

**Checkpoint**: User Story 4 is independently functional and validates XY binding assignment, live updates, and persistence.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Finish cross-story integration, performance, and release-readiness work.

- [ ] T033 [P] Document Phase 2.2 user validation flow and expected outcomes in `specs/003-signal-analysis-controls/quickstart.md`
- [ ] T034 Optimize analysis refresh/invalidation behavior across editor and runtime boundaries in `AuralForge/Source/PluginEditor.cpp`
- [ ] T035 [P] Run and stabilize Phase 2.2 regression coverage in `Tests/ProcessorIntegrationTests.cpp`
- [ ] T036 Validate final implementation against Phase 2.2 contracts and update notes in `specs/003-signal-analysis-controls/contracts/analysis-runtime-contract.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1: Setup**: No dependencies; starts immediately.
- **Phase 2: Foundational**: Depends on Phase 1 and blocks all story work.
- **Phase 3: US1**: Starts after Phase 2 and defines the MVP.
- **Phase 4: US2**: Starts after Phase 2; benefits from US1 analysis plumbing but remains independently testable.
- **Phase 5: US3**: Starts after Phase 2; depends on canonical property metadata from the foundational phase.
- **Phase 6: US4**: Starts after Phase 2 and after property metadata conventions are in place; can follow US3 for cleaner reuse of control-mode infrastructure.
- **Phase 7: Polish**: Starts after desired user stories are complete.

### User Story Dependencies

- **US1 (P1)**: No dependency on other user stories after Foundational phase.
- **US2 (P1)**: No strict dependency on US1, but analysis visibility from US1 improves validation of Gain behavior.
- **US3 (P2)**: No dependency on US1 or US2 after Foundational phase.
- **US4 (P2)**: Depends conceptually on the same canonical property/control metadata as US3 and is simplest after US3.

### Within Each User Story

- Data/model updates before rendering and runtime integration.
- Rendering and control entry points before end-to-end orchestration.
- Runtime invalidation/update logic before regression coverage.
- Story checkpoint validation before moving to the next priority.

### Parallel Opportunities

- `T002` and `T003`
- `T007` and `T008`
- `T010` and `T012`
- `T017` and `T018`
- `T023` can proceed independently from other US3 tasks once Foundational work is done
- `T028` can proceed independently from other US4 tasks once Foundational work is done

---

## Parallel Example: User Story 1

```bash
Task: "Add analysis snapshot types and revision-tracking helpers in AuralForge/Source/dsp/LiveGraphEngine.h"
Task: "Add analysis-panel rendering data and probe/live status presentation in AuralForge/Source/ui/InfoPanel.h"
```

## Parallel Example: User Story 2

```bash
Task: "Add gain-capable node-property definitions for Activation and TCN nodes in AuralForge/Source/graph/NodeGraph.cpp"
Task: "Extend node-property rendering and validation for the Gain field in AuralForge/Source/graph/NodeRenderer.cpp"
```

## Parallel Example: User Story 3

```bash
Task: "Add knob-mode metadata and persistence support for node properties in AuralForge/Source/graph/GraphTypes.h"
Task: "Add knob-mode persistence and boundary-behavior regression coverage in Tests/LiveGraphEngineTests.cpp"
```

## Parallel Example: User Story 4

```bash
Task: "Add XY binding structures, validation rules, and persistence fields in AuralForge/Source/graph/GraphTypes.h"
Task: "Add XY binding recall and simultaneous-parameter update regression coverage in Tests/ProcessorIntegrationTests.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational
3. Complete Phase 3: User Story 1
4. Validate cumulative analysis, fallback probe behavior, and Gold-node parity
5. Stop and review before expanding controls

### Incremental Delivery

1. Deliver US1 for analysis visibility
2. Add US2 for gain-driven sonic shaping
3. Add US3 for knob-based parameter control
4. Add US4 for XY paired control
5. Finish with performance/polish validation

### Parallel Team Strategy

1. One developer completes Foundational graph/runtime plumbing
2. After Phase 2:
   - Developer A: US1 analysis views
   - Developer B: US2 gain integration
   - Developer C: US3 knob mode
3. US4 begins once shared property-mode conventions are stable

---

## Notes

- All tasks follow the required checklist format.
- Story labels appear only in user-story phases.
- Each user story is scoped to remain independently testable after the foundational phase.
- File paths point to the current AuralForge plug-in structure rather than generic template paths.
