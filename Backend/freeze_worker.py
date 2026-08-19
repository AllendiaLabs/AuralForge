#!/usr/bin/env python3
"""Compile an AuralForge selected graph into a local TorchScript artifact."""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
import time
from pathlib import Path
from typing import Any

import torch
from torch import nn
from torch.nn import functional as functional

_UINT64_MASK = (1 << 64) - 1


def _float32(value: float) -> float:
    """Round one Python float exactly to IEEE-754 binary32."""
    return struct.unpack("<f", struct.pack("<f", value))[0]


def _split_mix_64(state: int) -> tuple[int, int]:
    """Advance the C++ SplitMix64 sequence and return state plus output."""
    state = (state + 0x9E3779B97F4A7C15) & _UINT64_MASK
    value = state
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & _UINT64_MASK
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & _UINT64_MASK
    return state, value ^ (value >> 31)


def _make_weight(
    output_channels: int,
    input_channels: int,
    kernel_size: int,
    state: int,
) -> tuple[torch.Tensor, int]:
    """Reproduce one deterministic LiveGraphEngine weight tensor."""
    scale = _float32(math.sqrt(6.0 / max(1, input_channels * kernel_size)))
    values: list[float] = []
    for _ in range(output_channels * input_channels * kernel_size):
        state, bits = _split_mix_64(state)
        unit = float(bits >> 11) / float(1 << 53)
        signed = _float32(unit * 2.0 - 1.0)
        values.append(_float32(signed * scale))
    return (
        torch.tensor(values, dtype=torch.float32).reshape(
            output_channels, input_channels, kernel_size
        ),
        state,
    )


def _assign_deterministic_weights(module: nn.Module, seed: int) -> None:
    """Copy the live C++ engine's seeded weights into one weighted element."""
    state = (seed & 0xFFFF_FFFF) ^ 0xA0761D6478BD642F
    with torch.no_grad():
        for layer in module.modules():
            if not isinstance(layer, nn.Conv1d):
                continue
            weight, state = _make_weight(
                layer.out_channels,
                layer.in_channels,
                layer.kernel_size[0],
                state,
            )
            layer.weight.copy_(weight)


class CausalConv1d(nn.Module):
    """Apply a left-padded causal one-dimensional convolution."""

    def __init__(
        self, input_channels: int, output_channels: int, kernel_size: int, dilation: int
    ) -> None:
        """Create a causal convolution with validated dimensions."""
        super().__init__()
        self.left_padding = (kernel_size - 1) * dilation
        self.convolution = nn.Conv1d(
            input_channels,
            output_channels,
            kernel_size,
            dilation=dilation,
            bias=False,
        )

    def forward(self, samples: torch.Tensor) -> torch.Tensor:
        """Process ``[batch, channels, time]`` samples without future context."""
        return self.convolution(functional.pad(samples, (self.left_padding, 0)))


class ChannelLinear(nn.Module):
    """Apply a linear projection independently at every audio sample."""

    def __init__(self, input_channels: int, output_channels: int) -> None:
        """Create a per-sample channel projection."""
        super().__init__()
        self.projection = nn.Conv1d(input_channels, output_channels, 1, bias=False)

    def forward(self, samples: torch.Tensor) -> torch.Tensor:
        """Project the channel axis of an audio tensor."""
        return self.projection(samples)


class ZeroPreservingSigmoid(nn.Module):
    """Apply sigmoid while retaining the audio engine's exact-zero contract."""

    def forward(self, samples: torch.Tensor) -> torch.Tensor:
        """Map nonzero values through sigmoid and keep zeros exactly zero."""
        return torch.where(
            samples == 0.0, torch.zeros_like(samples), torch.sigmoid(samples)
        )


