#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <juce_opengl/juce_opengl.h>
#endif

// The Dear ImGui backend treats a custom loader as caller-provided GL symbols.
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <imgui_impl_opengl3.cpp>
