#include "scene3d.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "rlgl.h"
#include "util.hpp"

namespace {

constexpr int LEV_SEGS = 48;
const Vector3 CRACK_DIR{-.3f, .78f, .55f};  // crack direction in the skull model (glTF axes)

void smoothNormals(Mesh& m) {  // average face normals per position so lumps shade smoothly
  std::unordered_map<long long, Vector3> acc;
  auto key = [&](int i) {
    auto q = [](float v) { return (long long)std::llround(v * 1000); };
    return (q(m.vertices[3 * i]) * 73856093) ^ (q(m.vertices[3 * i + 1]) * 19349663) ^ (q(m.vertices[3 * i + 2]) * 83492791);
  };
  auto vert = [&](int i) { return Vector3{m.vertices[3 * i], m.vertices[3 * i + 1], m.vertices[3 * i + 2]}; };
  int tris = m.indices ? m.triangleCount : m.vertexCount / 3;
  auto idx = [&](int k) { return m.indices ? int(m.indices[k]) : k; };
  for (int f = 0; f < tris; f++) {
    int a = idx(3 * f), b = idx(3 * f + 1), c = idx(3 * f + 2);
    Vector3 n = Vector3CrossProduct(Vector3Subtract(vert(b), vert(a)), Vector3Subtract(vert(c), vert(a)));
    for (int v : {a, b, c}) acc[key(v)] = Vector3Add(acc[key(v)], n);
  }
  for (int i = 0; i < m.vertexCount; i++) {
    Vector3 n = Vector3Normalize(acc[key(i)]);
    m.normals[3 * i] = n.x; m.normals[3 * i + 1] = n.y; m.normals[3 * i + 2] = n.z;
  }
  UpdateMeshBuffer(m, 0, m.vertices, m.vertexCount * 3 * sizeof(float), 0);
  UpdateMeshBuffer(m, 2, m.normals, m.vertexCount * 3 * sizeof(float), 0);
}

Mesh roughened(Mesh m, float amt, float f) {
  for (int i = 0; i < m.vertexCount; i++) {
    Vector3 v{m.vertices[3 * i], m.vertices[3 * i + 1], m.vertices[3 * i + 2]};
    float len = Vector3Length(v);
    if (len < 1e-5f) continue;
    v = Vector3Add(v, Vector3Scale(v, nz(v.x * f, v.y * f, v.z * f) * amt / len));
    m.vertices[3 * i] = v.x; m.vertices[3 * i + 1] = v.y; m.vertices[3 * i + 2] = v.z;
  }
  smoothNormals(m);
  return m;
}

Image galaxyImage() {  // core glow + two spiral arms of dust
  Image img = GenImageColor(512, 512, BLANK);
  Color* px = (Color*)img.data;
  for (int y = 0; y < 512; y++)
    for (int x = 0; x < 512; x++) {
      float d = std::hypot(x - 256.f, y - 256.f) / 256;
      Vector3 c = d < .12f  ? Vector3Lerp(V3(232, 220, 196), V3(170, 150, 190), d / .12f)
                  : d < .5f ? Vector3Lerp(V3(170, 150, 190), V3(50, 40, 80), (d - .12f) / .38f)
                            : Vector3Lerp(V3(50, 40, 80), V3(10, 5, 20), clampf((d - .5f) / .5f, 0, 1));
      float a = d < .12f ? 1 - d / .12f * .3f : d < .5f ? .7f - (d - .12f) / .38f * .4f : std::max(0.f, .3f - (d - .5f) / .5f * .3f);
      px[y * 512 + x] = {(unsigned char)c.x, (unsigned char)c.y, (unsigned char)c.z, (unsigned char)(a * 255)};
    }
  for (int i = 0; i < 5000; i++) {
    int arm = i % 2;
    float r = std::pow(frand(), .6f) * 230, a = r * .035f + arm * PI + (frand() - .5f) * .6f;
    int x = int(256 + std::cos(a) * r), y = int(256 + std::sin(a) * r * .75f);
    if (x < 0 || y < 0 || x >= 511 || y >= 511) continue;
    Color c = r < 60 ? hex("#e8dcc4") : frand() < .5f ? hex("#a596b8") : hex("#8a9cb0");
    float al = .25f + frand() * .6f;
    for (int k : {0, 1, 512, 513}) {
      Color& p = px[y * 512 + x + k];
      p = {(unsigned char)(p.r + (c.r - p.r) * al), (unsigned char)(p.g + (c.g - p.g) * al), (unsigned char)(p.b + (c.b - p.b) * al),
           (unsigned char)std::max<int>(p.a, int(al * 255))};
    }
  }
  return img;
}

Image scleraImage() {  // bloodshot veins crawling toward the iris side
  Image img = GenImageGradientLinear(512, 256, 90, hex("#5a4a3c"), hex("#b0a68e"));
  for (int i = 0; i < 70; i++) {
    float x = frand() * 512, y = frand() * 256;
    Color c = hex("#8e1418", .4f + frand() * .5f);
    for (int k = 0; k < 8; k++) {
      float nx = x + (256 - x) * .12f + (frand() - .5f) * 18, ny = y + (128 - y) * .1f + (frand() - .5f) * 18;
      ImageDrawLine(&img, int(x), int(y), int(nx), int(ny), c);
      x = nx; y = ny;
    }
  }
  return img;
}

template <class Seg> void poseLeviathan(std::vector<Seg>& L, float s, float hx, float hy, float hz, float dir, float time, float amp) {
  for (int i = 0; i < LEV_SEGS; i++) {
    float r = (i < 5 ? 2.2f + i * .55f : 4.6f * std::pow(1 - i / float(LEV_SEGS), .8f) + .3f) * s;
    L[i] = {V3(hx - dir * i * 1.7f * s, hy + std::sin(time * 1.1f - i * .3f) * amp * std::min(1.f, i / 6.f), hz + std::sin(time * .6f - i * .2f) * amp * .5f), r};
  }
}

}  // namespace

