#pragma once

#include "../graph/GraphTypes.h"

#include <JuceHeader.h>

#include <atomic>
#include <functional>
#include <optional>
#include <string>

namespace auralforge::freeze {
/** @brief User-visible lifecycle state of one manual freeze request. */
enum class FreezeStatus {
  /** @brief No request is active or awaiting consumption. */
  idle,
  /** @brief Python compilation or artifact preparation is running. */
  compiling,
  /** @brief A successful result is ready for the message thread. */
  succeeded,
  /** @brief A failed result is ready for the message thread. */
  failed
};

/**
 * @class FreezeCoordinator
 * @brief Runs the Python freeze worker and artifact preparation off-thread.
 *
 * The message thread starts requests and polls immutable status snapshots.
 * Worker output is not exposed as successful until the supplied artifact
 * preparation callback has loaded and validated the TorchScript file.
 */
class FreezeCoordinator final : private juce::Thread {
public:
  /**
   * @brief Callback that validates and atomically publishes an artifact.
   * @param result Worker result containing artifact path and shape metadata.
   * @param error Receives a human-readable preparation failure.
   * @return True only after publication has completed.
   */
  using ArtifactPreparer =
      std::function<bool(const graph::FreezeSelectionResult &result,
                         std::string &error)>;

  /**
   * @brief Creates a coordinator for one editor instance.
   * @param artifactPreparer Off-thread processor artifact preparation callback.
   */
  explicit FreezeCoordinator(ArtifactPreparer artifactPreparer);

  /** @brief Stops the worker process and joins its thread. */
  ~FreezeCoordinator() override;

  /**
   * @brief Starts one manual freeze request without blocking the caller.
   * @param request Complete JSON worker request.
   * @return False when another request is already active.
   */
  bool start(const graph::FreezeSelectionRequest &request);

  /** @brief Returns the current lifecycle state without blocking. */
  [[nodiscard]] FreezeStatus getStatus() const noexcept;

  /** @brief Returns a short user-facing status message. */
  [[nodiscard]] juce::String getStatusMessage() const;

  /**
   * @brief Takes the completed worker result once.
   * @return Success or failure result, or no value while work is pending.
   */
  [[nodiscard]] std::optional<graph::FreezeSelectionResult> takeResult();

private:
  /** @brief Executes the worker process on the coordinator thread. */
  void run() override;

  /**
   * @brief Publishes one worker or preparation failure for message-thread use.
   * @param requestId Correlation identifier of the failed request.
   * @param message Human-readable failure detail.
   */
  void publishFailure(const std::string &requestId, const std::string &message);

  /** @brief Processor callback used after worker compilation succeeds. */
  ArtifactPreparer prepareArtifact;
  /** @brief Lock-free lifecycle state polled by the editor. */
  std::atomic<FreezeStatus> status{FreezeStatus::idle};
  /** @brief Protects request, result, message, and process ownership. */
  mutable juce::CriticalSection stateLock;
  /** @brief Immutable request copied before the worker thread starts. */
  graph::FreezeSelectionRequest pendingRequest;
  /** @brief Single completion consumed by the message thread. */
  std::optional<graph::FreezeSelectionResult> completedResult;
  /** @brief User-facing lifecycle or failure text. */
  juce::String statusMessage{"Ready"};
  /** @brief Killable child process owned for the active request. */
  std::unique_ptr<juce::ChildProcess> childProcess;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreezeCoordinator)
};
} // namespace auralforge::freeze
