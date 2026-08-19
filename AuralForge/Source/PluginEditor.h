#pragma once

#include "PluginProcessor.h"
#include "graph/NodeGraph.h"
#include "graph/NodeRenderer.h"
#include "ui/ImGuiHost.h"
#include "ui/InfoPanel.h"
#include "ui/RandomizeButton.h"
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
  void renderParameters();
  void updateGraphIfNeeded();

  AuralForgeAudioProcessor &audioProcessor;
  auralforge::graph::NodeGraph nodeGraph;
  auralforge::ui::InfoPanel infoPanel;
  auralforge::ui::RandomizeButton randomizeButton;
  auralforge::ui::ImGuiHost imguiHost;
  auralforge::graph::NodeRenderer nodeRenderer;
  auralforge::dsp::TCNConfiguration displayedConfiguration;
  bool graphInitialized = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuralForgeAudioProcessorEditor)
};
