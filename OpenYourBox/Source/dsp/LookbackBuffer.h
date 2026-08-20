#pragma once

#include <JuceHeader.h>
#include <torch/torch.h>

#include <cstddef>
#include <vector>

namespace openyourbox::dsp {
/**
 * @class LookbackBuffer
 * @brief Per-channel circular history used for causal block processing.
 */
class LookbackBuffer {
public:
  /**
   * @brief Allocates history storage.
   * @param channels Number of audio channels.
   * @param historySamples Number of previous samples retained per channel.
   */
  void resize(int channels, std::size_t historySamples);

  /** @brief Clears all history to silence without changing capacity. */
  void clear() noexcept;

  /**
   * @brief Copies ordered history followed by a JUCE block into a tensor.
   * @param destination Preallocated float tensor shaped [1, channels,
   * capacity].
   * @param currentBlock Current audio input.
   * @param numSamples Number of valid current samples.
   */
  void prependToTensor(torch::Tensor &destination,
                       const juce::AudioBuffer<float> &currentBlock,
                       int numSamples) const noexcept;

  /**
   * @brief Advances history using the newest input block.
   * @param currentBlock Current audio input.
   * @param numSamples Number of valid samples.
   */
  void updateFromBlock(const juce::AudioBuffer<float> &currentBlock,
                       int numSamples) noexcept;

  /** @brief Returns retained history length per channel. */
  [[nodiscard]] std::size_t getHistorySamples() const noexcept;

  /** @brief Returns the configured channel count. */
  [[nodiscard]] int getNumChannels() const noexcept;

private:
  std::vector<float> storage;
  int numChannels = 0;
  std::size_t historyLength = 0;
  std::size_t writePosition = 0;
};
} // namespace openyourbox::dsp
