# Tasks: Embedded Builder & Interactive Graph (Phase 2)

**Input**: Design documents from `specs/002-embedded-builder-graph/`

**Prerequisites**: `plan.md` (required), `spec.md` (required), `research.md`, `data-model.md`, `contracts/`, `quickstart.md`

**Tests**: No explicit TDD/test-first request in the feature specification; implementation tasks include validation steps and existing test-target integration points.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare project scaffolding and documentation contracts for Phase 2 implementation.

- [X] T001 Create implementation traceability matrix mapping FR/SC -> task IDs in `specs/002-embedded-builder-graph/quickstart.md`
- [X] T002 [P] Add graph feature flags/constants for Phase 2 flows in `OpenYourBox/Source/params/ParamIDs.h`
- [X] T003 [P] Add Phase 2 parameter registration placeholders in `OpenYourBox/Source/params/ParamLayout.cpp`
- [X] T004 [P] Add shared graph editor configuration constants (zoom limits, map defaults) in `OpenYourBox/Source/graph/GraphTypes.h`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core architecture and runtime safety prerequisites required before any user story implementation.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T005 Refactor graph state container to support stable element/connection IDs and persisted viewport state in `OpenYourBox/Source/graph/NodeGraph.h`
- [X] T006 Implement graph state container updates and serialization hooks in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T006A Implement immutable DAG compilation for Audio I/O, Linear, Conv1D, Activation, and TCN live nodes in `OpenYourBox/Source/dsp/LiveGraphEngine.h`
- [X] T006B Implement topological runtime construction, shape validation, and live graph execution in `OpenYourBox/Source/dsp/LiveGraphEngine.cpp`
- [X] T007 [P] Add immutable-to-audio-thread graph snapshot handoff interface in `OpenYourBox/Source/PluginProcessor.h`
- [X] T008 Implement atomic graph snapshot apply path in `OpenYourBox/Source/PluginProcessor.cpp`
- [X] T009 [P] Add weighted-element metadata and randomization capability flags to node definitions in `OpenYourBox/Source/graph/GraphTypes.h`
- [X] T010 [P] Add freeze-operation request/response DTOs aligned to contract in `OpenYourBox/Source/graph/GraphTypes.h`
- [X] T011 Add freeze workflow coordinator interface (request queue, progress, completion states) in `OpenYourBox/Source/freeze/FreezeCoordinator.h` and own it from `OpenYourBox/Source/PluginEditor.h`
- [X] T012 Implement detached freeze coordination and non-blocking UI completion handling in `OpenYourBox/Source/freeze/FreezeCoordinator.cpp` and `OpenYourBox/Source/PluginEditor.cpp`

**Checkpoint**: Foundation ready - user story implementation can now begin in priority order.

---

## Phase 3: User Story 1 - Build a Custom Neural Network from the Element Menu (Priority: P1) 🎯 MVP

**Goal**: Deliver an editable graph with element palette, drag/drop creation, movement, and connection logic with shape validation.

**Independent Test**: Add Audio In -> Conv1D -> Audio Out from palette, connect successfully, and verify incompatible link rejects with mismatch feedback.

- [X] T013 [P] [US1] Implement element palette data source (Audio I/O, Linear, Conv1D, Activations, TCN) in `OpenYourBox/Source/graph/NodeRenderer.h`
- [X] T014 [US1] Implement element palette rendering and drag start behavior in `OpenYourBox/Source/graph/NodeRenderer.cpp`
- [X] T015 [US1] Implement graph element creation from drop target coordinates in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T016 [US1] Implement node dragging/movement state updates in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T017 [US1] Implement connection begin/complete logic with direction validation in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T018 [US1] Implement shape compatibility checks and rejected connection signaling in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T018A [US1] Implement cycle-detection guard before committing new connections in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T018B [US1] Add cycle-rejection user feedback path (tooltip/message) in `OpenYourBox/Source/graph/NodeRenderer.cpp`
- [X] T018C [US1] Add cycle-prevention validation case in `Tests/ProcessorIntegrationTests.cpp`
- [X] T019 [US1] Render invalid connection preview (red cable + tooltip text) in `OpenYourBox/Source/graph/NodeRenderer.cpp`
- [X] T020 [US1] Remove remaining live TCN menu entry points from graph UI menus in `OpenYourBox/Source/PluginEditor.cpp`

**Checkpoint**: User Story 1 is independently functional and testable (MVP graph editing core).

---

