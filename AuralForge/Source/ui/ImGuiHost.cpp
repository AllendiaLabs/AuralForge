#include "ImGuiHost.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>

#include <utility>

namespace auralforge::ui {
ImGuiHost::ImGuiHost(RenderCallback callback)
    : renderCallback(std::move(callback)) {
  setOpaque(true);
  setWantsKeyboardFocus(true);
  openGLContext.setRenderer(this);
  openGLContext.setOpenGLVersionRequired(
      juce::OpenGLContext::OpenGLVersion::openGL3_2);
  openGLContext.setContinuousRepainting(true);
  openGLContext.setMultisamplingEnabled(true);
  openGLContext.attachTo(*this);
}

ImGuiHost::~ImGuiHost() { openGLContext.detach(); }

void ImGuiHost::mouseMove(const juce::MouseEvent &event) { updateMouse(event); }

void ImGuiHost::mouseDrag(const juce::MouseEvent &event) { updateMouse(event); }

void ImGuiHost::mouseDown(const juce::MouseEvent &event) {
  grabKeyboardFocus();
  updateMouse(event);
}

void ImGuiHost::mouseUp(const juce::MouseEvent &event) { updateMouse(event); }

void ImGuiHost::mouseWheelMove(const juce::MouseEvent &event,
                               const juce::MouseWheelDetails &wheel) {
  updateMouse(event);
  if (imguiContext == nullptr)
    return;

  ImGui::SetCurrentContext(imguiContext);
  ImGui::GetIO().AddMouseWheelEvent(wheel.deltaX, wheel.deltaY);
}

bool ImGuiHost::keyPressed(const juce::KeyPress &key) {
  if (imguiContext == nullptr)
    return false;

  ImGui::SetCurrentContext(imguiContext);
  const auto modifiers = key.getModifiers();
  auto &io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Ctrl, modifiers.isCtrlDown());
  io.AddKeyEvent(ImGuiMod_Shift, modifiers.isShiftDown());
  io.AddKeyEvent(ImGuiMod_Alt, modifiers.isAltDown());
  io.AddKeyEvent(ImGuiMod_Super, modifiers.isCommandDown());
  const auto character = key.getTextCharacter();
  if (character > 0)
    io.AddInputCharacter(static_cast<unsigned int>(character));
  return io.WantCaptureKeyboard;
}

bool ImGuiHost::keyStateChanged(bool isKeyDown) {
  juce::ignoreUnused(isKeyDown);
  if (imguiContext == nullptr)
    return false;

  ImGui::SetCurrentContext(imguiContext);
  auto &io = ImGui::GetIO();
  const auto forward = [&io](ImGuiKey key, int juceKey) {
    io.AddKeyEvent(key, juce::KeyPress::isKeyCurrentlyDown(juceKey));
  };

  forward(ImGuiKey_Tab, juce::KeyPress::tabKey);
  forward(ImGuiKey_LeftArrow, juce::KeyPress::leftKey);
  forward(ImGuiKey_RightArrow, juce::KeyPress::rightKey);
  forward(ImGuiKey_UpArrow, juce::KeyPress::upKey);
  forward(ImGuiKey_DownArrow, juce::KeyPress::downKey);
  forward(ImGuiKey_PageUp, juce::KeyPress::pageUpKey);
  forward(ImGuiKey_PageDown, juce::KeyPress::pageDownKey);
  forward(ImGuiKey_Home, juce::KeyPress::homeKey);
  forward(ImGuiKey_End, juce::KeyPress::endKey);
  forward(ImGuiKey_Insert, juce::KeyPress::insertKey);
  forward(ImGuiKey_Delete, juce::KeyPress::deleteKey);
  forward(ImGuiKey_Backspace, juce::KeyPress::backspaceKey);
  forward(ImGuiKey_Space, juce::KeyPress::spaceKey);
  forward(ImGuiKey_Enter, juce::KeyPress::returnKey);
  forward(ImGuiKey_Escape, juce::KeyPress::escapeKey);
  return io.WantCaptureKeyboard;
}

void ImGuiHost::newOpenGLContextCreated() {
  imguiContext = ImGui::CreateContext();
  ImGui::SetCurrentContext(imguiContext);
  ImGui::StyleColorsDark();
  auto &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui_ImplOpenGL3_Init(nullptr);
}

void ImGuiHost::renderOpenGL() {
  if (imguiContext == nullptr)
    return;

  ImGui::SetCurrentContext(imguiContext);
  ImGui_ImplOpenGL3_NewFrame();

  auto &io = ImGui::GetIO();
  const auto scale = static_cast<float>(openGLContext.getRenderingScale());
  io.DisplaySize =
      ImVec2(static_cast<float>(getWidth()), static_cast<float>(getHeight()));
  io.DisplayFramebufferScale = ImVec2(scale, scale);

  ImGui::NewFrame();
  if (renderCallback)
    renderCallback();
  ImGui::Render();

  juce::OpenGLHelpers::clear(juce::Colour(20, 23, 30));
  juce::gl::glViewport(
      0, 0, juce::roundToInt(static_cast<float>(getWidth()) * scale),
      juce::roundToInt(static_cast<float>(getHeight()) * scale));
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiHost::openGLContextClosing() {
  if (imguiContext == nullptr)
    return;

  ImGui::SetCurrentContext(imguiContext);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui::DestroyContext(imguiContext);
  imguiContext = nullptr;
}

void ImGuiHost::updateMouse(const juce::MouseEvent &event) {
  if (imguiContext == nullptr)
    return;

  ImGui::SetCurrentContext(imguiContext);
  ImGui::GetIO().AddMousePosEvent(event.position.x, event.position.y);
  updateButtons(event.mods);
}

void ImGuiHost::updateButtons(const juce::ModifierKeys &modifiers) {
  auto &io = ImGui::GetIO();
  io.AddMouseButtonEvent(0, modifiers.isLeftButtonDown());
  io.AddMouseButtonEvent(1, modifiers.isRightButtonDown());
  io.AddMouseButtonEvent(2, modifiers.isMiddleButtonDown());
}
} // namespace auralforge::ui
