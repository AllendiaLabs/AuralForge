#include "LiveGraphEngine.h"

#include "TCNModel.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
/** @brief Immutable execution information for one topologically ordered node.
 */
struct CompiledElement {
  /** @brief Stable source node identifier. */
  std::int32_t nodeId = 0;
  /** @brief Source graph node type. */
  auralforge::graph::NodeType type = auralforge::graph::NodeType::activation;
  /** @brief Indices of upstream elements in topological order. */
  std::vector<std::int64_t> inputIndices;
  /** @brief Index of the sole upstream element, or -1 for Audio Input. */
  std::int64_t inputIndex = -1;
  /** @brief Exact input channels, or zero for Audio Input. */
  int inputChannels = 0;
  /** @brief Exact output channels, or zero for Audio Output. */
  int outputChannels = 0;
  /** @brief Kernel size for Conv1D or TCN elements. */
  int kernelSize = 1;
  /** @brief Dilation for Conv1D or base dilation for TCN elements. */
  int dilation = 1;
  /** @brief Number of internal temporal layers in a TCN element. */
  int depth = 0;
  /** @brief TCN hidden channel count. */
  int hiddenChannels = 0;
  /** @brief Element activation selection. */
  auralforge::dsp::ActivationType activation =
      auralforge::dsp::ActivationType::relu;
  /** @brief Immutable bias-free parameter tensors in execution order. */
  std::vector<torch::Tensor> weights;
  /** @brief Frozen artifact metadata and off-thread kernel constructor. */
  std::shared_ptr<const auralforge::dsp::FrozenBlackBoxFactory> blackBoxFactory;
  /** @brief Element-local receptive field in samples. */
  std::uint64_t receptiveField = 1;
  /** @brief Mutable scalar parameter count owned by this element. */
  std::uint64_t parameterCount = 0;
  /** @brief True for live weighted elements that may be randomized. */
  bool randomizable = false;
  /** @brief Merge operating mode: add, multiply, or concatenate. */
  int mergeMode = 0;
};

/** @brief Pin ownership and direction used during graph validation. */
struct PinOwner {
  /** @brief Stable owner node identifier. */
  std::int32_t nodeId = 0;
  /** @brief Declared pin direction. */
  auralforge::graph::PinKind kind = auralforge::graph::PinKind::input;
  /** @brief Declared temporal domain. */
  std::string domain;
};

/** @brief Advances a deterministic SplitMix64 generator. */
std::uint64_t splitMix64(std::uint64_t &state) noexcept {
  auto value = (state += 0x9e3779b97f4a7c15ULL);
  value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31U);
}

/** @brief Returns one deterministic float in the closed signed unit range. */
float uniformSigned(std::uint64_t &state) noexcept {
  constexpr auto inverse = 1.0 / static_cast<double>(std::uint64_t{1} << 53U);
  const auto unit = static_cast<double>(splitMix64(state) >> 11U) * inverse;
  return static_cast<float>(unit * 2.0 - 1.0);
}

/** @brief Converts a signed user seed to a stable unsigned generator state. */
std::uint64_t seedState(std::int32_t seed) noexcept {
  return static_cast<std::uint64_t>(static_cast<std::uint32_t>(seed)) ^
         0xa0761d6478bd642fULL;
}

/** @brief Saturating addition used by receptive-field accumulation. */
std::uint64_t saturatedAdd(std::uint64_t left, std::uint64_t right) noexcept {
  return right > std::numeric_limits<std::uint64_t>::max() - left
             ? std::numeric_limits<std::uint64_t>::max()
             : left + right;
}

/** @brief Saturating multiplication used by metadata calculations. */
std::uint64_t saturatedMultiply(std::uint64_t left,
                                std::uint64_t right) noexcept {
  return left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left
             ? std::numeric_limits<std::uint64_t>::max()
             : left * right;
}

/** @brief Finds an integer node property by canonical key. */
bool readProperty(const auralforge::graph::GraphNode &node, const char *key,
                  int &value) noexcept {
  const auto found =
      std::find_if(node.properties.begin(), node.properties.end(),
                   [key](const auralforge::graph::NodeProperty &property) {
                     return property.key == key;
                   });
  if (found == node.properties.end())
    return false;
  value = found->value;
  return true;
}

/** @brief Creates one deterministic bias-free convolution weight tensor. */
torch::Tensor makeWeight(int outputChannels, int inputChannels, int kernelSize,
                         std::uint64_t &state) {
  auto weight = torch::empty(
      {outputChannels, inputChannels, kernelSize},
      torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCPU));
  auto *data = weight.data_ptr<float>();
  const auto count = weight.numel();
  const auto fanIn =
      std::max<std::int64_t>(1, static_cast<std::int64_t>(inputChannels) *
                                    static_cast<std::int64_t>(kernelSize));
  const auto scale =
      static_cast<float>(std::sqrt(6.0 / static_cast<double>(fanIn)));
  for (std::int64_t index = 0; index < count; ++index)
    data[index] = uniformSigned(state) * scale;
  return weight;
}

/** @brief Rebuilds all immutable parameters for one weighted element. */
void randomizeElementWeights(CompiledElement &element, std::int32_t seed) {
  auto state = seedState(seed);
  element.weights.clear();

  switch (element.type) {
  case auralforge::graph::NodeType::linear:
    element.weights.push_back(
        makeWeight(element.outputChannels, element.inputChannels, 1, state));
    break;
  case auralforge::graph::NodeType::convolution:
    element.weights.push_back(makeWeight(element.outputChannels,
                                         element.inputChannels,
                                         element.kernelSize, state));
    break;
  case auralforge::graph::NodeType::tcn:
    element.weights.push_back(
        makeWeight(element.hiddenChannels, element.inputChannels, 1, state));
    for (int layer = 0; layer < element.depth; ++layer)
      element.weights.push_back(makeWeight(element.hiddenChannels,
                                           element.hiddenChannels,
                                           element.kernelSize, state));
    element.weights.push_back(
        makeWeight(element.outputChannels, element.hiddenChannels, 1, state));
    break;
  default:
    throw std::invalid_argument("Element does not own randomizable weights");
  }
}

