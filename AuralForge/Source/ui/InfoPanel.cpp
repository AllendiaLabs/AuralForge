#include "InfoPanel.h"

#include <imgui.h>

namespace auralforge::ui {
void InfoPanel::render(std::uint64_t receptiveFieldSamples, double sampleRate,
                       std::uint64_t parameterCount,
                       const juce::String &buildError) const {
  const auto milliseconds =
      sampleRate > 0.0
          ? static_cast<double>(receptiveFieldSamples) * 1000.0 / sampleRate
          : 0.0;

  ImGui::Text("Receptive field: %llu samples (%.2f ms)",
              static_cast<unsigned long long>(receptiveFieldSamples),
              milliseconds);
  ImGui::Text("Trainable parameters: %llu",
              static_cast<unsigned long long>(parameterCount));

  if (milliseconds > 1000.0) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.2f, 1.0f));
    ImGui::TextWrapped("Warning: this receptive field exceeds one second and "
                       "may be too expensive for real-time playback.");
    ImGui::PopStyleColor();
  }

  if (buildError.isNotEmpty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    ImGui::TextWrapped("Model unchanged: %s", buildError.toRawUTF8());
    ImGui::PopStyleColor();
  }
}
} // namespace auralforge::ui