Material Scene3D::sceneMat(Color c) { Material m = LoadMaterialDefault(); m.shader = sceneSh_; m.maps[MATERIAL_MAP_DIFFUSE].color = c; return m; }

void Scene3D::load() {
  sceneSh_ = LoadShader("shaders/scene.vs", "shaders/scene.fs");
  sceneSh_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(sceneSh_, "matModel");
  sceneSh_.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(sceneSh_, "matNormal");
  Image g = galaxyImage(); galaxy = LoadTextureFromImage(g); UnloadImage(g);
  Image sc = scleraImage(); sclera_ = LoadTextureFromImage(sc); UnloadImage(sc);
  SetTextureFilter(galaxy, TEXTURE_FILTER_BILINEAR);
  rock_ = sceneMat(hex("#161b1e")); bone_ = sceneMat(hex("#4f4b42")); floor_ = sceneMat(hex("#15181a"));
  skin_ = sceneMat(hex("#191d1c")); fin_ = sceneMat(hex("#2b2e29")); tooth_ = sceneMat(hex("#b8b2a0"));
  eye_ = sceneMat(WHITE); eye_.maps[MATERIAL_MAP_DIFFUSE].texture = sclera_;
  galaxyMat_ = LoadMaterialDefault(); galaxyMat_.maps[MATERIAL_MAP_DIFFUSE].texture = galaxy;
  black_ = LoadMaterialDefault(); black_.maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
  seg_ = roughened(GenMeshSphere(1, 14, 20), .22f, 2.4f);
  spine_ = GenMeshCone(.12f, 1.6f, 5);
  fang_ = GenMeshCone(.06f, .9f, 4);
  sphere_ = GenMeshSphere(1, 16, 24);
  for (int i = 0; i < 6; i++) rocks_.push_back(roughened(i % 2 ? GenMeshCone(1, 4 + i, 9) : GenMeshSphere(1, 8, 10), .35f, .9f + i * .15f));
  floorMesh_ = GenMeshPlane(240, 140, 80, 50);
  for (int i = 0; i < floorMesh_.vertexCount; i++)
    floorMesh_.vertices[3 * i + 1] = nz(floorMesh_.vertices[3 * i] * .15f, floorMesh_.vertices[3 * i + 2] * .15f, 0) * 1.6f;
  smoothNormals(floorMesh_);
  skull_ = LoadModel("assets/giant_skull.glb");
  skullOK_ = IsModelValid(skull_) && skull_.meshCount >= 1;
  tower_ = LoadModel("assets/drowned_tower.glb");
  towerOK_ = IsModelValid(tower_);
}

