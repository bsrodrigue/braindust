#include "world.hpp"

#include <algorithm>
#include <cmath>

#include "util.hpp"

namespace {

Box boxOf(const Player& p) { return {p.x, p.y, p.w, p.h}; }
Box boxOf(const Enemy& e) { return {e.x, e.y, e.w, e.h}; }
Box boxOf(const Rectangle& r) { return {r.x, r.y, r.width, r.height}; }

// axis-separated collision against the solids; waterOnly keeps the body under the surface.
// The direction of travel is kept apart from the velocity: the first solid hit zeroes the velocity, and where
// solids overlap the next one must still push the body back the way it came, not through the rock.
template <class Body> void moveBody(Body& b, const std::vector<Box>& solids, float dt, bool waterOnly) {
  b.ground = false;
  float vx = b.vx, vy = b.vy;
  b.x += vx * dt;
  for (auto& s : solids) if (overlaps(boxOf(b), s)) { b.x = vx > 0 ? s.x - b.w : s.x + s.w; b.vx = 0; }
  b.y += vy * dt;
  for (auto& s : solids)
    if (overlaps(boxOf(b), s)) {
      if (vy > 0) { b.y = s.y - b.h; b.ground = true; } else b.y = s.y + s.h;
      b.vy = 0;
    }
  if (waterOnly && b.y < 0) { b.y = 0; b.vy = std::max(0.f, b.vy); }
}

Enemy makeEnemy(Vector2 s, bool thought) {
  Enemy e;
  e.thought = thought; e.spawn = s; e.x = s.x; e.y = s.y; e.w = thought ? 28.f : 40.f; e.ph = rnd() * 6;
  return e;
}

void applyBonus(Tunables& t, const std::string& kind) {  // lore: wrappers protect the cranium and grant bonuses
  if (kind == "focus") t.focusTime *= .6f;
  else if (kind == "protection") t.maxHp += 25;
  else if (kind == "dust") t.dustPerBlood *= .6f;
  else t.manaRegen += 3;
}

}  // namespace

// ---------------------------------------------------------------- queries
float World::headY() const { return player.y + player.h - (character ? character->height : 96); }
float World::lightR() const {
  const Player& P = player;
  return (tun.lightBase + P.dust * tun.dustLight + (P.flare > 0 ? tun.flareLight * std::min(1.f, P.flare / .6f) : 0)) *
         (1 + std::sin(t * 13) * std::sin(t * 7.1f) * .03f);
}
int World::depthM() const { return std::max(0, int(std::round(player.y / 10))); }
bool World::solidAt(float x, float y) const {
  return std::any_of(solids->begin(), solids->end(), [&](const Box& s) { return x >= s.x && x < s.x + s.w && y >= s.y && y < s.y + s.h; });
}

void World::burst(float x, float y, int n, Color c, float speed, float g, float life) {
  for (int i = 0; i < n; i++) {
    float a = frand() * 6.283f, v = frand() * speed;
    particles.push_back({x, y, std::cos(a) * v, std::sin(a) * v, g, life * (.5f + frand() * .5f), life, c, 1 + frand() * 2});
  }
}
void World::say(std::string title, std::string line) { banner = {std::move(title), std::move(line), 4}; }

// ---------------------------------------------------------------- game flow
void World::enterSea() {
  inPocket = false; solids = &level.solids; W = level.width; H = level.height;
  enemies.clear();
  for (auto s : level.lurkers) enemies.push_back(makeEnemy(s, false));
  for (auto s : level.thoughts) enemies.push_back(makeEnemy(s, true));
  shots.clear();
}

void World::reset() {
  if (pocketSolids_.empty())
    pocketSolids_ = {{-300, -300, 300, pocket::H + 600}, {pocket::W, -300, 300, pocket::H + 600}, {-300, -300, pocket::W + 600, 300},
                     {-300, pocket::H, pocket::W + 600, 300}, {60, 640, 260, 24}, {420, 520, 160, 24}, {650, 600, 180, 24},
                     {880, 450, 160, 24}, {1100, 340, 220, 24}};
  for (auto& w : level.wrappers) w.taken = false;
  retune();
  enterSea();
  player = Player{};
  player.x = level.spawn.x - player.w / 2; player.y = level.spawn.y; player.hp = tun.maxHp; player.mana = tun.maxMana * .6f;
  player.anim.reset();
  particles.clear(); outcome = Outcome::Playing; msg.clear(); t = 0; photoOff = 0; warp = 0;
  cracks[0] = {{level.portal.x - 30, level.portal.y - 40, 60, 80}, true, false};
  cracks[1] = {{level.empty.x - 15, level.empty.y - 24, 30, 48}, false, false};
  portalSeen = false;
  for (auto& x : level.texts) x.read = false;
  cam = {player.x - view.x / 2, player.y - view.y * .33f};
  say(level.name == "The Abyss" ? "THE ABYSS" : level.name, "your mind is the only lamp down here");
}

