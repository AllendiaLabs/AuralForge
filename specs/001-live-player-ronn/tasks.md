# Tasks: The Live Player & RONN (Phase 1)

**Input**: Design documents from `specs/001-live-player-ronn/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Not explicitly requested — test tasks omitted.

**Organization**: Tasks grouped by user story for independent implementation.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization, build system, and dependency integration

- [ ] T001 Create CMakeLists.txt at repository root (top-level) with JUCE 8.x, LibTorch, Dear ImGui, and imgui-node-editor as dependencies
- [ ] T002 [P] Create source directory structure: AuralForge/Source/dsp/, AuralForge/Source/graph/, AuralForge/Source/params/, AuralForge/Source/ui/
- [ ] T003 [P] Add LibTorch CMake find-package configuration and verify torch::Tensor compiles in CMakeLists.txt
- [ ] T004 [P] Add Dear ImGui and imgui-node-editor as vendored dependencies under AuralForge/ThirdParty/ and integrate into CMakeLists.txt
- [ ] T005 [P] Add NOTICE file at repository root with MIT license attribution for ML Forge derived patterns
- [ ] T006 Update AuralForge/AuralForge.jucer or CMakeLists.txt to enable juce_opengl module for ImGui rendering

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that ALL user stories depend on

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T007 Define parameter string IDs and ranges in AuralForge/Source/params/ParamIDs.h (depth, kernel_size, channels, activation, randomize, global_seed, dry_wet)
- [ ] T008 Implement APVTS parameter layout builder in AuralForge/Source/params/ParamLayout.h/.cpp returning juce::AudioProcessorValueTreeState::ParameterLayout
- [ ] T009 Implement TCNModel as torch::nn::Module subclass in AuralForge/Source/dsp/TCNModel.h/.cpp with configurable depth, kernel_size, channels, activation, causal padding, input/output projection, and forward() method
- [ ] T010 Implement LookbackBuffer (circular buffer) in AuralForge/Source/dsp/LookbackBuffer.h/.cpp with prepend-to-tensor, update-from-block, resize, and clear operations
- [ ] T011 Implement atomic model swap infrastructure in AuralForge/Source/dsp/WeightRandomizer.h/.cpp — buildModel() on GUI thread, atomic shared_ptr publish, randomizeWeights() using seed + counter
- [ ] T012 Wire APVTS into AuralForge/Source/PluginProcessor.h/.cpp — construct APVTS in constructor, add parameter listeners that trigger model rebuild via WeightRandomizer

**Checkpoint**: Foundation ready — TCN model builds, look-back buffer works, parameters are declared, atomic swap compiles.

---

## Phase 3: User Story 1 — Load Plugin and Process Audio Immediately (Priority: P1) 🎯 MVP

**Goal**: Audio plays through the default TCN with zero configuration on plugin load.

**Independent Test**: Load plugin on audio track, play audio, verify output is audibly processed with no user action.

- [ ] T013 [US1] Implement prepareToPlay() in AuralForge/Source/PluginProcessor.cpp — build default TCN model (depth=4, kernel_size=3, channels=16, ReLU), allocate look-back buffer and padded input tensor, store sample rate
- [ ] T014 [US1] Implement processBlock() in AuralForge/Source/PluginProcessor.cpp — atomic-load current model, prepend look-back buffer to input, run forward(), extract last blockSize samples, apply dry/wet mix, update look-back buffer (zero allocations)
- [ ] T015 [US1] Implement releaseResources() in AuralForge/Source/PluginProcessor.cpp — release pre-allocated tensors and clear look-back buffer
- [ ] T016 [US1] Implement isBusesLayoutSupported() in AuralForge/Source/PluginProcessor.cpp — accept mono and stereo configurations, reject mismatched input/output channel counts
- [ ] T017 [US1] Verify silence-in-silence-out behavior — ensure processBlock outputs silence when input RMS < -120 dBFS

**Checkpoint**: Plugin loads, default TCN processes audio immediately. Core value proposition works.

---

## Phase 4: User Story 2 — Manipulate TCN Parameters in Real Time (Priority: P1)

**Goal**: Changing depth, kernel size, channels, or activation audibly changes the effect without audio interruption.

**Independent Test**: Load plugin, adjust each parameter while audio plays, verify audible change with no dropouts.

- [ ] T018 [US2] Implement parameter change listener callback in AuralForge/Source/PluginProcessor.cpp — detect changes to depth/kernel_size/channels/activation, trigger asynchronous model rebuild on GUI thread via WeightRandomizer::buildModel()
- [ ] T019 [US2] Implement crossfade logic in processBlock() in AuralForge/Source/PluginProcessor.cpp — when new model is swapped in, crossfade over ~64 samples between old and new model output to prevent clicks
- [ ] T020 [US2] Handle look-back buffer invalidation on model swap in AuralForge/Source/PluginProcessor.cpp — clear buffer and accept brief transient on architecture change
- [ ] T021 [US2] Implement receptive field and parameter count computation in AuralForge/Source/dsp/TCNModel.h/.cpp — getReceptiveField() returning samples and milliseconds, getParameterCount() returning total trainable params

**Checkpoint**: All four architecture parameters are adjustable in real time with audible results and no dropouts.

---

## Phase 5: User Story 5 — Stable DAW Integration (Priority: P1)

**Goal**: Plugin passes VST3/AU validation, saves/restores state, multiple instances work independently.

**Independent Test**: Run VST3 validator and auval, save/load DAW project, run multiple instances simultaneously.

- [ ] T022 [US5] Implement getStateInformation() in AuralForge/Source/PluginProcessor.cpp — serialize APVTS state as XML, append model weights as base64 blob via torch::save, include architecture hash for validation
- [ ] T023 [US5] Implement setStateInformation() in AuralForge/Source/PluginProcessor.cpp — deserialize APVTS state, rebuild model from parameters, load weight blob if architecture hash matches, else re-randomize with stored seed
- [ ] T024 [US5] Verify no shared mutable static state across instances — audit all static/global variables in AuralForge/Source/ and ensure all state is per-instance
- [ ] T025 [US5] Validate plugin builds as both VST3 and AU targets in CMakeLists.txt and passes basic host scan

**Checkpoint**: Plugin is DAW-stable — saves/restores state, passes validation, multiple instances are independent.

---

## Phase 6: User Story 3 — Randomize Weights for Glitch/Discovery (Priority: P2)

**Goal**: User triggers weight randomization via UI, MIDI CC, or DAW automation and hears an instant new timbre.

**Independent Test**: Click randomize repeatedly, verify each press produces different sound with no dropouts.

- [ ] T026 [US3] Implement randomize trigger detection in AuralForge/Source/PluginProcessor.cpp — listen for rising edge on `randomize` parameter, call WeightRandomizer::randomizeWeights() with global_seed + incrementing counter. Verify deterministic reproduction: given identical seed + counter + architecture params, two calls produce byte-identical weights
- [ ] T027 [US3] Implement MIDI CC mapping for randomize in AuralForge/Source/PluginProcessor.cpp — audio thread scans incoming MIDI buffer for assigned CC and sets an atomic flag; GUI thread polls the flag and performs actual randomization via WeightRandomizer (no randomization logic on audio thread). Add `randomize_cc` parameter (AudioParameterInt, range 0–127, default 64) to APVTS for user-assignable CC number
- [ ] T028 [US3] Ensure rapid successive randomizations are safe — WeightRandomizer queues or coalesces rebuild requests, audio thread never blocks in AuralForge/Source/dsp/WeightRandomizer.cpp
- [ ] T029 [US3] Verify randomize parameter auto-resets to false after triggering in AuralForge/Source/PluginProcessor.cpp

**Checkpoint**: Weight randomization works via UI, MIDI CC, and DAW automation with zero audio dropouts.

---

## Phase 7: User Story 4 — Visual Node Graph with Blue Modular Nodes (Priority: P2)

**Goal**: Plugin UI shows the TCN architecture as Blue interconnected nodes that update when parameters change.

**Independent Test**: Open plugin, verify Blue nodes visible, change depth, verify nodes added/removed at 60 FPS.

- [ ] T030 [P] [US4] Implement Dear ImGui ↔ JUCE integration in AuralForge/Source/ui/ImGuiHost.h/.cpp — create OpenGLContext, initialize ImGui with OpenGL3 backend, forward JUCE mouse/keyboard events to ImGui IO
- [ ] T031 [P] [US4] Define graph data types in AuralForge/Source/graph/GraphTypes.h — GraphNode struct (id, label, type, color, position, pins), GraphLink struct, Pin struct, NodeType enum
- [ ] T032 [US4] Implement NodeGraph data model in AuralForge/Source/graph/NodeGraph.h/.cpp — rebuildFromModel() that creates Audio In → [Conv1d + Activation] × depth → Audio Out nodes and links, auto-layout positions
- [ ] T033 [US4] Implement NodeRenderer in AuralForge/Source/graph/NodeRenderer.h/.cpp — render nodes as Blue imgui-node-editor nodes with labels showing layer type and params, render links between pins
- [ ] T034 [US4] Wire node graph into PluginEditor in AuralForge/Source/PluginEditor.h/.cpp — create ImGuiHost, call NodeRenderer each frame, trigger NodeGraph::rebuildFromModel() on parameter changes
- [ ] T035 [US4] Implement info panel overlay in AuralForge/Source/ui/InfoPanel.h/.cpp — display receptive field (ms) and parameter count, update within 200 ms of parameter change (depends on T021 for getReceptiveField()/getParameterCount())
- [ ] T036 [US4] Implement randomize button in AuralForge/Source/ui/RandomizeButton.h/.cpp — Dear ImGui button that triggers the randomize APVTS parameter

**Checkpoint**: Full UI with Blue node graph, info panel, and randomize button. Graph updates on parameter changes at 60 FPS.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Edge cases, warnings, and final hardening

- [ ] T037 [P] Add receptive field size warning in AuralForge/Source/ui/InfoPanel.cpp — display warning when receptive field exceeds configurable threshold (e.g., >1 second)
- [ ] T038 [P] Handle sample rate change in AuralForge/Source/PluginProcessor.cpp — detect rate change in prepareToPlay(), reinitialize look-back buffer and update receptive field display
- [ ] T039 [P] Handle edge case of buffer size smaller than minimum required input in AuralForge/Source/PluginProcessor.cpp — accumulate via look-back buffer (already handled by design, verify)
- [ ] T040 [P] Add Doxygen documentation headers to all public classes and methods across AuralForge/Source/
- [ ] T041 Run quickstart.md validation scenarios (V1–V9) and document results

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 completion — BLOCKS all user stories
- **US1 (Phase 3)**: Depends on Phase 2 — MVP target
- **US2 (Phase 4)**: Depends on Phase 3 (needs working processBlock to test parameter changes)
- **US5 (Phase 5)**: Depends on Phase 3 (needs working plugin to validate)
- **US3 (Phase 6)**: Depends on Phase 2 (WeightRandomizer foundation); can parallel with US2/US5
- **US4 (Phase 7)**: Depends on Phase 2 (model and params); can parallel with US1–US3
- **Polish (Phase 8)**: Depends on all desired user stories being complete

### User Story Dependencies

```
Phase 1 (Setup) → Phase 2 (Foundation)
                        │
                        ├──→ US1 (Phase 3) ──→ US2 (Phase 4)
                        │                  ──→ US5 (Phase 5)
                        ├──→ US3 (Phase 6) [parallel with US2/US5]
                        └──→ US4 (Phase 7) [parallel with all stories]
                                                    │
                                                    ▼
                                            Phase 8 (Polish)
