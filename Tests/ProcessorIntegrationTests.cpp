#include "PluginProcessor.h"

#include <JuceHeader.h>

#include <cmath>
#include <iostream>

namespace {
/**
 * @brief Fills a stereo buffer with a deterministic sine wave.
 * @param buffer Destination buffer.
 * @param sampleRate Test sample rate.
 */
void fillSine(juce::AudioBuffer<float> &buffer, double sampleRate) {
  for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
    const auto value =
        static_cast<float>(0.2 * std::sin(juce::MathConstants<double>::twoPi *
                                          440.0 * sample / sampleRate));
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
      buffer.setSample(channel, sample, value);
  }
}

/**
 * @brief Reports a failed processor invariant.
 * @param condition Invariant result.
 * @param message Human-readable failure.
 * @return The supplied condition.
 */
bool expect(bool condition, const char *message) {
  if (!condition)
    std::cerr << "FAIL: " << message << '\n';
  return condition;
}
} // namespace

/**
 * @brief Runs processor-level audio, silence, and state-recall checks.
 * @return Zero when every invariant passes.
 */
int main() {
  constexpr double sampleRate = 48000.0;
  constexpr int blockSize = 256;
  juce::ScopedJuceInitialiser_GUI juceInitialiser;
  juce::MidiBuffer midi;
  bool passed = true;

  AuralForgeAudioProcessor original;
  original.prepareToPlay(sampleRate, blockSize);

  juce::AudioBuffer<float> input(2, blockSize);
  fillSine(input, sampleRate);
  auto processed = input;
  original.processBlock(processed, midi);
  passed &= expect(processed.getMagnitude(0, 0, blockSize) > 0.0f,
                   "default model must produce non-silent output");

  float difference = 0.0f;
  for (int sample = 0; sample < blockSize; ++sample)
    difference = std::max(difference, std::abs(processed.getSample(0, sample) -
                                               input.getSample(0, sample)));
  passed &= expect(difference > 1.0e-5f,
                   "default wet model must audibly differ from the dry input");

  juce::AudioBuffer<float> silence(2, blockSize);
  silence.clear();
  original.processBlock(silence, midi);
  passed &= expect(silence.getMagnitude(0, 0, blockSize) == 0.0f,
                   "digital silence must remain digital silence");

  juce::MemoryBlock savedState;
  original.getStateInformation(savedState);
  original.releaseResources();
  original.prepareToPlay(sampleRate, blockSize);

  auto expectedRecall = input;
  original.processBlock(expectedRecall, midi);

  AuralForgeAudioProcessor restored;
  restored.setStateInformation(savedState.getData(),
                               static_cast<int>(savedState.getSize()));
  restored.prepareToPlay(sampleRate, blockSize);
  auto actualRecall = input;
  restored.processBlock(actualRecall, midi);

  float recallDifference = 0.0f;
  for (int channel = 0; channel < actualRecall.getNumChannels(); ++channel) {
    for (int sample = 0; sample < blockSize; ++sample) {
      recallDifference =
          std::max(recallDifference,
                   std::abs(actualRecall.getSample(channel, sample) -
                            expectedRecall.getSample(channel, sample)));
    }
  }
  passed &= expect(
      recallDifference < 1.0e-6f,
      "serialized parameters and weights must restore exact sonic state");

  if (passed)
    std::cout << "AuralForge processor integration tests passed\n";
  return passed ? 0 : 1;
}
