#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParamIDs.h"
#include "params/ParamLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace {
constexpr std::array<const char *, 8> listenedParameterIDs{
    auralforge::params::depth,      auralforge::params::kernelSize,
    auralforge::params::channels,   auralforge::params::activation,
    auralforge::params::randomize,  auralforge::params::randomizeCC,
    auralforge::params::globalSeed, auralforge::params::dryWet};

constexpr std::uint64_t maximumRealtimeHistory = 1U << 20U;
constexpr int minimumPreparedBlockSize = 8192;
constexpr int modelCrossfadeSamples = 64;

bool sameConfiguration(
    const auralforge::dsp::TCNConfiguration &left,
    const auralforge::dsp::TCNConfiguration &right) noexcept {
  return left.depth == right.depth && left.kernelSize == right.kernelSize &&
         left.channels == right.channels &&
         left.inputChannels == right.inputChannels &&
         left.outputChannels == right.outputChannels &&
         left.activation == right.activation;
}
} // namespace

AuralForgeAudioProcessor::AuralForgeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, auralforge::params::stateType,
                 auralforge::params::createParameterLayout())
#else
    : parameters(*this, nullptr, auralforge::params::stateType,
                 auralforge::params::createParameterLayout())
#endif
{
  for (const auto *identifier : listenedParameterIDs)
    parameters.addParameterListener(identifier, this);

  modelBuilder.setPublishCallback([this] {
    publishRuntime(modelBuilder.getPublishedModel());
    resetRandomizeParameter();
  });
}

AuralForgeAudioProcessor::~AuralForgeAudioProcessor() {
  cancelPendingUpdate();
  modelBuilder.setPublishCallback({});
  for (const auto *identifier : listenedParameterIDs)
    parameters.removeParameterListener(identifier, this);
}

const juce::String AuralForgeAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool AuralForgeAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool AuralForgeAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool AuralForgeAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double AuralForgeAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AuralForgeAudioProcessor::getNumPrograms() { return 1; }

int AuralForgeAudioProcessor::getCurrentProgram() { return 0; }

void AuralForgeAudioProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String AuralForgeAudioProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return "Default";
}

void AuralForgeAudioProcessor::changeProgramName(int index,
                                                 const juce::String &newName) {
  juce::ignoreUnused(index, newName);
}

void AuralForgeAudioProcessor::prepareToPlay(double sampleRate,
                                             int samplesPerBlock) {
  currentSampleRate.store(sampleRate, std::memory_order_release);
  preparedBlockSize.store(std::max(samplesPerBlock, minimumPreparedBlockSize),
                          std::memory_order_release);
  prepared.store(true, std::memory_order_release);

  auto snapshot = modelBuilder.getPublishedModel();
  const auto configuration = getRequestedConfiguration();
  if (snapshot == nullptr ||
      !sameConfiguration(snapshot->model->getConfiguration(), configuration)) {
    const auto seed = static_cast<std::uint64_t>(
        parameters.getRawParameterValue(auralforge::params::globalSeed)
            ->load());
    const auto counter = randomizationCounter.load(std::memory_order_acquire);
    snapshot = modelBuilder.buildNow(configuration, seed + counter, counter);
  }
  publishRuntime(snapshot);
}

