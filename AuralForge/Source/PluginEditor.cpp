#include "PluginEditor.h"
#include "params/ParamIDs.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
bool sameArchitecture(const auralforge::dsp::TCNConfiguration &left,
                      const auralforge::dsp::TCNConfiguration &right) noexcept {
  return left.depth == right.depth && left.kernelSize == right.kernelSize &&
         left.channels == right.channels && left.dilation == right.dilation &&
         left.inputChannels == right.inputChannels &&
         left.outputChannels == right.outputChannels &&
         left.activation == right.activation;
}
} // namespace

AuralForgeAudioProcessorEditor::AuralForgeAudioProcessorEditor(
    AuralForgeAudioProcessor &processorToUse)
    : AudioProcessorEditor(&processorToUse), audioProcessor(processorToUse),
      freezeCoordinator(
          [this](const auralforge::graph::FreezeSelectionResult &result,
                 std::string &error) {
            return audioProcessor.prepareFrozenArtifact(result, error);
          }),
      imguiHost([this] { renderFrame(); }) {
  setOpaque(true);
  setResizable(true, true);
  setResizeLimits(760, 480, 1920, 1200);
  addAndMakeVisible(imguiHost);
  setSize(1100, 680);
}

AuralForgeAudioProcessorEditor::~AuralForgeAudioProcessorEditor() = default;

void AuralForgeAudioProcessorEditor::paint(juce::Graphics &graphics) {
  graphics.fillAll(juce::Colour(20, 23, 30));
}

void AuralForgeAudioProcessorEditor::resized() {
  imguiHost.setBounds(getLocalBounds());
}

void AuralForgeAudioProcessorEditor::renderFrame() {
  updateGraphIfNeeded();
  applyCompletedFreeze();
  for (auto &node : nodeGraph.getNodes()) {
    if (node.state != auralforge::graph::NodeState::frozenGold ||
        !node.metrics.has_value())
      continue;
    const auto measured =
        audioProcessor.getFrozenInferenceTimeMilliseconds(node.id);
    if (measured > 0.0)
      node.metrics->inferenceTimeMilliseconds = measured;
  }

  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  constexpr auto flags = ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::Begin("AuralForge", nullptr, flags);

  ImGui::TextColored(ImVec4(0.39f, 0.70f, 1.0f, 1.0f), "AuralForge");
  ImGui::SameLine();
  const auto sampleRate = audioProcessor.getCurrentSampleRate();
  const auto receptiveField = audioProcessor.getReceptiveFieldSamples();
  const auto receptiveFieldMs =
      sampleRate > 0.0
          ? static_cast<double>(receptiveField) * 1000.0 / sampleRate
          : 0.0;
  ImGui::TextDisabled(
      "Embedded Builder  |  RF %.2f ms / %llu samples  |  %llu params",
      receptiveFieldMs, static_cast<unsigned long long>(receptiveField),
      static_cast<unsigned long long>(audioProcessor.getModelParameterCount()));
  const auto runtimeError = audioProcessor.getModelError();
  if (runtimeError.isNotEmpty()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s",
                       runtimeError.toRawUTF8());
  }
  if (graphMessage.isNotEmpty() && ImGui::GetTime() < graphMessageDeadline) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "%s",
                       graphMessage.toRawUTF8());
  }
  const auto freezeStatus = freezeCoordinator.getStatus();
  if (freezeStatus == auralforge::freeze::FreezeStatus::compiling) {
    ImGui::SameLine();
    const auto status = freezeCoordinator.getStatusMessage();
    const auto progress =
        static_cast<float>(std::fmod(ImGui::GetTime() * 0.35, 1.0));
    ImGui::ProgressBar(progress, ImVec2(110.0f, 0.0f), "");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "%s",
                       status.toRawUTF8());
  }
  ImGui::Separator();

  auralforge::graph::NodeRendererCallbacks callbacks;
  callbacks.propertyChanged = [this](std::int32_t nodeId,
                                     const std::string &key, int value) {
    handlePropertyChanged(nodeId, key, value);
  };
  callbacks.randomizeNode = [this](std::int32_t nodeId, std::int32_t seed) {
    handleRandomize(nodeId, seed);
  };
  callbacks.freezeSelection =
      [this](const std::vector<std::int32_t> &selection) {
        handleFreeze(selection);
      };
  callbacks.unfreezeNode = [this](std::int32_t nodeId) {
    handleUnfreeze(nodeId);
  };
  callbacks.showMessage = [this](const std::string &message) {
    showGraphMessage(message);
  };
  callbacks.documentChanged = [this](bool recompile) { persistGraph(recompile); };
  nodeRenderer.render(nodeGraph, callbacks, imguiHost.takeMagnification());

  ImGui::End();
}