/** @brief Applies a configured zero-preserving activation. */
torch::Tensor applyActivation(torch::Tensor value,
                              auralforge::dsp::ActivationType activation) {
  switch (activation) {
  case auralforge::dsp::ActivationType::relu:
    return torch::relu(value);
  case auralforge::dsp::ActivationType::sigmoid:
    return torch::where(value == 0.0, torch::zeros_like(value),
                        torch::sigmoid(value));
  case auralforge::dsp::ActivationType::tanh:
    return torch::tanh(value);
  case auralforge::dsp::ActivationType::leakyRelu:
    return torch::leaky_relu(value, 0.01);
  }
  return value;
}

/** @brief Executes one bias-free valid Conv1D operation. */
torch::Tensor convolve(const torch::Tensor &input, const torch::Tensor &weight,
                       std::int64_t dilation) {
  const std::array<std::int64_t, 1> stride{1};
  const std::array<std::int64_t, 1> padding{0};
  const std::array<std::int64_t, 1> dilationValues{dilation};
  return torch::conv1d(input, weight, std::optional<torch::Tensor>{}, stride,
                       padding, dilationValues, 1);
}

/** @brief Executes a bias-free channel projection. */
torch::Tensor project(const torch::Tensor &input, const torch::Tensor &weight) {
  return convolve(input, weight, 1);
}

/** @brief Prepends retained causal input and updates it for the next block. */
torch::Tensor extendCausalInput(const torch::Tensor &input,
                                torch::Tensor &history,
                                std::int64_t historyLength) {
  if (historyLength <= 0)
    return input;

  auto extended = torch::cat({history, input}, 2);
  history = extended.narrow(2, extended.size(2) - historyLength, historyLength)
                .clone();
  return extended;
}

/** @brief Returns whether a graph node has the required port layout. */
bool hasValidPortLayout(const auralforge::graph::GraphNode &node) noexcept {
  using auralforge::graph::NodeType;
  if (node.type == NodeType::audioInput)
    return node.inputs.empty() && node.outputs.size() == 1;
  if (node.type == NodeType::audioOutput)
    return node.inputs.size() == 1 && node.outputs.empty();
  if (auralforge::graph::isMixerType(node.type))
    return node.inputs.size() >= 2 && node.outputs.size() == 1;
  return node.inputs.size() == 1 && node.outputs.size() == 1;
}

/**
 * @brief Collects compiled upstream indices for pins that already have a source.
 * @param pins Input pins of the node or frozen-group source.
 * @param sourceNodeByDestinationPin Destination pin to source node map.
 * @param compiledIndex Compiled node identifier to element index map.
 * @return Topological indices of connected, already-compiled sources.
 */
std::vector<std::int64_t> collectCompiledInputs(
    const std::vector<auralforge::graph::Pin> &pins,
    const std::unordered_map<std::int32_t, std::int32_t>
        &sourceNodeByDestinationPin,
    const std::unordered_map<std::int32_t, std::size_t> &compiledIndex) {
  std::vector<std::int64_t> indices;
  indices.reserve(pins.size());
  for (const auto &pin : pins) {
    const auto source = sourceNodeByDestinationPin.find(pin.id);
    if (source == sourceNodeByDestinationPin.end())
      continue;
    const auto compiledSource = compiledIndex.find(source->second);
    if (compiledSource == compiledIndex.end())
      continue;
    indices.push_back(static_cast<std::int64_t>(compiledSource->second));
  }
  return indices;
}

/** @brief Creates a compile failure value. */
auralforge::dsp::LiveGraphCompileResult
failure(auralforge::dsp::LiveGraphErrorCode code, std::int32_t nodeId,
        std::string message) {
  auralforge::dsp::LiveGraphCompileResult result;
  result.error.code = code;
  result.error.nodeId = nodeId;
  result.error.message = std::move(message);
  return result;
}

/** @brief Clears every writable host output channel. */
void clearHostOutput(float *const *channels, std::size_t channelCount,
                     std::size_t sampleCount) noexcept {
  if (channels == nullptr)
    return;
  for (std::size_t channel = 0; channel < channelCount; ++channel) {
    if (channels[channel] != nullptr)
      std::fill_n(channels[channel], sampleCount, 0.0f);
  }
}
} // namespace

