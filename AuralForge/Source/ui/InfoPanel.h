#pragma once

#include <JuceHeader.h>

#include <cstdint>

namespace auralforge::ui {
/**
 * @class InfoPanel
 * @brief Renders live architecture metrics and safety warnings.
 */
class InfoPanel {
public:
  /**
   * @brief Draws the model metrics in the current ImGui window.
   * @param receptiveFieldSamples Receptive field in samples.
   * @param sampleRate Current host sample rate.
   * @param parameterCount Number of model parameters.
   * @param buildError Latest asynchronous model error.
   */
  void render(std::uint64_t receptiveFieldSamples, double sampleRate,
              std::uint64_t parameterCount,
              const juce::String &buildError) const;
};
} // namespace auralforge::ui
