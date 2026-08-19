#pragma once

#include <JuceHeader.h>

namespace auralforge::params {
/**
 * @brief Builds the complete DAW-automatable parameter layout.
 * @return Parameter layout suitable for constructing an APVTS.
 */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace auralforge::params
