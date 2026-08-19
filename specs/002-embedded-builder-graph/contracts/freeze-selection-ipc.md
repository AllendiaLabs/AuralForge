# Contract: Freeze Selection IPC

## Purpose

Defines the local request/response contract between the plugin runtime and the detached Python worker for manual freeze of a selected subgraph.

## Request

```json
{
  "request_id": "string",
  "operation": "freeze_selection",
  "selected_element_ids": ["element-1", "element-2"],
  "graph_fragment": {
    "elements": [],
    "connections": [],
    "io_boundary": {
      "inputs": [],
      "outputs": []
    }
  },
  "compile_options": {
    "mode": "manual_freeze"
  }
}
```

## Request Rules

- `operation` is always `freeze_selection` for this Phase 2 flow.
- `selected_element_ids` must describe a valid selection prepared from the current live graph.
- `graph_fragment` must include enough information for the worker to reconstruct the selected computation graph.
- Requests are generated on the GUI/background side only, never on the audio thread.

## Success Response

```json
{
  "request_id": "string",
  "status": "success",
  "artifact_path": "absolute/local/path/to/blackbox.pt",
  "blackbox_metadata": {
    "display_name": "Frozen Selection",
    "ports": [],
    "shape_signature": {},
    "baseline_metrics": {
      "compile_time_ms": 1420,
      "estimated_latency_ms": 3.6
    }
  }
}
```

## Failure Response

```json
{
  "request_id": "string",
  "status": "failure",
  "error_message": "Human-readable failure summary"
}
```

## Response Rules

- `request_id` must echo the originating request.
- `status = success` requires a loadable `artifact_path`.
- `status = failure` preserves the original selected live nodes with no graph mutation.
- Failure text must be suitable for progress/status UI display.

## Runtime Guarantees

- The plugin only swaps to a Gold BlackBox after successful off-thread load/preparation of the compiled artifact.
- Audio processing continues through the original live graph until the atomic swap point.
- Unfreeze uses the plugin-side stored original subgraph snapshot; it is not delegated to the worker.