```

### Parallel Opportunities

- **Phase 1**: T002, T003, T004, T005 are all parallel (different files/directories)
- **Phase 2**: T007 and T008 parallel; T009 and T010 parallel (different files)
- **Phase 7**: T030 and T031 parallel (different files); T035 and T036 parallel
- **Cross-story**: US3 and US4 can run in parallel with US2/US5 after Foundation

---

## Parallel Example: Phase 7 (User Story 4)

```bash
# Launch ImGui host and graph types in parallel:
Task: "Implement Dear ImGui ↔ JUCE integration in AuralForge/Source/ui/ImGuiHost.h/.cpp"
Task: "Define graph data types in AuralForge/Source/graph/GraphTypes.h"

# Then sequentially:
Task: "Implement NodeGraph data model in AuralForge/Source/graph/NodeGraph.h/.cpp"
Task: "Implement NodeRenderer in AuralForge/Source/graph/NodeRenderer.h/.cpp"

# Then info panel and randomize button in parallel:
Task: "Implement info panel overlay in AuralForge/Source/ui/InfoPanel.h/.cpp"
Task: "Implement randomize button in AuralForge/Source/ui/RandomizeButton.h/.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational
3. Complete Phase 3: User Story 1
4. **STOP and VALIDATE**: Load plugin in DAW, play audio through it, confirm effect is audible
5. This alone delivers the core value proposition

### Incremental Delivery

1. Setup + Foundational → Build compiles with JUCE + LibTorch + ImGui
2. Add US1 → Audio processes through TCN → **MVP!**
3. Add US2 → Parameters change sound in real time
4. Add US5 → DAW state save/load works, plugin passes validation
5. Add US3 → Weight randomization for creative discovery
6. Add US4 → Visual node graph UI
7. Polish → Edge cases, warnings, documentation

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- All tensor/buffer allocations happen in prepareToPlay() or GUI thread — never in processBlock()
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
