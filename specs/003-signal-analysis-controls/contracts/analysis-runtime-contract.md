# Contract: Analysis Runtime

## Purpose

Define the expected behavior between the graph editor, processor/runtime, and analysis pipeline for Phase 2.2.

## Participants

- Editor orchestration layer
- Graph document
- Live runtime / frozen runtime
- Analysis snapshot producer
- Analysis panel consumer

## Inputs

- Selected node identifier
- Current graph/runtime revision
- Node runtime state (`liveBlue` or `frozenGold`)
- Preferred analysis view (`transfer`, `frequency`, `phase`)
- Availability of suitable live input

## Required Behavior

1. Selecting an analyzable node MUST allow the editor to request cumulative analysis for that node.
2. The analysis pipeline MUST use suitable live audio when available.
3. If suitable live audio is not available, the analysis pipeline MUST fall back to a standard internal probe signal.
4. Returned analysis data MUST represent cumulative behavior from graph input through the selected node boundary.
5. Frozen Gold BlackBox nodes MUST return the same analysis view types as Blue live nodes.
6. Analysis generation MUST NOT block the audio thread or require audio-thread allocation.
7. Analysis results MUST be tagged with the graph/runtime revision they were generated from.
8. When topology, properties, weights, or runtime mode change, stale analysis results MUST be invalidated or refreshed before being presented as current.
9. Left and right channel traces MUST be provided separately so the UI can render them on shared axes.

## Output Shape

Each analysis response should expose at least:

- `nodeId`
- `runtimeState`
- `sourceMode` (`live` or `probe`)
- `view`
- `leftChannelSeries`
- `rightChannelSeries`
- `generatedAtRevision`
- `isStale`

## Failure / Degraded Modes

- If the selected node cannot be analyzed, the editor MUST receive an explicit unavailable status rather than empty silent success.
- If live audio is unsuitable, the pipeline MUST degrade to probe mode instead of failing analysis entirely.
- If a newer graph/runtime revision supersedes the result, the UI MUST treat the older result as stale.