## Phase 4: User Story 2 - Edit Element Parameters Inline from the Graph View (Priority: P1)

**Goal**: Enable per-node inline property editing with ML Forge-style row layout, including full TCN parameter editing.

**Independent Test**: Select Conv1D and TCN nodes, edit inline values (kernel size, channels, dilation), and verify live behavior updates.

- [X] T021 [P] [US2] Define ordered inline property schema for element types in `OpenYourBox/Source/graph/GraphTypes.h`
- [X] T022 [US2] Implement property value binding between graph node properties and runtime parameters in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T023 [US2] Render one-row-per-property UI (input box then label) in `OpenYourBox/Source/graph/NodeRenderer.cpp`
- [X] T024 [US2] Implement inline text input parsing/validation feedback for numeric parameters in `OpenYourBox/Source/graph/NodeRenderer.cpp`
- [X] T025 [US2] Implement TCN single-node property panel with full editable parameter set in `OpenYourBox/Source/dsp/TCNModel.h`
- [X] T026 [US2] Wire TCN inline edits to live model update path in `OpenYourBox/Source/dsp/TCNModel.cpp`
- [X] T027 [US2] Add plugin-level synchronization for inline graph edits to processor state in `OpenYourBox/Source/PluginProcessor.cpp`
- [X] T027A [US2] Publish validated live graph runtime snapshots after topology or property edits in `OpenYourBox/Source/PluginProcessor.cpp`

**Checkpoint**: User Story 2 is independently functional and testable.

---

## Phase 5: User Story 5 - Per-Element Weight Randomization and Seed (Priority: P1)

**Goal**: Add per-weighted-element randomize control and persisted signed 32-bit seed with deterministic, element-local behavior.

**Independent Test**: Randomize one of two weighted nodes with a seed, verify only that node changes, reapply seed for deterministic repeat, save/reload and confirm seed restoration.

- [X] T028 [P] [US5] Add per-element seed field and signed 32-bit validation model in `OpenYourBox/Source/graph/GraphTypes.h`
- [X] T029 [US5] Implement per-element seed persistence in plugin state serialization/deserialization in `OpenYourBox/Source/PluginProcessor.cpp`
- [X] T030 [P] [US5] Add randomization request API scoped to one target element in `OpenYourBox/Source/dsp/LiveGraphEngine.h`
- [X] T031 [US5] Implement deterministic randomization using signed 32-bit seed for all mutable parameters in `OpenYourBox/Source/dsp/LiveGraphEngine.cpp`
- [X] T032 [US5] Implement auto-initialize-then-randomize behavior for uninitialized weighted elements in `OpenYourBox/Source/dsp/LiveGraphEngine.cpp`
- [X] T033 [US5] Add per-node randomize button + seed input UI in inline property renderer in `OpenYourBox/Source/graph/NodeRenderer.cpp`
- [X] T034 [US5] Hide or disable randomization controls for non-weighted and frozen nodes in `OpenYourBox/Source/graph/NodeRenderer.cpp`
- [X] T035 [US5] Connect UI randomize action to element-scoped runtime update path in `OpenYourBox/Source/PluginEditor.cpp`

**Checkpoint**: User Story 5 is independently functional and testable.

---

## Phase 6: User Story 3 - Navigate the Graph with Trackpad and Map View (Priority: P2)

**Goal**: Provide trackpad pan/zoom and map view click-to-navigate for large graph workflows.

**Independent Test**: Create 10+ node graph, pan/zoom via trackpad, and navigate through map clicks to distant regions.

- [X] T036 [P] [US3] Add viewport pan/zoom state model and clamp logic in `OpenYourBox/Source/graph/NodeGraph.h`
- [X] T037 [US3] Implement trackpad gesture event handling to graph viewport updates in `OpenYourBox/Source/ui/ImGuiHost.cpp`
- [X] T038 [US3] Implement map view projection model and viewport rectangle calculation in `OpenYourBox/Source/graph/NodeRenderer.cpp`
- [X] T039 [US3] Implement map view click-to-center behavior in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T040 [US3] Persist and restore viewport/map state in plugin project state handling in `OpenYourBox/Source/PluginProcessor.cpp`

**Checkpoint**: User Story 3 is independently functional and testable.

---

## Phase 7: User Story 4 - Freeze and Unfreeze Selected Elements (Priority: P2)

**Goal**: Deliver manual right-click freeze/unfreeze with background compilation, atomic swap, and frozen-node metrics.