void AuralForgeAudioProcessor::releaseResources() {
  prepared.store(false, std::memory_order_release);
  std::atomic_store_explicit(&publishedRuntime, std::shared_ptr<RuntimeState>{},
                             std::memory_order_release);
  activeRuntime.reset();
  previousRuntime.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AuralForgeAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  const auto output = layouts.getMainOutputChannelSet();
  if (output != juce::AudioChannelSet::mono() &&
      output != juce::AudioChannelSet::stereo())
    return false;

#if !JucePlugin_IsSynth
  if (output != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}
#endif

void AuralForgeAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                            juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  const auto inputChannels = getTotalNumInputChannels();
  const auto outputChannels = getTotalNumOutputChannels();
  const auto numSamples = buffer.getNumSamples();

  for (auto channel = inputChannels; channel < outputChannels; ++channel)
    buffer.clear(channel, 0, numSamples);

  const auto randomizeCC = juce::roundToInt(
      parameters.getRawParameterValue(auralforge::params::randomizeCC)->load());
  for (const auto metadata : midiMessages) {
    const auto message = metadata.getMessage();
    if (message.isController() &&
        message.getControllerNumber() == randomizeCC &&
        message.getControllerValue() >= 64) {
      midiRandomizePending.store(true, std::memory_order_release);
      triggerAsyncUpdate();
    }
  }

  auto latestRuntime =
      std::atomic_load_explicit(&publishedRuntime, std::memory_order_acquire);
  if (latestRuntime != nullptr && latestRuntime != activeRuntime) {
    previousRuntime = activeRuntime;
    activeRuntime = std::move(latestRuntime);
    activeRuntime->lookback.clear();
    crossfadeSamplesRemaining =
        previousRuntime != nullptr ? modelCrossfadeSamples : 0;
  }

  if (activeRuntime == nullptr || numSamples > activeRuntime->maximumBlockSize)
    return;

  float peak = 0.0f;
  for (int channel = 0; channel < inputChannels; ++channel)
    peak = std::max(peak, buffer.getMagnitude(channel, 0, numSamples));

  if (peak < 1.0e-6f) {
    activeRuntime->lookback.updateFromBlock(buffer, numSamples);
    if (previousRuntime != nullptr)
      previousRuntime->lookback.updateFromBlock(buffer, numSamples);
    buffer.clear();
    return;
  }

  try {
    auto wet = runModel(*activeRuntime, buffer, numSamples);
    torch::Tensor oldWet;
    const auto useCrossfade = previousRuntime != nullptr &&
                              crossfadeSamplesRemaining > 0 &&
                              previousRuntime->maximumBlockSize >= numSamples;
    if (useCrossfade)
      oldWet = runModel(*previousRuntime, buffer, numSamples);

    activeRuntime->lookback.updateFromBlock(buffer, numSamples);
    if (useCrossfade)
      previousRuntime->lookback.updateFromBlock(buffer, numSamples);

    const auto wetStart = wet.size(2) - numSamples;
    const auto oldStart = useCrossfade ? oldWet.size(2) - numSamples : 0;
    const auto wetChannelStride = wet.stride(1);
    const auto wetSampleStride = wet.stride(2);
    const auto oldChannelStride = useCrossfade ? oldWet.stride(1) : 0;
    const auto oldSampleStride = useCrossfade ? oldWet.stride(2) : 0;
    const auto *wetData = wet.data_ptr<float>();
    const auto *oldData = useCrossfade ? oldWet.data_ptr<float>() : nullptr;
    const auto dryWet =
        parameters.getRawParameterValue(auralforge::params::dryWet)->load();

    for (int sample = 0; sample < numSamples; ++sample) {
      const auto fade =
          useCrossfade && crossfadeSamplesRemaining > 0
              ? 1.0f - static_cast<float>(crossfadeSamplesRemaining) /
                           static_cast<float>(modelCrossfadeSamples)
              : 1.0f;

      for (int channel = 0; channel < inputChannels; ++channel) {
        auto processed = wetData[channel * wetChannelStride +
                                 (wetStart + sample) * wetSampleStride];
        if (useCrossfade) {
          const auto previous = oldData[channel * oldChannelStride +
                                        (oldStart + sample) * oldSampleStride];
          processed = previous + fade * (processed - previous);
        }

        const auto dry = buffer.getSample(channel, sample);
        buffer.setSample(channel, sample, dry + dryWet * (processed - dry));
      }

      if (crossfadeSamplesRemaining > 0)
        --crossfadeSamplesRemaining;
    }
  } catch (const std::exception &) {
    // Preserve the original input when an unsupported runtime shape reaches the
    // host.
  }
}

bool AuralForgeAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *AuralForgeAudioProcessor::createEditor() {
  return new AuralForgeAudioProcessorEditor(*this);
}

void AuralForgeAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  auto xml = parameters.copyState().createXml();
  const auto snapshot = modelBuilder.getPublishedModel();

  if (snapshot != nullptr && snapshot->model != nullptr) {
    std::ostringstream stream(std::ios::binary);
    torch::serialize::OutputArchive archive;
    snapshot->model->save(archive);
    archive.save_to(stream);
    const auto bytes = stream.str();
    const juce::MemoryBlock weights(bytes.data(), bytes.size());

    xml->setAttribute("architectureHash",
                      juce::String::toHexString(static_cast<juce::int64>(
                          snapshot->model->getArchitectureHash())));
    xml->setAttribute("randomizationCounter",
                      juce::String(snapshot->randomizationCounter));
    xml->setAttribute("weights", weights.toBase64Encoding());
  }

  copyXmlToBinary(*xml, destData);
}