void World::enterPocket() {
  inPocket = true; solids = &pocketSolids_; W = pocket::W; H = pocket::H;
  enemies.clear();
  for (Vector2 s : {Vector2{520, 420}, {800, 380}, {1000, 260}, {700, 250}}) enemies.push_back(makeEnemy(s, true));
  shots.clear();
  player.x = 200; player.y = 640 - player.h; player.vx = player.vy = 0; player.portalCd = 1.5f;
  cam = {0, pocket::H - view.y}; fade = 1;
  say("INSIDE THE CRACK", "a mind that never stopped dreaming");
}

void World::leavePocket() {
  enterSea();
  player.x = cracks[0].r.x - 60; player.y = cracks[0].r.y + 20; player.vx = player.vy = 0; player.portalCd = 1.5f;
  cam.y = player.y - view.y * .33f; fade = 1;
}

void World::die(const char* why) {
  outcome = Outcome::Dead; msg = why;
  burst(player.x + player.w / 2, player.y, 60, hex("#8c7aa8"), 200, -30, 2);
  audio.thump(1);
}

void World::pay(float cost) {  // the core mechanic: mana first, then blood; blood turns the brain to stardust
  Player& P = player;
  float m = std::min(P.mana, cost), owed = cost - m;
  P.mana -= m;
  if (owed <= 0) return;
  P.hp -= owed * tun.bloodRatio;
  P.dust = std::min(100.f, P.dust + owed * tun.dustPerBlood);
  P.bleed = .4f;
  burst(P.x + P.w / 2, P.y + P.h * .3f, 18, hex("#5e1019"), 120, 40, 1.2f);
  audio.hiss(300, 80, .45f, .4f);
  if (P.hp <= 0) die("You fed on the last of your blood.");
  else if (P.dust >= 100) die("Your mind scattered into the dark water.");
}

void World::damage(Enemy& e, float d, float dir) {
  e.hp -= d; e.flash = .1f; e.vx += dir * 220;
  if (e.mode == EnemyMode::Lunge) { e.mode = EnemyMode::Recover; e.mt = 1; }
  burst(e.x + e.w / 2, e.y + e.h / 2, 8, hex("#9aa6a8"), 120);
  audio.hiss(1500, 500, .12f, .2f);
  if (e.hp <= 0) { e.alive = false; e.respawn = tun.respawn; burst(e.x + e.w / 2, e.y + e.h / 2, 26, hex("#2c3638"), 160, 0, 1.4f); }
}

void World::hurt(float d, float dir) {
  Player& P = player;
  P.hp -= d; P.inv = 1; P.kb = .2f; P.vx = dir * 260; P.vy = -120; shake = .35f;
  burst(P.x + P.w / 2, P.y + P.h * .4f, 16, hex("#5e1019"), 140, 0, 1.2f);
  audio.thump(1); audio.hiss(400, 100, .3f, .3f);
  if (P.hp <= 0) die("Something in the dark took you.");
}

void World::retune() {
  tun = base;
  for (auto& w : level.wrappers) if (w.taken) applyBonus(tun, w.kind);
}

const char* World::wrapperBonus(const Wrapper& w) {
  retune();
  if (w.kind == "focus") return "Focus: you mend faster";
  if (w.kind == "protection") { player.hp += 25; return "Protection: your body holds more"; }
  if (w.kind == "dust") return "Status: the dust takes longer to claim you";
  return "Mana: the wrapper drinks from the sea for you";
}

// ---------------------------------------------------------------- update
void World::update(float dt, const Input& in) {
  step(dt, in);
  if (outcome == Outcome::Playing) animatePlayer(dt);
}

