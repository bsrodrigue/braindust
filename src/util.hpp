// Small shared helpers: math, colours, random numbers, file reading.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "raylib.h"
#include "raymath.h"

inline float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
inline float smooth(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }

// rnd(): a seeded LCG for layouts that must come out the same every time (set rngSeed first). frand(): anything else.
inline uint32_t rngSeed = 11;
inline float rnd() { rngSeed = uint32_t(uint64_t(rngSeed) * 16807 % 2147483647); return rngSeed / 2147483647.f; }
inline float frand() { return GetRandomValue(0, 1000000) / 1000000.f; }

inline Color hex(const char* h, float a = 1) {
  unsigned v = 0;
  sscanf(h + (h[0] == '#'), "%x", &v);
  return Color{(unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, (unsigned char)(255 * clampf(a, 0, 1))};
}
inline Color alpha(Color c, float a) { c.a = (unsigned char)(255 * clampf(a, 0, 1)); return c; }
inline Vector3 rgb(Color c, float k = 1) { return {c.r / 255.f * k, c.g / 255.f * k, c.b / 255.f * k}; }
inline Vector3 V3(float x, float y, float z) { return {x, y, z}; }
inline Matrix trs(Vector3 pos, Vector3 rot, Vector3 scl) {
  return MatrixMultiply(MatrixMultiply(MatrixScale(scl.x, scl.y, scl.z), MatrixRotateXYZ(rot)), MatrixTranslate(pos.x, pos.y, pos.z));
}
inline float nz(float x, float y, float z) {  // cheap deterministic noise for lumpy geometry
  return std::sin(x * 1.7f + std::sin(y * 2.3f + z)) * std::cos(z * 1.9f + y * .7f) * .5f + std::sin(x * 4.1f + z * 3.7f + y) * .25f +
         std::sin(y * 7.3f + x * 5.9f - z * 2.1f) * .12f;
}

inline std::string readFile(const std::string& path) {  // throws when missing
  char* txt = LoadFileText(path.c_str());
  if (!txt) throw std::runtime_error("cannot read " + path);
  std::string s = txt;
  UnloadFileText(txt);
  return s;
}
