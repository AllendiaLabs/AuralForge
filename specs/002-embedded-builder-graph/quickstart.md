# Quickstart: Validate Embedded Builder & Interactive Graph

## Prerequisites

- macOS development environment with JUCE, LibTorch, and project dependencies available
- Python worker environment available for freeze compilation
- Buildable plugin target from repository root

## Implementation Traceability

| Requirement / criterion | Implementation tasks |
|---|---|
| FR-001–FR-003, SC-001 | T013–T016 |
| FR-004–FR-005, FR-020 | T017–T019, T018A–T018C |
| FR-006–FR-009, SC-002 | T021–T027 |
| FR-010–FR-012, SC-005–SC-006 | T036–T040 |
| FR-013–FR-017, SC-003–SC-004, SC-008 | T041–T048, T058B, T059 |
| FR-018 | T049–T052, T060 |
| FR-019, SC-007 | T053–T055, T060 |
| FR-021–FR-029, SC-009–SC-012 | T028–T035, T058–T060 |
| Cross-cutting UX and validation | T056–T061 |

## Build

From the repository root:

```sh
./build.sh
```

If the normal local workflow uses the generated Xcode/CMake project directly, build the plugin and test targets for the active configuration.

## Validation Scenarios

### 1. Build and connect a graph

1. Open the plugin in a host.
2. Open graph view.
3. Drag `Audio Input`, `Conv1D`, and `Audio Output` from the palette.
4. Connect them in order.
5. Verify a valid cable is rendered and audio passes through.
6. Attempt one incompatible connection and verify it is rejected with shape-mismatch feedback.

Expected result:
- Graph editing is interactive.
- Invalid connections do not commit.

Related references:
- `contracts/graph-editor-ui.md`
- `data-model.md` (`Element`, `Port`, `Connection`)

### 2. Edit inline node properties

1. Select the `Conv1D` node.
2. Change inline parameters such as kernel size and dilation.
3. Confirm the node renders one property per row and audio behavior updates.

Expected result:
- Inline property layout matches the node contract.
- Parameter changes affect the active live graph.

### 3. Validate per-element randomization

1. Add two weighted nodes.
2. Enter a signed 32-bit seed on one node.
3. Trigger `Randomize Weights`.
4. Confirm only the selected node changes.
5. Reapply the same seed and confirm the result is reproducible.
6. Save and reload the plugin/project state and confirm the seed value is restored.

Expected result:
- Randomization is local to the selected weighted node.
- Seeded randomization is deterministic and persistent.

Timed validation for SC-010:

1. Prepare a project containing at least two weighted live nodes.
2. Start a monotonic timer when the tester first selects the target node.
3. Stop the timer when `Randomize Weights` publishes the replacement runtime
   and the processed output changes.
4. Repeat three times without changing the graph.
5. Record every duration; all three runs must be below 5 seconds.

### 4. Validate trackpad navigation and map view

1. Create a graph with 10+ nodes spread across the canvas.
2. Use two-finger pan and pinch zoom.
3. Open map view and click multiple distant regions.

Expected result:
- Pan/zoom stay responsive.
- Map view navigates accurately to the selected area.

### 5. Freeze and unfreeze a selected subgraph

1. Select a valid connected live subgraph.
2. Trigger `Freeze Selection`.
3. Observe progress feedback.
4. Confirm the selection becomes one Gold locked node.
5. Inspect the frozen node for live performance metrics.
6. Trigger `Unfreeze`.

Expected result:
- Freeze compiles through the worker contract in `contracts/freeze-selection-ipc.md`.
- Audio remains uninterrupted through swap.
- Unfreeze restores the prior live subgraph.

### 6. Validate DC blocker behavior

1. Run a graph configuration likely to generate DC offset.
2. Inspect output audio behavior with the feature enabled.

Expected result:
- Output remains free of damaging DC offset drift.

## Timed Build Validation

Use this procedure for SC-001:

1. Reset to an empty graph and start a monotonic timer when the plug-in editor
   becomes interactive.
2. Add `Audio Input`, one weighted processing element, and `Audio Output`.
3. Connect the three nodes and confirm non-silent processed output.
4. Stop the timer at the first confirmed processed buffer.
5. Repeat from a reset graph three times; all runs must complete within 60
   seconds.

Record measured results only from an instrumented DAW session:

| Criterion | Run 1 | Run 2 | Run 3 | Result |
|---|---:|---:|---:|---|
| SC-001 graph build (≤ 60 s) | pending | pending | pending | pending |
| SC-010 randomize (≤ 5 s) | pending | pending | pending | pending |

For SC-008, construct one connected subgraph, measure average per-buffer
inference time for at least 1,000 buffers in Live Blue mode, freeze the same
selection, then repeat in Gold mode with the same host sample rate and buffer
size. Gold passes only when its measured average is lower than Blue.

## Suggested Test Commands

If test binaries are available in the local workflow, run the existing test targets after building. Current repository test sources include:

- `Tests/ProcessorIntegrationTests.cpp`
- `Tests/TCNModelTests.cpp`
- `Tests/LiveGraphEngineTests.cpp`
- `Tests/test_freeze_worker.py`

## Automated Validation Record

Validation run on 2026-08-19 using the Release CMake build:

| Check | Result | Notes |
|---|---|---|
| C++ target build | PASS | `OpenYourBoxTests`, `OpenYourBoxProcessorTests`, and `OpenYourBoxLiveGraphTests` built successfully |
| CTest suite | PASS | 4/4 tests passed: TCN model, processor integration, live graph, and freeze worker |
| Frozen causal continuity | PASS | Whole-buffer and split-buffer frozen execution produced identical output |
| Signed-seed compatibility | PASS | C++ live weights and Python freeze-worker weights matched the same fixed fixture |
| Runtime replacement | PASS | Processor integration published an edited graph and produced finite non-silent crossfaded output |
| DC rejection | PASS | Processor integration remained below the 0.001 threshold |

The DAW-operated timing rows for SC-001, SC-008, and SC-010 remain pending.
They must be measured in an instrumented host and must not be inferred from
headless unit-test duration.

## Exit Criteria

- All palette, graph editing, navigation, randomization, and freeze flows complete successfully
- No audio interruption occurs during randomization or freeze swap
- Frozen node metrics are visible
- Saved seeds restore after reload