void World::step(float dt, const Input& in) {
  t += dt; shake = std::max(0.f, shake - dt); banner.t -= dt; fade = std::max(0.f, fade - dt * 1.5f); photoOff -= dt;
  for (auto& p : particles) { p.x += p.vx * dt; p.y += p.vy * dt; p.vy += p.g * dt; p.vx *= std::pow(.3f, dt); p.life -= dt; }
  std::erase_if(particles, [](const Particle& p) { return p.life <= 0; });
  if (particles.size() > 900) particles.erase(particles.begin(), particles.end() - 900);
  if (outcome != Outcome::Playing) { if (in.tap(A_RESTART) || in.tap(A_UP)) reset(); return; }
  if (in.tap(A_RESTART)) { reset(); return; }

  movePlayer(dt, in);
  updateBody(dt);
  if (outcome != Outcome::Playing) return;
  castSpells(in);
  updateShots(dt);
  updateEnemies(dt);
  if (updateTriggers(dt)) return;
  updateFearAndCamera(dt);
}

void World::movePlayer(float dt, const Input& in) {
  Player& P = player;
  int dir = int(in.held(A_RIGHT)) - int(in.held(A_LEFT));
  bool wasIn = P.inWater;
  P.inWater = inPocket || P.y + P.h * .35f > 0;  // chest below the surface
  if (P.inWater != wasIn) {                       // splash
    burst(P.x + P.w / 2, 0, 26, hex("#cfe4ea"), 170, 320, .9f);
    audio.hiss(1800, 300, .5f, .22f);
    if (P.inWater) P.vy *= .35f;
  }
  P.focusing = in.held(A_FOCUS) && !dir && P.mana >= tun.focusCost && P.kb <= 0 && (P.inWater || P.ground);
  if (P.kb > 0) P.kb -= dt;
  if (P.inWater) {  // swimming
    if (P.kb <= 0 && !P.focusing) { P.vx += dir * 1100 * dt; if (dir) P.face = dir; }
    P.vx *= std::pow(.04f, dt); P.vx = clampf(P.vx, -tun.swim, tun.swim);
    P.vy += ((inPocket ? 50 : tun.sink) + (in.held(A_DOWN) ? 380 : 0)) * dt; P.vy *= std::pow(.25f, dt);
    if (P.focusing) P.vy *= std::pow(.01f, dt);
    if (in.tap(A_UP) && P.strokeCd <= 0 && !P.focusing) {
      bool breach = !inPocket && P.y < 30;  // just under the surface: leap out of the water
      P.vy = breach ? -tun.jump * .95f : -tun.stroke; P.strokeCd = .22f; P.stroke = 1; audio.hiss(900, 300, .25f, .06f);
      for (int i = 0; i < 4; i++) particles.push_back({P.x + P.w / 2, P.y + P.h, (frand() - .5f) * 40, 20, -120, 1, 1, hex("#8fa6ae"), 2});
    }
    P.vy = std::min(P.vy, 420.f);
  } else {  // on land: weighty jump. Fast rise, heavier fall, little float at the apex, committed air control
    float accel = tun.groundAccel * (P.ground ? 1 : tun.airControl);
    if (P.kb <= 0 && !P.focusing) { P.vx += (dir * tun.runSpeed - P.vx) * std::min(1.f, dt * accel); if (dir) P.face = dir; }
    if (P.focusing) P.vx = 0;
    P.coyote = P.ground ? tun.coyoteTime : P.coyote - dt;
    P.jumpBuf = in.tap(A_UP) ? tun.jumpBuffer : P.jumpBuf - dt;
    if (P.jumpBuf > 0 && P.coyote > 0 && !P.focusing) {
      P.vy = -tun.jump; P.coyote = P.jumpBuf = 0; P.jumping = true;
      audio.hiss(500, 200, .15f, .05f);
      burst(P.x + P.w / 2, P.y + P.h, 6, hex("#8a6a4a"), 60, 200, .4f);
    }
    if (P.jumping && !in.held(A_UP) && P.vy < 0) { P.vy *= tun.jumpCut; P.jumping = false; }  // release early: the rise is cut short
    if (P.vy >= 0) P.jumping = false;
    P.vy = std::min(P.vy + (P.vy < 0 ? tun.landGravity : tun.fallGravity) * dt, tun.maxFall);
  }
  float fallSpeed = P.vy;
  bool wasGround = P.ground;
  moveBody(P, *solids, dt, false);
  if (!P.inWater && P.ground && !wasGround && fallSpeed > 350) {  // a landing with weight
    P.land = .14f;
    burst(P.x + P.w / 2, P.y + P.h, 10 + int(fallSpeed / 120), hex("#8a6a4a"), 90, 300, .5f);
    audio.thump(clampf(fallSpeed / 1800, .15f, .5f));
  }
  if (P.y < level.top && !inPocket) { P.y = level.top; P.vy = std::max(0.f, P.vy); }
  if (P.x < 0 || P.x > W - P.w) { P.x = clampf(P.x, 0, W - P.w); P.vx = 0; }  // the level's edges are walls: the camera stops there too

  // squash on a stroke and on a heavy landing (the player never rotates)
  P.stroke = std::max(0.f, P.stroke - dt * 2.2f);
  P.castHold = std::max(0.f, P.castHold - dt);
  P.strike = std::max(0.f, P.strike - dt);
  P.land = std::max(0.f, P.land - dt);
  P.squash = 1 - .08f * std::sin(P.stroke * PI) - .14f * P.land / .14f;
}

