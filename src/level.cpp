#include "level.hpp"

#include <algorithm>

#include "json.hpp"
#include "util.hpp"

Level loadLevel(const std::string& path) {
  Json j = Json::parse(readFile(path));
  Level L;
  auto pt = [](const Json* v, Vector2 d) { return v ? Vector2{v->numOr("x", d.x), v->numOr("y", d.y)} : d; };
  auto arr = [&](const char* k) -> const std::vector<Json>& {
    static const Json none;
    auto v = j.get(k);
    return (v && v->type == Json::Arr ? v : &none)->arr;
  };
  L.name = j.strOr("name", "Untitled");
  L.width = j.numOr("width", 1000);
  L.height = j.numOr("height", 5200);
  L.top = std::min(-140.f, j.numOr("top", -140));
  L.spawn = pt(j.get("spawn"), L.spawn);
  L.portal = pt(j.get("portal"), {L.width / 2, L.height - 105});
  L.empty = pt(j.get("emptyCrack"), {L.width / 2 + 265, L.height - 40});
  auto flag = [](const Json& s, const char* k) { auto v = s.get(k); return v && v->type == Json::Bool && v->b; };
  auto reveal = [](const Json& s) {
    auto v = s.get("hidden");
    return !v ? -1.f : v->type == Json::Num ? float(v->num) : v->type == Json::Bool && v->b ? 0.f : -1.f;
  };
  for (auto& s : arr("solids"))
    L.solids.push_back({s.numOr("x", 0), s.numOr("y", 0), s.numOr("w", 10), s.numOr("h", 10), s.strOr("tile", ""), s.numOr("seal", 0)});
  for (auto& s : arr("lurkers")) L.enemies.push_back({"lurker", s.numOr("x", 0), s.numOr("y", 0)});
  for (auto& s : arr("thoughts")) L.enemies.push_back({"thought", s.numOr("x", 0), s.numOr("y", 0)});
  for (auto& s : arr("enemies")) L.enemies.push_back({s.strOr("type", "lurker"), s.numOr("x", 0), s.numOr("y", 0)});
  for (auto& s : arr("items")) L.items.push_back({s.numOr("x", 0), s.numOr("y", 0), s.strOr("kind", "dustVial"), reveal(s)});
  for (auto& s : arr("decor"))
    L.decor.push_back({s.strOr("model", "skull"), s.numOr("x", 0), s.numOr("y", 0), s.numOr("z", -20), s.numOr("scale", 1), s.numOr("rx", 0),
                       s.numOr("ry", 0), s.numOr("rz", 0)});
  for (auto& s : arr("wrappers")) L.wrappers.push_back({s.numOr("x", 0), s.numOr("y", 0), s.strOr("kind", "mana"), reveal(s)});
  for (auto& s : arr("dustZones"))
    L.zones.push_back({{s.numOr("x", 0), s.numOr("y", 0), s.numOr("w", 100), s.numOr("h", 100)}, s.numOr("rate", 1.5f)});
  for (auto& s : arr("texts")) L.texts.push_back({s.numOr("x", 0), s.numOr("y", 0), s.strOr("text", "")});
  for (auto& s : arr("lights"))
    L.lights.push_back({s.numOr("x", 0), s.numOr("y", 0), s.numOr("r", 120), hex(s.strOr("color", "#c89a50").c_str())});
  for (auto& s : arr("props")) {
    L.props.push_back({s.strOr("type", "post"), s.numOr("x", 0), s.numOr("y", 0), s.strOr("name", ""), s.numOr("scale", 1), flag(s, "flip")});
  }
  if (auto p = j.get("palette")) {
    L.surface = hex(p->strOr("surface", "#2a6a80").c_str());
    L.deep = hex(p->strOr("deep", "#0e2a36").c_str());
  }
  return L;
}