namespace auralforge::dsp {
/** @brief Immutable implementation backing LiveGraphSnapshot. */
struct LiveGraphSnapshot::Impl {
  /** @brief Validated host input channel count. */
  int inputChannels = 0;
  /** @brief Validated host output channel count. */
  int outputChannels = 0;
  /** @brief Largest accepted audio block. */
  std::int64_t maximumBlockSize = 0;
  /** @brief Topologically ordered immutable execution plan. */
  std::vector<CompiledElement> elements;
  /** @brief Public per-element metadata in topological order. */
  std::vector<LiveGraphElementStatistics> statistics;
  /** @brief End-to-end causal receptive field. */
  std::uint64_t receptiveField = 1;
  /** @brief Total mutable scalar parameters. */
  std::uint64_t parameterCount = 0;
};

/** @brief Mutable implementation backing LiveGraphRuntime. */
struct LiveGraphRuntime::Impl {
  /** @brief Immutable graph program retained for the runtime lifetime. */
  std::shared_ptr<const LiveGraphSnapshot> snapshot;
  /** @brief Intermediate tensors indexed by topological element index. */
  std::vector<torch::Tensor> outputs;
  /** @brief Per-element raw-input causal histories. */
  std::vector<torch::Tensor> histories;
  /** @brief Per-element frozen kernels created outside the audio callback. */
  std::vector<std::unique_ptr<FrozenBlackBoxKernel>> blackBoxKernels;
  /** @brief Reusable planar-to-tensor host input storage. */
  torch::Tensor hostInput;
  /** @brief Lock-free latest per-element inference durations in milliseconds.
   */
  std::unique_ptr<std::atomic<double>[]> inferenceMilliseconds;
};

/** @brief Constructs a snapshot from validated immutable storage. */
LiveGraphSnapshot::LiveGraphSnapshot(
    std::shared_ptr<const Impl> implementationToAdopt)
    : implementation(std::move(implementationToAdopt)) {}

/** @brief Destroys immutable snapshot storage. */
LiveGraphSnapshot::~LiveGraphSnapshot() = default;

/** @brief Returns the snapshot input channel count. */
int LiveGraphSnapshot::getInputChannels() const noexcept {
  return implementation->inputChannels;
}

/** @brief Returns the snapshot output channel count. */
int LiveGraphSnapshot::getOutputChannels() const noexcept {
  return implementation->outputChannels;
}

/** @brief Returns the prepared-runtime maximum block size. */
std::int64_t LiveGraphSnapshot::getMaximumBlockSize() const noexcept {
  return implementation->maximumBlockSize;
}

/** @brief Returns the end-to-end graph receptive field. */
std::uint64_t LiveGraphSnapshot::getReceptiveField() const noexcept {
  return implementation->receptiveField;
}

/** @brief Returns the total graph parameter count. */
std::uint64_t LiveGraphSnapshot::getParameterCount() const noexcept {
  return implementation->parameterCount;
}

/** @brief Returns immutable topological element statistics. */
const std::vector<LiveGraphElementStatistics> &
LiveGraphSnapshot::getElementStatistics() const noexcept {
  return implementation->statistics;
}

/** @brief Produces an immutable copy with one element deterministically reset.
 */
std::shared_ptr<const LiveGraphSnapshot>
LiveGraphSnapshot::withRandomizedElement(std::int32_t nodeId, std::int32_t seed,
                                         LiveGraphCompileError &error) const {
  error = {};
  try {
    auto replacement = std::make_shared<Impl>(*implementation);
    const auto target =
        std::find_if(replacement->elements.begin(), replacement->elements.end(),
                     [nodeId](const CompiledElement &element) {
                       return element.nodeId == nodeId;
                     });
    if (target == replacement->elements.end() || !target->randomizable) {
      error.code = LiveGraphErrorCode::invalidRandomization;
      error.nodeId = nodeId;
      error.message =
          "Target element is absent, frozen, or does not own live weights";
      return {};
    }

    randomizeElementWeights(*target, seed);
    return std::shared_ptr<const LiveGraphSnapshot>(
        new LiveGraphSnapshot(std::move(replacement)));
  } catch (const std::exception &exception) {
    error.code = LiveGraphErrorCode::torchFailure;
    error.nodeId = nodeId;
    error.message = exception.what();
    return {};
  }
}

/** @brief Constructs a runtime from fully prepared mutable storage. */
LiveGraphRuntime::LiveGraphRuntime(std::unique_ptr<Impl> implementationToAdopt)
    : implementation(std::move(implementationToAdopt)) {}

/** @brief Destroys runtime state and prepared frozen kernels. */
LiveGraphRuntime::~LiveGraphRuntime() = default;

/** @brief Returns the immutable snapshot paired with this runtime. */
const std::shared_ptr<const LiveGraphSnapshot> &
LiveGraphRuntime::getSnapshot() const noexcept {
  return implementation->snapshot;
}

/** @brief Executes the complete topological graph for one tensor block. */
torch::Tensor LiveGraphRuntime::processTensor(const torch::Tensor &input) {
  const auto &snapshot = *implementation->snapshot->implementation;
  if (!input.defined() || input.device().type() != torch::kCPU ||
      input.scalar_type() != torch::kFloat32 || input.dim() != 3 ||
      input.size(0) != 1 || input.size(1) != snapshot.inputChannels ||
      input.size(2) < 1 || input.size(2) > snapshot.maximumBlockSize)
    throw std::invalid_argument(
        "Live graph input must be CPU float [1, channels, valid samples]");

  torch::InferenceMode inferenceGuard;
  for (std::size_t index = 0; index < snapshot.elements.size(); ++index) {
    const auto &element = snapshot.elements[index];
    auto &output = implementation->outputs[index];

    if (element.type == graph::NodeType::audioInput) {
      output = input;
      continue;
    }
    if (element.inputIndices.empty()) {
      output = torch::zeros({1, snapshot.outputChannels, input.size(2)},
                            input.options());
      continue;
    }

    const auto gatherInputs = [&] {
      std::vector<torch::Tensor> inputs;
      inputs.reserve(element.inputIndices.size());
      for (const auto inputIndex : element.inputIndices)
        inputs.push_back(
            implementation->outputs[static_cast<std::size_t>(inputIndex)]);
      return inputs;
    };

    const auto &upstream =
        implementation->outputs[static_cast<std::size_t>(element.inputIndex)];
    switch (element.type) {
    case graph::NodeType::audioOutput:
      output = upstream;
      break;
    case graph::NodeType::linear:
      output = project(upstream, element.weights.front());
      break;
    case graph::NodeType::convolution: {
      const auto historyLength =
          static_cast<std::int64_t>(element.receptiveField - 1);
      auto extended = extendCausalInput(
          upstream, implementation->histories[index], historyLength);
      output = convolve(extended, element.weights.front(), element.dilation);
      break;
    }
    case graph::NodeType::activation:
      output = applyActivation(upstream, element.activation);
      break;
    case graph::NodeType::tcn: {
      const auto historyLength =
          static_cast<std::int64_t>(element.receptiveField - 1);
      auto value = extendCausalInput(upstream, implementation->histories[index],
                                     historyLength);
      value = project(value, element.weights.front());
      for (int layer = 0; layer < element.depth; ++layer) {
        const auto layerDilation = static_cast<std::int64_t>(element.dilation)
                                   << layer;
        const auto leftPadding =
            static_cast<std::int64_t>(element.kernelSize - 1) * layerDilation;
        value = torch::nn::functional::pad(
            value, torch::nn::functional::PadFuncOptions({leftPadding, 0})
                       .mode(torch::kConstant)
                       .value(0.0));
        value = convolve(value,
                         element.weights[static_cast<std::size_t>(layer) + 1],
                         layerDilation);
        value = applyActivation(std::move(value), element.activation);
      }
      value = project(value, element.weights.back());
      output = value.narrow(2, value.size(2) - input.size(2), input.size(2));
      break;
    }
    case graph::NodeType::merge: {
      auto inputs = gatherInputs();
      switch (element.mergeMode) {
      case 1: {
        output = torch::ones_like(inputs.front());
        for (const auto &value : inputs)
          output = output * value;
        break;
      }
      case 2:
        output = torch::cat(inputs, 1);
        break;
      default: {
        output = torch::zeros_like(inputs.front());
        for (const auto &value : inputs)
          output = output + value;
        break;
      }
      }
      break;
    }
    case graph::NodeType::blackBox: {
      const auto started = std::chrono::steady_clock::now();
      const auto historyLength =
          static_cast<std::int64_t>(element.receptiveField - 1);
      auto extended = extendCausalInput(
          upstream, implementation->histories[index], historyLength);
      output = implementation->blackBoxKernels[index]->forward(extended);
      if (!output.defined() || output.device().type() != torch::kCPU ||
          output.scalar_type() != torch::kFloat32 || output.dim() != 3 ||
          output.size(0) != 1 || output.size(1) != element.outputChannels ||
          output.size(2) != extended.size(2))
        throw std::runtime_error(
            "Frozen BlackBox returned an invalid tensor shape or type");
      output =
          output.narrow(2, output.size(2) - upstream.size(2), upstream.size(2));
      const auto elapsed = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - started);
      implementation->inferenceMilliseconds[index].store(
          elapsed.count(), std::memory_order_relaxed);
      break;
    }
    case graph::NodeType::audioInput:
      break;
    }
  }

