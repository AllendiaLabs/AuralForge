#pragma once

#include "PluginProcessor.h"
#include "freeze/FreezeCoordinator.h"
#include "graph/NodeGraph.h"
#include "graph/NodeRenderer.h"
#include "ui/ImGuiHost.h"
#include "ui/InfoPanel.h"
#include <JuceHeader.h>

/**
 * @class OpenYourBoxAudioProcessorEditor
 * @brief Resizable Dear ImGui editor for TCN controls and graph visualization.
 */
class OpenYourBoxAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
  /** @brief Creates and attaches the OpenGL-backed editor. */
  OpenYourBoxAudioProcessorEditor(OpenYourBoxAudioProcessor &);
  /** @brief Releases editor resources before the processor. */
  ~OpenYourBoxAudioProcessorEditor() override;

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
   * @brief Publishes Knob/XY/Gain values without rebuilding the audio graph.
   */
  void publishRuntimeControls();
  /**
   * @brief Bumps analysis revision and refreshes the selected snapshot.
   */
  void invalidateAnalysis();
  /**
   * @brief Marks the current analysis snapshot as current for the graph revision.
   */
  void syncAnalysisRevision();
  /**
   * @brief Requests or refreshes analysis for the current target node.
   */
  void refreshAnalysisIfNeeded();
  /**
   * @brief Applies an inline node property to the live processor.
   * @param nodeId Stable graph node identifier.
   * @param key Canonical property key.
   * @param value Validated property value.
   */
  void handlePropertyChanged(std::int32_t nodeId, const std::string &key,
                             int value);
  /**
   * @brief Applies a real inline property such as Gain.
   * @param nodeId Stable graph node identifier.
   * @param key Canonical property key.
   * @param value Validated real value.
   */
  void handleFloatPropertyChanged(std::int32_t nodeId, const std::string &key,
                                  float value);
  /**
   * @brief Opens analysis on a Blue or Gold node.
   * @param nodeId Stable graph node identifier.
   */
  void handleAnalysisRequested(std::int32_t nodeId);
  /**
   * @brief Publishes Knob Input conditioning without a graph recompile.
   * @param nodeId Knob Input node identifier.
   * @param value Current conditioning scalar.
   */
  void handleKnobChanged(std::int32_t nodeId, float value);
  /**
   * @brief Publishes XY Trackpad conditioning without a graph recompile.
   * @param nodeId XY Trackpad node identifier.
   * @param x Current X conditioning scalar.
   * @param y Current Y conditioning scalar.
   */
  void handleXyChanged(std::int32_t nodeId, float x, float y);
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

  OpenYourBoxAudioProcessor &audioProcessor;
  /** @brief Owns detached worker execution and thread-safe completion state. */
  openyourbox::freeze::FreezeCoordinator freezeCoordinator;
  openyourbox::graph::NodeGraph nodeGraph;
  openyourbox::ui::ImGuiHost imguiHost;
  openyourbox::graph::NodeRenderer nodeRenderer;
  openyourbox::dsp::TCNConfiguration displayedConfiguration;
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
  /** @brief Side panel showing architecture metrics and analysis plots. */
  openyourbox::ui::InfoPanel infoPanel;
  /** @brief Node currently shown in the analysis panel, or zero. */
  std::int32_t analysisNodeId = 0;
  /** @brief Latest computed analysis snapshot. */
  openyourbox::dsp::AnalysisSnapshot analysisSnapshot;
  /** @brief Graph revision consumed by the latest snapshot. */
  std::uint64_t analysisRevision = 0;
  /** @brief Dear ImGui time of the last analysis computation. */
  double lastAnalysisTime = 0.0;
  /** @brief True while the Dry/Wet slider is recording a host gesture. */
  bool dryWetGestureActive = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenYourBoxAudioProcessorEditor)
};
