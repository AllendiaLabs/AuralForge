#pragma once

#include "TCNModel.h"

#include <JuceHeader.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

namespace auralforge::dsp {
/** @brief Immutable model publication consumed by the audio thread. */
struct ModelSnapshot {
  std::shared_ptr<TCNModel> model;
  std::uint64_t generation = 0;
  std::uint64_t randomizationCounter = 0;
};

/**
 * @class WeightRandomizer
 * @brief Coalesces model requests and builds models on the JUCE message thread.
 */
class WeightRandomizer final : private juce::AsyncUpdater {
public:
  /** @brief Callback invoked after a model has been published. */
  using PublishCallback = std::function<void()>;

  /** @brief Creates a model builder with no published model. */
  WeightRandomizer();

  /** @brief Cancels pending message-thread work. */
  ~WeightRandomizer() override;

  /**
   * @brief Requests an asynchronously constructed architecture.
   * @param configuration Architecture to build.
   * @param seed Deterministic initial weight seed.
   * @param counter Randomization counter persisted with the snapshot.
   */
  void requestBuild(const TCNConfiguration &configuration, std::uint64_t seed,
                    std::uint64_t counter = 0) noexcept;

  /**
   * @brief Requests deterministic randomized weights for an architecture.
   * @param configuration Architecture to build.
   * @param globalSeed User-facing base seed.
   * @param counter Monotonic per-instance randomization count.
   */
  void requestRandomization(const TCNConfiguration &configuration,
                            std::uint64_t globalSeed,
                            std::uint64_t counter) noexcept;

  /**
   * @brief Builds and publishes synchronously on a non-audio thread.
   * @param configuration Architecture to build.
   * @param seed Deterministic weight seed.
   * @param counter Randomization counter.
   * @return Published snapshot, or null when the configuration is invalid.
   */
  std::shared_ptr<const ModelSnapshot>
  buildNow(const TCNConfiguration &configuration, std::uint64_t seed,
           std::uint64_t counter);

  /** @brief Atomically loads the latest immutable model snapshot. */
  [[nodiscard]] std::shared_ptr<const ModelSnapshot>
  getPublishedModel() const noexcept;

  /** @brief Installs a message-thread publication callback. */
  void setPublishCallback(PublishCallback callback);

  /** @brief Returns the most recent build error for UI display. */
  [[nodiscard]] juce::String getLastError() const;

private:
  void handleAsyncUpdate() override;
  TCNConfiguration readRequestedConfiguration() const noexcept;

  std::atomic<std::uint64_t> requestSequence{0};
  std::atomic<int> requestedDepth{4};
  std::atomic<int> requestedKernelSize{3};
  std::atomic<int> requestedChannels{16};
  std::atomic<int> requestedInputChannels{2};
  std::atomic<int> requestedOutputChannels{2};
  std::atomic<int> requestedActivation{0};
  std::atomic<std::uint64_t> requestedSeed{42};
  std::atomic<std::uint64_t> requestedCounter{0};
  std::atomic<std::uint64_t> publishedGeneration{0};

  mutable std::shared_ptr<const ModelSnapshot> publishedModel;
  PublishCallback onPublished;
  mutable juce::CriticalSection statusLock;
  juce::String lastError;
};
} // namespace auralforge::dsp