class ResidualTCN(nn.Module):
    """Small causal temporal-convolution stack used for frozen TCN elements."""

    def __init__(
        self,
        input_channels: int,
        hidden_channels: int,
        depth: int,
        kernel_size: int,
        dilation: int,
        activation: int,
    ) -> None:
        """Create a deterministic stack matching the serialized TCN controls."""
        super().__init__()
        layers: list[nn.Module] = []
        layers.append(ChannelLinear(input_channels, hidden_channels))
        receptive_field = 1
        for layer_index in range(depth):
            effective_dilation = dilation * (1 << layer_index)
            receptive_field += (kernel_size - 1) * effective_dilation
            if receptive_field > 1 << 20:
                raise ValueError("TCN receptive field exceeds the runtime limit")
            layers.append(
                CausalConv1d(
                    hidden_channels,
                    hidden_channels,
                    kernel_size,
                    effective_dilation,
                )
            )
            layers.append(_activation(activation))
        layers.append(ChannelLinear(hidden_channels, input_channels))
        self.network = nn.Sequential(*layers)

    def forward(self, samples: torch.Tensor) -> torch.Tensor:
        """Run the frozen temporal stack."""
        return self.network(samples)


def _activation(index: int) -> nn.Module:
    """Return the activation represented by an AuralForge enum value."""
    activations: tuple[nn.Module, ...] = (
        nn.ReLU(),
        ZeroPreservingSigmoid(),
        nn.Tanh(),
        nn.LeakyReLU(0.01),
    )
    if index < 0 or index >= len(activations):
        raise ValueError(f"unsupported activation index {index}")
    return activations[index]


def _properties(element: dict[str, Any]) -> dict[str, int]:
    """Convert an element's ordered property array to a lookup dictionary."""
    return {
        str(item["key"]): int(item["value"])
        for item in element.get("properties", [])
    }


def _topological_elements(fragment: dict[str, Any]) -> list[dict[str, Any]]:
    """Return selected elements in stable topological order."""
    elements = fragment.get("elements", [])
    if not elements:
        raise ValueError("selected graph contains no elements")

    by_id = {int(element["id"]): element for element in elements}
    indegree = {element_id: 0 for element_id in by_id}
    outgoing: dict[int, list[int]] = {element_id: [] for element_id in by_id}
    for connection in fragment.get("connections", []):
        source = int(connection["source_element_id"])
        destination = int(connection["destination_element_id"])
        if source in by_id and destination in by_id:
            outgoing[source].append(destination)
            indegree[destination] += 1

    ready = sorted(element_id for element_id, count in indegree.items() if count == 0)
    ordered: list[dict[str, Any]] = []
    while ready:
        current = ready.pop(0)
        ordered.append(by_id[current])
        for destination in sorted(outgoing[current]):
            indegree[destination] -= 1
            if indegree[destination] == 0:
                ready.append(destination)
                ready.sort()

    if len(ordered) != len(elements):
        raise ValueError("selected graph is cyclic")
    if any(len(destinations) > 1 for destinations in outgoing.values()):
        raise ValueError("branched selected graphs are not supported by this worker")
    return ordered


def build_module(fragment: dict[str, Any], input_channels: int = 1) -> nn.Module:
    """Construct a module for a fragment with the specified input channels."""
    modules: list[nn.Module] = []
    channels = input_channels
    for element in _topological_elements(fragment):
        element_type = str(element["type"])
        properties = _properties(element)
        seed = int(element.get("seed", 42))

        if element_type in {"audio_input", "audio_output"}:
            continue
        if element_type == "activation":
            modules.append(_activation(properties.get("activation", 0)))
        elif element_type == "linear":
            output_channels = properties.get("features", channels)
            module = ChannelLinear(channels, output_channels)
            _assign_deterministic_weights(module, seed)
            modules.append(module)
            channels = output_channels
        elif element_type == "conv1d":
            output_channels = properties.get("channels", channels)
            module = CausalConv1d(
                channels,
                output_channels,
                properties.get("kernel_size", 3),
                properties.get("dilation", 1),
            )
            _assign_deterministic_weights(module, seed)
            modules.append(module)
            channels = output_channels
        elif element_type == "tcn":
            module = ResidualTCN(
                channels,
                properties.get("channels", channels),
                properties.get("depth", 1),
                properties.get("kernel_size", 3),
                properties.get("dilation", 1),
                properties.get("activation", 0),
            )
            _assign_deterministic_weights(module, seed)
            modules.append(module)
        elif element_type in {"merge", "sum", "multiply"}:
            raise ValueError(
                "mixer elements cannot be frozen by the linear freeze worker"
            )

    return nn.Sequential(*modules) if modules else nn.Identity()


