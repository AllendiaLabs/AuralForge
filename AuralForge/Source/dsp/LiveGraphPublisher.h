#pragma once

#include "LiveGraphEngine.h"

#include <JuceHeader.h>

#include <atomic>
#include <memory>

namespace auralforge::dsp {
/**
 * @class LiveGraphPublisher
 * @brief Compiles graph edits and randomization requests on a dedicated thread.
 *
 * Requests are coalesced and immutable prepared runtimes are published through
 * atomic shared-pointer operations for lock-free audio-thread acquisition.
 */
class LiveGraphPublisher final : private juce::Thread {
public:
  /** @brief Starts the dedicated graph compiler thread. */
  LiveGraphPublisher();

  /** @brief Stops compilation and joins the dedicated thread. */
  ~LiveGraphPublisher() override;

  /**
   * @brief Queues the newest editable graph for compilation.
   * @param graphState Complete serialized GraphDocument.
   * @param options Current host channel and block constraints.
   * @param resolver Frozen BlackBox resolver used during compilation.
   */
  void requestCompile(const juce::ValueTree &graphState,
                      const LiveGraphCompileOptions &options,
                      FrozenBlackBoxResolver resolver = {});

  /**
   * @brief Queues deterministic replacement of one weighted live element.
   * @param nodeId Stable target node identifier.
   * @param seed Signed deterministic seed.
   */
  void requestRandomization(std::int32_t nodeId,
                            std::int32_t seed) noexcept;

  /** @brief Atomically returns the latest prepared runtime. */
  [[nodiscard]] std::shared_ptr<LiveGraphRuntime>
  getPublishedRuntime() const noexcept;

  /** @brief Returns the latest compiler or preparation error. */
  [[nodiscard]] juce::String getLastError() const;

private:
  /** @brief Waits for and processes coalesced off-thread requests. */
  void run() override;

  /** @brief Compiles the latest serialized graph request. */
  void compileLatest();

  /** @brief Randomizes one element in the latest published snapshot. */
  void randomizeLatest();

  /**
   * @brief Publishes one prepared runtime and clears the error state.
   * @param runtime Fully prepared immutable-snapshot runtime.
   */
  void publish(std::shared_ptr<LiveGraphRuntime> runtime);

  /**
   * @brief Stores a user-facing background build failure.
   * @param error Detailed graph compiler error.
   */
  void publishError(const LiveGraphCompileError &error);

  /**
   * @brief Clears the published runtime without reporting a user-facing error.
   *
   * Used when the graph is idle because it has no complete Audio
   * Input-to-Output path. Audio stays silent until the path is restored.
   */
  void publishIdle();

  /** @brief Guards request payloads shared with producer threads. */
  mutable juce::CriticalSection requestLock;
  /** @brief Latest serialized graph request. */
  juce::ValueTree requestedGraph{"GraphDocument"};
  /** @brief Latest host constraints for graph compilation. */
  LiveGraphCompileOptions requestedOptions;
  /** @brief Latest frozen-node resolver. */
  FrozenBlackBoxResolver requestedResolver;
  /** @brief Latest requested randomization target. */
  std::int32_t requestedNodeId = 0;
  /** @brief Latest requested signed randomization seed. */
  std::int32_t requestedSeed = 0;
  /** @brief Whether a newer graph compilation is pending. */
  std::atomic<bool> compilePending{false};
  /** @brief Whether element randomization is pending after compilation. */
  std::atomic<bool> randomizationPending{false};
  /** @brief Atomically published prepared graph runtime. */
  mutable std::shared_ptr<LiveGraphRuntime> publishedRuntime;
  /** @brief Guards the user-facing compiler error. */
  mutable juce::CriticalSection errorLock;
  /** @brief Latest compiler or runtime preparation failure. */
  juce::String lastError;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveGraphPublisher)
};
} // namespace auralforge::dsp
