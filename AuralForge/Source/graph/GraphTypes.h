#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <string>
#include <vector>

namespace auralforge::graph {
/** @brief Semantic role of a node in the live processing graph. */
enum class NodeType { audioInput, convolution, activation, audioOutput };

/** @brief Direction of a graph pin. */
enum class PinKind { input, output };

/** @brief A stable endpoint belonging to one graph node. */
struct Pin {
  std::int32_t id = 0;
  std::string label;
  PinKind kind = PinKind::input;
};

/** @brief Read-only visual representation of one processing operation. */
struct GraphNode {
  std::int32_t id = 0;
  std::string label;
  std::string detail;
  NodeType type = NodeType::convolution;
  juce::Colour colour{100, 180, 255};
  juce::Point<float> position;
  std::vector<Pin> inputs;
  std::vector<Pin> outputs;
};

/** @brief Directed connection between two graph pins. */
struct GraphLink {
  std::int32_t id = 0;
  std::int32_t sourcePinId = 0;
  std::int32_t destinationPinId = 0;
};
} // namespace auralforge::graph
