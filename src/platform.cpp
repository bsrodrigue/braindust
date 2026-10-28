#include "platform.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "rlgl.h"

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <unistd.h>
extern "C" struct android_app* GetAndroidApp(void);  // raylib (rcore_android.c), not in raylib.h
#endif

namespace platform {

namespace {
float scaleOverride = 0;
}
void setRenderScale(float s) { scaleOverride = s; }
float renderScaleOverride() { return scaleOverride; }

#if defined(__ANDROID__)
namespace {
// a saved copy lives flat in the app's storage: data/moves.json -> user_data_moves.json
std::string savedName(const std::string& path) {
  std::string s = "user_" + path;
  std::replace(s.begin(), s.end(), '/', '_');
  return s;
}
std::string storage() { return GetAndroidApp()->activity->internalDataPath; }
bool saved(const std::string& path) { return access((storage() + "/" + savedName(path)).c_str(), F_OK) == 0; }
AAssetManager* assets() { return GetAndroidApp()->activity->assetManager; }
}  // namespace

bool android() { return true; }

bool exists(const std::string& path) {
  if (saved(path)) return true;
  AAsset* a = AAssetManager_open(assets(), path.c_str(), AASSET_MODE_UNKNOWN);
  if (a) AAsset_close(a);
  return a != nullptr;
}

// raylib's file reading tries the APK's assets first and then the app's storage: a saved copy is asked for by its
// own name, which no asset has, so it is found in storage
std::string readText(const std::string& path) {
  std::string name = saved(path) ? savedName(path) : path;
  char* txt = LoadFileText(name.c_str());
  if (!txt) throw std::runtime_error("cannot read " + path);
  std::string s = txt;
  UnloadFileText(txt);
  return s;
}

bool writeText(const std::string& path, const std::string& text) {
  std::string copy = text;
  return SaveFileText(savedName(path).c_str(), copy.data());  // raylib writes into the app's storage
}

std::vector<std::string> listFiles(const std::string& dir, const std::string& ext) {
  std::vector<std::string> out;
  if (AAssetDir* d = AAssetManager_openDir(assets(), dir.c_str())) {
    while (const char* f = AAssetDir_getNextFileName(d)) {
      std::string name = f;
      if (name.size() >= ext.size() && name.compare(name.size() - ext.size(), ext.size(), ext) == 0) out.push_back(name);
    }
    AAssetDir_close(d);
  }
  std::sort(out.begin(), out.end());
  return out;
}



#else

bool android() { return false; }
bool exists(const std::string& path) { return FileExists(path.c_str()); }

std::string readText(const std::string& path) {
  char* txt = LoadFileText(path.c_str());
  if (!txt) throw std::runtime_error("cannot read " + path);
  std::string s = txt;
  UnloadFileText(txt);
  return s;
}

bool writeText(const std::string& path, const std::string& text) {
  std::string copy = text;
  return SaveFileText(path.c_str(), copy.data());
}

std::vector<std::string> listFiles(const std::string& dir, const std::string& ext) {
  std::vector<std::string> out;
  if (!DirectoryExists(dir.c_str())) return out;
  FilePathList files = LoadDirectoryFilesEx(dir.c_str(), ext.c_str(), false);
  for (unsigned i = 0; i < files.count; i++) out.push_back(GetFileName(files.paths[i]));
  UnloadDirectoryFiles(files);
  std::sort(out.begin(), out.end());
  return out;
}



#endif

// GLSL 330 -> GLSL ES 100: a version and precision header, attribute/varying for in/out, texture2D and gl_FragColor.
// Our shaders keep to what both languages share apart from those words (checked by tests/platform_test.cpp).
std::string toGlsl100(const std::string& src, bool vertex) {
  std::string out = "#version 100\n";
  out += vertex ? "precision highp float;\n"
                : "#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n"
                  "#define texture texture2D\n#define finalColor gl_FragColor\n";
  std::istringstream lines(src);
  for (std::string line; std::getline(lines, line);) {
    size_t at = line.find_first_not_of(" \t");
    std::string body = at == std::string::npos ? "" : line.substr(at);
    if (body.rfind("#version", 0) == 0) continue;
    if (body.rfind("in ", 0) == 0) line = (vertex ? "attribute " : "varying ") + body.substr(3);
    else if (body.rfind("out ", 0) == 0) line = vertex ? "varying " + body.substr(4) : "// " + body;  // the fragment output is gl_FragColor
    out += line + "\n";
  }
  return out;
}

Shader loadShader(const char* vsPath, const char* fsPath) {
  if (rlGetVersion() != RL_OPENGL_ES_20) return LoadShader(vsPath, fsPath);
  std::string vs = vsPath ? toGlsl100(readText(vsPath), true) : "", fs = fsPath ? toGlsl100(readText(fsPath), false) : "";
  return LoadShaderFromMemory(vsPath ? vs.c_str() : nullptr, fsPath ? fs.c_str() : nullptr);
}

}  // namespace platform