void AuralForgeAudioProcessor::setStateInformation(const void *data,
                                                   int sizeInBytes) {
  const auto xml = getXmlFromBinary(data, sizeInBytes);
  if (xml == nullptr || !xml->hasTagName(parameters.state.getType().toString()))
    return;

  restoringState.store(true, std::memory_order_release);
  parameters.replaceState(juce::ValueTree::fromXml(*xml));
  restoringState.store(false, std::memory_order_release);

  const auto counter = static_cast<std::uint64_t>(
      xml->getStringAttribute("randomizationCounter", "0").getLargeIntValue());
  randomizationCounter.store(counter, std::memory_order_release);
  const auto seed = static_cast<std::uint64_t>(
      parameters.getRawParameterValue(auralforge::params::globalSeed)->load());
  auto snapshot = modelBuilder.buildNow(getRequestedConfiguration(),
                                        seed + counter, counter);

  if (snapshot != nullptr && xml->hasAttribute("weights")) {
    const auto expectedHash = static_cast<std::uint64_t>(
        xml->getStringAttribute("architectureHash").getHexValue64());
    juce::MemoryBlock weights;

    if (expectedHash == snapshot->model->getArchitectureHash() &&
        weights.fromBase64Encoding(xml->getStringAttribute("weights"))) {
      try {
        const std::string bytes(static_cast<const char *>(weights.getData()),
                                weights.getSize());
        std::istringstream stream(bytes, std::ios::binary);
        torch::serialize::InputArchive archive;
        archive.load_from(stream);
        snapshot->model->load(archive);
      } catch (const std::exception &) {
        // The deterministic seed/counter model remains a valid fallback.
      }
    }
  }

  publishRuntime(snapshot);
}

juce::AudioProcessorValueTreeState &
AuralForgeAudioProcessor::getParameterState() noexcept {
  return parameters;
}

auralforge::dsp::TCNConfiguration
AuralForgeAudioProcessor::getRequestedConfiguration() const noexcept {
  auralforge::dsp::TCNConfiguration configuration;
  configuration.depth = juce::roundToInt(
      parameters.getRawParameterValue(auralforge::params::depth)->load());
  configuration.kernelSize = juce::roundToInt(
      parameters.getRawParameterValue(auralforge::params::kernelSize)->load());
  configuration.channels = juce::roundToInt(
      parameters.getRawParameterValue(auralforge::params::channels)->load());
  configuration.activation =
      static_cast<auralforge::dsp::ActivationType>(juce::roundToInt(
          parameters.getRawParameterValue(auralforge::params::activation)
              ->load()));
  configuration.inputChannels = std::max(1, getTotalNumInputChannels());
  configuration.outputChannels = configuration.inputChannels;
  return configuration;
}

std::uint64_t
AuralForgeAudioProcessor::getReceptiveFieldSamples() const noexcept {
  const auto snapshot = modelBuilder.getPublishedModel();
  return snapshot != nullptr ? snapshot->model->getReceptiveField() : 0;
}

std::uint64_t
AuralForgeAudioProcessor::getModelParameterCount() const noexcept {
  const auto snapshot = modelBuilder.getPublishedModel();
  return snapshot != nullptr ? snapshot->model->getParameterCount() : 0;
}

double AuralForgeAudioProcessor::getCurrentSampleRate() const noexcept {
  return currentSampleRate.load(std::memory_order_acquire);
}

juce::String AuralForgeAudioProcessor::getModelError() const {
  const juce::ScopedLock lock(errorLock);
  return runtimeError.isNotEmpty() ? runtimeError : modelBuilder.getLastError();
}

