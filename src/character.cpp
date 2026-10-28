#include "character.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "json.hpp"
#include "util.hpp"

namespace {

// "strike > 0", "inWater", "!ground"
Condition parseCondition(const std::string& s) {
  Condition c;
  char name[64] = {}, op[3] = {};
  float v = 0;
  int n = std::sscanf(s.c_str(), " %63[A-Za-z0-9_] %2[<>=!] %f", name, op, &v);
  if (n == 3) {
    static const std::pair<const char*, Condition::Op> ops[] = {{">", Condition::GT}, {">=", Condition::GE}, {"<", Condition::LT},
                                                                {"<=", Condition::LE}, {"==", Condition::EQ}, {"!=", Condition::NE}};
    auto it = std::find_if(std::begin(ops), std::end(ops), [&](auto& o) { return !std::strcmp(o.first, op); });
    if (it == std::end(ops)) throw std::runtime_error("bad operator in \"" + s + "\"");
    c.op = it->second; c.param = name; c.value = v;
  } else if (std::sscanf(s.c_str(), " !%63[A-Za-z0-9_]", name) == 1) {
    c.op = Condition::EQ; c.param = name;
  } else if (n == 1) {
    c.param = name;  // != 0
  } else {
    throw std::runtime_error("bad condition \"" + s + "\"");
  }
  return c;
}

bool holds(const Condition& c, const AnimParams& p) {
  auto it = p.find(c.param);
  float v = it == p.end() ? 0 : it->second;
  switch (c.op) {
    case Condition::GT: return v > c.value;
    case Condition::GE: return v >= c.value;
    case Condition::LT: return v < c.value;
    case Condition::LE: return v <= c.value;
    case Condition::EQ: return v == c.value;
    case Condition::NE: return v != c.value;
  }
  return false;
}

float param(const AnimParams& p, const std::string& name) {
  auto it = p.find(name);
  return it == p.end() ? 0 : it->second;
}

}  // namespace

float Character::scale() const { return height / -anims.at(states[0].anim).head[0].y; }

Character loadCharacter(const std::string& path) {
  Character c = parseCharacter(path);
  std::string sheet = Json::parse(readFile(path)).strOr("sheet", "");
  c.sheet = LoadTexture(sheet.c_str());
  if (!IsTextureValid(c.sheet)) throw std::runtime_error(path + ": cannot load sheet \"" + sheet + "\"");
  GenTextureMipmaps(&c.sheet);
  SetTextureFilter(c.sheet, TEXTURE_FILTER_TRILINEAR);
  return c;
}

Character parseCharacter(const std::string& path) {
  Character c;
  c.path = path;
  Json j = Json::parse(readFile(path));
  auto fail = [&](const std::string& what) { throw std::runtime_error(path + ": " + what); };

  // the sheet's layout, written by tools/extract_player_sprites.py
  std::string atlasPath = j.strOr("atlas", "");
  Json atlas = Json::parse(readFile(atlasPath));
  auto cell = atlas.get("cell"), anchor = atlas.get("anchor"), anims = atlas.get("anims");
  if (!cell || !anchor || !anims || cell->arr.size() < 2 || anchor->arr.size() < 2) fail(atlasPath + " needs cell, anchor and anims");
  c.cellW = float(cell->arr[0].num); c.cellH = float(cell->arr[1].num);
  c.anchorX = float(anchor->arr[0].num); c.anchorY = float(anchor->arr[1].num);
  for (auto& [name, a] : anims->obj) {
    SheetAnim s{int(a.numOr("row", 0)), int(a.numOr("frames", 1)), {}};
    if (auto h = a.get("head")) for (auto& p : h->arr) s.head.push_back({float(p.arr[0].num), float(p.arr[1].num)});
    s.head.resize(s.frames, s.head.empty() ? Vector2{0, -c.anchorY} : s.head.back());
    c.anims[name] = s;
  }

  c.height = j.numOr("height", 96);
  c.tint = hex(j.strOr("tint", "#ffffff").c_str());
  auto states = j.get("states");
  if (!states || states->obj.empty()) fail("no states");
  for (auto& [name, s] : states->obj) {
    AnimState st;
    st.name = name;
    st.anim = s.strOr("anim", "");
    auto a = c.anims.find(st.anim);
    if (a == c.anims.end()) fail("state " + name + ": no sheet anim \"" + st.anim + "\"");
    if (auto f = s.get("frames")) for (auto& i : f->arr) st.frames.push_back(std::clamp(int(i.num), 0, a->second.frames - 1));
    if (st.frames.empty()) for (int i = 0; i < a->second.frames; i++) st.frames.push_back(i);
    st.fps = s.numOr("fps", 0);
    st.fpsParam = s.strOr("fpsParam", "");
    st.fpsGain = s.numOr("fpsGain", 0);
    st.drive = s.strOr("drive", "");
    st.driveLength = std::max(1e-4f, s.numOr("driveLength", 1));
    c.states.push_back(st);
  }
  auto index = [&](const std::string& name) {
    for (size_t i = 0; i < c.states.size(); i++) if (c.states[i].name == name) return int(i);
    fail("no state \"" + name + "\"");
    return 0;
  };
  if (auto ts = j.get("transitions"))
    for (auto& t : ts->arr) {
      Transition tr;
      std::string from = t.strOr("from", "any");
      tr.from = from == "any" ? -1 : index(from);
      tr.to = index(t.strOr("to", ""));
      if (auto w = t.get("when")) for (auto& cond : w->arr) tr.when.push_back(parseCondition(cond.str));
      c.transitions.push_back(tr);
    }
  return c;
}

void unloadCharacter(Character& c) {
  if (c.sheet.id) UnloadTexture(c.sheet);
  c.sheet = {};
}

void Animator::update(const Character& c, const AnimParams& p, float dt) {
  clock_ += dt;
  stateTime_ += dt;
  for (auto& t : c.transitions) {
    if (t.from >= 0 && t.from != state_) continue;
    if (!std::all_of(t.when.begin(), t.when.end(), [&](const Condition& k) { return holds(k, p); })) continue;
    if (t.to != state_) { state_ = t.to; stateTime_ = 0; }
    break;  // the first match wins, even when it keeps the current state
  }
  const AnimState& s = c.states[state_];
  int n = int(s.frames.size());
  int i;
  if (!s.drive.empty()) {
    float progress = clampf(1 - param(p, s.drive) / s.driveLength, 0, 1);
    i = std::min(n - 1, int(progress * n));
  } else {
    float fps = s.fps + s.fpsGain * (s.fpsParam.empty() ? 0 : param(p, s.fpsParam));
    i = int(clock_ * fps) % n;
  }
  frame_ = s.frames[i];
}

const SheetAnim& Animator::anim(const Character& c) const { return c.anims.at(c.states[state_].anim); }
int Animator::frame(const Character& c) const { return frame_ >= 0 ? frame_ : c.states[state_].frames[0]; }
