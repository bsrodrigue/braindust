// What differs between desktop and Android: where files are read and written, which shaders the GPU takes,
// how large the render targets are, and whether the game shows touch controls.
//
// The game reads its files by relative path (levels/, data/, shaders/, assets/, ...). On the desktop they are files in
// the project folder. On Android they are the APK's assets, which are read-only: a save goes to the app's own storage,
// and from then on that saved copy is the one read.
#pragma once
#include <string>
#include <vector>

#include "raylib.h"

namespace platform {

bool android();
bool exists(const std::string& path);                                         // a file the game can read
std::string readText(const std::string& path);                                // throws when missing
bool writeText(const std::string& path, const std::string& text);
std::vector<std::string> listFiles(const std::string& dir, const std::string& ext);  // names in dir, sorted

// Shaders are written in GLSL 330. Under OpenGL ES 2 (Android) they are rewritten to GLSL ES 100 on load.
Shader loadShader(const char* vsPath, const char* fsPath);  // either may be nullptr: raylib's default stage
std::string toGlsl100(const std::string& src, bool vertex);  // the rewrite (exposed for tests)

// --scale: render targets at this fraction of the screen, whatever the graphics quality says; 0 when not set
float renderScaleOverride();
void setRenderScale(float s);

}  // namespace platform