// rocks, floor, skulls, tower, and the two playfield skulls around the cracks
void Scene3D::place(const World& w) {
  const Level& LV = w.level;
  statics_.clear(); towerLights_.clear(); crackAnchors_.clear(); dreamer_.ok = false;
  rngSeed = 4242;
  float FY = -LV.height / 100, X0 = LV.width / 200, spread = std::max(40.f, LV.width / 25);
  for (int i = 0; i < 150; i++) {
    float side = rnd() < .5f ? -1.f : 1.f, s = 1 + rnd() * 5;
    statics_.push_back({Kind::Rock, i % 6, trs(V3(X0 + side * (7 + LV.width / 200 + rnd() * spread), -rnd() * (LV.height / 100 + 6), -9 - rnd() * 60),
                                               V3((rnd() - .5f) * .5f, rnd() * 3, (rnd() - .5f) * .5f), V3(s, s, s))});
  }
  statics_.push_back({Kind::Floor, 0, MatrixTranslate(X0, FY, -60)});
  auto skull = [&](Vector3 pos, Vector3 rot, float scale, bool giant) {
    statics_.push_back({skullOK_ ? (giant ? Kind::UniverseSkull : Kind::Skull) : Kind::ProceduralSkull, 0, trs(pos, rot, V3(scale, scale, scale))});
    if (giant) crackAnchors_.push_back(Vector3Transform(Vector3Scale(CRACK_DIR, .9f), statics_.back().m));
  };
  auto tower = [&](Vector3 pos, float ry, float scale) {
    Matrix m = trs(pos, V3(0, ry, 0), V3(scale, scale, scale));
    statics_.push_back({Kind::Tower, 0, m});
    for (int i = 0; i < 16; i++) {  // lit windows on the side facing the camera (shaft radius from the Blender model)
      float u = .08f + rnd() * .75f,
            r = 5.2f - 2.6f * u + 3.5f * (1 - smooth(0, .15f, u)) + 1.2f * std::exp(-std::pow((u - .55f) / .12f, 2)) - 1.4f * smooth(.85f, 1, u);
      float a = -PI / 2 - .5f + (rnd() - .5f) * 1.6f;
      towerLights_.push_back({Vector3Transform(V3(4.5f * u * u + std::cos(a) * r * 1.05f, u * 62, -(1.35f * u * u + std::sin(a) * r * 1.05f)), m),
                              rnd() * 6, rnd() < .25f, false});
    }
    towerLights_.push_back({Vector3Transform(V3(4.5f, 64, -1.35f), m), 0, false, true});
  };
  if (LV.decor.empty()) {  // the original Abyss layout
    skull(V3(X0 - 11, FY + 12 * .35f, -30), V3(0, .35f, .12f), 12, true);
    for (int i = 0; i < 18; i++) {
      float x = X0 + (rnd() - .5f) * 70, z = -8 - rnd() * 45, R = .8f + rnd() * 2.2f;
      skull(V3(x, FY + R * .75f, z), V3((rnd() - .5f) * .4f, (rnd() - .5f) * 1.4f, (rnd() - .5f) * .5f), R, false);
    }
    if (towerOK_) tower(V3(X0 - 8, FY - .5f, -40), .5f, .75f);
  } else {
    for (auto& d : LV.decor) {
      Vector3 pos = V3(d.x / 100, -d.y / 100, d.z), rot = V3(d.rx, d.ry, d.rz);
      if (d.model == "tower") { if (towerOK_) tower(pos, d.ry, d.scale); }
      else skull(pos, rot, d.scale, d.model == "giant");
    }
  }
  // playfield skulls: crack pinned to the gameplay plane (z = .15) so the portal lines up with its hitbox
  auto inPlay = [&](Rectangle k, float Sp, Vector3 rot) {
    Matrix R = MatrixMultiply(MatrixScale(Sp, Sp, Sp), MatrixRotateXYZ(rot));
    Vector3 c = Vector3Transform(Vector3Scale(CRACK_DIR, .9f), R);
    Vector3 pos = V3((k.x + k.width / 2) / 100 - c.x, -(k.y + k.height / 2) / 100 - c.y, .15f - c.z);
    return MatrixMultiply(R, MatrixTranslate(pos.x, pos.y, pos.z));
  };
  if (skullOK_) {
    Matrix d = inPlay(w.cracks[0].r, 2.6f, V3(.4f, .15f, .1f));
    statics_.push_back({Kind::UniverseSkull, 1, d});
    dreamer_.ok = true; dreamer_.m = d;
    dreamer_.eye = Vector3Transform(Vector3Scale(CRACK_DIR, .42f), d);
    statics_.push_back({Kind::Skull, 1, inPlay(w.cracks[1].r, 1.25f, V3(.2f, -.7f, -.5f))});
  }
}

