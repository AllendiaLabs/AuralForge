#pragma once

#include "../dsp/TCNModel.h"
#include "GraphTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace auralforge::graph {
/**
 * @class NodeGraph
 * @brief Editable, serializable graph document owned by the message thread.
 *
 * Stable node, pin, and link identifiers keep the Dear ImGui editor and saved
 * plug-in state synchronized without exposing mutable graph data to the audio
 * thread.
 */
class NodeGraph {
public:
  /**
   * @brief Replaces the graph with a linear TCN signal-flow projection.
   * @param configuration Architecture represented by the graph.
   */
  void rebuildFromModel(const dsp::TCNConfiguration &configuration);

  /**
   * @brief Adds one element with its default ports and properties.
   * @param type Element type to create.
   * @param position Initial canvas position.
   * @return Stable identifier of the new node.
   */
  std::int32_t addNode(NodeType type, juce::Point<float> position);

  /**
   * @brief Ensures the graph has exactly one undeletable stereo input and output.
   */
  void ensureFixedStereoIo();

  /**
   * @brief Removes a node and every connection attached to it.
   * @param nodeId Stable node identifier.
   * @return True when a deletable node was removed.
   */
  bool removeNode(std::int32_t nodeId);

  /**
   * @brief Splits a connection and inserts a processing element on that cable.
   * @param linkId Existing connection to replace.
   * @param type Processing element to insert.
   * @param position Canvas position for the new element.
   * @return Identifier of the inserted node, or no value when insertion failed.
   */
  std::optional<std::int32_t> insertNodeOnLink(std::int32_t linkId, NodeType type,
                                               juce::Point<float> position);

  /**
   * @brief Adds a processing element onto an unconnected input or output pin.
   * @param pinId Unconnected pin that receives the new cable.
   * @param type Processing element to create.
   * @param position Canvas position for the new element.
   * @return Identifier of the added node, or no value when attachment failed.
   */
  std::optional<std::int32_t> attachNodeToPin(std::int32_t pinId, NodeType type,
                                              juce::Point<float> position);

  /**
   * @brief Stores a node position reported by the editor.
   * @param nodeId Stable node identifier.
   * @param position New canvas position.
   */
  void moveNode(std::int32_t nodeId, juce::Point<float> position);

  /**
   * @brief Validates and commits a directed connection.
   * @param firstPinId First endpoint selected by the user.
   * @param secondPinId Second endpoint selected by the user.
   * @return Acceptance state and user-facing rejection reason.
   */
  ConnectionResult connect(std::int32_t firstPinId, std::int32_t secondPinId);

  /**
   * @brief Removes a stable graph link.
   * @param linkId Link identifier.
   * @return True when a link was removed.
   */
  bool removeLink(std::int32_t linkId);

  /**
   * @brief Updates one validated inline property.
   * @param nodeId Target node identifier.
   * @param key Canonical property key.
   * @param value Proposed integer value.
   * @return True when the property exists and was updated.
   */
  bool setProperty(std::int32_t nodeId, const std::string &key, int value);

  /**
   * @brief Stores a deterministic signed seed on a weighted live node.
   * @param nodeId Target node identifier.
   * @param seed Seed clamped to `[minimumSeed, maximumSeed]`.
   * @return True when the target accepts randomization.
   */
  bool setSeed(std::int32_t nodeId, std::int32_t seed);

  /**
   * @brief Marks a connected live selection as frozen Gold without regrouping.
   * @param selectedNodeIds Stable identifiers selected for freezing.
   * @param result Successful worker result containing artifact metadata.
   * @return Identifier of one frozen node, or no value for an invalid selection.
   */
  std::optional<std::int32_t>
  freezeSelection(const std::vector<std::int32_t> &selectedNodeIds,
                  const FreezeSelectionResult &result);

  /**
   * @brief Restores frozen Gold elements in the same compiled group to Live Blue.
   * @param nodeId Any frozen node in the group.
   * @return True when restoration succeeded.
   */
  bool unfreeze(std::int32_t nodeId);

  /**
   * @brief Builds a worker request from a connected live selection.
   * @param selectedNodeIds Stable identifiers selected by the user.
   * @return Request DTO, or no value when selection is invalid.
   */
  [[nodiscard]] std::optional<FreezeSelectionRequest>
  createFreezeRequest(const std::vector<std::int32_t> &selectedNodeIds) const;

