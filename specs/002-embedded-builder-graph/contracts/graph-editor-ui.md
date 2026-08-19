# Contract: Graph Editor UI

## Purpose

Defines the user-visible behavior contract for the editable graph builder inside the plugin.

## Element Palette Contract

- The palette lists `Audio Input`, `Audio Output`, `Linear`, `Conv1D`, `Activation Functions`, and `TCN`.
- Dragging an item from the palette creates a node at the drop location.
- The former live `TCN` menu is removed; `TCN` configuration is done on the node itself.

## Node Property Contract

- Each node displays properties as ordered row entries: input first, label second.
- Weighted nodes display:
  - `Randomize Weights` action
  - `Seed` input
- Non-weighted nodes do not display randomization controls.
- `TCN` remains a single visible node while exposing its editable parameters inline.

## Connection Contract

- Users can start a cable from an output port and complete it on a compatible input port.
- Invalid shape matches remain visible only as rejected previews and are not committed.
- Cyclic graph connections are blocked.

## Navigation Contract

- Two-finger trackpad drag pans the canvas.
- Pinch zoom changes graph scale.
- Map view shows the full graph and current viewport.
- Clicking the map view re-centers the main graph viewport.

## Freeze Contract

- Selected nodes expose a context action for `Freeze Selection`.
- Freeze shows progress feedback until success or failure.
- Successful freeze replaces the selection with one Gold locked node.
- Frozen nodes expose `Unfreeze`.
- Frozen nodes display live performance metrics in or near the node.

## Randomization Contract

- Randomization applies only to the selected weighted node.
- Randomization reinitializes all mutable parameters owned by that node.
- If the node is not initialized yet, the same action auto-initializes then randomizes.
- Seed input accepts only signed 32-bit integer values.
- Seed values persist with saved plugin/project state.
