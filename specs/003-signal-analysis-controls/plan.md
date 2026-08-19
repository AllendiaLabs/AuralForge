# Implementation Plan: Signal Analysis & Expressive Input Controls (Phase 2.2)

**Branch**: `003-signal-analysis-controls` | **Date**: 2026-08-19 | **Spec**: `specs/003-signal-analysis-controls/spec.md`

**Input**: Feature specification from `specs/003-signal-analysis-controls/spec.md`

## Summary

Add Phase 2.2 graph-inspection and expressive-control features to the existing JUCE + Dear ImGui plug-in editor: per-element cumulative analysis views, gain-as-slope controls for Activation and TCN nodes, rotary knob editing for continuous properties, and per-element XY control for paired parameters. The design extends the current `NodeGraph`/`NodeRenderer` document-renderer split, keeps all heavy analysis work off the audio thread, and preserves freeze/unfreeze parity so Gold BlackBox nodes expose the same analysis views as live Blue nodes.

## Technical Context

**Language/Version**: C++17 for plug-in/runtime code, Python 3 for the existing embedded freeze worker

**Primary Dependencies**: JUCE 9, Dear ImGui, `imgui-node-editor`, LibTorch, embedded Python worker for freeze compilation

**Storage**: JUCE `ValueTree` plug-in state for graph document, viewport, seeds, and new control/analysis preferences; local TorchScript artifact files for frozen nodes

**Testing**: CTest-driven JUCE console apps (`AuralForgeProcessorTests`, `AuralForgeLiveGraphTests`, `AuralForgeTests`) plus Python freeze-worker tests

**Target Platform**: Desktop audio plug-in on macOS first, building AU/VST3 via CMake

**Project Type**: Single desktop audio plug-in project with embedded worker and native tests

**Performance Goals**: 60 FPS UI responsiveness during audio processing, frozen latency under 5 ms at 256-sample buffers, live latency under 7 ms, zero audible glitches during interactive control updates

**Constraints**: No standalone app, no audio-thread allocations, no audio-thread blocking, analysis and UI updates must not interfere with real-time processing, Phase 2.2 must extend the Phase 2 graph/editor model instead of replacing it

**Scale/Scope**: One plug-in editor surface, one graph document, 10s of nodes per session, stereo plots for each analysis view, multiple interactive control modes per editable node

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- `Single Interface, Decoupled Compute`: PASS. All Phase 2.2 capabilities stay inside the existing plug-in editor; no external tooling is introduced.
- `Dual-Engine Execution Model`: PASS. Blue live nodes and Gold frozen nodes remain first-class runtime modes, and the plan preserves analysis parity across both.
- `Manual Granular Freeze Policy`: PASS. Phase 2.2 does not alter freeze initiation or policy; it only adds analysis and control surfaces around existing nodes.
- `Shape Integrity & Legal Constraints`: PASS. No design weakens connection validation or legal acknowledgment flows.
- `Zero Audio Allocations / Non-Blocking Audio Thread`: PASS with design guardrail. Analysis computation, plot generation, and control-state persistence must occur on message/background threads with snapshot handoff into the audio runtime.

**Post-Design Re-Check**: PASS. Planned artifacts keep runtime/UI separation explicit, store new preferences in graph state, and avoid any constitution violations requiring justification.

## Project Structure

### Documentation (this feature)

```text
specs/003-signal-analysis-controls/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── analysis-runtime-contract.md
│   └── graph-control-ui-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
AuralForge/
└── Source/
    ├── PluginEditor.cpp
    ├── PluginEditor.h
    ├── PluginProcessor.cpp
    ├── PluginProcessor.h
    ├── dsp/
    │   ├── LiveGraphEngine.cpp
    │   ├── LiveGraphEngine.h
    │   ├── LiveGraphPublisher.cpp
    │   ├── LiveGraphPublisher.h
    │   ├── LookbackBuffer.cpp
    │   ├── LookbackBuffer.h
    │   ├── TCNModel.cpp
    │   ├── TCNModel.h
    │   ├── TorchScriptBlackBox.cpp
    │   ├── TorchScriptBlackBox.h
    │   ├── WeightRandomizer.cpp
    │   └── WeightRandomizer.h
    ├── freeze/
    │   ├── FreezeCoordinator.cpp
    │   └── FreezeCoordinator.h
    ├── graph/
    │   ├── GraphTypes.h
    │   ├── NodeGraph.cpp
    │   ├── NodeGraph.h
    │   ├── NodeRenderer.cpp
    │   └── NodeRenderer.h
    ├── params/
    │   ├── ParamIDs.h
    │   ├── ParamLayout.cpp
    │   └── ParamLayout.h
    └── ui/
        ├── ImGuiHost.cpp
        ├── ImGuiHost.h
        ├── ImGuiOpenGLBackend.cpp
        ├── InfoPanel.cpp
        ├── InfoPanel.h
        ├── RandomizeButton.cpp
        └── RandomizeButton.h
Tests/
├── LiveGraphEngineTests.cpp
├── ProcessorIntegrationTests.cpp
└── TCNModelTests.cpp
```

**Structure Decision**: Keep the existing single-project plug-in structure. Phase 2.2 work will primarily extend `graph/` for persisted graph/control state, `ui/` and `PluginEditor.*` for rendering and interaction orchestration, `dsp/` for analysis snapshots and gain-aware runtime behavior, and `Tests/` for processor and graph-level regression coverage.

## Phase 0: Research Focus

- Determine the safest architecture for cumulative analysis snapshots without performing allocations or heavyweight transforms on the audio thread.
- Define a control-state persistence model for knob mode, XY bindings, and analysis view preferences that fits the existing `ValueTree` graph serialization.
- Decide how Gold BlackBox analysis should consume compiled behavior while matching Blue-node analysis semantics.
- Confirm practical validation strategy using existing CTest console apps plus targeted graph/processor assertions.

## Phase 1: Design Focus

- Model analysis view state, per-parameter control mode, XY bindings, and gain-enabled properties in the graph document.
- Define runtime contracts between editor, graph document, processor, and DSP analysis pipeline.
- Define end-to-end validation scenarios for live nodes, frozen nodes, state recall, and glitch-free interaction.

## Complexity Tracking

No constitution violations currently require justification.
