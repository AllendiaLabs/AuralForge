#pragma once

#include "../dsp/TCNModel.h"
#include "GraphTypes.h"

#include <vector>

namespace auralforge::graph {
/**
 * @class NodeGraph
 * @brief Read-only graph projection of the currently requested TCN
 * architecture.
 *
 * Its separate node/link collections mirror ML Forge's live graph state while
 * keeping rendering and DSP ownership independent.
 */
class NodeGraph {
public:
  /**
   * @brief Replaces the graph with a linear TCN signal-flow projection.
   * @param configuration Architecture represented by the graph.
   */
  void rebuildFromModel(const dsp::TCNConfiguration &configuration);

  /** @brief Returns graph nodes in signal-flow order. */
  [[nodiscard]] const std::vector<GraphNode> &getNodes() const noexcept;

  /** @brief Returns directed links in signal-flow order. */
  [[nodiscard]] const std::vector<GraphLink> &getLinks() const noexcept;

private:
  std::vector<GraphNode> nodes;
  std::vector<GraphLink> links;
};
} // namespace auralforge::graph