void Scene3D::update(float dt, World& w) {
  float X = (w.cam.x + w.view.x / 2) / 100, Y = -(w.cam.y + w.view.y / 2) / 100;
  camera.position = V3(X, Y, 10); camera.target = V3(X, Y, 0); camera.up = V3(0, 1, 0);
  camera.fovy = 2 * std::atan(w.view.y / 200 / 10) * RAD2DEG; camera.projection = CAMERA_PERSPECTIVE;
  if (w.inPocket) return;
  // a leviathan passes every 40 s; every third pass it comes between you and the camera
  levT_ += dt;
  int n = int(levT_ / 40);
  float k = std::fmod(levT_, 40.f) / 40, dir = n % 2 ? -1.f : 1.f;
  bool isNear = n % 3 == 2 && w.cam.y > 1800;
  bool was = w.nearActive;
  w.nearActive = isNear && k > .2f && k < .6f;
  if (!isNear && w.cam.y > 1200) {
    poseLeviathan(farLev_, 1, X + dir * (-90 + 180 * k), Y - 4 + std::sin(levT_ * .3f) * 2, -40, dir, levT_, 1.6f);
    if (lastPass_ != n && k > .05f) { lastPass_ = n; w.audio.groan(false); }
  }
  if (w.nearActive) poseLeviathan(nearLev_, .45f, X - 12 * dir + dir * (k - .2f) / .4f * 110, Y + .2f, 4, dir, levT_, .5f);
  if (w.nearActive && !was) { w.audio.groan(true); w.shake = .6f; }
  if (dreamer_.ok) {
    Vector2 pc = w.portalCentre(), me = w.playerCentre();
    if (!w.portalSeen && std::hypot(me.x - pc.x, me.y - pc.y) < 320) {  // the first time you come close, the dust surges out of the crack
      w.portalSeen = true; w.shake = .3f; w.audio.hiss(220, 60, 1.4f, .3f); w.audio.hiss(3000, 900, 1.6f, .08f);
      w.burst(pc.x, pc.y, 90, hex("#b89cff"), 160, -40, 2.5f);
    }
  }
  float deep = clampf(w.cam.y / 3500, 0, 1);
  w.audio.drone(.12f + deep * .45f, 220 - deep * 130);
}

