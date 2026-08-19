#pragma once

#include <JuceHeader.h>

#include "dsp/LookbackBuffer.h"
#include "dsp/WeightRandomizer.h"

#include <atomic>
#include <memory>

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

  /** @brief Returns the current host sample rate. */
  [[nodiscard]] double getCurrentSampleRate() const noexcept;

  /** @brief Returns the most recent asynchronous build/runtime error. */
  [[nodiscard]] juce::String getModelError() const;

private:
  struct RuntimeState {
    std::shared_ptr<const auralforge::dsp::ModelSnapshot> snapshot;
    auralforge::dsp::LookbackBuffer lookback;
    torch::Tensor inputTensor;
    int maximumBlockSize = 0;
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
  void requestCurrentArchitecture(bool randomizeWeights);
  void resetRandomizeParameter();

  juce::AudioProcessorValueTreeState parameters;
  auralforge::dsp::WeightRandomizer modelBuilder;
  mutable std::shared_ptr<RuntimeState> publishedRuntime;
  std::shared_ptr<RuntimeState> activeRuntime;
  std::shared_ptr<RuntimeState> previousRuntime;

  std::atomic<double> currentSampleRate{44100.0};
  std::atomic<int> preparedBlockSize{0};
  std::atomic<bool> prepared{false};
  std::atomic<bool> architectureChangePending{false};
  std::atomic<bool> randomizePending{false};
  std::atomic<bool> midiRandomizePending{false};
  std::atomic<bool> restoringState{false};
  std::atomic<bool> lastRandomizeValue{false};
  std::atomic<std::uint64_t> randomizationCounter{0};
  int crossfadeSamplesRemaining = 0;

  mutable juce::CriticalSection errorLock;
  juce::String runtimeError;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuralForgeAudioProcessor)
};