void AuralForgeAudioProcessor::parameterChanged(const juce::String &parameterID,
                                                float newValue) {
  if (restoringState.load(std::memory_order_acquire))
    return;

  if (parameterID == auralforge::params::randomize) {
    const auto isActive = newValue >= 0.5f;
    const auto wasActive =
        lastRandomizeValue.exchange(isActive, std::memory_order_acq_rel);
    if (isActive && !wasActive)
      randomizePending.store(true, std::memory_order_release);
  } else if (parameterID == auralforge::params::depth ||
             parameterID == auralforge::params::kernelSize ||
             parameterID == auralforge::params::channels ||
             parameterID == auralforge::params::activation) {
    architectureChangePending.store(true, std::memory_order_release);
  } else {
    return;
  }

  triggerAsyncUpdate();
}

void AuralForgeAudioProcessor::handleAsyncUpdate() {
  const auto shouldRandomize =
      randomizePending.exchange(false, std::memory_order_acq_rel) ||
      midiRandomizePending.exchange(false, std::memory_order_acq_rel);
  const auto architectureChanged =
      architectureChangePending.exchange(false, std::memory_order_acq_rel);

  if (shouldRandomize)
    requestCurrentArchitecture(true);
  else if (architectureChanged)
    requestCurrentArchitecture(false);
}

void AuralForgeAudioProcessor::publishRuntime(
    const std::shared_ptr<const auralforge::dsp::ModelSnapshot> &snapshot) {
  if (snapshot == nullptr || !prepared.load(std::memory_order_acquire))
    return;

  if (auto runtime = createRuntime(snapshot)) {
    std::atomic_store_explicit(&publishedRuntime, std::move(runtime),
                               std::memory_order_release);
    const juce::ScopedLock lock(errorLock);
    runtimeError.clear();
  }
}

std::shared_ptr<AuralForgeAudioProcessor::RuntimeState>
AuralForgeAudioProcessor::createRuntime(
    const std::shared_ptr<const auralforge::dsp::ModelSnapshot> &snapshot) {
  const auto receptiveField = snapshot->model->getReceptiveField();
  if (receptiveField == 0 || receptiveField - 1 > maximumRealtimeHistory) {
    const juce::ScopedLock lock(errorLock);
    runtimeError = "Receptive field exceeds the real-time workspace limit";
    return {};
  }

  try {
    auto runtime = std::make_shared<RuntimeState>();
    runtime->snapshot = snapshot;
    runtime->maximumBlockSize =
        preparedBlockSize.load(std::memory_order_acquire);
    const auto history = static_cast<std::size_t>(receptiveField - 1);
    const auto channels = snapshot->model->getConfiguration().inputChannels;
    runtime->lookback.resize(channels, history);
    runtime->inputTensor = torch::zeros(
        {1, channels,
         static_cast<std::int64_t>(history) + runtime->maximumBlockSize},
        torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCPU));
    return runtime;
  } catch (const std::exception &error) {
    const juce::ScopedLock lock(errorLock);
    runtimeError = error.what();
  }

  return {};
}

torch::Tensor
AuralForgeAudioProcessor::runModel(RuntimeState &runtime,
                                   const juce::AudioBuffer<float> &input,
                                   int numSamples) {
  runtime.lookback.prependToTensor(runtime.inputTensor, input, numSamples);
  const auto validSamples =
      static_cast<std::int64_t>(runtime.lookback.getHistorySamples()) +
      numSamples;
  torch::InferenceMode inferenceGuard;
  return runtime.snapshot->model->forward(
      runtime.inputTensor.narrow(2, 0, validSamples));
}

void AuralForgeAudioProcessor::requestCurrentArchitecture(
    bool randomizeWeights) {
  const auto configuration = getRequestedConfiguration();
  const auto globalSeed = static_cast<std::uint64_t>(
      parameters.getRawParameterValue(auralforge::params::globalSeed)->load());

  if (randomizeWeights) {
    const auto counter =
        randomizationCounter.fetch_add(1, std::memory_order_acq_rel) + 1;
    modelBuilder.requestRandomization(configuration, globalSeed, counter);
  } else {
    const auto counter = randomizationCounter.load(std::memory_order_acquire);
    modelBuilder.requestBuild(configuration, globalSeed + counter, counter);
  }
}

void AuralForgeAudioProcessor::resetRandomizeParameter() {
  auto *parameter = parameters.getParameter(auralforge::params::randomize);
  if (parameter != nullptr && parameter->getValue() >= 0.5f) {
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(0.0f);
    parameter->endChangeGesture();
  }
  lastRandomizeValue.store(false, std::memory_order_release);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new AuralForgeAudioProcessor();
}