void Scene3D::setUniforms(Color fog, float density, Vector3 amb, Vector3 sun, const std::vector<std::pair<Vector3, Vector4>>& lights) {
  auto loc = [&](const char* n) { return GetShaderLocation(sceneSh_, n); };
  SetShaderValue(sceneSh_, loc("viewPos"), &camera.position, SHADER_UNIFORM_VEC3);
  Vector3 f = rgb(fog);
  SetShaderValue(sceneSh_, loc("fogColor"), &f, SHADER_UNIFORM_VEC3);
  SetShaderValue(sceneSh_, loc("fogDensity"), &density, SHADER_UNIFORM_FLOAT);
  SetShaderValue(sceneSh_, loc("ambient"), &amb, SHADER_UNIFORM_VEC3);
  Vector3 sd = Vector3Normalize(V3(-.1f, -1, -.3f));
  SetShaderValue(sceneSh_, loc("sunDir"), &sd, SHADER_UNIFORM_VEC3);
  SetShaderValue(sceneSh_, loc("sunCol"), &sun, SHADER_UNIFORM_VEC3);
  Vector3 pos[4]{}, col[4]{};
  float range[4]{};
  for (size_t i = 0; i < lights.size() && i < 4; i++) {
    pos[i] = lights[i].first;
    col[i] = V3(lights[i].second.x, lights[i].second.y, lights[i].second.z);
    range[i] = lights[i].second.w;
  }
  SetShaderValueV(sceneSh_, loc("lPos"), pos, SHADER_UNIFORM_VEC3, 4);
  SetShaderValueV(sceneSh_, loc("lCol"), col, SHADER_UNIFORM_VEC3, 4);
  SetShaderValueV(sceneSh_, loc("lRange"), range, SHADER_UNIFORM_FLOAT, 4);
}

void Scene3D::drawLeviathan(const std::vector<Seg>& L, bool withEye) {
  for (int i = 0; i < LEV_SEGS; i++) {
    Vector3 nxt = i < LEV_SEGS - 1 ? L[i + 1].p : Vector3Add(L[i].p, Vector3Subtract(L[i].p, L[i - 1].p));
    Vector3 d = Vector3Subtract(L[i].p, nxt);  // points toward the head
    float th = std::atan2(d.y, d.x), r = L[i].r;
    DrawMesh(seg_, skin_, trs(L[i].p, V3(i * .7f, 0, th), V3(1.7f * r, r, .82f * r)));
    if (i > 3 && i % 2 == 0)
      DrawMesh(spine_, fin_, trs(Vector3Add(L[i].p, V3(0, r * .6f, 0)), V3(0, 0, th + (d.x > 0 ? -.7f : .7f)), V3(r * .8f, r * .8f, r * .8f)));
  }
  Vector3 h = L[0].p, fw = Vector3Normalize(Vector3Subtract(L[0].p, L[1].p));
  float r = L[0].r;
  for (int i = 0; i < 16; i++) {
    float u = i / 15.f - .5f;
    bool up = i % 2;
    Vector3 p = Vector3Add(h, V3(fw.x * r * (1.35f - std::fabs(u) * .6f), up ? -r * .1f : -r * .55f, u * r * 1.3f));
    float sc = r / 2.2f * (1 + (i * 7 % 3) * .6f);
    DrawMesh(fang_, tooth_, trs(p, V3(up ? PI : 0, 0, 0), V3(sc, sc, sc)));
  }
  if (withEye) {
    Vector3 e = Vector3Add(h, V3(fw.x * r * .6f, r * .2f, r * .7f));
    DrawMesh(sphere_, eye_, trs(e, V3(0, 0, 0), V3(.35f, .35f, .35f)));
    DrawMesh(sphere_, black_, trs(Vector3Add(e, V3(0, 0, .3f)), V3(0, 0, 0), V3(.07f, .2f, .05f)));
  }
}

void Scene3D::farLeviathanEyes(Vector3 out[2]) const {
  Vector3 h = farLev_[0].p, fw = Vector3Normalize(Vector3Subtract(farLev_[0].p, farLev_[1].p));
  float r = farLev_[0].r;
  for (int i = 0; i < 2; i++) out[i] = Vector3Add(h, V3(fw.x * r * .9f, r * .25f, (i ? 1.f : -1.f) * r * .6f));
}

