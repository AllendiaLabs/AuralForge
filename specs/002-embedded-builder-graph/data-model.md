# Data Model: Embedded Builder & Interactive Graph

## Element

- **Purpose**: Represents one graph node in the editable builder.
- **Fields**:
  - `element_id`: Stable unique identifier within the graph
  - `element_type`: `audio_input`, `audio_output`, `linear`, `conv1d`, `activation`, `tcn`, `blackbox`
  - `display_name`: User-visible label
  - `position`: Canvas coordinate used by graph and map view
  - `size`: Rendered bounds for hit-testing and map projection
  - `state`: `live_blue` or `frozen_gold`
  - `properties`: Ordered list of editable property rows shown inline
  - `ports`: Input/output port definitions with shape metadata
  - `has_weights`: Boolean controlling randomization/seed UI
  - `seed`: Optional signed 32-bit integer persisted with project state
  - `metrics`: Optional live performance metrics shown for frozen nodes
- **Validation rules**:
  - `element_id` must be unique per graph
  - `seed`, when present, must be within signed 32-bit integer range
  - `state = frozen_gold` implies randomization controls are hidden or disabled
  - `tcn` remains one visible element even if it owns internal layers

## ElementProperty

- **Purpose**: One inline editable row on a node.
- **Fields**:
  - `property_key`: Canonical property identifier
  - `label`: User-visible property name
  - `value`: Current displayed value
  - `editable`: Whether the row accepts direct input
  - `value_kind`: Numeric, enum-like text, or derived display value
  - `validation_rule`: Range or format constraint used for input feedback
- **Validation rules**:
  - Properties are rendered in stable order
  - Weighted element randomization/seed controls appear only when `has_weights = true`

## Port

- **Purpose**: Defines one connection endpoint for an element.
- **Fields**:
  - `port_id`: Unique within an element
  - `direction`: `input` or `output`
  - `shape_signature`: Channel/temporal compatibility descriptor
  - `label`: Optional user-visible text
- **Validation rules**:
  - Connections must match directionality
  - Shape mismatch prevents final connection creation and shows error feedback

## Connection

- **Purpose**: Represents a cable between two compatible ports.
- **Fields**:
  - `connection_id`: Stable unique identifier
  - `source_element_id`
  - `source_port_id`
  - `target_element_id`
  - `target_port_id`
  - `status`: `valid`, `invalid_preview`, or `blocked`
- **Validation rules**:
  - No cycles allowed
  - Connection can be persisted only when shape compatibility passes

## GraphDocument

- **Purpose**: Persisted editable graph state for the plugin/project.
- **Fields**:
  - `elements`: Collection of `Element`
  - `connections`: Collection of `Connection`
  - `viewport`: Current pan/zoom state
  - `map_view_state`: Visibility and last viewport mapping
  - `selected_element_ids`: Current selection set
- **Validation rules**:
  - References between elements and connections must resolve
  - Persisted seeds and property values restore with the graph

## FreezeSelectionRequest

- **Purpose**: Serializable contract sent to the Python worker for manual freeze.
- **Fields**:
  - `request_id`: Unique operation identifier
  - `selected_element_ids`: Ordered selection
  - `selected_connections`: Relevant internal/external wiring
  - `graph_fragment`: Serialized subgraph payload
  - `compile_mode`: Manual freeze
- **Validation rules**:
  - Selection must form a valid connected subgraph
  - Invalid shape states must not be submitted

## FreezeSelectionResult

- **Purpose**: Worker response used to complete a freeze swap.
- **Fields**:
  - `request_id`: Correlates to request
  - `status`: `success` or `failure`
  - `artifact_path`: Local path to compiled `.pt` on success
  - `blackbox_metadata`: Ports, shape, display label, metrics baseline
  - `error_message`: User-facing failure summary when unsuccessful
- **Validation rules**:
  - Success requires a loadable artifact path
  - Failure must preserve the original live graph

## BlackBoxElement

- **Purpose**: Frozen replacement node for a compiled subgraph.
- **Fields**:
  - Inherits `Element`
  - `source_subgraph_snapshot`: Serialized original graph fragment for unfreeze
  - `artifact_path`: Loaded compiled model reference
  - `lock_icon_visible`: Always true
- **State transitions**:
  - `live_blue` selection -> `frozen_gold` blackbox on successful freeze
  - `frozen_gold` blackbox -> restored `live_blue` subgraph on unfreeze

## RandomizationAction

- **Purpose**: Runtime action applied to one weighted element.
- **Fields**:
  - `target_element_id`
  - `seed`
  - `parameter_scope`: All mutable parameters of the targeted element
  - `requires_auto_init`: Boolean derived from current element state
- **Validation rules**:
  - Must never target non-weighted elements
  - Must never affect other graph elements
  - Must be prepared off the audio thread and applied atomically

## MapViewProjection

- **Purpose**: Relates full graph bounds to the miniature overview.
- **Fields**:
  - `graph_bounds`
  - `viewport_rect`
  - `scale_factor`
- **Validation rules**:
  - Empty graphs still produce a safe default map state
  - Click navigation must resolve to a valid graph viewport target
