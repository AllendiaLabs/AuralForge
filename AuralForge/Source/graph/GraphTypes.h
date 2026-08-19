#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace auralforge::graph {
/** @brief Semantic role of a node in the live processing graph. */
enum class NodeType {
  audioInput,
  audioOutput,
  linear,
  convolution,
  activation,
  tcn,
  merge,
  blackBox
};

/** @brief Merge element operating mode. */
enum class MergeMode : int { add = 0, multiply = 1, concatenate = 2 };

/** @brief Live modular processing node colour. */
inline const juce::Colour liveBlueColour{100, 180, 255};
/** @brief Frozen Gold node colour. */
inline const juce::Colour frozenGoldColour{218, 165, 32};
/** @brief Fixed stereo host-input node colour. */
inline const juce::Colour audioInputColour{70, 200, 150};
/** @brief Fixed stereo host-output node colour. */
inline const juce::Colour audioOutputColour{240, 160, 80};

/** @brief Returns true for the undeletable host audio boundary nodes. */
inline bool isFixedIoType(NodeType type) noexcept {
  return type == NodeType::audioInput || type == NodeType::audioOutput;
}

/** @brief Returns true for nodes that combine several input ports. */
inline bool isMixerType(NodeType type) noexcept {
  return type == NodeType::merge;
}

/** @brief Runtime mode represented by a graph node. */
enum class NodeState { liveBlue, frozenGold };

/** @brief Direction of a graph pin. */
enum class PinKind { input, output };

/** @brief Coarse audio tensor shape used for interactive link validation. */
struct ShapeSignature {
  /** @brief Channel count, or zero when inferred from the adjacent node. */
  int channels = 0;
  /** @brief Human-readable temporal domain name. */
  std::string domain{"audio"};

  /** @brief Returns whether this shape can connect to another endpoint. */
  [[nodiscard]] bool
  isCompatibleWith(const ShapeSignature &other) const noexcept {
    return domain == other.domain &&
           (channels == 0 || other.channels == 0 || channels == other.channels);
  }
};

/** @brief A stable endpoint belonging to one graph node. */
struct Pin {
  /** @brief Stable graph-wide endpoint identifier. */
  std::int32_t id = 0;
  /** @brief User-visible endpoint label. */
  std::string label;
  /** @brief Input or output direction. */
  PinKind kind = PinKind::input;
  /** @brief Audio shape accepted or produced by the endpoint. */
  ShapeSignature shape;
};

/** @brief Value type accepted by an inline graph property. */
enum class PropertyKind { integer, choice, readOnly };

/** @brief Ordered, validated inline property belonging to a graph node. */
struct NodeProperty {
  /** @brief Stable property key used for persistence and runtime binding. */
  std::string key;
  /** @brief User-visible property label. */
  std::string label;
  /** @brief Current validated integer or choice index. */
  int value = 0;
  /** @brief Inclusive minimum accepted value. */
  int minimum = std::numeric_limits<int>::min();
  /** @brief Inclusive maximum accepted value. */
  int maximum = std::numeric_limits<int>::max();
  /** @brief Editor control and parsing behavior. */
  PropertyKind kind = PropertyKind::integer;
  /** @brief Ordered labels used when the property is a choice. */
  std::vector<std::string> choices;

  /** @brief Clamps and stores a proposed integer value. */
  void setValue(int proposed) noexcept {
    value = std::clamp(proposed, minimum, maximum);
  }
};

/** @brief Live performance values displayed by a frozen BlackBox node. */
struct NodeMetrics {
  /** @brief Backend compilation duration. */
  double compileTimeMilliseconds = 0.0;
  /** @brief Most recent per-buffer inference duration. */
  double inferenceTimeMilliseconds = 0.0;
};

