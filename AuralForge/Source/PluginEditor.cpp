#include "PluginEditor.h"
#include "params/ParamIDs.h"

#include <imgui.h>

namespace {
void setParameterValue(juce::AudioProcessorValueTreeState &state,
                       const char *identifier, float plainValue) {
  if (auto *parameter = state.getParameter(identifier)) {
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
    parameter->endChangeGesture();
  }
}

bool sameArchitecture(const auralforge::dsp::TCNConfiguration &left,
                      const auralforge::dsp::TCNConfiguration &right) noexcept {
  return left.depth == right.depth && left.kernelSize == right.kernelSize &&
         left.channels == right.channels &&
         left.inputChannels == right.inputChannels &&
         left.outputChannels == right.outputChannels &&
         left.activation == right.activation;
}
} // namespace

AuralForgeAudioProcessorEditor::AuralForgeAudioProcessorEditor(
    AuralForgeAudioProcessor &processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor),
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

  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  constexpr auto flags = ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::Begin("AuralForge", nullptr, flags);

  ImGui::BeginChild("Controls", ImVec2(250.0f, 0.0f), true);
  ImGui::TextColored(ImVec4(0.39f, 0.70f, 1.0f, 1.0f), "AuralForge");
  ImGui::TextDisabled("Live TCN");
  ImGui::Separator();
  renderParameters();
  ImGui::Spacing();
  randomizeButton.render(audioProcessor.getParameterState());
  ImGui::Spacing();
  ImGui::Separator();
  infoPanel.render(audioProcessor.getReceptiveFieldSamples(),
                   audioProcessor.getCurrentSampleRate(),
                   audioProcessor.getModelParameterCount(),
                   audioProcessor.getModelError());
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginChild("Graph", ImVec2(0.0f, 0.0f), true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);
  nodeRenderer.render(nodeGraph);
  ImGui::EndChild();

  ImGui::End();
}

void AuralForgeAudioProcessorEditor::renderParameters() {
  using namespace auralforge;
  auto &state = audioProcessor.getParameterState();

  auto depth =
      juce::roundToInt(state.getRawParameterValue(params::depth)->load());
  if (ImGui::InputInt("Depth", &depth))
    setParameterValue(state, params::depth,
                      static_cast<float>(juce::jlimit(1, 999, depth)));

  auto kernel =
      juce::roundToInt(state.getRawParameterValue(params::kernelSize)->load());
  if (ImGui::SliderInt("Kernel", &kernel, 2, 65))
    setParameterValue(state, params::kernelSize, static_cast<float>(kernel));

  auto channels =
      juce::roundToInt(state.getRawParameterValue(params::channels)->load());
  if (ImGui::InputInt("Channels", &channels))
    setParameterValue(state, params::channels,
                      static_cast<float>(juce::jlimit(1, 512, channels)));

  auto activation =
      juce::roundToInt(state.getRawParameterValue(params::activation)->load());
  constexpr const char *activations[]{"ReLU", "Sigmoid", "Tanh", "LeakyReLU"};
  if (ImGui::Combo("Activation", &activation, activations, 4))
    setParameterValue(state, params::activation,
                      static_cast<float>(activation));

  auto seed =
      juce::roundToInt(state.getRawParameterValue(params::globalSeed)->load());
  if (ImGui::InputInt("Seed", &seed))
    setParameterValue(state, params::globalSeed,
                      static_cast<float>(std::max(0, seed)));

  auto midiCC =
      juce::roundToInt(state.getRawParameterValue(params::randomizeCC)->load());
  if (ImGui::SliderInt("Randomize CC", &midiCC, 0, 127))
    setParameterValue(state, params::randomizeCC, static_cast<float>(midiCC));

  auto mix = state.getRawParameterValue(params::dryWet)->load();
  if (ImGui::SliderFloat("Dry / Wet", &mix, 0.0f, 1.0f, "%.2f"))
    setParameterValue(state, params::dryWet, mix);
}

void AuralForgeAudioProcessorEditor::updateGraphIfNeeded() {
  const auto requested = audioProcessor.getRequestedConfiguration();
  if (!graphInitialized ||
      !sameArchitecture(requested, displayedConfiguration)) {
    displayedConfiguration = requested;
    nodeGraph.rebuildFromModel(displayedConfiguration);
    graphInitialized = true;
  }
}
