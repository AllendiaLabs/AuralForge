#include "dsp/LiveGraphEngine.h"

#include <torch/torch.h>

#include <cmath>
#include <iostream>

namespace {
/**
 * @brief Reports a failed live-graph invariant.
 * @param condition Invariant result.
 * @param message Human-readable failure.
 * @return The supplied condition.
 */
bool expect(bool condition, const char *message) {
  if (!condition)
    std::cerr << "FAIL: " << message << '\n';
  return condition;
}

/**
 * @class TestFrozenKernel
 * @brief Stateless two-tap causal kernel used to verify runtime history.
 */
class TestFrozenKernel final : public auralforge::dsp::FrozenBlackBoxKernel {
public:
  /** @brief Applies a causal current-plus-previous-sample operation. */
  torch::Tensor forward(const torch::Tensor &input) override {
    const auto delayed =
        torch::nn::functional::pad(
            input, torch::nn::functional::PadFuncOptions({1, 0}))
            .narrow(2, 0, input.size(2));
    return input + delayed;
  }
};

/**
 * @class TestFrozenFactory
 * @brief Supplies deterministic metadata and kernels for frozen graph tests.
 */
class TestFrozenFactory final : public auralforge::dsp::FrozenBlackBoxFactory {
public:
  /** @brief Returns the stereo test input width. */
  int getInputChannels() const noexcept override { return 2; }
  /** @brief Returns the stereo test output width. */
  int getOutputChannels() const noexcept override { return 2; }
  /** @brief Returns the two-sample causal receptive field. */
  std::uint64_t getReceptiveField() const noexcept override { return 2; }
  /** @brief Reports that the test kernel owns no trainable parameters. */
  std::uint64_t getParameterCount() const noexcept override { return 0; }
  /** @brief Reports exact digital-silence preservation. */
  bool preservesSilence() const noexcept override { return true; }
  /** @brief Creates one runtime-local deterministic test kernel. */
  std::unique_ptr<auralforge::dsp::FrozenBlackBoxKernel>
  createKernel() const override {
    return std::make_unique<TestFrozenKernel>();
  }
};

/**
 * @brief Builds a valid stereo Conv1D graph for runtime tests.
 * @param firstConvolution Receives the first weighted node identifier.
 * @param secondConvolution Receives the second weighted node identifier.
 * @return Editable graph document.
 */
auralforge::graph::NodeGraph makeGraph(std::int32_t &firstConvolution,
                                       std::int32_t &secondConvolution) {
  using namespace auralforge::graph;
  NodeGraph graph;
  const auto input = graph.addNode(NodeType::audioInput, {0.0f, 0.0f});
  firstConvolution = graph.addNode(NodeType::convolution, {180.0f, 0.0f});
  secondConvolution = graph.addNode(NodeType::convolution, {360.0f, 0.0f});
  const auto output = graph.addNode(NodeType::audioOutput, {540.0f, 0.0f});
  graph.setProperty(firstConvolution, "channels", 2);
  graph.setProperty(secondConvolution, "channels", 2);
  graph.setSeed(firstConvolution, 42);
  graph.setSeed(secondConvolution, 91);

  const auto *inputNode = graph.findNode(input);
  const auto *firstNode = graph.findNode(firstConvolution);
  const auto *secondNode = graph.findNode(secondConvolution);
  const auto *outputNode = graph.findNode(output);
  if (inputNode != nullptr && firstNode != nullptr && secondNode != nullptr &&
      outputNode != nullptr) {
    graph.connect(inputNode->outputs.front().id, firstNode->inputs.front().id);
    graph.connect(firstNode->outputs.front().id, secondNode->inputs.front().id);
    graph.connect(secondNode->outputs.front().id,
                  outputNode->inputs.front().id);
  }
  return graph;
}
} // namespace

/**
 * @brief Runs immutable live-graph compilation and randomization checks.
 * @return Zero when every invariant passes.
 */