  return implementation->outputs.back();
}

/** @brief Executes planar host audio and converts failures to silence. */
bool LiveGraphRuntime::processHost(const float *const *inputChannels,
                                   std::size_t inputChannelCount,
                                   float *const *outputChannels,
                                   std::size_t outputChannelCount,
                                   std::size_t sampleCount) noexcept {
  clearHostOutput(outputChannels, outputChannelCount, sampleCount);
  const auto &snapshot = *implementation->snapshot->implementation;
  if (inputChannels == nullptr || outputChannels == nullptr ||
      inputChannelCount != static_cast<std::size_t>(snapshot.inputChannels) ||
      outputChannelCount != static_cast<std::size_t>(snapshot.outputChannels) ||
      sampleCount == 0 ||
      sampleCount > static_cast<std::size_t>(snapshot.maximumBlockSize))
    return false;

  for (std::size_t channel = 0; channel < inputChannelCount; ++channel) {
    if (inputChannels[channel] == nullptr || outputChannels[channel] == nullptr)
      return false;
  }

  try {
    auto input = implementation->hostInput.narrow(
        2, 0, static_cast<std::int64_t>(sampleCount));
    for (std::size_t channel = 0; channel < inputChannelCount; ++channel) {
      auto plane = input[0][static_cast<std::int64_t>(channel)];
      std::memcpy(plane.data_ptr<float>(), inputChannels[channel],
                  sampleCount * sizeof(float));
    }

    auto output = processTensor(input).contiguous();
    for (std::size_t channel = 0; channel < outputChannelCount; ++channel) {
      const auto plane = output[0][static_cast<std::int64_t>(channel)];
      std::memcpy(outputChannels[channel], plane.data_ptr<float>(),
                  sampleCount * sizeof(float));
    }
    return true;
  } catch (...) {
    clearHostOutput(outputChannels, outputChannelCount, sampleCount);
    return false;
  }
}

/** @brief Reads the latest lock-free timing sample for a frozen element. */
double LiveGraphRuntime::getFrozenInferenceTimeMilliseconds(
    std::int32_t nodeId) const noexcept {
  const auto &elements = implementation->snapshot->implementation->elements;
  for (std::size_t index = 0; index < elements.size(); ++index) {
    if (elements[index].nodeId == nodeId &&
        elements[index].type == graph::NodeType::blackBox)
      return implementation->inferenceMilliseconds[index].load(
          std::memory_order_relaxed);
  }
  return 0.0;
}

/** @brief Clears all retained causal samples in place. */
void LiveGraphRuntime::reset() noexcept {
  try {
    for (auto &history : implementation->histories) {
      if (history.defined())
        history.zero_();
    }
    const auto elementCount =
        implementation->snapshot->implementation->elements.size();
    for (std::size_t index = 0; index < elementCount; ++index)
      implementation->inferenceMilliseconds[index].store(
          0.0, std::memory_order_relaxed);
  } catch (...) {
  }
}