void AuralForgeAudioProcessorEditor::updateGraphIfNeeded() {
  const auto requested = audioProcessor.getRequestedConfiguration();
  if (!graphInitialized) {
    const auto restored = audioProcessor.getGraphState();
    if (restored.getNumChildren() > 0)
      nodeGraph.restoreFromValueTree(restored);
    else
      nodeGraph.rebuildFromModel(requested);
    displayedConfiguration = requested;
    graphInitialized = true;
    persistGraph();
    return;
  }

  if (!sameArchitecture(requested, displayedConfiguration)) {
    displayedConfiguration = requested;
    const auto tcn =
        std::find_if(nodeGraph.getNodes().begin(), nodeGraph.getNodes().end(),
                     [](const auralforge::graph::GraphNode &node) {
                       return node.type == auralforge::graph::NodeType::tcn;
                     });
    if (tcn != nodeGraph.getNodes().end()) {
      nodeGraph.setProperty(tcn->id, "depth", requested.depth);
      nodeGraph.setProperty(tcn->id, "kernel_size", requested.kernelSize);
      nodeGraph.setProperty(tcn->id, "channels", requested.channels);
      nodeGraph.setProperty(tcn->id, "dilation", requested.dilation);
      nodeGraph.setProperty(tcn->id, "activation",
                            static_cast<int>(requested.activation));
      persistGraph();
    }
  }
}

void AuralForgeAudioProcessorEditor::persistGraph(bool compileRuntime) {
  audioProcessor.setGraphState(nodeGraph.toValueTree(), compileRuntime);
}

void AuralForgeAudioProcessorEditor::handlePropertyChanged(
    std::int32_t nodeId, const std::string &key, int value) {
  juce::ignoreUnused(key, value);
  const auto *node = nodeGraph.findNode(nodeId);
  if (node == nullptr || node->type != auralforge::graph::NodeType::tcn) {
    persistGraph();
    return;
  }

  auto configuration = audioProcessor.getRequestedConfiguration();
  for (const auto &property : node->properties) {
    if (property.key == "depth")
      configuration.depth = property.value;
    else if (property.key == "kernel_size")
      configuration.kernelSize = property.value;
    else if (property.key == "channels")
      configuration.channels = property.value;
    else if (property.key == "dilation")
      configuration.dilation = property.value;
    else if (property.key == "activation")
      configuration.activation =
          static_cast<auralforge::dsp::ActivationType>(property.value);
  }
  displayedConfiguration = configuration;
  audioProcessor.applyGraphConfiguration(configuration);
  persistGraph();
}

void AuralForgeAudioProcessorEditor::handleRandomize(std::int32_t nodeId,
                                                     std::int32_t seed) {
  audioProcessor.randomizeGraphElement(nodeId, seed);
  persistGraph();
}

void AuralForgeAudioProcessorEditor::handleFreeze(
    const std::vector<std::int32_t> &selectedNodeIds) {
  auto chains = nodeGraph.partitionFreezeChains(selectedNodeIds);
  if (chains.empty()) {
    showGraphMessage(
        "Freeze requires live Blue chains with a single input and output");
    return;
  }

  pendingFreezeChains = std::move(chains);
  if (!startNextFreezeChain()) {
    pendingFreezeChains.clear();
    pendingFreezeSelection.clear();
    pendingFreezeGraph.clear();
  }
}