void Scene3D::drawStatic(const Static& s) {
  switch (s.kind) {
    case Kind::Rock: DrawMesh(rocks_[s.aux], rock_, s.m); break;
    case Kind::Floor: DrawMesh(floorMesh_, floor_, s.m); break;
    case Kind::ProceduralSkull: DrawMesh(seg_, bone_, s.m); break;
    case Kind::Tower:
      for (int i = 0; i < tower_.meshCount; i++) {
        Material& mt = tower_.materials[tower_.meshMaterial[i]];
        mt.shader = sceneSh_;
        DrawMesh(tower_.meshes[i], mt, s.m);
      }
      break;
    case Kind::Skull:
    case Kind::UniverseSkull:
      for (int i = 0; i < skull_.meshCount; i++) {
        Material& mt = skull_.materials[skull_.meshMaterial[i]];
        if (skull_.meshes[i].colors) { mt.shader = sceneSh_; DrawMesh(skull_.meshes[i], mt, s.m); }
        else if (s.kind == Kind::UniverseSkull) {  // the universe sphere: seen from inside, unlit, no fog
          rlDrawRenderBatchActive(); rlSetCullFace(RL_CULL_FACE_FRONT);
          galaxyMat_.maps[MATERIAL_MAP_DIFFUSE].color = s.aux ? hex("#6e6884") : WHITE;
          DrawMesh(skull_.meshes[i], galaxyMat_, s.m);
          rlDrawRenderBatchActive(); rlSetCullFace(RL_CULL_FACE_BACK);
        }
      }
      break;
  }
}

void Scene3D::render(const World& w, RenderTexture2D& bg, RenderTexture2D& fg) {
  const Player& P = w.player;
  float deep = clampf(w.cam.y / 3500, 0, 1);
  Color fog = ColorLerp(w.level.surface, w.level.deep, deep);
  float li = w.lightR() / 130;
  Vector3 lp = V3((P.x + P.w / 2) / 100, -(w.headY() + 20) / 100, 3);
  std::vector<std::pair<Vector3, Vector4>> lights{{lp, {.72f * li, .66f * li, .78f * li, 30}}};
  if (dreamer_.ok) {
    float pulse = 2.6f + std::sin(w.t * 1.3f) * .8f + (P.flare > 0 ? 2 : 0);
    lights.push_back({Vector3Add(dreamer_.eye, V3(0, 0, 1.5f)), {.48f * pulse, .42f * pulse, .66f * pulse, 22}});
  }
  for (auto& a : crackAnchors_) if (lights.size() < 4) lights.push_back({Vector3Add(a, V3(0, 3, 4)), {.42f * 2.2f, .37f * 2.2f, .66f * 2.2f, 40}});

  BeginTextureMode(bg);
  ClearBackground(w.inPocket ? BLACK : fog);
  if (!w.inPocket) {
    rlEnableBackfaceCulling();
    BeginMode3D(camera);
    setUniforms(fog, .015f, rgb(hex("#6d8e9a"), .5f * (1 - deep) + .22f), rgb(hex("#a8c8d4"), .7f * (1 - deep) + .05f), lights);
    for (auto& s : statics_) drawStatic(s);
    if (farLeviathanVisible(w)) drawLeviathan(farLev_, false);
    EndMode3D();
  }
  EndTextureMode();

  BeginTextureMode(fg);  // the near pass: in front of the playfield
  ClearBackground(BLANK);
  if (w.nearActive && !w.inPocket) {
    BeginMode3D(camera);
    std::vector<std::pair<Vector3, Vector4>> fl{{V3(lp.x, lp.y, 6), {.72f * li * 1.4f, .66f * li * 1.4f, .78f * li * 1.4f, 20}}};
    setUniforms(BLACK, .05f, rgb(hex("#2a3636"), .12f), V3(0, 0, 0), fl);
    drawLeviathan(nearLev_, true);
    EndMode3D();
  }
  EndTextureMode();
}