void World::updateBody(float dt) {
  Player& P = player;
  if (P.focusing) {
    P.focus += dt;
    if (P.focus >= tun.focusTime) {
      P.focus = 0; P.mana -= tun.focusCost; P.hp = std::min(tun.maxHp, P.hp + tun.focusHeal);
      burst(P.x + P.w / 2, P.y + P.h / 2, 24, hex("#7a98a8"), 140);
    }
  } else P.focus = 0;
  P.mana = std::min(tun.maxMana, P.mana + tun.manaRegen * dt);
  P.dust = std::max(0.f, P.dust - tun.dustDecay * dt);
  if (!inPocket && depthM() > tun.pressureDepth) P.dust = std::min(100.f, P.dust + tun.pressure * dt);
  // places that burst the skulls of the mages who imagined them
  float warpTarget = 0;
  if (!inPocket)
    for (auto& z : level.zones)
      if (overlaps(boxOf(P), z.r)) { P.dust = std::min(100.f, P.dust + z.rate * dt); warpTarget = 1; }
  warp += (warpTarget - warp) * std::min(1.f, dt * 2);
  if (P.dust >= 100) { die("The deep crushed your mind into stars."); return; }
  for (float* k : {&P.inv, &P.atk, &P.atkCd, &P.cast, &P.bleed, &P.strokeCd, &P.flare, &P.portalCd}) *k -= dt;
  if (P.inWater && frand() < dt * 1.5f) particles.push_back({P.x + P.w / 2 + 6.f * P.face, headY() + 14, 0, 0, -60, 1.5f, 1.5f, hex("#8fa6ae"), 1.5f});
  if (frand() < dt * P.dust * .3f) {  // stardust leaking from the swelling cranium
    float r = 10 * (1 + P.dust / 100 * 1.4f);
    particles.push_back({P.x + P.w / 2 + (frand() - .5f) * r, headY() - r, (frand() - .5f) * 30, -20 - frand() * 30, -10, 1.4f, 1.4f,
                         frand() < .5f ? hex("#8f7448") : hex("#8c7aa8"), 1.5f});
  }
}

void World::castSpells(const Input& in) {
  Player& P = player;
  float cx = P.x + P.w / 2, cy = P.y + P.h / 2;
  if (in.tap(A_STAFF) && P.atkCd <= 0) {
    P.atk = .15f; P.atkCd = .32f; P.strike = .36f; audio.hiss(700, 250, .15f, .08f);
    Box hb{P.face > 0 ? P.x + P.w : P.x - 70, P.y, 70, P.h};  // the spear reaches further
    for (auto& e : enemies)
      if (e.alive && overlaps(hb, boxOf(e))) { damage(e, tun.staffDmg, P.face); P.mana = std::min(tun.maxMana, P.mana + tun.manaPerHit); }
  }
  if (in.tap(A_BOLT) && P.cast <= 0) {
    P.cast = .25f; P.castHold = .35f; pay(tun.boltCost); audio.hiss(2500, 300, .5f, .2f);
    shots.push_back({cx + P.face * 26, P.y + P.h * .3f, P.face * 560.f, 1.4f, tun.boltDmg * power(), 5 + P.dust / 25});
  }
  if (in.tap(A_FLARE) && P.cast <= 0) {
    P.cast = .5f; P.castHold = .35f; pay(tun.flareCost); P.flare = tun.flareTime; shake = .15f; photoOff = 25;
    audio.hiss(600, 4000, .9f, .25f); audio.thump(.6f);
    burst(cx, P.y + P.h * .2f, 40, hex("#c8c0d4"), 260);
    for (auto& e : enemies)
      if (e.alive && std::hypot(e.x + e.w / 2 - cx, e.y + e.h / 2 - cy) < 170) damage(e, tun.flareDmg * power(), (e.x > P.x) ? 1.f : -1.f);
  }
}

