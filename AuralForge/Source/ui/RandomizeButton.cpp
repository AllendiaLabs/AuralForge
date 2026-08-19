#include "RandomizeButton.h"
#include "../params/ParamIDs.h"

#include <imgui.h>

namespace auralforge::ui {
void RandomizeButton::render(juce::AudioProcessorValueTreeState &state) const {
  if (!ImGui::Button("Randomize Weights", ImVec2(-1.0f, 36.0f)))
    return;

  if (auto *parameter = state.getParameter(params::randomize)) {
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(1.0f);
    parameter->endChangeGesture();
  }
}
} // namespace auralforge::ui
