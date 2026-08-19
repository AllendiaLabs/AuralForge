# Quickstart: Validate Embedded Builder & Interactive Graph

## Prerequisites

- macOS development environment with JUCE, LibTorch, and project dependencies available
- Python worker environment available for freeze compilation
- Buildable plugin target from repository root

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

## Suggested Test Commands

If test binaries are available in the local workflow, run the existing test targets after building. Current repository test sources include:

- `Tests/ProcessorIntegrationTests.cpp`
- `Tests/TCNModelTests.cpp`

## Exit Criteria

- All palette, graph editing, navigation, randomization, and freeze flows complete successfully
- No audio interruption occurs during randomization or freeze swap
- Frozen node metrics are visible
- Saved seeds restore after reload