**Independent Test**: Freeze a valid connected selection into one Gold node with progress UI and uninterrupted audio; unfreeze restores original nodes and links.

- [X] T041 [P] [US4] Implement multi-node selection and context menu freeze action in `OpenYourBox/Source/graph/NodeRenderer.cpp`
- [X] T042 [US4] Build freeze request payload serialization from selected subgraph in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T043 [US4] Implement Python worker IPC dispatch/reply handling in `OpenYourBox/Source/freeze/FreezeCoordinator.cpp` and connect it from `OpenYourBox/Source/PluginEditor.cpp`
- [X] T044 [US4] Render non-blocking compile lifecycle progress and completion feedback in `OpenYourBox/Source/PluginEditor.cpp`
- [X] T045 [US4] Implement successful freeze replacement to single Gold BlackBox node in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T046 [US4] Implement unfreeze restoration from stored source-subgraph snapshot in `OpenYourBox/Source/graph/NodeGraph.cpp`
- [X] T047 [US4] Implement frozen-node live performance metrics rendering in `OpenYourBox/Source/graph/NodeRenderer.cpp`
- [X] T048 [US4] Enforce prepared runtime publication and click-free audio-thread swaps for freeze/unfreeze in `OpenYourBox/Source/PluginProcessor.cpp`

**Checkpoint**: User Story 4 is independently functional and testable.

---

## Phase 8: User Story 7 - Convolution Dilation Support (Priority: P2)

**Goal**: Expose and apply dilation for Conv1D and TCN from inline graph editing.

**Independent Test**: Set dilation > 1 on Conv1D and TCN, confirm parameter is accepted and reflected in runtime behavior.

- [X] T049 [P] [US7] Add Conv1D dilation property definitions and validation bounds in `OpenYourBox/Source/graph/GraphTypes.h`
- [X] T050 [US7] Apply Conv1D dilation updates to runtime convolution configuration in `OpenYourBox/Source/dsp/TCNModel.cpp`
- [X] T051 [US7] Add TCN dilation property mapping across internal layers in `OpenYourBox/Source/dsp/TCNModel.cpp`
- [X] T052 [US7] Expose dilation rows in inline node property rendering for Conv1D and TCN in `OpenYourBox/Source/graph/NodeRenderer.cpp`

**Checkpoint**: User Story 7 is independently functional and testable.

---

## Phase 9: User Story 6 - DC Offset Protection via High-Pass Filter (Priority: P3)

**Goal**: Add standard DC blocker protection in the audio path to prevent harmful DC offset accumulation.

**Independent Test**: Run offset-prone configurations and verify output remains below DC threshold without obvious audible degradation.

- [X] T053 [P] [US6] Add DC blocker parameter/state container to DSP runtime in `OpenYourBox/Source/dsp/TCNModel.h`
- [X] T054 [US6] Implement first-order high-pass DC blocker stage in audio processing path in `OpenYourBox/Source/dsp/TCNModel.cpp`
- [X] T055 [US6] Integrate DC blocker into plugin processing chain for graph output in `OpenYourBox/Source/PluginProcessor.cpp`

**Checkpoint**: User Story 6 is independently functional and testable.

---

## Phase 10: Polish & Cross-Cutting Concerns

**Purpose**: Final consistency, performance, and validation across all stories.