bool AuralForgeAudioProcessorEditor::startNextFreezeChain() {
  if (pendingFreezeChains.empty())
    return false;
  auto request = nodeGraph.createFreezeRequest(pendingFreezeChains.front());
  if (!request.has_value()) {
    showGraphMessage(
        "Freeze requires a valid connected selection of live Blue nodes");
    return false;
  }

  auto payload = juce::JSON::parse(juce::String(request->graphFragment));
  auto *root = payload.getDynamicObject();
  auto *options = root != nullptr
                      ? root->getProperty("compile_options").getDynamicObject()
                      : nullptr;
  if (options == nullptr) {
    showGraphMessage("Freeze request could not be configured for host audio");
    return false;
  }
  if (static_cast<int>(options->getProperty("host_input_channels")) < 1)
    options->setProperty("host_input_channels",
                         audioProcessor.getTotalNumInputChannels());
  if (static_cast<int>(options->getProperty("host_output_channels")) < 1)
    options->setProperty("host_output_channels",
                         audioProcessor.getTotalNumOutputChannels());
  options->setProperty("example_samples", 256);
  request->graphFragment = juce::JSON::toString(payload, true).toStdString();

  if (!freezeCoordinator.start(*request)) {
    showGraphMessage("A freeze compilation is already running");
    return false;
  }
  pendingFreezeSelection = pendingFreezeChains.front();
  pendingFreezeGraph = nodeGraph.toJson();
  return true;
}

void AuralForgeAudioProcessorEditor::handleUnfreeze(std::int32_t nodeId) {
  const auto *blackBox = nodeGraph.findNode(nodeId);
  const auto artifactPath =
      blackBox != nullptr ? blackBox->artifactPath : std::string{};
  if (nodeGraph.unfreeze(nodeId)) {
    audioProcessor.releaseFrozenArtifact(artifactPath);
    showGraphMessage("Selection restored to Live Blue");
    persistGraph();
  }
}

void AuralForgeAudioProcessorEditor::applyCompletedFreeze() {
  auto result = freezeCoordinator.takeResult();
  if (!result.has_value())
    return;

  if (!result->succeeded) {
    showGraphMessage("Freeze failed: " + result->errorMessage);
    pendingFreezeSelection.clear();
    pendingFreezeChains.clear();
    pendingFreezeGraph.clear();
    return;
  }

  if (pendingFreezeGraph != nodeGraph.toJson()) {
    audioProcessor.releaseFrozenArtifact(result->artifactPath);
    pendingFreezeSelection.clear();
    pendingFreezeChains.clear();
    pendingFreezeGraph.clear();
    showGraphMessage("Freeze result was discarded because the graph changed "
                     "while compiling");
    return;
  }

  const auto frozen =
      nodeGraph.freezeSelection(pendingFreezeSelection, *result);
  if (!pendingFreezeChains.empty())
    pendingFreezeChains.erase(pendingFreezeChains.begin());
  pendingFreezeSelection.clear();
  pendingFreezeGraph.clear();
  if (!frozen.has_value()) {
    audioProcessor.releaseFrozenArtifact(result->artifactPath);
    pendingFreezeChains.clear();
    showGraphMessage(
        "Freeze result was discarded because the graph selection changed");
    return;
  }

  persistGraph();
  if (!pendingFreezeChains.empty() && startNextFreezeChain()) {
    showGraphMessage("Freeze succeeded: compiling the next frozen chain");
    return;
  }
  pendingFreezeChains.clear();
  showGraphMessage("Freeze succeeded: selection is frozen Gold");
}

void AuralForgeAudioProcessorEditor::showGraphMessage(
    const std::string &message) {
  graphMessage = message;
  graphMessageDeadline = ImGui::GetTime() + 3.0;
}
