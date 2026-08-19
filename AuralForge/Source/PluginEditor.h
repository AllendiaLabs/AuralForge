#pragma once

#include "PluginProcessor.h"
#include "freeze/FreezeCoordinator.h"
#include "graph/NodeGraph.h"
#include "graph/NodeRenderer.h"
#include "ui/ImGuiHost.h"
#include <JuceHeader.h>

/**
 * @class AuralForgeAudioProcessorEditor
 * @brief Resizable Dear ImGui editor for TCN controls and graph visualization.
 */
class AuralForgeAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
  /** @brief Creates and attaches the OpenGL-backed editor. */
  AuralForgeAudioProcessorEditor(AuralForgeAudioProcessor &);
  /** @brief Releases editor resources before the processor. */
  ~AuralForgeAudioProcessorEditor() override;

  /** @brief Paints the fallback JUCE background behind OpenGL. */
  void paint(juce::Graphics &) override;
  /** @brief Fits the ImGui host to the complete editor area. */
  void resized() override;

private:
  void renderFrame();
  /** @brief Restores or synchronizes the editable graph document. */
  void updateGraphIfNeeded();
  /** @brief Publishes the current graph document to processor state.
   *  @param compileRuntime Whether the live audio graph should be rebuilt.
   */
  void persistGraph(bool compileRuntime = true);
  /**
   * @brief Applies an inline node property to the live processor.
   * @param nodeId Stable graph node identifier.
   * @param key Canonical property key.
   * @param value Validated property value.
   */
  void handlePropertyChanged(std::int32_t nodeId, const std::string &key,
                             int value);
  /**
   * @brief Requests element-scoped deterministic randomization.
   * @param nodeId Stable graph node identifier.
   * @param seed Signed per-element seed.
   */
  void handleRandomize(std::int32_t nodeId, std::int32_t seed);
  /**
   * @brief Starts a manual freeze for one or more source-to-sink chains.
   * @param selectedNodeIds Stable selected node identifiers.
   */
  void handleFreeze(const std::vector<std::int32_t> &selectedNodeIds);
  /**
   * @brief Starts compilation of the next queued freeze chain.
   * @return False when the next chain could not be submitted.
   */
  bool startNextFreezeChain();
  /**
   * @brief Restores a frozen Gold group to editable Live Blue elements.
   * @param nodeId Any frozen node in the compiled group.
   */
  void handleUnfreeze(std::int32_t nodeId);
  /** @brief Applies a completed background freeze result on the message thread.
   */
  void applyCompletedFreeze();
  /**
   * @brief Displays transient graph feedback.
   * @param message Human-readable status or validation text.
   */
  void showGraphMessage(const std::string &message);

  AuralForgeAudioProcessor &audioProcessor;
  /** @brief Owns detached worker execution and thread-safe completion state. */
  auralforge::freeze::FreezeCoordinator freezeCoordinator;
  auralforge::graph::NodeGraph nodeGraph;
  auralforge::ui::ImGuiHost imguiHost;
  auralforge::graph::NodeRenderer nodeRenderer;
  auralforge::dsp::TCNConfiguration displayedConfiguration;
  bool graphInitialized = false;
  /** @brief Current transient graph workflow message. */
  juce::String graphMessage;
  /** @brief Dear ImGui time when graph workflow feedback expires. */
  double graphMessageDeadline = 0.0;
  /** @brief Selection retained unchanged until worker and artifact success. */
  std::vector<std::int32_t> pendingFreezeSelection;
  /** @brief Remaining freeze chains waiting after the active request. */
  std::vector<std::vector<std::int32_t>> pendingFreezeChains;
  /** @brief Exact graph snapshot used to reject stale worker completions. */
  std::string pendingFreezeGraph;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuralForgeAudioProcessorEditor)
};