- [X] T056 [P] Normalize terminology and UI labels (Blue/Gold, Freeze/Unfreeze, Randomize Weights) across editor surfaces in `OpenYourBox/Source/PluginEditor.cpp`
- [X] T057 [P] Improve edge-case user feedback messages (invalid seed, failed freeze, shape mismatch, cycle prevention) in `OpenYourBox/Source/graph/NodeRenderer.cpp` and `OpenYourBox/Source/PluginEditor.cpp`
- [X] T058 [P] Define timed validation procedure for SC-001 (build graph <=60s) and SC-010 (randomize <=5s) in `specs/002-embedded-builder-graph/quickstart.md`
- [ ] T058A Record 3-run timing results for SC-001 and SC-010 and append pass/fail table in `specs/002-embedded-builder-graph/quickstart.md`
- [ ] T058B Benchmark equivalent subgraph in Live (Blue) vs Frozen (Gold) mode and document latency comparison against SC-008 in `specs/002-embedded-builder-graph/quickstart.md`
- [X] T059 Run and update processor/runtime integration assertions (freeze/unfreeze restoration, causal frozen continuity, atomic swap, cycle rejection) in `Tests/ProcessorIntegrationTests.cpp` and `Tests/LiveGraphEngineTests.cpp`
- [X] T060 Run and update DSP/model assertions (TCN dilation, seeded randomization determinism, DC blocker) in `Tests/TCNModelTests.cpp`
- [ ] T061 Run full quickstart validation flow and document pass/fail notes in `specs/002-embedded-builder-graph/quickstart.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies; starts immediately
- **Phase 2 (Foundational)**: Depends on Phase 1; blocks all user stories
- **Phase 3 (US1)**: Depends on Phase 2
- **Phase 4 (US2)**: Depends on Phase 3 for reliable editable graph base
- **Phase 5 (US5)**: Depends on Phase 4 inline property system
- **Phase 6 (US3)**: Depends on Phase 3 graph rendering/state base
- **Phase 7 (US4)**: Depends on Phases 3 and 2 (graph selection + foundation)
- **Phase 8 (US7)**: Depends on Phase 4 inline property editing
- **Phase 9 (US6)**: Depends on Phase 2 DSP/runtime foundation
- **Phase 10 (Polish)**: Depends on all targeted user stories

### User Story Dependencies

- **US1 (P1)**: First MVP story; unlocks core graph editing surface
- **US2 (P1)**: Builds on US1 node rendering/state
- **US5 (P1)**: Builds on US2 inline properties and weighted-node metadata
- **US3 (P2)**: Can proceed after US1
- **US4 (P2)**: Needs foundational freeze infrastructure and stable graph editing from US1
- **US7 (P2)**: Builds on US2 property editing
- **US6 (P3)**: Mostly DSP-side and can proceed after foundation

### Parallel Opportunities

- Setup tasks marked `[P]` can run in parallel (`T002`, `T003`, `T004`)
- Foundational tasks marked `[P]` can run in parallel after `T005` (`T007`, `T009`, `T010`)
- Within US1, `T013` and `T019` can run in parallel once renderer skeleton exists
- Within US5, `T028`, `T030`, and `T033` can run in parallel before integration tasks
- Within US3, viewport state (`T036`) and gesture integration (`T037`) can proceed in parallel initially
- Within US4, payload build (`T042`) and UI progress states (`T044`) can proceed in parallel
- Within US7, property schema (`T049`) and UI exposure (`T052`) can proceed in parallel

---

## Parallel Example: User Story 5

```bash
# Parallelizable schema/runtime/UI tasks for US5:
T028 [P] [US5] Add per-element seed field and signed 32-bit validation model in OpenYourBox/Source/graph/GraphTypes.h
T030 [P] [US5] Add randomization request API scoped to one target element in OpenYourBox/Source/dsp/WeightRandomizer.h
T033 [US5] Add per-node randomize button + seed input UI in inline property renderer in OpenYourBox/Source/graph/NodeRenderer.cpp
```

---

## Implementation Strategy

### MVP First (Core Value)

1. Complete Phase 1 (Setup)
2. Complete Phase 2 (Foundational)
3. Complete Phase 3 (US1)
4. Complete Phase 4 (US2)
5. Complete Phase 5 (US5)
6. Validate graph editing + inline editing + seeded randomization as MVP

### Incremental Delivery

1. Deliver graph core (US1)
2. Deliver editable nodes (US2)
3. Deliver seeded randomization continuity (US5)
4. Add navigation ergonomics (US3)
5. Add freeze/unfreeze and metrics (US4)
6. Add dilation enhancements (US7)
7. Add DC safety stage (US6)

### Parallel Team Strategy

After Phase 2 completion:
- Developer A: US1 -> US2
- Developer B: US3 + US7
- Developer C: US4 + US6
- Developer D: US5 and cross-cutting polish/test updates

---

## Notes

- All tasks follow required checklist format: `- [ ] T### [P?] [US?] Description with file path`
- `[Story]` labels are present only for user-story phases
- Keep audio-thread safety constraints explicit in every runtime state-mutation implementation

---

## Phase 11: Convergence

- [ ] T062 [CRITICAL] Eliminate or formally guard audio-thread dynamic allocations in live/frozen graph execution (`LiveGraphRuntime::processHost`/`processTensor`) to satisfy Constitution Principle II + zero-allocation mandate in `OpenYourBox/Source/dsp/LiveGraphEngine.cpp`, `OpenYourBox/Source/PluginProcessor.cpp`, and related runtime publication paths per Constitution Governance #4 (contradicts)
