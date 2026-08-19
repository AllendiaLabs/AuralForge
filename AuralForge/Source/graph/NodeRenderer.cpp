#include "NodeRenderer.h"

#include <imgui.h>

namespace ed = ax::NodeEditor;

namespace auralforge::graph {
NodeRenderer::NodeRenderer() = default;

NodeRenderer::~NodeRenderer() { ed::DestroyEditor(context); }

void NodeRenderer::render(const NodeGraph &graph) {
  if (context == nullptr) {
    ed::Config configuration;
    configuration.SettingsFile = nullptr;
    context = ed::CreateEditor(&configuration);
  }

  ed::SetCurrentEditor(context);
  ed::Begin("TCN Graph");

  for (const auto &node : graph.getNodes()) {
    ed::SetNodePosition(ed::NodeId(node.id),
                        ImVec2(node.position.x, node.position.y));
    ed::PushStyleColor(ed::StyleColor_NodeBg,
                       ImColor(node.colour.getRed(), node.colour.getGreen(),
                               node.colour.getBlue(), 70));
    ed::PushStyleColor(ed::StyleColor_NodeBorder,
                       ImColor(node.colour.getRed(), node.colour.getGreen(),
                               node.colour.getBlue(), 255));

    ed::BeginNode(ed::NodeId(node.id));
    ImGui::TextUnformatted(node.label.c_str());
    ImGui::Separator();

    for (const auto &pin : node.inputs) {
      ed::BeginPin(ed::PinId(pin.id), ed::PinKind::Input);
      ImGui::Text("-> %s", pin.label.c_str());
      ed::EndPin();
    }

    ImGui::TextDisabled("%s", node.detail.c_str());

    for (const auto &pin : node.outputs) {
      ed::BeginPin(ed::PinId(pin.id), ed::PinKind::Output);
      ImGui::Text("%s ->", pin.label.c_str());
      ed::EndPin();
    }

    ed::EndNode();
    ed::PopStyleColor(2);
  }

  for (const auto &link : graph.getLinks()) {
    ed::Link(ed::LinkId(link.id), ed::PinId(link.sourcePinId),
             ed::PinId(link.destinationPinId), ImColor(100, 180, 255, 220),
             2.0f);
  }

  ed::End();
  ed::SetCurrentEditor(nullptr);
}
} // namespace auralforge::graph
