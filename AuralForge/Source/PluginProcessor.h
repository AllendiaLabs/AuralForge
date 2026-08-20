#pragma once

#include <JuceHeader.h>

#include "dsp/LiveGraphPublisher.h"
#include "dsp/LookbackBuffer.h"
#include "dsp/TorchScriptBlackBox.h"
#include "dsp/WeightRandomizer.h"
#include "graph/NodeGraph.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

/**
 * @class AuralForgeAudioProcessor
 * @brief Host-facing audio effect that owns parameter, model, and runtime
 * state.
 */
class AuralForgeAudioProcessor final
    : public juce::AudioProcessor,
      private juce::AudioProcessorValueTreeState::Listener,
      private juce::AsyncUpdater {
public:
  /** @brief Constructs the plugin and registers all parameter listeners. */
  AuralForgeAudioProcessor();
  /** @brief Unregisters listeners and cancels pending asynchronous work. */
  ~AuralForgeAudioProcessor() override;

  /** @brief Preallocates inference resources for the host configuration. */
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  /** @brief Releases inference resources after playback stops. */
  void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
  /** @brief Accepts matching mono or stereo main buses. */
  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

  /** @brief Processes one audio block through the current immutable TCN. */
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  /** @brief Creates the Dear ImGui plugin editor. */
  juce::AudioProcessorEditor *createEditor() override;
  /** @brief Reports that this processor supplies an editor. */
  bool hasEditor() const override;

  /** @brief Returns the host-visible plugin name. */
  const juce::String getName() const override;

  /** @brief Reports MIDI input support for randomize CC control. */
  bool acceptsMidi() const override;
  /** @brief Reports that the effect does not emit MIDI. */
  bool producesMidi() const override;
  /** @brief Reports that this is an audio effect, not a MIDI effect. */
  bool isMidiEffect() const override;
  /** @brief Returns zero tail because the TCN has finite causal history. */
  double getTailLengthSeconds() const override;

  /** @brief Returns the mandatory single host program. */
  int getNumPrograms() override;
  /** @brief Returns the active host program index. */
  int getCurrentProgram() override;
  /** @brief Accepts the only valid program index. */
  void setCurrentProgram(int index) override;
  /** @brief Returns the default program name. */
  const juce::String getProgramName(int index) override;
  /** @brief Ignores host attempts to rename the fixed program. */
  void changeProgramName(int index, const juce::String &newName) override;

  /** @brief Serializes parameters, architecture hash, and exact model weights.
   */
  void getStateInformation(juce::MemoryBlock &destData) override;
  /** @brief Restores parameters and exact model weights when compatible. */
  void setStateInformation(const void *data, int sizeInBytes) override;

  /** @brief Returns mutable APVTS access for editor controls and attachments.
   */
  juce::AudioProcessorValueTreeState &getParameterState() noexcept;

  /** @brief Returns a snapshot of the currently requested architecture. */
  [[nodiscard]] auralforge::dsp::TCNConfiguration
  getRequestedConfiguration() const noexcept;

  /** @brief Returns the current receptive field in samples. */
  [[nodiscard]] std::uint64_t getReceptiveFieldSamples() const noexcept;

  /** @brief Returns the current model parameter count. */
  [[nodiscard]] std::uint64_t getModelParameterCount() const noexcept;

  /**
   * @brief Returns the latest per-buffer inference time for a frozen node.
   * @param nodeId Stable frozen graph node identifier.
   * @return Duration in milliseconds, or zero before the first processed block.
   */
  [[nodiscard]] double
  getFrozenInferenceTimeMilliseconds(std::int32_t nodeId) const noexcept;

  /** @brief Returns the current host sample rate. */
  [[nodiscard]] double getCurrentSampleRate() const noexcept;

  /** @brief Returns the most recent asynchronous build/runtime error. */
  [[nodiscard]] juce::String getModelError() const;

  /**
   * @brief Applies an inline TCN graph configuration through host parameters.
   * @param configuration Valid live TCN configuration.
   */
  void applyGraphConfiguration(
      const auralforge::dsp::TCNConfiguration &configuration);

  /**
   * @brief Requests deterministic randomization for one live weighted element.
   * @param nodeId Stable graph element identifier.
   * @param seed Signed per-element seed.
   */
  void randomizeGraphElement(std::int32_t nodeId, std::int32_t seed);

  /** @brief Returns the latest persisted message-thread graph snapshot. */
  [[nodiscard]] juce::ValueTree getGraphState() const;

  /**
   * @brief Publishes a graph snapshot for project-state persistence.
   * @param graphState Complete serialized graph document.
   * @param compileRuntime Whether the audio graph should be recompiled.
   */
  void setGraphState(const juce::ValueTree &graphState,
                     bool compileRuntime = true);

  /**
   * @brief Publishes Gain and Knob/XY values without recompiling the graph.
   * @param controls Immutable control table consumed on the audio thread.
   */
  void setRuntimeControls(
      const auralforge::dsp::RuntimeControlState &controls);

  /** @brief Returns the monotonic graph/control revision used by analysis. */
  [[nodiscard]] std::uint64_t getGraphRevision() const noexcept;

  /** @brief Returns true when the host transport is currently playing. */
  [[nodiscard]] bool isTransportPlaying() const noexcept;

  /**
   * @brief Copies the latest lock-free live-capture slot for analysis.
   * @param inputPeak Receives the host-input peak.
   * @param outputPeak Receives the host-output peak.
   * @param suitable Receives whether live audio can drive analysis.
   * @param input Destination for planar captured input, or null to skip copy.
   * @param maxSamples Capacity of each `input` channel.
   * @param channels Receives captured channel count.
   * @param samples Receives captured sample count.
   */
  void copyLiveCapture(float &inputPeak, float &outputPeak, bool &suitable,
                       float *const *input, int maxSamples, int &channels,
                       int &samples) const noexcept;

  /**
   * @brief Reads the latest audio-thread tap peaks for one compiled node.
   * @param nodeId Stable graph node identifier.
   * @param inputPeak Receives the upstream peak.
   * @param outputPeak Receives the node-output peak.
   * @return True when a published runtime contains the node.
   */
  [[nodiscard]] bool getAnalysisTapPeaks(std::int32_t nodeId, float &inputPeak,
                                         float &outputPeak) const noexcept;

  /**
   * @brief Loads, warms, and publishes a validated frozen artifact handle.
   * @param result Worker result containing artifact path and channel metadata.
   * @param error Receives a human-readable load or validation error.
   * @return True only when the prepared artifact handle has been published.
   *
   * This function performs blocking file and Torch work and must be called from
   * a background thread. Publication alone does not switch processBlock to the
   * artifact; graph-runtime integration owns that separate transition.
   */
  bool
  prepareFrozenArtifact(const auralforge::graph::FreezeSelectionResult &result,
                        std::string &error);

  /**
   * @brief Releases the matching published frozen artifact on unfreeze.
   * @param artifactPath Artifact owned by the frozen group being restored.
   */
  void releaseFrozenArtifact(const std::string &artifactPath);

  /**
   * @brief Reports whether a matching validated artifact is published.
   * @param artifactPath Absolute local artifact path.
   * @return True when the artifact is ready for frozen runtime integration.
   */
  [[nodiscard]] bool
  hasPreparedFrozenArtifact(const std::string &artifactPath) const noexcept;

  /**
   * @brief Resolves a frozen graph node for off-thread analysis.
   * @param node Frozen BlackBox graph node.
   * @return Matching immutable factory, or null when unavailable.
   */
  [[nodiscard]] std::shared_ptr<const auralforge::dsp::FrozenBlackBoxFactory>
  resolveFrozenBlackBoxForAnalysis(
      const auralforge::graph::GraphNode &node) const;

private:
  /** @brief Immutable artifact map atomically replaced by background work. */
  struct FrozenArtifactRegistry {
    /** @brief Validated factories indexed by canonical artifact path. */
    std::unordered_map<
        std::string,
        std::shared_ptr<const auralforge::dsp::TorchScriptBlackBoxFactory>>
        artifacts;
  };

  /** @brief Prepared state for the legacy Phase 1 whole-TCN runtime. */
  struct RuntimeState {
    std::shared_ptr<const auralforge::dsp::ModelSnapshot> snapshot;
    auralforge::dsp::LookbackBuffer lookback;
    torch::Tensor inputTensor;
    int maximumBlockSize = 0;
    /** @brief Per-channel previous DC-blocker input samples. */
    std::array<float, 2> dcInput{};
    /** @brief Per-channel previous DC-blocker output samples. */
    std::array<float, 2> dcOutput{};
  };

  void parameterChanged(const juce::String &parameterID,
                        float newValue) override;
  void handleAsyncUpdate() override;
  void publishRuntime(
      const std::shared_ptr<const auralforge::dsp::ModelSnapshot> &snapshot);
  std::shared_ptr<RuntimeState> createRuntime(
      const std::shared_ptr<const auralforge::dsp::ModelSnapshot> &snapshot);
  torch::Tensor runModel(RuntimeState &runtime,
                         const juce::AudioBuffer<float> &input, int numSamples);
  /**
   * @brief Applies a preallocated first-order high-pass stage in place.
   * @param runtime Active runtime containing per-channel filter memory.
   * @param buffer Host audio buffer.
   * @param channels Number of channels to process.
   * @param samples Number of valid samples.
   */
  void applyDcBlocker(RuntimeState &runtime, juce::AudioBuffer<float> &buffer,
                      int channels, int samples) noexcept;
  /**
   * @brief Applies the DC blocker with explicit per-channel state.
   * @param inputState Previous input samples.
   * @param outputState Previous output samples.
   * @param buffer Host audio buffer.
   * @param channels Number of channels to process.
   * @param samples Number of valid samples.
   */
  void applyDcBlocker(std::array<float, 2> &inputState,
                      std::array<float, 2> &outputState,
                      juce::AudioBuffer<float> &buffer, int channels,
                      int samples) noexcept;
  /**
   * @brief Processes through the latest published modular graph when ready.
   * @param buffer In-place host audio buffer.
   * @param inputChannels Number of readable input channels.
   * @param numSamples Number of valid samples.
   * @return True when a modular runtime produced the block.
   */
  bool processLiveGraph(juce::AudioBuffer<float> &buffer, int inputChannels,
                        int numSamples) noexcept;
  /** @brief Queues the latest persisted graph for off-thread compilation. */
  void requestGraphCompile();
  /**
   * @brief Resolves a frozen graph node to a validated artifact factory.
   * @param node Frozen BlackBox graph node.
   * @return Matching immutable factory, or null when unavailable.
   */
  [[nodiscard]] std::shared_ptr<const auralforge::dsp::FrozenBlackBoxFactory>
  resolveFrozenBlackBox(const auralforge::graph::GraphNode &node) const;
  void requestCurrentArchitecture(bool randomizeWeights);
  void resetRandomizeParameter();

  juce::AudioProcessorValueTreeState parameters;
  auralforge::dsp::WeightRandomizer modelBuilder;
  /** @brief Atomically published immutable frozen artifact registry. */
  mutable std::shared_ptr<const FrozenArtifactRegistry>
      publishedFrozenArtifacts;
  /** @brief Dedicated off-thread modular graph compiler and publisher. */
  auralforge::dsp::LiveGraphPublisher graphPublisher;
  mutable std::shared_ptr<RuntimeState> publishedRuntime;
  std::shared_ptr<RuntimeState> activeRuntime;
  std::shared_ptr<RuntimeState> previousRuntime;
  /** @brief Modular runtime currently owned by the audio thread. */
  std::shared_ptr<auralforge::dsp::LiveGraphRuntime> activeGraphRuntime;
  /** @brief Prior modular runtime retained during click-free replacement. */
  std::shared_ptr<auralforge::dsp::LiveGraphRuntime> previousGraphRuntime;
  /** @brief Preallocated output workspace for the active modular graph. */
  juce::AudioBuffer<float> graphWetBuffer;
  /** @brief Preallocated output workspace for graph crossfades. */
  juce::AudioBuffer<float> previousGraphWetBuffer;
  /** @brief Per-channel graph DC-blocker input memory. */
  std::array<float, 2> graphDcInput{};
  /** @brief Per-channel graph DC-blocker output memory. */
  std::array<float, 2> graphDcOutput{};

  std::atomic<double> currentSampleRate{44100.0};
  /** @brief Runtime high-pass feedback coefficient derived from sample rate. */
  std::atomic<float> dcBlockerCoefficient{0.997f};
  std::atomic<int> preparedBlockSize{0};
  std::atomic<bool> prepared{false};
  std::atomic<bool> architectureChangePending{false};
  std::atomic<bool> randomizePending{false};
  std::atomic<bool> midiRandomizePending{false};
  std::atomic<bool> restoringState{false};
  std::atomic<bool> lastRandomizeValue{false};
  std::atomic<std::uint64_t> randomizationCounter{0};
  int crossfadeSamplesRemaining = 0;
  /** @brief Samples remaining in the current modular-runtime crossfade. */
  int graphCrossfadeSamplesRemaining = 0;

  mutable juce::CriticalSection errorLock;
  juce::String runtimeError;
  /** @brief Protects host-state access to the UI-owned graph snapshot. */
  mutable juce::CriticalSection graphStateLock;
  /** @brief Latest immutable graph document copy used for project recall. */
  juce::ValueTree persistedGraphState{"GraphDocument"};
  /** @brief Monotonic revision bumped on graph or conditioning publication. */
  std::atomic<std::uint64_t> graphRevision{1};
  /** @brief Latest Gain/conditioning table published for the audio thread. */
  mutable std::shared_ptr<const auralforge::dsp::RuntimeControlState>
      publishedControls;
  /** @brief Double-buffered live input capture used by analysis. */
  struct LiveCaptureSlot {
    /** @brief Host-input peak of the captured block. */
    float inputPeak = 0.0f;
    /** @brief Host-output peak of the captured block. */
    float outputPeak = 0.0f;
    /** @brief True when the captured block is loud enough for live analysis. */
    bool suitable = false;
    /** @brief Captured channel count. */
    int channels = 0;
    /** @brief Captured sample count. */
    int samples = 0;
    /** @brief Planar captured input, stereo maximum of 512 samples. */
    std::array<std::array<float, 512>, 2> input{};
  };
  /** @brief Two capture slots swapped with `liveCaptureIndex`. */
  std::array<LiveCaptureSlot, 2> liveCaptureSlots{};
  /** @brief Index of the slot that message-thread readers should consume. */
  std::atomic<int> liveCaptureIndex{0};
  /** @brief True when the host play-head reports playback. */
  std::atomic<bool> transportPlaying{false};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuralForgeAudioProcessor)
};
