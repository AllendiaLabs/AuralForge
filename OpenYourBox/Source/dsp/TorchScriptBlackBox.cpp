#include "TorchScriptBlackBox.h"

#include <limits>
#include <utility>

namespace {
/**
 * @class TorchScriptKernel
 * @brief Runtime-local executor for one validated frozen artifact.
 */
class TorchScriptKernel final : public openyourbox::dsp::FrozenBlackBoxKernel {
public:
  /**
   * @brief Adopts a runtime-local inference module.
   * @param moduleToAdopt Loaded and evaluated TorchScript module.
   */
  explicit TorchScriptKernel(torch::jit::Module moduleToAdopt)
      : module(std::move(moduleToAdopt)) {
    module.eval();
  }

  /** @brief Executes one frozen inference call. */
  torch::Tensor forward(const torch::Tensor &input) override {
    torch::InferenceMode inferenceGuard;
    return module.forward({input}).toTensor();
  }

private:
  /** @brief Runtime-local module never mutated after construction. */
  torch::jit::Module module;
};
} // namespace

namespace openyourbox::dsp {
std::shared_ptr<const TorchScriptBlackBoxFactory>
TorchScriptBlackBoxFactory::load(const std::string &artifactPath,
                                 int inputChannels,
                                 std::uint64_t receptiveFieldSamples,
                                 std::string &error) {
  error.clear();
  if (artifactPath.empty() || inputChannels < 1 || receptiveFieldSamples < 1) {
    error = "Frozen artifact path, channels, and receptive field must be valid";
    return {};
  }

  try {
    auto module = torch::jit::load(artifactPath, torch::kCPU);
    module.eval();
    torch::InferenceMode inferenceGuard;
    const auto silence = torch::zeros(
        {1, inputChannels, 256},
        torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCPU));
    const auto value = module.forward({silence});
    if (!value.isTensor()) {
      error = "Frozen artifact did not return an audio tensor";
      return {};
    }
    const auto output = value.toTensor();
    if (!output.defined() || output.device().type() != torch::kCPU ||
        output.scalar_type() != torch::kFloat32 || output.dim() != 3 ||
        output.size(0) != 1 || output.size(1) < 1 ||
        output.size(1) > std::numeric_limits<int>::max() ||
        output.size(2) != silence.size(2)) {
      error = "Frozen artifact returned an invalid audio tensor shape";
      return {};
    }

    std::uint64_t parameters = 0;
    for (const auto &parameter : module.parameters())
      parameters += static_cast<std::uint64_t>(parameter.numel());
    const auto preserves =
        torch::count_nonzero(output).item<std::int64_t>() == 0;
    if (!preserves) {
      error = "Frozen artifact does not preserve digital silence";
      return {};
    }

    return std::shared_ptr<const TorchScriptBlackBoxFactory>(
        new TorchScriptBlackBoxFactory(
            artifactPath, inputChannels, static_cast<int>(output.size(1)),
            receptiveFieldSamples, parameters, preserves));
  } catch (const std::exception &exception) {
    error = exception.what();
    return {};
  }
}

int TorchScriptBlackBoxFactory::getInputChannels() const noexcept {
  return validatedInputChannels;
}

int TorchScriptBlackBoxFactory::getOutputChannels() const noexcept {
  return validatedOutputChannels;
}

std::uint64_t TorchScriptBlackBoxFactory::getReceptiveField() const noexcept {
  return receptiveField;
}

std::uint64_t TorchScriptBlackBoxFactory::getParameterCount() const noexcept {
  return parameterCount;
}

bool TorchScriptBlackBoxFactory::preservesSilence() const noexcept {
  return silencePreserving;
}

std::unique_ptr<FrozenBlackBoxKernel>
TorchScriptBlackBoxFactory::createKernel() const {
  try {
    auto module = torch::jit::load(artifactPath, torch::kCPU);
    return std::make_unique<TorchScriptKernel>(std::move(module));
  } catch (...) {
    return {};
  }
}

const std::string &
TorchScriptBlackBoxFactory::getArtifactPath() const noexcept {
  return artifactPath;
}

TorchScriptBlackBoxFactory::TorchScriptBlackBoxFactory(std::string path,
                                                       int inputs, int outputs,
                                                       std::uint64_t field,
                                                       std::uint64_t parameters,
                                                       bool silence)
    : artifactPath(std::move(path)), validatedInputChannels(inputs),
      validatedOutputChannels(outputs), receptiveField(field),
      parameterCount(parameters), silencePreserving(silence) {}
} // namespace openyourbox::dsp