void World::updateShots(float dt) {
  for (auto& s : shots) {
    s.x += s.vx * dt; s.vx *= std::pow(.6f, dt); s.life -= dt;
    particles.push_back({s.x, s.y + (frand() - .5f) * 6, 0, 0, -20, .4f, .4f, hex("#8c7aa8"), 2});
    if (solidAt(s.x, s.y)) { s.life = 0; burst(s.x, s.y, 10, hex("#8c7aa8"), 100); }
    for (auto& e : enemies)
      if (e.alive && s.life > 0 && s.x > e.x && s.x < e.x + e.w && s.y > e.y - 8 && s.y < e.y + e.h + 8) {
        damage(e, s.dmg, s.vx > 0 ? 1.f : -1.f); s.life = 0; burst(s.x, s.y, 14, hex("#8f7448"), 140);
      }
  }
  std::erase_if(shots, [](const Shot& s) { return s.life <= 0; });
}

// lurkers drift with lure lit, notice you in proportion to your light, go dark, stalk, then lunge
void World::updateEnemies(float dt) {
  Player& P = player;
  float cx = P.x + P.w / 2, cy = P.y + P.h / 2;
  float sight = lightR() * tun.lureSight, closest = 0;
  for (auto& e : enemies) {
    if (!e.alive) { if ((e.respawn -= dt) <= 0) e = makeEnemy(e.spawn, e.thought); continue; }
    e.flash -= dt; e.ph += dt; e.mt -= dt;
    float dx = cx - (e.x + e.w / 2), dy = cy - (e.y + e.h / 2), d = std::max(1.f, std::hypot(dx, dy));
    closest = std::max(closest, clampf(1 - d / 320, 0, 1));
    float sees = e.thought ? 320 : sight, tx = 0, ty = 0;
    switch (e.mode) {
      case EnemyMode::Drift:
        tx = ((e.spawn.x - e.x) / 100 + std::cos(e.ph * .5f)) * 40; ty = ((e.spawn.y - e.y) / 100 + std::sin(e.ph * .7f) * .5f) * 40;
        if (d < sees) { e.mode = EnemyMode::Stalk; e.mt = .8f + frand() * 1.6f; }
        break;
      case EnemyMode::Stalk: {
        float k = d > 160 ? 90 : d < 110 ? -70 : 0;
        tx = dx / d * k - dy / d * 40; ty = dy / d * k + dx / d * 40;
        if (e.mt <= 0 && d < 280) {
          e.mode = EnemyMode::Lunge; e.mt = .4f; e.lx = dx / d; e.ly = dy / d;
          if (!e.thought) audio.hiss(5000, 1800, .3f, .14f);
        }
        if (d > sees * 1.6f && d > 260) e.mode = EnemyMode::Drift;
        break;
      }
      case EnemyMode::Lunge:
        e.vx = e.lx * tun.lungeSpeed; e.vy = e.ly * tun.lungeSpeed;
        if (e.mt <= 0) { e.mode = EnemyMode::Recover; e.mt = 1.2f; }
        break;
      case EnemyMode::Recover:
        tx = -dx / d * 50; ty = -dy / d * 50;
        if (e.mt <= 0) e.mode = EnemyMode::Drift;
        break;
    }
    if (e.mode != EnemyMode::Lunge) { e.vx += (tx - e.vx) * dt * 2; e.vy += (ty - e.vy) * dt * 2; }
    if (e.mode == EnemyMode::Stalk || e.mode == EnemyMode::Lunge) e.face = dx >= 0 ? 1 : -1;
    else if (std::fabs(e.vx) > 5) e.face = e.vx > 0 ? 1 : -1;
    moveBody(e, *solids, dt, true);
    if (P.inv <= 0 && overlaps(boxOf(e), boxOf(P))) hurt(12, P.x >= e.x ? 1.f : -1.f);
  }
  closestEnemy_ = closest;
}