/** @brief Compiles a validated immutable program from the editable graph. */
LiveGraphCompileResult
LiveGraphEngine::compile(const graph::NodeGraph &graphDocument,
                         const LiveGraphCompileOptions &options,
                         FrozenBlackBoxResolver blackBoxResolver) {
  using graph::MergeMode;
  using graph::NodeType;

  if ((options.hostInputChannels != 1 && options.hostInputChannels != 2) ||
      (options.hostOutputChannels != 1 && options.hostOutputChannels != 2) ||
      options.maximumBlockSize < 1 || options.maximumHistorySamples < 1)
    return failure(LiveGraphErrorCode::invalidCompileOptions, 0,
                   "Live graph supports mono/stereo hosts and positive blocks");

  const auto &nodes = graphDocument.getNodes();
  const auto &links = graphDocument.getLinks();
  if (nodes.empty())
    return failure(LiveGraphErrorCode::invalidBoundary, 0, "Graph is empty");

  std::unordered_map<std::int32_t, const graph::GraphNode *> nodesById;
  std::unordered_map<std::int32_t, PinOwner> pins;
  std::int32_t inputNodeId = 0;
  std::int32_t outputNodeId = 0;
  int inputNodeCount = 0;
  int outputNodeCount = 0;

  for (const auto &node : nodes) {
    if (node.id == 0 || !nodesById.emplace(node.id, &node).second)
      return failure(LiveGraphErrorCode::invalidGraph, node.id,
                     "Node identifiers must be unique and non-zero");
    if ((node.type == NodeType::blackBox) &&
        (node.state != graph::NodeState::frozenGold))
      return failure(LiveGraphErrorCode::invalidGraph, node.id,
                     "BlackBox elements must use the frozen execution state");
    if (node.state == graph::NodeState::frozenGold &&
        node.artifactPath.empty() && node.type != NodeType::blackBox)
      return failure(LiveGraphErrorCode::invalidGraph, node.id,
                     "Frozen elements require a compiled artifact");
    if (!hasValidPortLayout(node))
      return failure(LiveGraphErrorCode::invalidGraph, node.id,
                     "Element has an unsupported input/output port layout");
    if (node.type == NodeType::audioInput) {
      inputNodeId = node.id;
      ++inputNodeCount;
    } else if (node.type == NodeType::audioOutput) {
      outputNodeId = node.id;
      ++outputNodeCount;
    }

    const auto addPin = [&](const graph::Pin &pin, graph::PinKind kind) {
      return pin.id != 0 && pin.kind == kind && pin.shape.domain == "audio" &&
             pins.emplace(pin.id, PinOwner{node.id, kind, pin.shape.domain})
                 .second;
    };
    for (const auto &pin : node.inputs) {
      if (!addPin(pin, graph::PinKind::input))
        return failure(LiveGraphErrorCode::invalidGraph, node.id,
                       "Input pin is duplicated, malformed, or non-audio");
    }
    for (const auto &pin : node.outputs) {
      if (!addPin(pin, graph::PinKind::output))
        return failure(LiveGraphErrorCode::invalidGraph, node.id,
                       "Output pin is duplicated, malformed, or non-audio");
    }
  }

  if (inputNodeCount != 1 || outputNodeCount != 1)
    return failure(
        LiveGraphErrorCode::invalidBoundary, 0,
        "Graph must contain exactly one Audio Input and Audio Output");

  std::unordered_map<std::int32_t, std::vector<std::int32_t>> outgoing;
  std::unordered_map<std::int32_t, std::vector<std::int32_t>> incoming;
  std::unordered_map<std::int32_t, int> indegree;
  std::unordered_map<std::int32_t, std::int32_t> sourceNodeByDestinationPin;
  std::unordered_set<std::int32_t> linkIds;
  for (const auto &node : nodes)
    indegree[node.id] = 0;

  for (const auto &link : links) {
    const auto source = pins.find(link.sourcePinId);
    const auto destination = pins.find(link.destinationPinId);
    if (link.id == 0 || !linkIds.insert(link.id).second ||
        source == pins.end() || destination == pins.end() ||
        source->second.kind != graph::PinKind::output ||
        destination->second.kind != graph::PinKind::input)
      return failure(LiveGraphErrorCode::invalidGraph, 0,
                     "Link identifiers and endpoints must resolve uniquely");
    if (source->second.nodeId == destination->second.nodeId)
      return failure(LiveGraphErrorCode::cycle, source->second.nodeId,
                     "Self-connections are not permitted");
    if (!sourceNodeByDestinationPin.emplace(link.destinationPinId,
                                            source->second.nodeId)
             .second)
      return failure(LiveGraphErrorCode::invalidGraph, destination->second.nodeId,
                     "An input port cannot have more than one connection");

    outgoing[source->second.nodeId].push_back(destination->second.nodeId);
    incoming[destination->second.nodeId].push_back(source->second.nodeId);
    ++indegree[destination->second.nodeId];
  }

  for (const auto &node : nodes) {
    if (node.type == NodeType::audioInput && !incoming[node.id].empty())
      return failure(LiveGraphErrorCode::invalidBoundary, node.id,
                     "Audio Input cannot have an incoming connection");
    if (node.type == NodeType::audioOutput && !outgoing[node.id].empty())
      return failure(LiveGraphErrorCode::invalidBoundary, node.id,
                     "Audio Output cannot have an outgoing connection");
  }

  std::queue<std::int32_t> ready;
  for (const auto &node : nodes) {
    if (indegree[node.id] == 0)
      ready.push(node.id);
  }
  std::vector<std::int32_t> topologicalIds;
  while (!ready.empty()) {
    const auto nodeId = ready.front();
    ready.pop();
    topologicalIds.push_back(nodeId);
    for (const auto destination : outgoing[nodeId]) {
      if (--indegree[destination] == 0)
        ready.push(destination);
    }
  }
  if (topologicalIds.size() != nodes.size())
    return failure(LiveGraphErrorCode::cycle, 0,
                   "Graph contains a directed cycle");

  std::unordered_set<std::int32_t> fromInput;
  std::queue<std::int32_t> traversal;
  traversal.push(inputNodeId);
  while (!traversal.empty()) {
    const auto nodeId = traversal.front();
    traversal.pop();
    if (!fromInput.insert(nodeId).second)
      continue;
    for (const auto destination : outgoing[nodeId])
      traversal.push(destination);
  }

  std::unordered_set<std::int32_t> toOutput;
  traversal.push(outputNodeId);
  while (!traversal.empty()) {
    const auto nodeId = traversal.front();
    traversal.pop();
    if (!toOutput.insert(nodeId).second)
      continue;
    for (const auto source : incoming[nodeId])
      traversal.push(source);
  }
  std::unordered_set<std::int32_t> livePath;
  for (const auto &node : nodes) {
    if (fromInput.count(node.id) != 0 && toOutput.count(node.id) != 0)
      livePath.insert(node.id);
  }
  if (livePath.count(outputNodeId) == 0)
    return failure(LiveGraphErrorCode::incompletePath, outputNodeId,
                   "Graph has no complete Audio Input-to-Output path");

  struct FrozenGroup {
    std::unordered_set<std::int32_t> members;
    std::int32_t sourceId = 0;
    std::int32_t sinkId = 0;
  };
  std::vector<FrozenGroup> frozenGroups;
  std::unordered_map<std::int32_t, std::size_t> frozenGroupIndex;
  std::unordered_map<std::string, std::vector<std::int32_t>> frozenByArtifact;
  for (const auto &node : nodes) {
    if (node.state != graph::NodeState::frozenGold)
      continue;
    const auto key =
        node.artifactPath.empty()
            ? std::string("node:") + std::to_string(node.id)
            : node.artifactPath;
    frozenByArtifact[key].push_back(node.id);
  }
  for (const auto &entry : frozenByArtifact) {
    FrozenGroup group;
    group.members.insert(entry.second.begin(), entry.second.end());
    std::vector<std::int32_t> sources;
    std::vector<std::int32_t> sinks;
    for (const auto nodeId : entry.second) {
      bool hasPredecessorInGroup = false;
      bool hasSuccessorInGroup = false;
      for (const auto predecessor : incoming[nodeId]) {
        if (group.members.count(predecessor) != 0)
          hasPredecessorInGroup = true;
      }
      for (const auto successor : outgoing[nodeId]) {
        if (group.members.count(successor) != 0)
          hasSuccessorInGroup = true;
      }
      if (!hasPredecessorInGroup)
        sources.push_back(nodeId);
      if (!hasSuccessorInGroup)
        sinks.push_back(nodeId);
    }
    if (sources.size() != 1 || sinks.size() != 1)
      return failure(
          LiveGraphErrorCode::invalidGraph, sources.empty() ? 0 : sources.front(),
          "A frozen selection must keep a single input and output boundary");
    group.sourceId = sources.front();
    group.sinkId = sinks.front();
    const auto index = frozenGroups.size();
    frozenGroups.push_back(std::move(group));
    for (const auto nodeId : entry.second)
      frozenGroupIndex[nodeId] = index;
  }

  try {
    auto compiled = std::make_shared<LiveGraphSnapshot::Impl>();
    compiled->inputChannels = options.hostInputChannels;
    compiled->outputChannels = options.hostOutputChannels;
    compiled->maximumBlockSize = options.maximumBlockSize;
    compiled->elements.reserve(nodes.size());
    compiled->statistics.reserve(nodes.size());

    std::unordered_map<std::int32_t, std::size_t> compiledIndex;
    std::unordered_map<std::int32_t, std::uint64_t> pathReceptiveField;
    for (const auto nodeId : topologicalIds) {
      const auto &node = *nodesById.at(nodeId);
      if (livePath.count(nodeId) == 0)
        continue;

      const auto frozenGroup = frozenGroupIndex.find(nodeId);
      if (frozenGroup != frozenGroupIndex.end() &&
          frozenGroups[frozenGroup->second].sinkId != nodeId)
        continue;

      CompiledElement element;
      element.nodeId = node.id;
      element.type = node.type;
      const auto compileFrozenSink =
          frozenGroup != frozenGroupIndex.end() &&
          frozenGroups[frozenGroup->second].sinkId == nodeId;
      if (node.type != NodeType::audioInput) {
        const auto *inputNode = &node;
        if (compileFrozenSink)
          inputNode = nodesById.at(frozenGroups[frozenGroup->second].sourceId);
        element.inputIndices = collectCompiledInputs(
            inputNode->inputs, sourceNodeByDestinationPin, compiledIndex);
        if (compileFrozenSink && element.inputIndices.empty())
          return failure(
              LiveGraphErrorCode::invalidGraph, node.id,
              "Frozen subgraph is missing a live input connection");
        if (element.inputIndices.empty()) {
          if (node.type != NodeType::audioOutput)
            continue;
          element.inputChannels = options.hostOutputChannels;
        } else {
          element.inputIndex = element.inputIndices.front();
          element.inputChannels =
              compiled->elements[static_cast<std::size_t>(element.inputIndex)]
                  .outputChannels;
        }
      }
      if (compileFrozenSink)
        element.type = NodeType::blackBox;

      switch (compileFrozenSink ? NodeType::blackBox : node.type) {
      case NodeType::audioInput:
        element.outputChannels = options.hostInputChannels;
        break;
      case NodeType::audioOutput:
        element.outputChannels = options.hostOutputChannels;
        if (!element.inputIndices.empty() &&
            element.inputChannels != options.hostOutputChannels)
          return failure(
              LiveGraphErrorCode::invalidShape, node.id,
              "Graph output channels do not match the mono/stereo host output");
        break;
      case NodeType::linear: {
        int features = 0;
        if (!readProperty(node, "features", features) || features < 1 ||
            features > 512)
          return failure(LiveGraphErrorCode::invalidProperty, node.id,
                         "Linear requires Features in the range 1..512");
        element.outputChannels = features;
        element.randomizable = true;
        element.parameterCount = saturatedMultiply(
            static_cast<std::uint64_t>(features),
            static_cast<std::uint64_t>(element.inputChannels));
        randomizeElementWeights(element, node.seed);
        break;
      }
      case NodeType::convolution: {
        int channels = 0;
        if (!readProperty(node, "channels", channels) || channels < 1 ||
            channels > 512 ||
            !readProperty(node, "kernel_size", element.kernelSize) ||
            element.kernelSize < 2 || element.kernelSize > 65 ||
            !readProperty(node, "dilation", element.dilation) ||
            element.dilation < 1 || element.dilation > 64)
          return failure(LiveGraphErrorCode::invalidProperty, node.id,
                         "Conv1D requires Channels 1..512, Kernel Size 2..65, "
                         "and Dilation 1..64");
        element.outputChannels = channels;
        element.receptiveField =
            1 + static_cast<std::uint64_t>(element.kernelSize - 1) *
                    static_cast<std::uint64_t>(element.dilation);
        if (element.receptiveField - 1 > options.maximumHistorySamples)
          return failure(LiveGraphErrorCode::invalidProperty, node.id,
                         "Conv1D receptive field exceeds the history limit");
        element.randomizable = true;
        element.parameterCount = saturatedMultiply(
            saturatedMultiply(
                static_cast<std::uint64_t>(channels),
                static_cast<std::uint64_t>(element.inputChannels)),
            static_cast<std::uint64_t>(element.kernelSize));
        randomizeElementWeights(element, node.seed);
        break;
      }
      case NodeType::activation: {
        int activation = 0;
        if (!readProperty(node, "activation", activation) || activation < 0 ||
            activation > 3)
          return failure(LiveGraphErrorCode::invalidProperty, node.id,
                         "Activation function selection is invalid");
        element.outputChannels = element.inputChannels;
        element.activation = static_cast<ActivationType>(activation);
        break;
      }
      case NodeType::tcn: {
        int activation = 0;
        if (!readProperty(node, "depth", element.depth) ||
            !readProperty(node, "kernel_size", element.kernelSize) ||
            !readProperty(node, "channels", element.hiddenChannels) ||
            !readProperty(node, "dilation", element.dilation) ||
            !readProperty(node, "activation", activation))
          return failure(LiveGraphErrorCode::invalidProperty, node.id,
                         "TCN is missing one or more required properties");

        const auto dilationRepresentable =
            element.depth >= 1 && element.depth <= 30 &&
            element.dilation >= 1 &&
            element.dilation <=
                (std::numeric_limits<int>::max() >> (element.depth - 1));
        if (!dilationRepresentable || element.kernelSize < 2 ||
            element.kernelSize > 65 || element.hiddenChannels < 1 ||
            element.hiddenChannels > 512 || element.inputChannels < 1 ||
            element.inputChannels > 512 || activation < 0 || activation > 3)
          return failure(LiveGraphErrorCode::invalidProperty, node.id,
                         "TCN configuration is outside supported bounds");

        element.activation = static_cast<ActivationType>(activation);
        element.outputChannels = element.inputChannels;
        std::uint64_t receptiveField = 1;
        for (int layer = 0; layer < element.depth; ++layer) {
          const auto layerDilation =
              static_cast<std::uint64_t>(element.dilation) << layer;
          receptiveField = saturatedAdd(
              receptiveField, saturatedMultiply(static_cast<std::uint64_t>(
                                                    element.kernelSize - 1),
                                                layerDilation));
        }
        element.receptiveField = receptiveField;
        if (element.receptiveField - 1 > options.maximumHistorySamples)
          return failure(LiveGraphErrorCode::invalidProperty, node.id,
                         "TCN receptive field exceeds the history limit");
        const auto projectionIn = saturatedMultiply(
            static_cast<std::uint64_t>(element.inputChannels),
            static_cast<std::uint64_t>(element.hiddenChannels));
        const auto temporal = saturatedMultiply(
            saturatedMultiply(
                static_cast<std::uint64_t>(element.depth),
                saturatedMultiply(
                    static_cast<std::uint64_t>(element.hiddenChannels),
                    static_cast<std::uint64_t>(element.hiddenChannels))),
            static_cast<std::uint64_t>(element.kernelSize));
        const auto projectionOut = saturatedMultiply(
            static_cast<std::uint64_t>(element.hiddenChannels),
            static_cast<std::uint64_t>(element.outputChannels));
        element.parameterCount =
            saturatedAdd(saturatedAdd(projectionIn, temporal), projectionOut);
        element.randomizable = true;
        randomizeElementWeights(element, node.seed);
        break;
      }
      case NodeType::merge: {
        int mode = static_cast<int>(MergeMode::add);
        if (!readProperty(node, "mode", mode) || mode < 0 || mode > 2)
          return failure(LiveGraphErrorCode::invalidProperty, node.id,
                         "Merge mode must be Add, Multiply, or Concatenate");
        element.mergeMode = mode;
        if (mode == static_cast<int>(MergeMode::concatenate)) {
          int outputChannels = 0;
          for (const auto inputIndex : element.inputIndices) {
            const auto channels =
                compiled->elements[static_cast<std::size_t>(inputIndex)]
                    .outputChannels;
            if (channels < 1)
              return failure(LiveGraphErrorCode::invalidShape, node.id,
                             "Merge concatenate inputs must have positive "
                             "channel counts");
            outputChannels += channels;
          }
          if (outputChannels < 1 || outputChannels > 512)
            return failure(LiveGraphErrorCode::invalidShape, node.id,
                           "Merge concatenate output channels must stay in "
                           "1..512");
          element.outputChannels = outputChannels;
        } else {
          for (const auto inputIndex : element.inputIndices) {
            const auto channels =
                compiled->elements[static_cast<std::size_t>(inputIndex)]
                    .outputChannels;
            if (channels != element.inputChannels)
              return failure(LiveGraphErrorCode::invalidShape, node.id,
                             "Merge requires every connected input to have the "
                             "same channel count");
          }
          element.outputChannels = element.inputChannels;
        }
        break;
      }
      case NodeType::blackBox:
        if (!blackBoxResolver)
          return failure(LiveGraphErrorCode::invalidBlackBox, node.id,
                         "Frozen BlackBox requires an off-thread resolver");
        element.blackBoxFactory = blackBoxResolver(node);
        if (!element.blackBoxFactory ||
            element.blackBoxFactory->getInputChannels() !=
                element.inputChannels ||
            element.blackBoxFactory->getOutputChannels() < 1 ||
            element.blackBoxFactory->getOutputChannels() > 512 ||
            !element.blackBoxFactory->preservesSilence())
          return failure(LiveGraphErrorCode::invalidBlackBox, node.id,
                         "Frozen hook metadata is absent, shape-incompatible, "
                         "or not silence-preserving");
        element.outputChannels = element.blackBoxFactory->getOutputChannels();
        element.receptiveField = std::max<std::uint64_t>(
            1, element.blackBoxFactory->getReceptiveField());
        if (element.receptiveField - 1 > options.maximumHistorySamples)
          return failure(LiveGraphErrorCode::invalidBlackBox, node.id,
                         "Frozen receptive field exceeds the history limit");
        element.parameterCount = element.blackBoxFactory->getParameterCount();
        break;
      }

      std::uint64_t upstreamReceptiveField = 1;
      if (node.type != NodeType::audioInput) {
        const auto *rfNode = &node;
        if (compileFrozenSink)
          rfNode = nodesById.at(frozenGroups[frozenGroup->second].sourceId);
        for (const auto &pin : rfNode->inputs) {
          const auto source = sourceNodeByDestinationPin.find(pin.id);
          if (source == sourceNodeByDestinationPin.end())
            continue;
          const auto upstream = pathReceptiveField.find(source->second);
          if (upstream == pathReceptiveField.end())
            continue;
          upstreamReceptiveField =
              std::max(upstreamReceptiveField, upstream->second);
        }
      }
      pathReceptiveField[node.id] =
          saturatedAdd(upstreamReceptiveField, element.receptiveField - 1);
      compiled->parameterCount =
          saturatedAdd(compiled->parameterCount, element.parameterCount);
      compiled->statistics.push_back(
          {element.nodeId, element.type, element.inputChannels,
           element.outputChannels, element.receptiveField,
           element.parameterCount, element.randomizable});
      compiledIndex[node.id] = compiled->elements.size();
      compiled->elements.push_back(std::move(element));
    }

    compiled->receptiveField = pathReceptiveField.at(outputNodeId);
    LiveGraphCompileResult result;
    result.snapshot = std::shared_ptr<const LiveGraphSnapshot>(
        new LiveGraphSnapshot(std::move(compiled)));
    return result;
  } catch (const std::exception &exception) {
    return failure(LiveGraphErrorCode::torchFailure, 0, exception.what());
  }
}

