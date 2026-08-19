#include "WeightRandomizer.h"

#include <utility>

namespace auralforge::dsp {
WeightRandomizer::WeightRandomizer() = default;

WeightRandomizer::~WeightRandomizer() { cancelPendingUpdate(); }

void WeightRandomizer::requestBuild(const TCNConfiguration &configuration,
                                    std::uint64_t seed,
                                    std::uint64_t counter) noexcept {
  requestSequence.fetch_add(1, std::memory_order_acq_rel);
  requestedDepth.store(configuration.depth, std::memory_order_relaxed);
  requestedKernelSize.store(configuration.kernelSize,
                            std::memory_order_relaxed);
  requestedChannels.store(configuration.channels, std::memory_order_relaxed);
  requestedInputChannels.store(configuration.inputChannels,
                               std::memory_order_relaxed);
  requestedOutputChannels.store(configuration.outputChannels,
                                std::memory_order_relaxed);
  requestedActivation.store(static_cast<int>(configuration.activation),
                            std::memory_order_relaxed);
  requestedSeed.store(seed, std::memory_order_relaxed);
  requestedCounter.store(counter, std::memory_order_relaxed);
  requestSequence.fetch_add(1, std::memory_order_release);
  triggerAsyncUpdate();
}

void WeightRandomizer::requestRandomization(
    const TCNConfiguration &configuration, std::uint64_t globalSeed,
    std::uint64_t counter) noexcept {
  requestBuild(configuration, globalSeed + counter, counter);
}

std::shared_ptr<const ModelSnapshot>
WeightRandomizer::buildNow(const TCNConfiguration &configuration,
                           std::uint64_t seed, std::uint64_t counter) {
  try {
    auto model = std::make_shared<TCNModel>(configuration);
    model->randomizeWeights(seed);
    model->eval();

    auto snapshot = std::make_shared<ModelSnapshot>();
    snapshot->model = std::move(model);
    snapshot->generation =
        publishedGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
    snapshot->randomizationCounter = counter;
    std::atomic_store_explicit(&publishedModel,
                               std::shared_ptr<const ModelSnapshot>(snapshot),
                               std::memory_order_release);

    {
      const juce::ScopedLock lock(statusLock);
      lastError.clear();
    }
    return snapshot;
  } catch (const std::exception &exception) {
    const juce::ScopedLock lock(statusLock);
    lastError = exception.what();
  }

  return {};
}

std::shared_ptr<const ModelSnapshot>
WeightRandomizer::getPublishedModel() const noexcept {
  return std::atomic_load_explicit(&publishedModel, std::memory_order_acquire);
}

void WeightRandomizer::setPublishCallback(PublishCallback callback) {
  onPublished = std::move(callback);
}

juce::String WeightRandomizer::getLastError() const {
  const juce::ScopedLock lock(statusLock);
  return lastError;
}

void WeightRandomizer::handleAsyncUpdate() {
  TCNConfiguration configuration;
  std::uint64_t seed = 0;
  std::uint64_t counter = 0;
  std::uint64_t handledSequence = 0;

  for (;;) {
    const auto before = requestSequence.load(std::memory_order_acquire);
    if ((before & 1U) != 0U)
      continue;

    configuration = readRequestedConfiguration();
    seed = requestedSeed.load(std::memory_order_relaxed);
    counter = requestedCounter.load(std::memory_order_relaxed);
    const auto after = requestSequence.load(std::memory_order_acquire);
    if (before == after) {
      handledSequence = after;
      break;
    }
  }

  if (buildNow(configuration, seed, counter) != nullptr && onPublished)
    onPublished();

  // A request arriving during construction is coalesced into one more build.
  const auto sequenceAfterBuild =
      requestSequence.load(std::memory_order_acquire);
  if (sequenceAfterBuild != handledSequence)
    triggerAsyncUpdate();
}

TCNConfiguration WeightRandomizer::readRequestedConfiguration() const noexcept {
  TCNConfiguration configuration;

  for (;;) {
    const auto before = requestSequence.load(std::memory_order_acquire);
    if ((before & 1U) != 0U)
      continue;

    configuration.depth = requestedDepth.load(std::memory_order_relaxed);
    configuration.kernelSize =
        requestedKernelSize.load(std::memory_order_relaxed);
    configuration.channels = requestedChannels.load(std::memory_order_relaxed);
    configuration.inputChannels =
        requestedInputChannels.load(std::memory_order_relaxed);
    configuration.outputChannels =
        requestedOutputChannels.load(std::memory_order_relaxed);
    configuration.activation = static_cast<ActivationType>(
        requestedActivation.load(std::memory_order_relaxed));

    const auto after = requestSequence.load(std::memory_order_acquire);
    if (before == after)
      return configuration;
  }
}
} // namespace auralforge::dsp