bool World::updateTriggers(float dt) {
  Player& P = player;
  float cx = P.x + P.w / 2, cy = P.y + P.h / 2;
  if (!inPocket) {
    for (auto& k : cracks) {
      if (!overlaps(boxOf(P), boxOf(k.r))) continue;
      if (k.universe && P.portalCd <= 0) { enterPocket(); return true; }
      if (!k.universe && !k.seen) { k.seen = true; say("", "This crack is empty. Whoever dreamed here is gone."); }
    }
    for (auto& w : level.wrappers)
      if (!w.taken && std::hypot(cx - w.x, cy - w.y) < 26) {
        w.taken = true; say("A DROWNED WRAPPER", wrapperBonus(w)); audio.hiss(1200, 3000, .8f, .12f); burst(w.x, w.y, 30, hex("#c8a868"), 120);
      }
    for (auto& x : level.texts)
      if (!x.read && std::hypot(cx - x.x, cy - x.y) < 150) { x.read = true; audio.hiss(3200, 1400, 1.2f, .05f); }

    // stardust: the dreaming mind evaporates out of the portal crack, thicker as you come near
    Vector2 pc = portalCentre();
    float near = clampf(1 - std::hypot(cx - pc.x, cy - pc.y) / 600, 0, 1);
    for (float n = dt * (30 + near * 90); n > 0; n -= 1)
      if (frand() < n) {
        static const char* cols[] = {"#b89cff", "#8c7aa8", "#d8b45a", "#e8e2ff"};
        float a = frand() * 6.283f;
        particles.push_back({pc.x + std::cos(a) * 18 * frand(), pc.y + std::sin(a) * 26 * frand(),
                             std::cos(a) * (20 + 40 * frand()) + std::sin(t * .9f) * 25, -25 - frand() * 60, -18, 2 + frand() * 1.5f, 3.5f,
                             hex(cols[GetRandomValue(0, 3)]), 1.2f + frand() * 1.6f});
      }
  } else {
    if (P.portalCd <= 0 && overlaps(boxOf(P), boxOf(pocket::exitCrack))) { leavePocket(); return true; }
    if (overlaps(boxOf(P), boxOf(pocket::altar))) {
      outcome = Outcome::Won; msg = "A mind, still dreaming inside its skull.";
      burst(pocket::altar.x + 15, pocket::altar.y, 60, hex("#e8dcc4"), 200, -20, 2);
    }
  }
  return false;
}

void World::updateFearAndCamera(float dt) {
  Player& P = player;
  float cx = P.x + P.w / 2, cy = P.y + P.h / 2, closest = closestEnemy_;
  fear_ = inPocket ? closest * .5f
                   : clampf(clampf(P.y / 4000, 0, 1) * .35f + (1 - P.hp / tun.maxHp) * .35f + closest * .5f + (nearActive ? .6f : 0), 0, 1);
  if (fear_ > .3f && (beatT_ -= dt) <= 0) { audio.thump(fear_); beatT_ = 60 / (50 + fear_ * 90); }

  cam.x += (cx - view.x / 2 - cam.x) * std::min(1.f, dt * 5);
  cam.x = view.x > W ? (W - view.x) / 2 : clampf(cam.x, 0, W - view.x);
  cam.y += (cy - view.y * (P.inWater ? .33f : .55f) - cam.y) * std::min(1.f, dt * 4);  // in the water: more of the black below
  cam.y = clampf(cam.y, inPocket ? -140 : level.top, H + 100 - view.y);
}

// feeds the character's state machine; the names here are what characters/*.json conditions can test
void World::animatePlayer(float dt) {
  if (!character) return;
  const Player& P = player;
  AnimParams p{{"strike", P.strike},
               {"castHold", P.castHold},
               {"flare", P.flare > tun.flareTime - .45f},
               {"inWater", P.inWater},
               {"ground", P.ground},
               {"focusing", P.focusing},
               {"hurt", P.kb > 0},
               {"vx", P.vx},
               {"vy", P.vy},
               {"speed", std::fabs(P.vx)},
               {"runRatio", std::fabs(P.vx) / tun.runSpeed}};
  player.anim.update(*character, p, dt);
}
