#pragma once

#include <JuceHeader.h>

namespace openyourbox::ui {
/**
 * @class RandomizeButton
 * @brief ImGui control that drives the host-visible randomize trigger.
 */
class RandomizeButton {
public:
  /**
   * @brief Draws the trigger and notifies host automation when clicked.
   * @param state Processor parameter state containing the randomize parameter.
   */
  void render(juce::AudioProcessorValueTreeState &state) const;
};
} // namespace openyourbox::ui