/** @brief Prepares mutable histories, host storage, and frozen kernels. */
std::shared_ptr<LiveGraphRuntime>
LiveGraphEngine::prepare(std::shared_ptr<const LiveGraphSnapshot> snapshot,
                         LiveGraphCompileError &error) {
  error = {};
  if (!snapshot) {
    error.code = LiveGraphErrorCode::invalidGraph;
    error.message = "Cannot prepare a null live graph snapshot";
    return {};
  }

  try {
    auto runtime = std::make_unique<LiveGraphRuntime::Impl>();
    runtime->snapshot = snapshot;
    const auto &compiled = *snapshot->implementation;
    runtime->outputs.resize(compiled.elements.size());
    runtime->histories.resize(compiled.elements.size());
    runtime->blackBoxKernels.resize(compiled.elements.size());
    runtime->inferenceMilliseconds =
        std::make_unique<std::atomic<double>[]>(compiled.elements.size());
    for (std::size_t index = 0; index < compiled.elements.size(); ++index)
      runtime->inferenceMilliseconds[index].store(0.0,
                                                  std::memory_order_relaxed);
    runtime->hostInput = torch::empty(
        {1, compiled.inputChannels, compiled.maximumBlockSize},
        torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCPU));

    for (std::size_t index = 0; index < compiled.elements.size(); ++index) {
      const auto &element = compiled.elements[index];
      if ((element.type == graph::NodeType::convolution ||
           element.type == graph::NodeType::tcn ||
           element.type == graph::NodeType::blackBox) &&
          element.receptiveField > 1) {
        runtime->histories[index] = torch::zeros(
            {1, element.inputChannels,
             static_cast<std::int64_t>(element.receptiveField - 1)},
            torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCPU));
      }
      if (element.type == graph::NodeType::blackBox) {
        runtime->blackBoxKernels[index] =
            element.blackBoxFactory->createKernel();
        if (!runtime->blackBoxKernels[index]) {
          error.code = LiveGraphErrorCode::invalidBlackBox;
          error.nodeId = element.nodeId;
          error.message = "Frozen BlackBox kernel could not be prepared";
          return {};
        }
      }
    }

    return std::shared_ptr<LiveGraphRuntime>(
        new LiveGraphRuntime(std::move(runtime)));
  } catch (const std::exception &exception) {
    error.code = LiveGraphErrorCode::torchFailure;
    error.message = exception.what();
    return {};
  }
}
} // namespace auralforge::dsp
