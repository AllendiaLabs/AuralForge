#pragma once

#include <JuceHeader.h>

namespace openyourbox::params {
/**
 * @brief Builds the complete DAW-automatable parameter layout.
 * @return Parameter layout suitable for constructing an APVTS.
 */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace openyourbox::params
