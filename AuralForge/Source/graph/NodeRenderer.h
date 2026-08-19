#pragma once

#include "NodeGraph.h"

#include <imgui_node_editor.h>

namespace auralforge::graph {
/**
 * @class NodeRenderer
 * @brief Dear ImGui node-editor renderer for a read-only live graph.
 */
class NodeRenderer {
public:
  /** @brief Creates an in-memory node-editor context. */
  NodeRenderer();

  /** @brief Releases the node-editor context. */
  ~NodeRenderer();

  /**
   * @brief Draws all nodes and links into the current ImGui window.
   * @param graph Graph data to render.
   */
  void render(const NodeGraph &graph);

private:
  ax::NodeEditor::EditorContext *context = nullptr;
};
} // namespace auralforge::graph
