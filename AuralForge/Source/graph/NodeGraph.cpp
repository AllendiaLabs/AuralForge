#include "NodeGraph.h"

#include <algorithm>

namespace {
const juce::Colour liveBlue{100, 180, 255};

std::string activationName(auralforge::dsp::ActivationType activation) {
  using auralforge::dsp::ActivationType;
  switch (activation) {
  case ActivationType::relu:
    return "ReLU";
  case ActivationType::sigmoid:
    return "Sigmoid";
  case ActivationType::tanh:
    return "Tanh";
  case ActivationType::leakyRelu:
    return "LeakyReLU";
  }
  return "Activation";
}
} // namespace

namespace auralforge::graph {
void NodeGraph::rebuildFromModel(const dsp::TCNConfiguration &configuration) {
  nodes.clear();
  links.clear();

  std::int32_t nextNodeId = 1;
  std::int32_t nextPinId = 1001;
  std::int32_t nextLinkId = 2001;
  std::int32_t previousOutput = 0;
  float x = 24.0f;

  const auto appendNode = [&](std::string label, std::string detail,
                              NodeType type) {
    GraphNode node;
    node.id = nextNodeId++;
    node.label = std::move(label);
    node.detail = std::move(detail);
    node.type = type;
    node.colour = liveBlue;
    node.position = {x, 130.0f};
    x += 190.0f;

    if (type != NodeType::audioInput)
      node.inputs.push_back({nextPinId++, "in", PinKind::input});
    if (type != NodeType::audioOutput)
      node.outputs.push_back({nextPinId++, "out", PinKind::output});

    if (previousOutput != 0 && !node.inputs.empty())
      links.push_back({nextLinkId++, previousOutput, node.inputs.front().id});
    if (!node.outputs.empty())
      previousOutput = node.outputs.front().id;
    nodes.push_back(std::move(node));
  };

  appendNode("Audio In",
             std::to_string(configuration.inputChannels) + " channel(s)",
             NodeType::audioInput);
  appendNode("Input Projection",
             std::to_string(configuration.inputChannels) + " → " +
                 std::to_string(configuration.channels),
             NodeType::convolution);

  const auto visualDepth = std::min(configuration.depth, 30);
  for (int layer = 0; layer < visualDepth; ++layer) {
    appendNode("Conv1d " + std::to_string(layer + 1),
               "k=" + std::to_string(configuration.kernelSize) +
                   "  c=" + std::to_string(configuration.channels) +
                   "  d=" + std::to_string(std::uint64_t{1} << layer),
               NodeType::convolution);
    appendNode(activationName(configuration.activation), "Activation",
               NodeType::activation);
  }

  appendNode("Audio Out",
             std::to_string(configuration.outputChannels) + " channel(s)",
             NodeType::audioOutput);
}

const std::vector<GraphNode> &NodeGraph::getNodes() const noexcept {
  return nodes;
}

const std::vector<GraphLink> &NodeGraph::getLinks() const noexcept {
  return links;
}
} // namespace auralforge::graph
