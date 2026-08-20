# Implementation Plan: Embedded Builder & Interactive Graph

**Branch**: `002-embedded-builder-graph` | **Date**: 2026-08-19 | **Spec**: `specs/002-embedded-builder-graph/spec.md`

**Input**: Feature specification from `specs/002-embedded-builder-graph/spec.md`

## Summary

Implement Phase 2 of OpenYourBox by turning the current graph view into an editable ML Forge-style node editor inside the VST, adding an element palette, inline per-node parameter editing, per-element seeded randomization, trackpad navigation, map view, manual freeze/unfreeze with Python-backed compilation, live performance metrics, Conv1D/TCN dilation, and a built-in DC blocker. The design keeps all user interaction inside the plugin UI, preserves zero-allocation audio-thread rules, and routes all expensive graph compilation/state transitions through GUI/background-thread preparation before atomic swap into real-time processing.

## Technical Context

**Language/Version**: C++17 for plugin/runtime, Python 3 for backend worker

**Primary Dependencies**: JUCE, Dear ImGui, `imgui-node-editor`, LibTorch, Python/PyTorch backend worker

**Storage**: Local plugin/project state plus local `.pt` artifacts and JSON graph/freeze payloads

**Testing**: C++ test binaries under `Tests/`, integration validation via plugin/manual build-run scenarios

**Target Platform**: macOS VST3/AU plugin environment with desktop trackpad input

**Project Type**: Desktop audio plugin with embedded UI and local worker process

**Performance Goals**: 60 FPS UI responsiveness during backend load; < 7 ms live latency; < 5 ms frozen latency; < 2 s freeze compile for graphs under 10 layers; < 100 ms atomic swap

**Constraints**: VST is the only interface; zero audio-thread allocations; manual freeze only; Blue live nodes and Gold frozen nodes remain behaviorally distinct; graph edits and randomization swaps must be prepared off the audio thread

**Scale/Scope**: Single-plugin feature set supporting graphs of 50+ visible elements, weighted nodes including Linear/Conv1D/TCN, one local Python worker, and persisted per-element state including seeded randomization

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Single Interface, Decoupled Compute**: Pass. All user workflows remain in-plugin; backend worker stays detached and non-interactive.
- **Dual-Engine Execution Model**: Pass. Design keeps Blue live editability and Gold frozen execution paths distinct.
- **Manual Granular Freeze Policy**: Pass. Freeze is explicit right-click selection only; no auto-freeze introduced.
- **Shape Integrity & Legal Constraints**: Pass. Design preserves connection validation UI and does not weaken training acknowledgment requirements.
- **Zero Audio-Thread Allocation Rule**: Pass with implementation gate. All graph edits, randomization, and freeze swaps must be prepared on GUI/background threads and applied atomically.
- **Complexity Justification**: Pass. Added complexity is directly tied to Phase 2 goals in the constitution and is bounded to graph editing, freeze flow, and safe runtime state mutation.

## Project Structure

### Documentation (this feature)

```text
specs/002-embedded-builder-graph/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── freeze-selection-ipc.md
│   └── graph-editor-ui.md
└── tasks.md
```

### Source Code (repository root)

```text
OpenYourBox/
├── Source/
│   ├── PluginEditor.cpp
│   ├── PluginEditor.h
│   ├── PluginProcessor.cpp
│   ├── PluginProcessor.h
│   ├── dsp/
│   │   ├── LiveGraphEngine.cpp
│   │   ├── LiveGraphEngine.h
│   │   ├── LiveGraphPublisher.cpp
│   │   ├── LiveGraphPublisher.h
│   │   ├── LookbackBuffer.cpp
│   │   ├── LookbackBuffer.h
│   │   ├── TCNModel.cpp
│   │   ├── TCNModel.h
│   │   ├── TorchScriptBlackBox.cpp
│   │   ├── TorchScriptBlackBox.h
│   │   ├── WeightRandomizer.cpp
│   │   └── WeightRandomizer.h
│   ├── freeze/
│   │   ├── FreezeCoordinator.cpp
│   │   └── FreezeCoordinator.h
│   ├── graph/
│   │   ├── GraphTypes.h
│   │   ├── NodeGraph.cpp
│   │   ├── NodeGraph.h
│   │   ├── NodeRenderer.cpp
│   │   └── NodeRenderer.h
│   ├── params/
│   │   ├── ParamIDs.h
│   │   ├── ParamLayout.cpp
│   │   └── ParamLayout.h
│   └── ui/
│       ├── ImGuiHost.cpp
│       ├── ImGuiHost.h
│       ├── ImGuiOpenGLBackend.cpp
│       ├── InfoPanel.cpp
│       ├── InfoPanel.h
│       ├── RandomizeButton.cpp
│       └── RandomizeButton.h
├── Builds/
├── JuceLibraryCode/
└── OpenYourBox.jucer

Tests/
├── LiveGraphEngineTests.cpp
├── ProcessorIntegrationTests.cpp
├── TCNModelTests.cpp
└── test_freeze_worker.py

Backend/
└── freeze_worker.py
```

**Structure Decision**: Keep the existing single-plugin structure. Feature work concentrates in `OpenYourBox/Source/graph`, `OpenYourBox/Source/ui`, `OpenYourBox/Source/dsp`, `OpenYourBox/Source/params`, and targeted integration coverage in `Tests/`.

## Complexity Tracking

No constitution violations currently require justification.
