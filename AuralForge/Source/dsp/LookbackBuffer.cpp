#include "LookbackBuffer.h"

#include <algorithm>

namespace auralforge::dsp {
void LookbackBuffer::resize(int channels, std::size_t historySamples) {
  numChannels = std::max(0, channels);
  historyLength = historySamples;
  writePosition = 0;
  storage.assign(static_cast<std::size_t>(numChannels) * historyLength, 0.0f);
}

void LookbackBuffer::clear() noexcept {
  std::fill(storage.begin(), storage.end(), 0.0f);
  writePosition = 0;
}

void LookbackBuffer::prependToTensor(
    torch::Tensor &destination, const juce::AudioBuffer<float> &currentBlock,
    int numSamples) const noexcept {
  auto accessor = destination.accessor<float, 3>();
  const auto channelsToCopy =
      std::min(numChannels, currentBlock.getNumChannels());

  for (int channel = 0; channel < channelsToCopy; ++channel) {
    const auto channelOffset =
        static_cast<std::size_t>(channel) * historyLength;
    for (std::size_t sample = 0; sample < historyLength; ++sample) {
      const auto source =
          historyLength == 0 ? 0 : (writePosition + sample) % historyLength;
      accessor[0][channel][static_cast<std::int64_t>(sample)] =
          historyLength == 0 ? 0.0f : storage[channelOffset + source];
    }

    const auto *input = currentBlock.getReadPointer(channel);
    for (int sample = 0; sample < numSamples; ++sample)
      accessor[0][channel][static_cast<std::int64_t>(historyLength) + sample] =
          input[sample];
  }
}

void LookbackBuffer::updateFromBlock(
    const juce::AudioBuffer<float> &currentBlock, int numSamples) noexcept {
  if (historyLength == 0)
    return;

  const auto channelsToCopy =
      std::min(numChannels, currentBlock.getNumChannels());
  for (int sample = 0; sample < numSamples; ++sample) {
    for (int channel = 0; channel < channelsToCopy; ++channel) {
      storage[static_cast<std::size_t>(channel) * historyLength +
              writePosition] = currentBlock.getSample(channel, sample);
    }
    writePosition = (writePosition + 1) % historyLength;
  }
}

std::size_t LookbackBuffer::getHistorySamples() const noexcept {
  return historyLength;
}

int LookbackBuffer::getNumChannels() const noexcept { return numChannels; }
} // namespace auralforge::dsp