int main() {
  using namespace auralforge::dsp;
  std::int32_t firstConvolution = 0;
  std::int32_t secondConvolution = 0;
  const auto graph = makeGraph(firstConvolution, secondConvolution);

  LiveGraphCompileOptions options;
  options.hostInputChannels = 2;
  options.hostOutputChannels = 2;
  options.maximumBlockSize = 256;
  const auto compiled = LiveGraphEngine::compile(graph, options);
  bool passed = true;
  passed &= expect(compiled.succeeded(),
                   "valid stereo graph must compile to an immutable snapshot");
  if (!compiled.succeeded())
    return 1;
  passed &= expect(compiled.snapshot->getElementStatistics().size() == 4,
                   "compiled graph must retain every topological element");

  LiveGraphCompileError error;
  const auto runtime = LiveGraphEngine::prepare(compiled.snapshot, error);
  passed &= expect(runtime != nullptr && !error.hasError(),
                   "compiled graph must prepare outside the audio callback");
  if (runtime == nullptr)
    return 1;

  const auto silence = torch::zeros({1, 2, 128}, torch::kFloat32);
  const auto silentOutput = runtime->processTensor(silence);
  passed &= expect(silentOutput.abs().max().item<float>() == 0.0f,
                   "bias-free live graph must preserve digital silence");

  auralforge::graph::NodeGraph linearGraph;
  const auto linearInput = linearGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto linearNode =
      linearGraph.addNode(auralforge::graph::NodeType::linear, {180.0f, 0.0f});
  const auto linearOutput = linearGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {360.0f, 0.0f});
  linearGraph.setSeed(linearNode, 42);
  linearGraph.connect(linearGraph.findNode(linearInput)->outputs.front().id,
                      linearGraph.findNode(linearNode)->inputs.front().id);
  linearGraph.connect(linearGraph.findNode(linearNode)->outputs.front().id,
                      linearGraph.findNode(linearOutput)->inputs.front().id);
  const auto linearCompiled = LiveGraphEngine::compile(linearGraph, options);
  const auto linearRuntime =
      LiveGraphEngine::prepare(linearCompiled.snapshot, error);
  passed &= expect(linearRuntime != nullptr,
                   "linear fixture must prepare");
  if (linearRuntime == nullptr)
    return 1;
  auto basis = torch::zeros({1, 2, 1}, torch::kFloat32);
  basis[0][0][0] = 1.0f;
  const auto linearResult = linearRuntime->processTensor(basis);
  passed &= expect(linearResult.defined() && linearResult.abs().max().item<float>() > 0.0f,
                   "linear live weights must produce a non-silent output");

  const auto repeated =
      compiled.snapshot->withRandomizedElement(firstConvolution, 42, error);
  passed &= expect(repeated != nullptr && !error.hasError(),
                   "same signed seed must rebuild one weighted element");
  const auto repeatedRuntime = LiveGraphEngine::prepare(repeated, error);
  passed &= expect(repeatedRuntime != nullptr && !error.hasError(),
                   "same-seed snapshot must prepare successfully");
  if (repeatedRuntime == nullptr)
    return 1;
  const auto input = torch::randn({1, 2, 128}, torch::kFloat32);
  const auto originalOutput = runtime->processTensor(input);
  const auto repeatedOutput = repeatedRuntime->processTensor(input);
  passed &= expect(torch::equal(originalOutput, repeatedOutput),
                   "reapplying an element's seed must reproduce graph output");

  const auto changed =
      compiled.snapshot->withRandomizedElement(firstConvolution, -7, error);
  const auto changedRuntime = LiveGraphEngine::prepare(changed, error);
  passed &= expect(changedRuntime != nullptr && !error.hasError(),
                   "different-seed snapshot must prepare successfully");
  if (changedRuntime == nullptr)
    return 1;
  const auto changedOutput = changedRuntime->processTensor(input);
  passed &= expect(!torch::equal(originalOutput, changedOutput),
                   "a different signed seed must change the target element");

  auralforge::graph::NodeGraph frozenGraph;
  const auto frozenInput = frozenGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto frozenNode = frozenGraph.addNode(
      auralforge::graph::NodeType::blackBox, {180.0f, 0.0f});
  const auto frozenOutput = frozenGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {360.0f, 0.0f});
  frozenGraph.findNode(frozenNode)->artifactPath = "test-frozen-artifact";
  frozenGraph.connect(frozenGraph.findNode(frozenInput)->outputs.front().id,
                      frozenGraph.findNode(frozenNode)->inputs.front().id);
  frozenGraph.connect(frozenGraph.findNode(frozenNode)->outputs.front().id,
                      frozenGraph.findNode(frozenOutput)->inputs.front().id);
  const auto factory = std::make_shared<TestFrozenFactory>();
  const auto frozenCompiled = LiveGraphEngine::compile(
      frozenGraph, options,
      [factory](const auralforge::graph::GraphNode &) { return factory; });
  passed &= expect(frozenCompiled.succeeded(),
                   "valid frozen graph must compile with prepared metadata");
  const auto wholeRuntime =
      LiveGraphEngine::prepare(frozenCompiled.snapshot, error);
  const auto splitRuntime =
      LiveGraphEngine::prepare(frozenCompiled.snapshot, error);
  passed &= expect(wholeRuntime != nullptr && splitRuntime != nullptr,
                   "frozen graph must prepare independent runtime histories");
  if (wholeRuntime == nullptr || splitRuntime == nullptr)
    return 1;
  const auto frozenInputTensor = torch::randn({1, 2, 128}, torch::kFloat32);
  const auto wholeFrozenOutput = wholeRuntime->processTensor(frozenInputTensor);
  const auto firstHalf =
      splitRuntime->processTensor(frozenInputTensor.narrow(2, 0, 64));
  const auto secondHalf =
      splitRuntime->processTensor(frozenInputTensor.narrow(2, 64, 64));
  const auto splitFrozenOutput = torch::cat({firstHalf, secondHalf}, 2);
  passed &= expect(
      torch::equal(wholeFrozenOutput, splitFrozenOutput),
      "frozen causal history must produce block-size-independent output");
  passed &= expect(
      splitRuntime->getFrozenInferenceTimeMilliseconds(frozenNode) > 0.0,
      "frozen runtime must publish a live per-buffer inference duration");

  const auto invalidRandomization =
      compiled.snapshot->withRandomizedElement(999999, 1, error);
  passed &= expect(invalidRandomization == nullptr &&
                       error.code == LiveGraphErrorCode::invalidRandomization,
                   "unknown element randomization must fail without mutation");

  auralforge::graph::NodeGraph mixerGraph;
  const auto mixerInput = mixerGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto leftActivation = mixerGraph.addNode(
      auralforge::graph::NodeType::activation, {180.0f, -40.0f});
  const auto rightActivation = mixerGraph.addNode(
      auralforge::graph::NodeType::activation, {180.0f, 80.0f});
  const auto mergeNode =
      mixerGraph.addNode(auralforge::graph::NodeType::merge, {360.0f, 20.0f});
  const auto mixerOutput = mixerGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {540.0f, 20.0f});
  mixerGraph.connect(mixerGraph.findNode(mixerInput)->outputs.front().id,
                     mixerGraph.findNode(leftActivation)->inputs.front().id);
  mixerGraph.connect(mixerGraph.findNode(mixerInput)->outputs.front().id,
                     mixerGraph.findNode(rightActivation)->inputs.front().id);
  mixerGraph.connect(mixerGraph.findNode(leftActivation)->outputs.front().id,
                     mixerGraph.findNode(mergeNode)->inputs[0].id);
  mixerGraph.connect(mixerGraph.findNode(rightActivation)->outputs.front().id,
                     mixerGraph.findNode(mergeNode)->inputs[1].id);
  mixerGraph.connect(mixerGraph.findNode(mergeNode)->outputs.front().id,
                     mixerGraph.findNode(mixerOutput)->inputs.front().id);
  const auto mixerCompiled = LiveGraphEngine::compile(mixerGraph, options);
  passed &= expect(mixerCompiled.succeeded(),
                   "elementwise merge add of two stereo paths must compile");
  const auto mixerRuntime =
      LiveGraphEngine::prepare(mixerCompiled.snapshot, error);
  passed &= expect(mixerRuntime != nullptr,
                   "elementwise merge add graph must prepare");
  if (mixerRuntime != nullptr) {
    auto ones = torch::ones({1, 2, 8}, torch::kFloat32);
    const auto merged = mixerRuntime->processTensor(ones);
    passed &= expect(std::abs(merged[0][0][0].item<float>() - 2.0f) < 1.0e-6f,
                     "ReLU merge add of two unit paths must double the input");
  }

  auralforge::graph::NodeGraph multiplyMergeGraph;
  const auto multiplyMergeInput = multiplyMergeGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto leftPath = multiplyMergeGraph.addNode(
      auralforge::graph::NodeType::activation, {180.0f, -40.0f});
  const auto rightPath = multiplyMergeGraph.addNode(
      auralforge::graph::NodeType::activation, {180.0f, 80.0f});
  const auto multiplyMergeNode = multiplyMergeGraph.addNode(
      auralforge::graph::NodeType::merge, {360.0f, 20.0f});
  const auto multiplyMergeOutput = multiplyMergeGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {540.0f, 20.0f});
  multiplyMergeGraph.setProperty(multiplyMergeNode, "mode", 1);
  multiplyMergeGraph.connect(
      multiplyMergeGraph.findNode(multiplyMergeInput)->outputs.front().id,
      multiplyMergeGraph.findNode(leftPath)->inputs.front().id);
  multiplyMergeGraph.connect(
      multiplyMergeGraph.findNode(multiplyMergeInput)->outputs.front().id,
      multiplyMergeGraph.findNode(rightPath)->inputs.front().id);
  multiplyMergeGraph.connect(
      multiplyMergeGraph.findNode(leftPath)->outputs.front().id,
      multiplyMergeGraph.findNode(multiplyMergeNode)->inputs[0].id);
  multiplyMergeGraph.connect(
      multiplyMergeGraph.findNode(rightPath)->outputs.front().id,
      multiplyMergeGraph.findNode(multiplyMergeNode)->inputs[1].id);
  multiplyMergeGraph.connect(
      multiplyMergeGraph.findNode(multiplyMergeNode)->outputs.front().id,
      multiplyMergeGraph.findNode(multiplyMergeOutput)->inputs.front().id);
  const auto multiplyMergeCompiled =
      LiveGraphEngine::compile(multiplyMergeGraph, options);
  passed &= expect(multiplyMergeCompiled.succeeded(),
                   "elementwise merge multiply of two stereo paths must compile");
  const auto multiplyMergeRuntime =
      LiveGraphEngine::prepare(multiplyMergeCompiled.snapshot, error);
  passed &= expect(multiplyMergeRuntime != nullptr,
                   "elementwise merge multiply graph must prepare");
  if (multiplyMergeRuntime != nullptr) {
    auto ones = torch::ones({1, 2, 8}, torch::kFloat32);
    const auto product = multiplyMergeRuntime->processTensor(ones);
    passed &= expect(std::abs(product[0][0][0].item<float>() - 1.0f) < 1.0e-6f,
                     "ReLU merge multiply of two unit paths must stay unity");
  }

  auralforge::graph::NodeGraph concatGraph;
  const auto concatInput = concatGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto concatNode = concatGraph.addNode(
      auralforge::graph::NodeType::merge, {180.0f, 0.0f});
  const auto concatOutput = concatGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {360.0f, 0.0f});
  concatGraph.setProperty(concatNode, "mode", 2);
  concatGraph.setProperty(concatNode, "inputs", 2);
  concatGraph.connect(concatGraph.findNode(concatInput)->outputs.front().id,
                      concatGraph.findNode(concatNode)->inputs[0].id);
  concatGraph.connect(concatGraph.findNode(concatNode)->outputs.front().id,
                      concatGraph.findNode(concatOutput)->inputs.front().id);
  const auto concatCompiled = LiveGraphEngine::compile(concatGraph, options);
  passed &= expect(concatCompiled.succeeded(),
                   "a mixer with unused inputs must still compile");
  const auto concatRuntime =
      LiveGraphEngine::prepare(concatCompiled.snapshot, error);
  passed &= expect(concatRuntime != nullptr,
                   "a mixer with unused inputs must prepare");
  if (concatRuntime != nullptr) {
    auto ones = torch::ones({1, 2, 8}, torch::kFloat32);
    const auto concatenated = concatRuntime->processTensor(ones);
    passed &= expect(concatenated.size(1) == 2 &&
                         std::abs(concatenated[0][0][0].item<float>() - 1.0f) <
                             1.0e-6f,
                     "merge concatenate must omit unused inputs without extra channels");
  }

  auralforge::graph::NodeGraph addMergeGraph;
  const auto unusedAddInput = addMergeGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto unusedAddNode =
      addMergeGraph.addNode(auralforge::graph::NodeType::merge, {180.0f, 0.0f});
  const auto unusedAddOutput = addMergeGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {360.0f, 0.0f});
  addMergeGraph.connect(addMergeGraph.findNode(unusedAddInput)->outputs.front().id,
                        addMergeGraph.findNode(unusedAddNode)->inputs[0].id);
  addMergeGraph.connect(addMergeGraph.findNode(unusedAddNode)->outputs.front().id,
                        addMergeGraph.findNode(unusedAddOutput)->inputs.front().id);
  const auto addCompiled = LiveGraphEngine::compile(addMergeGraph, options);
  const auto addRuntime = LiveGraphEngine::prepare(addCompiled.snapshot, error);
  passed &= expect(addCompiled.succeeded() && addRuntime != nullptr,
                   "merge add with one connected input must compile");
  if (addRuntime != nullptr) {
    auto ones = torch::ones({1, 2, 8}, torch::kFloat32);
    const auto merged = addRuntime->processTensor(ones);
    passed &= expect(std::abs(merged[0][0][0].item<float>() - 1.0f) < 1.0e-6f,
                     "an unused merge add input must contribute zeros");
  }

  auralforge::graph::NodeGraph multiplyGraph;
  const auto multiplyInput = multiplyGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto multiplyNode = multiplyGraph.addNode(
      auralforge::graph::NodeType::merge, {180.0f, 0.0f});
  const auto multiplyOutput = multiplyGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {360.0f, 0.0f});
  multiplyGraph.setProperty(multiplyNode, "mode", 1);
  multiplyGraph.connect(multiplyGraph.findNode(multiplyInput)->outputs.front().id,
                        multiplyGraph.findNode(multiplyNode)->inputs[0].id);
  multiplyGraph.connect(multiplyGraph.findNode(multiplyNode)->outputs.front().id,
                        multiplyGraph.findNode(multiplyOutput)->inputs.front().id);
  const auto multiplyCompiled = LiveGraphEngine::compile(multiplyGraph, options);
  const auto multiplyRuntime =
      LiveGraphEngine::prepare(multiplyCompiled.snapshot, error);
  passed &= expect(multiplyCompiled.succeeded() && multiplyRuntime != nullptr,
                   "merge multiply with one connected input must compile");
  if (multiplyRuntime != nullptr) {
    auto ones = torch::ones({1, 2, 8}, torch::kFloat32);
    const auto product = multiplyRuntime->processTensor(ones);
    passed &= expect(std::abs(product[0][0][0].item<float>() - 1.0f) < 1.0e-6f,
                     "an unused merge multiply input must contribute ones");
  }

  auralforge::graph::NodeGraph orphanGraph;
  const auto orphanInput = orphanGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto orphanOutput = orphanGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {180.0f, 0.0f});
  orphanGraph.addNode(auralforge::graph::NodeType::convolution, {90.0f, 80.0f});
  orphanGraph.addNode(auralforge::graph::NodeType::tcn, {90.0f, -80.0f});
  orphanGraph.connect(orphanGraph.findNode(orphanInput)->outputs.front().id,
                      orphanGraph.findNode(orphanOutput)->inputs.front().id);
  const auto orphanCompiled = LiveGraphEngine::compile(orphanGraph, options);
  passed &= expect(orphanCompiled.succeeded(),
                   "unwired Conv1D and TCN nodes must not fail compilation");
  passed &= expect(orphanCompiled.snapshot->getElementStatistics().size() == 2,
                   "unwired processing nodes must be omitted from the live path");
  const auto orphanRuntime =
      LiveGraphEngine::prepare(orphanCompiled.snapshot, error);
  passed &= expect(orphanRuntime != nullptr,
                   "a graph with unwired Conv1D and TCN must prepare");
  if (orphanRuntime != nullptr) {
    auto ones = torch::ones({1, 2, 8}, torch::kFloat32);
    const auto passedThrough = orphanRuntime->processTensor(ones);
    passed &= expect(torch::equal(passedThrough, ones),
                     "unwired Conv1D and TCN must leave the I/O path unchanged");
  }

  auralforge::graph::NodeGraph openGraph;
  openGraph.addNode(auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  openGraph.addNode(auralforge::graph::NodeType::audioOutput, {180.0f, 0.0f});
  const auto openCompiled = LiveGraphEngine::compile(openGraph, options);
  passed &= expect(!openCompiled.succeeded() &&
                       openCompiled.error.code == LiveGraphErrorCode::incompletePath,
                   "disconnected stereo I/O must stay idle without a live runtime");

  auralforge::graph::NodeGraph activationGraph;
  const auto activationInput = activationGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto activationNode = activationGraph.addNode(
      auralforge::graph::NodeType::activation, {180.0f, 0.0f});
  const auto activationOutput = activationGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {360.0f, 0.0f});
  activationGraph.connect(
      activationGraph.findNode(activationInput)->outputs.front().id,
      activationGraph.findNode(activationNode)->inputs.front().id);
  activationGraph.connect(
      activationGraph.findNode(activationNode)->outputs.front().id,
      activationGraph.findNode(activationOutput)->inputs.front().id);
  passed &= expect(activationGraph.setFloatProperty(activationNode, "gain", 4.0f),
                   "Activation Gain must persist as a real property");
  const auto activationCompiled =
      LiveGraphEngine::compile(activationGraph, options);
  passed &= expect(activationCompiled.succeeded(),
                   "Activation graph with Gain must compile");

  auralforge::dsp::AnalysisRequest analysisRequest;
  analysisRequest.nodeId = activationNode;
  analysisRequest.view = auralforge::graph::AnalysisView::transfer;
  analysisRequest.revision = 1;
  const auto transfer = LiveGraphEngine::analyse(activationGraph,
                                                 analysisRequest, options);
  passed &= expect(transfer.channelCount >= 2 &&
                       transfer.chainSeries.size() ==
                           static_cast<std::size_t>(transfer.channelCount) &&
                       transfer.elementOnlySeries.size() ==
                           static_cast<std::size_t>(transfer.channelCount),
                   "analysis must return dual N-channel curve families");
  analysisRequest.view = auralforge::graph::AnalysisView::frequency;
  const auto frequency = LiveGraphEngine::analyse(activationGraph,
                                                  analysisRequest, options);
  passed &= expect(frequency.sourceMode == AnalysisSourceMode::probe &&
                       !frequency.chainSeries.empty() &&
                       !frequency.chainSeries.front().x.empty(),
                   "frequency analysis must fall back to a probe when silent");
  analysisRequest.view = auralforge::graph::AnalysisView::oscilloscope;
  analysisRequest.sampleRate = 44100.0;
  const auto oscilloscope = LiveGraphEngine::analyse(activationGraph,
                                                     analysisRequest, options);
  passed &= expect(oscilloscope.view ==
                           auralforge::graph::AnalysisView::oscilloscope &&
                       oscilloscope.sourceMode == AnalysisSourceMode::probe &&
                       oscilloscope.elementOnlySeries.size() >= 2 &&
                       oscilloscope.elementOnlySeries.front().x.size() >= 2 &&
                       oscilloscope.elementOnlySeries.front().y.size() >= 2,
                   "oscilloscope analysis must return element waveforms");

  auralforge::graph::NodeGraph knobGraph;
  const auto knobInput = knobGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto knob = knobGraph.addNode(auralforge::graph::NodeType::knobInput,
                                      {0.0f, 80.0f});
  const auto knobMerge =
      knobGraph.addNode(auralforge::graph::NodeType::merge, {180.0f, 20.0f});
  const auto knobTcn =
      knobGraph.addNode(auralforge::graph::NodeType::tcn, {360.0f, 20.0f});
  const auto knobOutput = knobGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {540.0f, 20.0f});
  knobGraph.setProperty(knobTcn, "channels", 2);
  knobGraph.setConditioningValue(knob, 1.5f);
  passed &= expect(
      knobGraph
          .connect(knobGraph.findNode(knobInput)->outputs.front().id,
                   knobGraph.findNode(knobMerge)->inputs[0].id)
          .accepted,
      "Audio In must connect to Merge");
  passed &= expect(
      knobGraph
          .connect(knobGraph.findNode(knob)->outputs.front().id,
                   knobGraph.findNode(knobMerge)->inputs[1].id)
          .accepted,
      "Knob Input must connect to Merge");
  passed &= expect(
      knobGraph
          .connect(knobGraph.findNode(knobMerge)->outputs.front().id,
                   knobGraph.findNode(knobTcn)->inputs.front().id)
          .accepted,
      "Merge must connect to TCN");
  passed &= expect(
      knobGraph
          .connect(knobGraph.findNode(knobTcn)->outputs.front().id,
                   knobGraph.findNode(knobOutput)->inputs.front().id)
          .accepted,
      "TCN must connect to Audio Out");
  const auto knobCompiled = LiveGraphEngine::compile(knobGraph, options);
  passed &= expect(knobCompiled.succeeded(),
                   "Knob-through-Merge graph must compile");
  LiveGraphCompileError knobError;
  const auto knobRuntime =
      LiveGraphEngine::prepare(knobCompiled.snapshot, knobError);
  passed &= expect(knobRuntime != nullptr, "Knob graph must prepare");
  if (knobRuntime != nullptr) {
    auto controls = collectRuntimeControlState(knobGraph);
    knobRuntime->bindControls(
        std::make_shared<const RuntimeControlState>(controls));
    auto ones = torch::ones({1, 2, 8}, torch::kFloat32);
    const auto withKnob = knobRuntime->processTensor(ones);
    passed &= expect(withKnob.defined() && withKnob.size(1) == 2,
                     "Knob-conditioned graph must produce stereo output");
    auto zeros = torch::zeros({1, 2, 64}, torch::kFloat32);
    auto blockOnes = torch::ones({1, 2, 64}, torch::kFloat32);
    knobRuntime->reset();
    const auto isolatedZeros =
        knobRuntime->processIsolated(knobMerge, zeros);
    knobRuntime->reset();
    const auto isolatedOnes = knobRuntime->processIsolated(knobMerge, blockOnes);
    passed &= expect(isolatedZeros.defined() && isolatedOnes.defined(),
                     "Merge isolation must produce output");
    passed &= expect(
        std::abs(isolatedZeros[0][0][0].item<float>() - 1.5f) < 1.0e-4f &&
            std::abs(isolatedOnes[0][0][0].item<float>() - 2.5f) < 1.0e-4f,
        "isolated Merge must keep conditioning inputs instead of the probe");
  }
  analysisRequest.nodeId = knob;
  analysisRequest.liveInputSuitable = true;
  analysisRequest.liveInputChannels = 2;
  analysisRequest.liveInputSamples = 64;
  std::array<float, 128> fakeLive{};
  for (std::size_t index = 0; index < fakeLive.size(); ++index)
    fakeLive[index] = std::sin(static_cast<float>(index) * 0.11f);
  analysisRequest.liveInput = fakeLive.data();
  analysisRequest.view = auralforge::graph::AnalysisView::oscilloscope;
  const auto knobOscilloscope = LiveGraphEngine::analyse(knobGraph, analysisRequest,
                                                         options);
  passed &= expect(
      knobOscilloscope.sourceMode == AnalysisSourceMode::probe &&
          knobOscilloscope.elementOnlySeries.size() >= 1 &&
          !knobOscilloscope.elementOnlySeries.front().y.empty() &&
          std::abs(knobOscilloscope.elementOnlySeries.front().y.front() - 1.5f) <
              1.0e-3f,
      "conditioning-source oscilloscope must ignore live audio capture");

  const auto restoredTree = knobGraph.toValueTree();
  auralforge::graph::NodeGraph restoredKnob;
  passed &= expect(restoredKnob.restoreFromValueTree(restoredTree),
                   "Knob graph must serialize");
  const auto *restoredKnobNode = restoredKnob.findNode(knob);
  passed &= expect(restoredKnobNode != nullptr &&
                       std::abs(restoredKnobNode->conditioningValue - 1.5f) <
                           1.0e-4f,
                   "Knob conditioning value must survive ValueTree recall");

  auralforge::graph::NodeGraph xyVolumeGraph;
  const auto xyVolumeInput = xyVolumeGraph.addNode(
      auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto xyPad = xyVolumeGraph.addNode(
      auralforge::graph::NodeType::xyTrackpad, {0.0f, 80.0f});
  const auto xyConcat =
      xyVolumeGraph.addNode(auralforge::graph::NodeType::merge, {180.0f, 80.0f});
  const auto xyMultiply =
      xyVolumeGraph.addNode(auralforge::graph::NodeType::merge, {360.0f, 20.0f});
  const auto xyVolumeOutput = xyVolumeGraph.addNode(
      auralforge::graph::NodeType::audioOutput, {540.0f, 20.0f});
  xyVolumeGraph.setProperty(xyConcat, "mode", 2);
  xyVolumeGraph.setProperty(xyMultiply, "mode", 1);
  xyVolumeGraph.setConditioningPad(xyPad, 0.5f, 2.0f);
  const auto *xyPadNode = xyVolumeGraph.findNode(xyPad);
  const auto *xyConcatNode = xyVolumeGraph.findNode(xyConcat);
  const auto *xyMultiplyNode = xyVolumeGraph.findNode(xyMultiply);
  passed &= expect(
      xyPadNode != nullptr && xyConcatNode != nullptr &&
          xyMultiplyNode != nullptr &&
          xyVolumeGraph
              .connect(xyPadNode->outputs[0].id, xyConcatNode->inputs[0].id)
              .accepted &&
          xyVolumeGraph
              .connect(xyPadNode->outputs[1].id, xyConcatNode->inputs[1].id)
              .accepted,
      "XY X and Y must concatenate");
  passed &= expect(
      xyVolumeGraph
          .connect(xyVolumeGraph.findNode(xyVolumeInput)->outputs.front().id,
                   xyMultiplyNode->inputs[0].id)
          .accepted &&
          xyVolumeGraph
              .connect(xyConcatNode->outputs.front().id,
                       xyMultiplyNode->inputs[1].id)
              .accepted &&
          xyVolumeGraph
              .connect(xyMultiplyNode->outputs.front().id,
                       xyVolumeGraph.findNode(xyVolumeOutput)->inputs.front().id)
              .accepted,
      "concatenated XY must multiply with audio");
  const auto xyVolumeCompiled = LiveGraphEngine::compile(xyVolumeGraph, options);
  passed &= expect(xyVolumeCompiled.succeeded(),
                   "XY concatenate-then-multiply graph must compile");
  LiveGraphCompileError xyVolumeError;
  const auto xyVolumeRuntime =
      LiveGraphEngine::prepare(xyVolumeCompiled.snapshot, xyVolumeError);
  passed &= expect(xyVolumeRuntime != nullptr,
                   "XY concatenate-then-multiply graph must prepare");
  if (xyVolumeRuntime != nullptr) {
    xyVolumeRuntime->bindControls(
        std::make_shared<const RuntimeControlState>(
            collectRuntimeControlState(xyVolumeGraph)));
    auto ones = torch::ones({1, 2, 8}, torch::kFloat32);
    const auto scaled = xyVolumeRuntime->processTensor(ones);
    passed &= expect(scaled.defined() && scaled.size(1) == 2 &&
                         std::abs(scaled[0][0][0].item<float>() - 0.5f) <
                             1.0e-5f &&
                         std::abs(scaled[0][1][0].item<float>() - 2.0f) <
                             1.0e-5f,
                     "concatenated XY must scale stereo audio per channel");
  }

  auralforge::graph::NodeGraph rampGraph;
  const auto rampInput =
      rampGraph.addNode(auralforge::graph::NodeType::audioInput, {0.0f, 0.0f});
  const auto rampKnob =
      rampGraph.addNode(auralforge::graph::NodeType::knobInput, {0.0f, 80.0f});
  const auto rampActivation = rampGraph.addNode(
      auralforge::graph::NodeType::activation, {180.0f, -40.0f});
  const auto rampXy =
      rampGraph.addNode(auralforge::graph::NodeType::xyTrackpad, {0.0f, 160.0f});
  const auto rampMerge =
      rampGraph.addNode(auralforge::graph::NodeType::merge, {360.0f, 20.0f});
  const auto rampOutput =
      rampGraph.addNode(auralforge::graph::NodeType::audioOutput, {540.0f, 20.0f});
  rampGraph.setProperty(rampMerge, "mode", 1);
  rampGraph.setProperty(rampMerge, "inputs", 3);
  passed &= expect(
      rampGraph
          .connect(rampGraph.findNode(rampInput)->outputs.front().id,
                   rampGraph.findNode(rampActivation)->inputs.front().id)
          .accepted &&
          rampGraph
              .connect(rampGraph.findNode(rampActivation)->outputs.front().id,
                       rampGraph.findNode(rampMerge)->inputs[0].id)
              .accepted &&
          rampGraph
              .connect(rampGraph.findNode(rampKnob)->outputs.front().id,
                       rampGraph.findNode(rampMerge)->inputs[1].id)
              .accepted &&
          rampGraph
              .connect(rampGraph.findNode(rampXy)->outputs[0].id,
                       rampGraph.findNode(rampMerge)->inputs[2].id)
              .accepted &&
          rampGraph
              .connect(rampGraph.findNode(rampMerge)->outputs.front().id,
                       rampGraph.findNode(rampOutput)->inputs.front().id)
              .accepted,
      "ramp graph must connect Gain, Knob, and XY into multiply");
  LiveGraphCompileOptions rampOptions = options;
  rampOptions.sampleRate = 48000.0;
  rampOptions.controlRampSeconds = 0.001;
  const auto rampCompiled = LiveGraphEngine::compile(rampGraph, rampOptions);
  LiveGraphCompileError rampError;
  const auto rampRuntime =
      LiveGraphEngine::prepare(rampCompiled.snapshot, rampError);
  passed &= expect(rampCompiled.succeeded() && rampRuntime != nullptr,
                   "control-ramp graph must compile and prepare");
  if (rampRuntime != nullptr) {
    RuntimeControlState rampControls;
    rampControls.gainByNodeId[rampActivation] = 2.0f;
    rampControls.conditioningByNodeId[rampKnob] = {1.0f, 0.0f};
    rampControls.conditioningByNodeId[rampXy] = {1.0f, 0.0f};
    rampRuntime->bindControls(
        std::make_shared<const RuntimeControlState>(rampControls));
    auto ones = torch::ones({1, 2, 64}, torch::kFloat32);
    const auto ramped = rampRuntime->processTensor(ones);
    passed &= expect(
        ramped.defined() && ramped.size(2) == 64 &&
            ramped[0][0][0].item<float>() < ramped[0][0][32].item<float>() &&
            ramped[0][0][32].item<float>() < ramped[0][0][63].item<float>() &&
            std::abs(ramped[0][0][63].item<float>() - 2.0f) < 1.0e-4f,
        "Gain, Knob, and XY must linear-ramp across the block to the target");
  }

  if (passed)
    std::cout << "AuralForge live graph engine tests passed\n";
  return passed ? 0 : 1;
}
