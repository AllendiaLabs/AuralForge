#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParamIDs.h"
#include "params/ParamLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <sstream>

namespace {
constexpr std::array<const char *, 9> listenedParameterIDs{
    auralforge::params::depth,       auralforge::params::kernelSize,
    auralforge::params::channels,    auralforge::params::dilation,
    auralforge::params::activation,  auralforge::params::randomize,
    auralforge::params::randomizeCC, auralforge::params::globalSeed,
    auralforge::params::dryWet};

constexpr std::uint64_t maximumRealtimeHistory = 1U << 20U;
constexpr int minimumPreparedBlockSize = 8192;
constexpr int modelCrossfadeSamples = 64;

bool sameConfiguration(
    const auralforge::dsp::TCNConfiguration &left,
    const auralforge::dsp::TCNConfiguration &right) noexcept {
  return left.depth == right.depth && left.kernelSize == right.kernelSize &&
         left.channels == right.channels && left.dilation == right.dilation &&
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
  constexpr auto dcBlockerCutoffHz = 20.0;
  dcBlockerCoefficient.store(
      static_cast<float>(
          std::exp(-juce::MathConstants<double>::twoPi * dcBlockerCutoffHz /
                   std::max(1.0, sampleRate))),
      std::memory_order_release);
  preparedBlockSize.store(std::max(samplesPerBlock, minimumPreparedBlockSize),
                          std::memory_order_release);
  const auto maximumBlock = preparedBlockSize.load(std::memory_order_acquire);
  const auto outputChannels = std::max(1, getTotalNumOutputChannels());
  graphWetBuffer.setSize(outputChannels, maximumBlock, false, false, true);
  previousGraphWetBuffer.setSize(outputChannels, maximumBlock, false, false,
                                 true);
  prepared.store(true, std::memory_order_release);

  dryWetSmoother.reset(sampleRate,
                       auralforge::dsp::controlRampSecondsDefault);
  if (auto *mix = parameters.getRawParameterValue(auralforge::params::dryWet))
    dryWetSmoother.setCurrentAndTargetValue(mix->load());

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
  requestGraphCompile();
}

void AuralForgeAudioProcessor::releaseResources() {
  prepared.store(false, std::memory_order_release);
  transportPlaying.store(false, std::memory_order_release);
  std::atomic_store_explicit(&publishedRuntime, std::shared_ptr<RuntimeState>{},
                             std::memory_order_release);
  if (auto graphRuntime = graphPublisher.getPublishedRuntime())
    graphRuntime->reset();
  activeRuntime.reset();
  previousRuntime.reset();
  activeGraphRuntime.reset();
  previousGraphRuntime.reset();
  graphWetBuffer.setSize(0, 0);
  previousGraphWetBuffer.setSize(0, 0);
  graphDcInput.fill(0.0f);
  graphDcOutput.fill(0.0f);
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
  transportPlaying.store(true, std::memory_order_release);
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

  syncDryWetSmoother();

  float graphInputPeak = 0.0f;
  for (int channel = 0; channel < inputChannels; ++channel)
    graphInputPeak =
        std::max(graphInputPeak, buffer.getMagnitude(channel, 0, numSamples));
  if (graphInputPeak < 1.0e-6f) {
    if (auto graphRuntime = graphPublisher.getPublishedRuntime()) {
      graphRuntime->reset();
      if (activeGraphRuntime != nullptr)
        activeGraphRuntime->reset();
      if (previousGraphRuntime != nullptr)
        previousGraphRuntime->reset();
      buffer.clear();
      graphDcInput.fill(0.0f);
      graphDcOutput.fill(0.0f);
      dryWetSmoother.skip(numSamples);
      return;
    }
  }

  if (processLiveGraph(buffer, inputChannels, numSamples))
    return;

  graphDcInput.fill(0.0f);
  graphDcOutput.fill(0.0f);
  for (int sample = 0; sample < numSamples; ++sample) {
    const auto dryWet = dryWetSmoother.getNextValue();
    const auto dryGain = 1.0f - dryWet;
    for (int channel = 0; channel < outputChannels; ++channel)
      buffer.setSample(channel, sample,
                       buffer.getSample(channel, sample) * dryGain);
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
  {
    const juce::ScopedLock lock(graphStateLock);
    if (persistedGraphState.isValid())
      xml->addChildElement(persistedGraphState.createXml().release());
  }

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
  auto restoredState = juce::ValueTree::fromXml(*xml);
  const auto restoredGraph = restoredState.getChildWithName("GraphDocument");
  if (restoredGraph.isValid()) {
    const juce::ScopedLock lock(graphStateLock);
    persistedGraphState = restoredGraph.createCopy();
    restoredState.removeChild(restoredGraph, nullptr);
  }
  parameters.replaceState(restoredState);
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
  requestGraphCompile();
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
  configuration.dilation = juce::roundToInt(
      parameters.getRawParameterValue(auralforge::params::dilation)->load());
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
  const auto graphRuntime = graphPublisher.getPublishedRuntime();
  if (graphRuntime != nullptr)
    return graphRuntime->getSnapshot()->getReceptiveField();
  return 0;
}

std::uint64_t
AuralForgeAudioProcessor::getModelParameterCount() const noexcept {
  const auto graphRuntime = graphPublisher.getPublishedRuntime();
  if (graphRuntime != nullptr)
    return graphRuntime->getSnapshot()->getParameterCount();
  return 0;
}

double AuralForgeAudioProcessor::getFrozenInferenceTimeMilliseconds(
    std::int32_t nodeId) const noexcept {
  const auto runtime = graphPublisher.getPublishedRuntime();
  if (runtime == nullptr)
    return 0.0;
  return runtime->getFrozenInferenceTimeMilliseconds(nodeId);
}

double AuralForgeAudioProcessor::getCurrentSampleRate() const noexcept {
  return currentSampleRate.load(std::memory_order_acquire);
}

juce::String AuralForgeAudioProcessor::getModelError() const {
  const auto graphError = graphPublisher.getLastError();
  if (graphError.isNotEmpty())
    return graphError;
  const juce::ScopedLock lock(errorLock);
  return runtimeError.isNotEmpty() ? runtimeError : modelBuilder.getLastError();
}

void AuralForgeAudioProcessor::applyGraphConfiguration(
    const auralforge::dsp::TCNConfiguration &configuration) {
  if (!configuration.isValid())
    return;
  const auto update = [this](const char *identifier, float value) {
    if (auto *parameter = parameters.getParameter(identifier)) {
      parameter->beginChangeGesture();
      parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
      parameter->endChangeGesture();
    }
  };
  update(auralforge::params::depth, static_cast<float>(configuration.depth));
  update(auralforge::params::kernelSize,
         static_cast<float>(configuration.kernelSize));
  update(auralforge::params::channels,
         static_cast<float>(configuration.channels));
  update(auralforge::params::dilation,
         static_cast<float>(configuration.dilation));
  update(auralforge::params::activation,
         static_cast<float>(configuration.activation));
}

void AuralForgeAudioProcessor::randomizeGraphElement(std::int32_t nodeId,
                                                     std::int32_t seed) {
  if (const auto runtime = graphPublisher.getPublishedRuntime()) {
    const auto &statistics = runtime->getSnapshot()->getElementStatistics();
    const auto compiled = std::find_if(
        statistics.begin(), statistics.end(),
        [nodeId](const auralforge::dsp::LiveGraphElementStatistics &stats) {
          return stats.nodeId == nodeId && stats.randomizable;
        });
    if (compiled != statistics.end())
      graphPublisher.requestRandomization(nodeId, seed);
    return;
  }
  const auto unsignedSeed =
      static_cast<std::uint64_t>(static_cast<std::uint32_t>(seed));
  const auto elementSalt =
      static_cast<std::uint64_t>(static_cast<std::uint32_t>(nodeId)) << 32U;
  modelBuilder.requestBuild(
      getRequestedConfiguration(), unsignedSeed ^ elementSalt,
      randomizationCounter.load(std::memory_order_acquire));
}

juce::ValueTree AuralForgeAudioProcessor::getGraphState() const {
  const juce::ScopedLock lock(graphStateLock);
  return persistedGraphState.createCopy();
}

void AuralForgeAudioProcessor::setGraphState(const juce::ValueTree &graphState,
                                             bool compileRuntime) {
  if (!graphState.hasType("GraphDocument"))
    return;
  {
    const juce::ScopedLock lock(graphStateLock);
    persistedGraphState = graphState.createCopy();
  }
  graphRevision.fetch_add(1, std::memory_order_acq_rel);
  if (compileRuntime)
    requestGraphCompile();
}

void AuralForgeAudioProcessor::setRuntimeControls(
    const auralforge::dsp::RuntimeControlState &controls) {
  auto published =
      std::make_shared<const auralforge::dsp::RuntimeControlState>(controls);
  std::atomic_store_explicit(&publishedControls, std::move(published),
                             std::memory_order_release);
  graphRevision.fetch_add(1, std::memory_order_acq_rel);
}

std::uint64_t AuralForgeAudioProcessor::getGraphRevision() const noexcept {
  return graphRevision.load(std::memory_order_acquire);
}

bool AuralForgeAudioProcessor::isTransportPlaying() const noexcept {
  if (auto *head = getPlayHead()) {
    if (const auto position = head->getPosition())
      return position->getIsPlaying();
  }
  return transportPlaying.load(std::memory_order_acquire);
}

void AuralForgeAudioProcessor::copyLiveCapture(
    float &inputPeak, float &outputPeak, bool &suitable, float *const *input,
    int maxSamples, int &channels, int &samples) const noexcept {
  const auto index = liveCaptureIndex.load(std::memory_order_acquire);
  const auto &slot =
      liveCaptureSlots[static_cast<std::size_t>(index & 1)];
  inputPeak = slot.inputPeak;
  outputPeak = slot.outputPeak;
  suitable = slot.suitable;
  channels = slot.channels;
  samples = std::min(slot.samples, std::max(0, maxSamples));
  if (input == nullptr)
    return;
  for (int channel = 0; channel < channels; ++channel) {
    if (input[channel] == nullptr)
      continue;
    std::memcpy(input[channel], slot.input[static_cast<std::size_t>(channel)].data(),
                static_cast<std::size_t>(samples) * sizeof(float));
  }
}

bool AuralForgeAudioProcessor::getAnalysisTapPeaks(std::int32_t nodeId,
                                                   float &inputPeak,
                                                   float &outputPeak) const
    noexcept {
  auto runtime = graphPublisher.getPublishedRuntime();
  if (runtime == nullptr)
    return false;
  return runtime->getTapPeaks(nodeId, inputPeak, outputPeak);
}

bool AuralForgeAudioProcessor::prepareFrozenArtifact(
    const auralforge::graph::FreezeSelectionResult &result,
    std::string &error) {
  if (result.inputChannels < 1 || result.outputChannels < 1) {
    error = "Compiled artifact has an invalid channel signature";
    return false;
  }

  const auto factory = auralforge::dsp::TorchScriptBlackBoxFactory::load(
      result.artifactPath, result.inputChannels, result.receptiveFieldSamples,
      error);
  if (factory == nullptr ||
      factory->getOutputChannels() != result.outputChannels)
    return false;

  const auto current = std::atomic_load_explicit(&publishedFrozenArtifacts,
                                                 std::memory_order_acquire);
  auto replacement = current != nullptr
                         ? std::make_shared<FrozenArtifactRegistry>(*current)
                         : std::make_shared<FrozenArtifactRegistry>();
  replacement->artifacts[result.artifactPath] = factory;
  std::atomic_store_explicit(
      &publishedFrozenArtifacts,
      std::shared_ptr<const FrozenArtifactRegistry>(std::move(replacement)),
      std::memory_order_release);
  return true;
}

void AuralForgeAudioProcessor::releaseFrozenArtifact(
    const std::string &artifactPath) {
  const auto current = std::atomic_load_explicit(&publishedFrozenArtifacts,
                                                 std::memory_order_acquire);
  if (current == nullptr || current->artifacts.count(artifactPath) == 0)
    return;
  auto replacement = std::make_shared<FrozenArtifactRegistry>(*current);
  replacement->artifacts.erase(artifactPath);
  std::atomic_store_explicit(
      &publishedFrozenArtifacts,
      std::shared_ptr<const FrozenArtifactRegistry>(std::move(replacement)),
      std::memory_order_release);
}

bool AuralForgeAudioProcessor::hasPreparedFrozenArtifact(
    const std::string &artifactPath) const noexcept {
  const auto current = std::atomic_load_explicit(&publishedFrozenArtifacts,
                                                 std::memory_order_acquire);
  return current != nullptr && current->artifacts.count(artifactPath) != 0;
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
             parameterID == auralforge::params::dilation ||
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

bool AuralForgeAudioProcessor::processLiveGraph(
    juce::AudioBuffer<float> &buffer, int inputChannels,
    int numSamples) noexcept {
  auto latest = graphPublisher.getPublishedRuntime();
  if (latest != activeGraphRuntime) {
    previousGraphRuntime = activeGraphRuntime;
    activeGraphRuntime = std::move(latest);
    if (activeGraphRuntime != nullptr)
      activeGraphRuntime->reset();
    graphCrossfadeSamplesRemaining =
        previousGraphRuntime != nullptr && activeGraphRuntime != nullptr
            ? modelCrossfadeSamples
            : 0;
  }
  if (activeGraphRuntime == nullptr ||
      numSamples > graphWetBuffer.getNumSamples())
    return false;

  if (auto controls = std::atomic_load_explicit(&publishedControls,
                                                std::memory_order_acquire))
    activeGraphRuntime->bindControls(controls);

  LiveCaptureSlot capture;
  capture.channels = std::min(2, inputChannels);
  capture.samples = std::min(512, numSamples);
  for (int channel = 0; channel < capture.channels; ++channel) {
    const auto *source = buffer.getReadPointer(channel);
    std::memcpy(capture.input[static_cast<std::size_t>(channel)].data(),
                source, static_cast<std::size_t>(capture.samples) * sizeof(float));
    capture.inputPeak =
        std::max(capture.inputPeak,
                 buffer.getMagnitude(channel, 0, numSamples));
  }

  const auto outputChannels = getTotalNumOutputChannels();
  if (!activeGraphRuntime->processHost(buffer.getArrayOfReadPointers(),
                                       static_cast<std::size_t>(inputChannels),
                                       graphWetBuffer.getArrayOfWritePointers(),
                                       static_cast<std::size_t>(outputChannels),
                                       static_cast<std::size_t>(numSamples)))
    return false;

  auto useCrossfade =
      previousGraphRuntime != nullptr && graphCrossfadeSamplesRemaining > 0;
  if (useCrossfade && !previousGraphRuntime->processHost(
                          buffer.getArrayOfReadPointers(),
                          static_cast<std::size_t>(inputChannels),
                          previousGraphWetBuffer.getArrayOfWritePointers(),
                          static_cast<std::size_t>(outputChannels),
                          static_cast<std::size_t>(numSamples))) {
    useCrossfade = false;
    previousGraphRuntime.reset();
    graphCrossfadeSamplesRemaining = 0;
  }

  for (int sample = 0; sample < numSamples; ++sample) {
    const auto fade =
        useCrossfade && graphCrossfadeSamplesRemaining > 0
            ? 1.0f - static_cast<float>(graphCrossfadeSamplesRemaining) /
                         static_cast<float>(modelCrossfadeSamples)
            : 1.0f;
    const auto mix = dryWetSmoother.getNextValue();
    for (int channel = 0; channel < outputChannels; ++channel) {
      auto processed = graphWetBuffer.getSample(channel, sample);
      if (useCrossfade) {
        const auto previous = previousGraphWetBuffer.getSample(channel, sample);
        processed = previous + fade * (processed - previous);
      }
      const auto dry = buffer.getSample(channel, sample);
      buffer.setSample(channel, sample, dry + mix * (processed - dry));
    }
    if (graphCrossfadeSamplesRemaining > 0)
      --graphCrossfadeSamplesRemaining;
  }
  if (graphCrossfadeSamplesRemaining == 0)
    previousGraphRuntime.reset();

  applyDcBlocker(graphDcInput, graphDcOutput, buffer, outputChannels,
                 numSamples);
  capture.outputPeak = buffer.getMagnitude(0, 0, numSamples);
  capture.suitable = capture.inputPeak > 1.0e-5f;
  const auto writeIndex =
      1 - (liveCaptureIndex.load(std::memory_order_relaxed) & 1);
  liveCaptureSlots[static_cast<std::size_t>(writeIndex)] = capture;
  liveCaptureIndex.store(writeIndex, std::memory_order_release);
  return true;
}

void AuralForgeAudioProcessor::applyDcBlocker(RuntimeState &runtime,
                                              juce::AudioBuffer<float> &buffer,
                                              int channels,
                                              int samples) noexcept {
  applyDcBlocker(runtime.dcInput, runtime.dcOutput, buffer, channels, samples);
}

void AuralForgeAudioProcessor::applyDcBlocker(std::array<float, 2> &inputState,
                                              std::array<float, 2> &outputState,
                                              juce::AudioBuffer<float> &buffer,
                                              int channels,
                                              int samples) noexcept {
  const auto coefficient = dcBlockerCoefficient.load(std::memory_order_relaxed);
  const auto processedChannels =
      std::min(channels, static_cast<int>(inputState.size()));
  for (int channel = 0; channel < processedChannels; ++channel) {
    auto previousInput = inputState[static_cast<std::size_t>(channel)];
    auto previousOutput = outputState[static_cast<std::size_t>(channel)];
    auto *samplesData = buffer.getWritePointer(channel);
    for (int sample = 0; sample < samples; ++sample) {
      const auto input = samplesData[sample];
      const auto output = input - previousInput + coefficient * previousOutput;
      samplesData[sample] = output;
      previousInput = input;
      previousOutput = output;
    }
    inputState[static_cast<std::size_t>(channel)] = previousInput;
    outputState[static_cast<std::size_t>(channel)] = previousOutput;
  }
}

void AuralForgeAudioProcessor::syncDryWetSmoother() noexcept {
  if (auto *mix = parameters.getRawParameterValue(auralforge::params::dryWet))
    dryWetSmoother.setTargetValue(mix->load());
}

void AuralForgeAudioProcessor::requestGraphCompile() {
  if (!prepared.load(std::memory_order_acquire))
    return;
  const auto graphState = getGraphState();
  if (!graphState.isValid() || graphState.getNumChildren() == 0)
    return;

  auralforge::dsp::LiveGraphCompileOptions options;
  options.hostInputChannels = std::max(1, getTotalNumInputChannels());
  options.hostOutputChannels = std::max(1, getTotalNumOutputChannels());
  options.maximumBlockSize = preparedBlockSize.load(std::memory_order_acquire);
  options.maximumHistorySamples = maximumRealtimeHistory;
  options.sampleRate =
      std::max(1.0, currentSampleRate.load(std::memory_order_acquire));
  graphPublisher.requestCompile(
      graphState, options, [this](const auralforge::graph::GraphNode &node) {
        return resolveFrozenBlackBox(node);
      });
}

std::shared_ptr<const auralforge::dsp::FrozenBlackBoxFactory>
AuralForgeAudioProcessor::resolveFrozenBlackBoxForAnalysis(
    const auralforge::graph::GraphNode &node) const {
  return resolveFrozenBlackBox(node);
}

std::shared_ptr<const auralforge::dsp::FrozenBlackBoxFactory>
AuralForgeAudioProcessor::resolveFrozenBlackBox(
    const auralforge::graph::GraphNode &node) const {
  if (node.artifactPath.empty())
    return {};
  const auto registry = std::atomic_load_explicit(&publishedFrozenArtifacts,
                                                  std::memory_order_acquire);
  if (registry == nullptr)
    return {};
  const auto found = registry->artifacts.find(node.artifactPath);
  return found != registry->artifacts.end() ? found->second : nullptr;
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
