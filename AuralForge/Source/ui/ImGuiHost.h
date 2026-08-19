#pragma once

#include <JuceHeader.h>

#include <functional>

struct ImGuiContext;

namespace auralforge::ui {
/**
 * @class ImGuiHost
 * @brief Owns a per-editor Dear ImGui context rendered by JUCE OpenGL.
 */
class ImGuiHost final : public juce::Component, private juce::OpenGLRenderer {
public:
  /** @brief User drawing callback executed between ImGui NewFrame and Render.
   */
  using RenderCallback = std::function<void()>;

  /**
   * @brief Attaches an OpenGL context and starts continuous rendering.
   * @param callback Function that builds the current ImGui frame.
   */
  explicit ImGuiHost(RenderCallback callback);

  /** @brief Detaches OpenGL and releases the associated ImGui context. */
  ~ImGuiHost() override;

  /** @brief Updates ImGui pointer position and button state. */
  void mouseMove(const juce::MouseEvent &event) override;
  /** @brief Updates ImGui drag pointer state. */
  void mouseDrag(const juce::MouseEvent &event) override;
  /** @brief Forwards a mouse-button press to ImGui. */
  void mouseDown(const juce::MouseEvent &event) override;
  /** @brief Forwards a mouse-button release to ImGui. */
  void mouseUp(const juce::MouseEvent &event) override;
  /** @brief Forwards wheel deltas to ImGui. */
  void mouseWheelMove(const juce::MouseEvent &event,
                      const juce::MouseWheelDetails &wheel) override;
  /** @brief Forwards typed text to ImGui. */
  bool keyPressed(const juce::KeyPress &key) override;
  /** @brief Forwards navigation-key press and release state to ImGui. */
  bool keyStateChanged(bool isKeyDown) override;

private:
  void newOpenGLContextCreated() override;
  void renderOpenGL() override;
  void openGLContextClosing() override;
  void updateMouse(const juce::MouseEvent &event);
  void updateButtons(const juce::ModifierKeys &modifiers);

  juce::OpenGLContext openGLContext;
  RenderCallback renderCallback;
  ImGuiContext *imguiContext = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiHost)
};
} // namespace auralforge::ui