  /**
   * @brief Splits a freeze selection into independent source-to-sink chains.
   * @param selectedNodeIds Stable identifiers selected by the user.
   * @return One node-id list per chain, or empty when any component is invalid.
   *
   * Each chain has a single source and a single sink. Disconnected selections
   * become multiple chains so each sink can show its own compile and inference
   * times.
   */
  [[nodiscard]] std::vector<std::vector<std::int32_t>> partitionFreezeChains(
      const std::vector<std::int32_t> &selectedNodeIds) const;

  /** @brief Returns true when a pin already has a cable. */
  [[nodiscard]] bool isPinConnected(std::int32_t pinId) const noexcept;

  /** @brief Returns graph nodes in stable insertion order. */
  [[nodiscard]] const std::vector<GraphNode> &getNodes() const noexcept;

  /** @brief Returns mutable graph nodes for message-thread rendering. */
  [[nodiscard]] std::vector<GraphNode> &getNodes() noexcept;

  /** @brief Returns directed links in stable insertion order. */
  [[nodiscard]] const std::vector<GraphLink> &getLinks() const noexcept;

  /** @brief Returns mutable links for message-thread rendering. */
  [[nodiscard]] std::vector<GraphLink> &getLinks() noexcept;

  /** @brief Finds a node by stable identifier. */
  [[nodiscard]] GraphNode *findNode(std::int32_t nodeId) noexcept;

  /** @brief Finds a node by stable identifier. */
  [[nodiscard]] const GraphNode *findNode(std::int32_t nodeId) const noexcept;

  /** @brief Finds a pin by stable identifier. */
  [[nodiscard]] const Pin *findPin(std::int32_t pinId) const noexcept;

  /** @brief Finds a link by stable identifier. */
  [[nodiscard]] const GraphLink *findLink(std::int32_t linkId) const noexcept;

  /** @brief Returns true for the undeletable host audio boundary nodes. */
  [[nodiscard]] bool isFixedIoNode(std::int32_t nodeId) const noexcept;

  /** @brief Returns the node that owns a pin, or no value. */
  [[nodiscard]] std::optional<std::int32_t>
  findNodeForPin(std::int32_t pinId) const noexcept;

  /** @brief Returns mutable viewport persistence state. */
  [[nodiscard]] ViewportState &getViewport() noexcept;

  /** @brief Returns immutable viewport persistence state. */
  [[nodiscard]] const ViewportState &getViewport() const noexcept;

  /** @brief Serializes the complete graph into a JUCE value tree. */
  [[nodiscard]] juce::ValueTree toValueTree() const;

  /**
   * @brief Restores a graph from a JUCE value tree.
   * @param tree Persisted graph state.
   * @return True when the tree was recognized and restored.
   */
  bool restoreFromValueTree(const juce::ValueTree &tree);

  /** @brief Serializes the graph into compact JSON for worker IPC. */
  [[nodiscard]] std::string toJson() const;

private:
  /** @brief Creates a fully initialized node without inserting it. */
  GraphNode makeNode(NodeType type, juce::Point<float> position);
  /**
   * @brief Rebuilds mixer input ports to match the Inputs property.
   * @param node Mixer node to update.
   * @param inputCount Requested input port count.
   */
  void setMixerInputCount(GraphNode &node, int inputCount);
  /** @brief Tests whether a candidate edge would introduce a directed cycle. */
  bool wouldCreateCycle(std::int32_t sourceNodeId,
                        std::int32_t destinationNodeId) const;
  /** @brief Tests whether selected live nodes form one connected component. */
  bool
  selectionIsConnected(const std::vector<std::int32_t> &selectedNodeIds) const;

  /** @brief Stable insertion-ordered node collection. */
  std::vector<GraphNode> nodes;
  /** @brief Stable insertion-ordered link collection. */
  std::vector<GraphLink> links;
  /** @brief Persisted graph navigation state. */
  ViewportState viewport;
  /** @brief Next unused node identifier. */
  std::int32_t nextNodeId = 1;
  /** @brief Next unused pin identifier. */
  std::int32_t nextPinId = 1001;
  /** @brief Next unused link identifier. */
  std::int32_t nextLinkId = 2001;
};
} // namespace auralforge::graph