/** @brief Editable visual and processing description of one operation. */
struct GraphNode {
  /** @brief Stable graph-wide node identifier. */
  std::int32_t id = 0;
  /** @brief User-visible node title. */
  std::string label;
  /** @brief Secondary node status or architecture summary. */
  std::string detail;
  /** @brief Processing operation represented by the node. */
  NodeType type = NodeType::tcn;
  /** @brief Live Blue or frozen Gold runtime state. */
  NodeState state = NodeState::liveBlue;
  /** @brief Primary node accent colour. */
  juce::Colour colour{100, 180, 255};
  /** @brief Persisted canvas position. */
  juce::Point<float> position;
  /** @brief Last measured rendered node size. */
  juce::Point<float> size{180.0f, 120.0f};
  /** @brief Ordered input endpoints. */
  std::vector<Pin> inputs;
  /** @brief Ordered output endpoints. */
  std::vector<Pin> outputs;
  /** @brief Ordered inline property rows. */
  std::vector<NodeProperty> properties;
  /** @brief Whether this element owns mutable trainable parameters. */
  bool hasWeights = false;
  /** @brief Last applied randomization seed, including one-shot random draws. */
  std::int32_t seed = 42;
  /** @brief Memorized typed seed restored when the seed checkbox is re-enabled. */
  std::int32_t explicitSeed = 42;
  /** @brief True when Randomize Weights uses `explicitSeed` instead of a new random value. */
  bool useExplicitSeed = false;
  /** @brief Optional frozen-runtime performance values. */
  std::optional<NodeMetrics> metrics;
  /** @brief Compiled TorchScript artifact used by a BlackBox. */
  std::string artifactPath;
  /** @brief Serialized live source fragment used for unfreeze. */
  std::string sourceSubgraph;
};

/** @brief Directed connection between two graph pins. */
struct GraphLink {
  /** @brief Stable graph-wide link identifier. */
  std::int32_t id = 0;
  /** @brief Stable output pin identifier. */
  std::int32_t sourcePinId = 0;
  /** @brief Stable input pin identifier. */
  std::int32_t destinationPinId = 0;
};

/** @brief Persisted graph canvas navigation state. */
struct ViewportState {
  /** @brief Persisted canvas translation. */
  juce::Point<float> pan;
  /** @brief Persisted editor zoom clamped to supported bounds. */
  float zoom = 1.0f;
  /** @brief Whether the clickable overview map is visible. */
  bool mapVisible = true;
};

/** @brief Outcome returned when an interactive connection is validated. */
struct ConnectionResult {
  /** @brief True when the link was committed. */
  bool accepted = false;
  /** @brief User-facing reason when the link was rejected. */
  std::string message;
};

/** @brief Serializable manual-freeze operation sent to the backend worker. */
struct FreezeSelectionRequest {
  /** @brief Unique request identifier used for response correlation. */
  std::string requestId;
  /** @brief Stable selected node identifiers. */
  std::vector<std::int32_t> selectedNodeIds;
  /** @brief JSON graph payload consumed by the backend worker. */
  std::string graphFragment;
};

/** @brief Serializable response produced by a manual-freeze worker. */
struct FreezeSelectionResult {
  /** @brief Request identifier echoed by the worker. */
  std::string requestId;
  /** @brief Whether compilation and artifact validation succeeded. */
  bool succeeded = false;
  /** @brief Absolute local path to the compiled TorchScript artifact. */
  std::string artifactPath;
  /** @brief User-facing failure reason. */
  std::string errorMessage;
  /** @brief Baseline compile and inference measurements. */
  NodeMetrics baselineMetrics;
  /** @brief Exact input channel count accepted by the artifact. */
  int inputChannels = 0;
  /** @brief Exact output channel count produced by the artifact. */
  int outputChannels = 0;
  /** @brief Causal receptive field required for continuous block processing. */
  std::uint64_t receptiveFieldSamples = 1;
};

/** @brief Inclusive lower bound for element randomization seeds. */
inline constexpr std::int32_t minimumSeed = 0;
/** @brief Inclusive upper bound for element randomization seeds. */
inline constexpr std::int32_t maximumSeed = 999999;

/**
 * @brief Clamps a seed into the supported UI range.
 * @param seed Proposed seed value.
 * @return Seed in `[minimumSeed, maximumSeed]`.
 */
inline std::int32_t clampSeed(std::int32_t seed) noexcept {
  return std::clamp(seed, minimumSeed, maximumSeed);
}

/** @brief Minimum supported node-editor zoom level. */
inline constexpr float minimumZoom = 0.25f;
/** @brief Maximum supported node-editor zoom level. */
inline constexpr float maximumZoom = 2.0f;
/** @brief Default minimap width in Dear ImGui pixels. */
inline constexpr float mapWidth = 190.0f;
/** @brief Default minimap height in Dear ImGui pixels. */
inline constexpr float mapHeight = 125.0f;
} // namespace auralforge::graph