def _receptive_field(module: nn.Module) -> int:
    """Return the aggregate causal receptive field of a sequential graph."""
    return 1 + sum(
        layer.left_padding
        for layer in module.modules()
        if isinstance(layer, CausalConv1d)
    )


def compile_request(request: dict[str, Any], artifact_dir: Path) -> dict[str, Any]:
    """Compile one validated manual-freeze request and return its response."""
    request_id = str(request.get("request_id", ""))
    if request.get("operation") != "freeze_selection" or not request_id:
        raise ValueError("invalid freeze request envelope")

    options = request.get("compile_options", {})
    input_channels = int(options.get("host_input_channels", 2))
    output_channels = int(options.get("host_output_channels", input_channels))
    example_samples = int(options.get("example_samples", 256))
    if not 1 <= input_channels <= 1024 or not 1 <= output_channels <= 1024:
        raise ValueError("freeze artifact channel counts must be between 1 and 1024")
    if example_samples < 1 or example_samples > 1 << 20:
        raise ValueError("invalid freeze example block size")

    started = time.perf_counter()
    module = build_module(request["graph_fragment"], input_channels).eval()
    receptive_field = _receptive_field(module)
    example = torch.zeros(1, input_channels, example_samples)
    with torch.inference_mode():
        scripted = torch.jit.trace(module, example, strict=True)
        scripted = torch.jit.freeze(scripted)
        output = scripted(example)
        if output.dim() != 3 or output.size(1) != output_channels:
            raise ValueError(
                "compiled artifact output channels do not match the host configuration"
            )
        for _ in range(2):
            output = scripted(example)
        latency_started = time.perf_counter()
        for _ in range(8):
            scripted(example)
        latency_ms = (time.perf_counter() - latency_started) * 1000.0 / 8.0

    artifact_dir.mkdir(parents=True, exist_ok=True)
    artifact_path = (artifact_dir / f"{request_id}.pt").resolve()
    temporary_path = artifact_path.with_suffix(".pt.tmp")
    torch.jit.save(scripted, str(temporary_path))
    temporary_path.replace(artifact_path)
    compile_ms = (time.perf_counter() - started) * 1000.0
    return {
        "request_id": request_id,
        "status": "success",
        "artifact_path": str(artifact_path),
        "blackbox_metadata": {
            "display_name": "Frozen Selection",
            "ports": [],
            "shape_signature": {
                "input_channels": input_channels,
                "output_channels": output_channels,
            },
            "receptive_field_samples": receptive_field,
            "baseline_metrics": {
                "compile_time_ms": compile_ms,
                "estimated_latency_ms": latency_ms,
            },
        },
    }


def main() -> int:
    """Run the command-line worker and emit exactly one JSON response."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--request", type=Path, required=True)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    arguments = parser.parse_args()

    request_id = ""
    try:
        request = json.loads(arguments.request.read_text(encoding="utf-8"))
        request_id = str(request.get("request_id", ""))
        response = compile_request(request, arguments.artifact_dir)
        print(json.dumps(response), flush=True)
        return 0
    except Exception as error:  # Worker boundary must return user-facing errors.
        print(
            json.dumps(
                {
                    "request_id": request_id,
                    "status": "failure",
                    "error_message": str(error),
                }
            ),
            flush=True,
        )
        return 1


if __name__ == "__main__":
    sys.exit(main())
