#include "world.hpp"

#include <algorithm>
#include <cmath>

#include "util.hpp"

namespace {

const Content NO_CONTENT;

Box boxOf(const Player& p) { return {p.x, p.y, p.w, p.h}; }
Box boxOf(const Enemy& e) { return {e.x, e.y, e.w, e.h}; }
Box boxOf(const Rectangle& r) { return {r.x, r.y, r.width, r.height}; }

// axis-separated collision against the solids (open seals let you through); waterOnly keeps the body under the surface.
// The direction of travel is kept apart from the velocity: the first solid hit zeroes the velocity, and where
// solids overlap the next one must still push the body back the way it came, not through the rock.
template <class Body> void moveBody(Body& b, const std::vector<Box>& solids, float dt, bool waterOnly) {
  b.ground = false;
  float vx = b.vx, vy = b.vy;
  b.x += vx * dt;
  for (auto& s : solids) if (!s.open && overlaps(boxOf(b), s)) { b.x = vx > 0 ? s.x - b.w : s.x + s.w; b.vx = 0; }
  b.y += vy * dt;
  for (auto& s : solids)
    if (!s.open && overlaps(boxOf(b), s)) {
      if (vy > 0) { b.y = s.y - b.h; b.ground = true; } else b.y = s.y + s.h;
      b.vy = 0;
    }
  if (waterOnly && b.y < 0) { b.y = 0; b.vy = std::max(0.f, b.vy); }
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
  return (tun.lightBase + P.dust * tun.dustLight + (P.flare > 0 ? P.flareLight * std::min(1.f, P.flare / .6f) : 0)) *
         (1 + std::sin(t * 13) * std::sin(t * 7.1f) * .03f);
}
bool World::lit(float x, float y) const { return std::hypot(x - (player.x + player.w / 2), y - (headY() + 20)) < lightR(); }
bool World::seen(float x, float y, float reveal) const { return reveal < 0 || (player.dust >= reveal && lit(x, y)); }
float World::cheapestSpell() const {
  float c = 0;
  for (auto& m : content->moves.list) if (float k = m.num("mana"); k > 0 && (c == 0 || k < c)) c = k;
  return c;
}
int World::depthM() const { return std::max(0, int(std::round(player.y / 10))); }
bool World::solidAt(float x, float y) const {
  return std::any_of(solids->begin(), solids->end(),
                     [&](const Box& s) { return !s.open && x >= s.x && x < s.x + s.w && y >= s.y && y < s.y + s.h; });
}
Box* World::solidHit(float x, float y) {
  for (auto& s : *solids) if (!s.open && x >= s.x && x < s.x + s.w && y >= s.y && y < s.y + s.h) return &s;
  return nullptr;
}

void World::burst(float x, float y, int n, Color c, float speed, float g, float life) {
  for (int i = 0; i < n; i++) {
    float a = frand() * 6.283f, v = frand() * speed;
    particles.push_back({x, y, std::cos(a) * v, std::sin(a) * v, g, life * (.5f + frand() * .5f), life, c, 1 + frand() * 2});
  }
}
void World::say(std::string title, std::string line) { banner = {std::move(title), std::move(line), 4}; }

// ---------------------------------------------------------------- game flow
Enemy World::makeEnemy(Vector2 s, int type) const {
  const Record& T = content->enemies.list[type];
  Enemy e;
  e.type = type; e.spawn = s; e.x = s.x; e.y = s.y; e.w = T.num("w", 40); e.h = T.num("h", 18); e.hp = T.num("hp", 30); e.ph = rnd() * 6;
  return e;
}

void World::spawnEnemies(const std::vector<Spawn>& list) {
  for (auto& s : list) {
    int k = content->enemies.find(s.type);
    if (k < 0) { TraceLog(LOG_WARNING, "level: no enemy type \"%s\" in data/enemies.json", s.type.c_str()); continue; }
    enemies.push_back(makeEnemy({s.x, s.y}, k));
  }
}

void World::enterSea() {
  inPocket = false; solids = &level.solids; W = level.width; H = level.height;
  enemies.clear();
  spawnEnemies(level.enemies);
  shots.clear();
}

void World::reset() {
  if (!content) content = &NO_CONTENT;
  if (pocketSolids_.empty())
    pocketSolids_ = {{-300, -300, 300, pocket::H + 600}, {pocket::W, -300, 300, pocket::H + 600}, {-300, -300, pocket::W + 600, 300},
                     {-300, pocket::H, pocket::W + 600, 300}, {60, 640, 260, 24}, {420, 520, 160, 24}, {650, 600, 180, 24},
                     {880, 450, 160, 24}, {1100, 340, 220, 24}};
  for (auto& w : level.wrappers) w.taken = false;
  for (auto& it : level.items) it.taken = false;
  for (auto& s : level.solids) s.open = s.tried = false;
  retune();
  enterSea();
  player = Player{};
  player.x = level.spawn.x - player.w / 2; player.y = level.spawn.y; player.hp = tun.maxHp; player.mana = tun.maxMana * .6f;
  player.items.assign(content->items.list.size(), 0);
  player.anim.reset();
  particles.clear(); outcome = Outcome::Playing; msg.clear(); t = 0; photoOff = 0; warp = 0; noise = 0;
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
  spawnEnemies({{"thought", 520, 420}, {"thought", 800, 380}, {"thought", 1000, 260}, {"thought", 700, 250}});
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

// the core mechanic: mana first, then blood (bloodRatio HP per missing mana point); blood turns the brain to stardust
void World::pay(float cost, float bloodRatio) {
  Player& P = player;
  float m = std::min(P.mana, cost), owed = cost - m;
  P.mana -= m;
  if (owed <= 0) return;
  P.hp -= owed * bloodRatio;
  P.dust = std::min(100.f, P.dust + owed * tun.dustPerBlood);
  P.bleed = .4f;
  burst(P.x + P.w / 2, P.y + P.h * .3f, 18, hex("#5e1019"), 120, 40, 1.2f);
  audio.hiss(300, 80, .45f, .4f);
  if (P.hp <= 0) die("You fed on the last of your blood.");
  else if (P.dust >= 100) die("Your mind scattered into the dark water.");
}

// the enemy type's "resist.<damage type>" multiplies the damage: 0 is immune (a dry glance, so you try something
// else), below 0 it feeds on it (heals, up to twice its health), above 1 it is weak to it
bool World::damage(Enemy& e, float d, const std::string& type, float dir, float knockback) {
  const Record& T = enemyType(e);
  float k = T.num("resist." + type, 1), cx = e.x + e.w / 2, cy = e.y + e.h / 2;
  if (k == 0) {
    e.vx += dir * 40;
    burst(cx - dir * e.w * .3f, cy, 7, hex("#f0ead8"), 240, 0, .2f);
    audio.hiss(5200, 3800, .07f, .18f);
    return false;
  }
  if (k < 0) {
    e.hp = std::min(T.num("hp", 30) * 2, e.hp - d * k);
    burst(cx, cy, 16, hex("#b89cff"), 70, -30, .9f);
    audio.hiss(260, 900, .35f, .2f);
    return false;
  }
  e.hp -= d * k; e.flash = .1f; e.vx += dir * knockback;
  if (e.mode == EnemyMode::Lunge) { e.mode = EnemyMode::Recover; e.mt = 1; }
  burst(cx, cy, 8, hex("#9aa6a8"), 120);
  if (k > 1) burst(cx, cy, 10, hex("#d8b45a"), 160, 0, .4f);  // a weak spot
  audio.hiss(1500, 500, .12f, .2f);
  if (e.hp <= 0) { e.alive = false; e.respawn = T.num("respawn", 10); burst(cx, cy, 26, hex("#2c3638"), 160, 0, 1.4f); }
  return true;
}

void World::hurt(float d, float dir) {
  Player& P = player;
  P.hp -= d; P.inv = 1; P.kb = .2f; P.vx = dir * 260; P.vy = -120; shake = .35f;
  burst(P.x + P.w / 2, P.y + P.h * .4f, 16, hex("#5e1019"), 140, 0, 1.2f);
  audio.thump(1); audio.hiss(400, 100, .3f, .3f);
  if (P.hp <= 0) die("Something in the dark took you.");
}

// a parry caught the enemy's touch: it reels (stun), you take nothing and drink mana from the clash
void World::parried(Enemy& e) {
  Player& P = player;
  const Record& m = content->moves.list[P.parryMove];
  float dir = e.x + e.w / 2 > P.x + P.w / 2 ? 1.f : -1.f;
  P.parry = 0; P.inv = std::max(P.inv, .3f);
  P.mana = std::min(tun.maxMana, P.mana + m.num("manaGain"));
  e.mode = EnemyMode::Recover; e.mt = m.num("stun", 1.4f); e.vx = dir * 320; e.vy = -60;
  damage(e, m.num("damage"), m.str("damageType", "pierce"), dir, 0);
  shake = .25f; makeNoise(1);
  burst(P.x + P.w / 2 + dir * 20, P.y + P.h * .4f, 22, hex("#f2e6c8"), 260, 0, .3f);
  audio.thump(.5f); audio.hiss(6000, 2500, .25f, .3f);
}

// a stardust seal opens to a star bolt cast with at least seal % stardust; anything less glances off
void World::hitSeal(Box& s, const std::string& type, float x, float y) {
  if (type == "star" && player.dust >= s.seal) {
    s.open = true; shake = .4f;
    for (int i = 0; i < 70; i++)
      particles.push_back({s.x + frand() * s.w, s.y + frand() * s.h, (frand() - .5f) * 80, -20 - frand() * 60, -15, 2.5f, 2.5f,
                           frand() < .5f ? hex("#b89cff") : hex("#d8b45a"), 1.5f + frand() * 1.5f});
    audio.thump(.8f); audio.hiss(200, 3000, 1.2f, .3f);
    return;
  }
  burst(x, y, 10, hex("#8c7aa8"), 80);
  audio.hiss(4800, 3600, .08f, .14f);
  if (!s.tried) { s.tried = true; say("", "The seal drinks the bolt. It wants more of your mind."); }
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
  noise *= std::pow(.25f, dt);
  for (auto& p : particles) { p.x += p.vx * dt; p.y += p.vy * dt; p.vy += p.g * dt; p.vx *= std::pow(.3f, dt); p.life -= dt; }
  std::erase_if(particles, [](const Particle& p) { return p.life <= 0; });
  if (particles.size() > 900) particles.erase(particles.begin(), particles.end() - 900);
  if (outcome != Outcome::Playing) { if (in.tap(A_RESTART) || in.tap(A_UP)) reset(); return; }
  if (in.tap(A_RESTART)) { reset(); return; }

  movePlayer(dt, in);
  updateBody(dt);
  if (outcome != Outcome::Playing) return;
  useMoves(dt, in);
  useItem(in);
  updateShots(dt);
  updateEnemies(dt);
  if (outcome != Outcome::Playing) return;  // no portal or altar on the frame you die
  if (updateTriggers(dt)) return;
  updateFearAndCamera(dt);
}

void World::movePlayer(float dt, const Input& in) {
  Player& P = player;
  P.dir = int(in.held(A_RIGHT)) - int(in.held(A_LEFT));
  int dir = P.dir;
  P.jumped = false;
  bool wasIn = P.inWater;
  P.inWater = inPocket || P.y + P.h * .35f > 0;  // chest below the surface
  if (P.inWater != wasIn) {                       // splash
    burst(P.x + P.w / 2, 0, 26, hex("#cfe4ea"), 170, 320, .9f);
    audio.hiss(1800, 300, .5f, .22f);
    if (P.inWater) P.vy *= .35f;
  }
  P.focusing = in.held(A_FOCUS) && !dir && P.mana >= tun.focusCost && P.kb <= 0 && P.dash <= 0 && (P.inWater || P.ground);
  if (P.kb > 0) P.kb -= dt;
  if (P.dash > 0) {  // a dodge: a straight dash, no gravity, no steering
    P.dash -= dt; P.coyote -= dt; P.jumpBuf -= dt;
    P.vx = P.dashDir * P.dashSpeed; P.vy = 0;
    if (P.dash <= 0) P.vx = P.dashDir * std::min(P.dashSpeed, P.inWater ? tun.swim : tun.runSpeed);
    if (frand() < .6f) particles.push_back({P.x + P.w / 2, P.y + P.h * (.3f + frand() * .6f), -P.vx * .1f, 0, 0, .35f, .35f, hex("#9aa6a8"), 2});
  } else if (P.inWater) {  // swimming
    if (P.kb <= 0 && !P.focusing) { P.vx += dir * 1100 * dt; if (dir) P.face = dir; }
    P.vx *= std::pow(.04f, dt); P.vx = clampf(P.vx, -tun.swim, tun.swim);
    P.vy += ((inPocket ? 50 : tun.sink) + (in.held(A_DOWN) ? 380 : 0)) * dt; P.vy *= std::pow(.25f, dt);
    if (P.focusing) P.vy *= std::pow(.01f, dt);
    if (in.tap(A_UP) && P.strokeCd <= 0 && !P.focusing) {
      bool breach = !inPocket && P.y < 30;  // just under the surface: leap out of the water
      P.vy = breach ? -tun.jump * .95f : -tun.stroke; P.strokeCd = .22f; P.stroke = 1; audio.hiss(900, 300, .25f, .06f);
      makeNoise(.6f);
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
      P.vy = -tun.jump; P.coyote = P.jumpBuf = 0; P.jumping = P.jumped = true;
      audio.hiss(500, 200, .15f, .05f); makeNoise(.5f);
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
    makeNoise(.9f);
  }
  if (P.y < level.top && !inPocket) { P.y = level.top; P.vy = std::max(0.f, P.vy); }
  if (P.x < 0 || P.x > W - P.w) { P.x = clampf(P.x, 0, W - P.w); P.vx = 0; }  // the level's edges are walls: the camera stops there too

  // squash on a stroke and on a heavy landing (the player never rotates)
  P.stroke = std::max(0.f, P.stroke - dt * 2.2f);
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
  for (float* k : {&P.inv, &P.bleed, &P.strokeCd, &P.flare, &P.portalCd, &P.parry}) *k -= dt;
  P.cooldowns.tick(dt);
  P.anims.tick(dt);
  if (P.inWater && frand() < dt * 1.5f) particles.push_back({P.x + P.w / 2 + 6.f * P.face, headY() + 14, 0, 0, -60, 1.5f, 1.5f, hex("#8fa6ae"), 1.5f});
  if (frand() < dt * P.dust * .3f) {  // stardust leaking from the swelling cranium
    float r = 10 * (1 + P.dust / 100 * 1.4f);
    particles.push_back({P.x + P.w / 2 + (frand() - .5f) * r, headY() - r, (frand() - .5f) * 30, -20 - frand() * 30, -10, 1.4f, 1.4f,
                         frand() < .5f ? hex("#8f7448") : hex("#8c7aa8"), 1.5f});
  }
}

// a move may start when its group is off cooldown, the medium fits ("any", "water", "land", "ground", "air"),
// and a leap has charges left. Spells ("mana" > 0) are always allowed: what you lack is paid in blood.
bool World::moveAllowed(const Record& m) const {
  const Player& P = player;
  if (P.cooldowns.get(m.str("group", m.name)) > 0) return false;
  std::string md = m.str("medium", "any");
  bool air = !P.inWater && !P.ground && P.coyote <= 0 && !P.jumped;
  if ((md == "water" && !P.inWater) || (md == "land" && P.inWater) || (md == "air" && !air) || (md == "ground" && (P.inWater || !P.ground)))
    return false;
  return m.str("kind") != "leap" || P.leaps > 0;
}

// moves come from data/moves.json. One with "hold" fires when its input is released after being held that long (the
// longest one reached wins: a charged thrust); the others fire on the press, so a tap is a quick strike.
void World::useMoves(float dt, const Input& in) {
  Player& P = player;
  const auto& moves = content->moves.list;
  if (P.ground || P.inWater) {
    P.leaps = 0;
    for (auto& m : moves) if (m.str("kind") == "leap") P.leaps = std::max(P.leaps, int(m.num("charges", 1)));
  }
  int charged[A_COUNT];
  std::fill(charged, charged + A_COUNT, -1);
  for (int i = 0; i < int(moves.size()); i++) {
    const Record& m = moves[i];
    Act a = actByName(m.str("input"));
    if (a == A_COUNT) continue;
    float hold = m.num("hold");
    if (hold <= 0) {
      if (in.tap(a) && moveAllowed(m)) doMove(i);
      if (outcome != Outcome::Playing) return;  // a spell paid with the last of your blood
      continue;
    }
    if (in.held(a) && P.holdT[a] < hold && P.holdT[a] + dt >= hold) {  // charged: a glint at the spear tip
      burst(P.x + P.w / 2 + P.face * 40, P.y + P.h * .2f, 10, hex("#f2e6c8"), 70, 0, .3f);
      audio.hiss(3800, 5200, .15f, .07f);
    }
    if (in.released(a) && P.holdT[a] >= hold && moveAllowed(m) && (charged[a] < 0 || hold > moves[charged[a]].num("hold"))) charged[a] = i;
  }
  for (int a = 0; a < A_COUNT; a++) {
    if (charged[a] >= 0) doMove(charged[a]);
    P.holdT[a] = in.held(Act(a)) ? P.holdT[a] + dt : 0;
  }
}

void World::doMove(int i) {
  Player& P = player;
  const Record& m = content->moves.list[i];
  const std::string kind = m.str("kind"), type = m.str("damageType", "slash");
  float cx = P.x + P.w / 2, cy = P.y + P.h / 2, cost = m.num("mana");
  P.cooldowns[m.str("group", m.name)] = m.num("cooldown");
  if (m.has("anim")) P.anims[m.str("anim")] = m.num("animTime", .3f);
  makeNoise(m.num("noise", .5f));
  if (cost > 0) {
    pay(cost, m.num("bloodRatio", 1.2f));
    if (outcome != Outcome::Playing) return;
  }
  float dmg = m.num("damage") * (cost > 0 ? power() : 1);  // stardust strengthens spells, not the spear
  if (kind == "melee") {
    float reach = m.num("reach", 70), hh = m.num("height", P.h);
    Box hb{P.face > 0 ? P.x + P.w : P.x - reach, P.y + (P.h - hh) / 2, reach, hh};
    if (m.has("lunge")) P.vx = P.face * m.num("lunge");
    bool heavy = m.num("hold") > 0;
    if (heavy) {
      audio.hiss(400, 150, .25f, .12f);
      for (int k = 0; k < 8; k++) particles.push_back({P.face > 0 ? hb.x + frand() * reach : hb.x + reach - frand() * reach, P.y + P.h * .35f,
                                                       P.face * 60.f, 0, 0, .25f, .25f, hex("#d8cfbc"), 1.5f});
    } else audio.hiss(700, 250, .15f, .08f);
    for (auto& e : enemies)
      if (e.alive && overlaps(hb, boxOf(e)) && damage(e, dmg, type, float(P.face), m.num("knockback", 220)))
        P.mana = std::min(tun.maxMana, P.mana + m.num("manaGain"));
  } else if (kind == "projectile") {
    shots.push_back({cx + P.face * 26, P.y + P.h * .3f, P.face * m.num("speed", 560), m.num("life", 1.4f), dmg, 5 + P.dust / 25, type});
    audio.hiss(2500, 300, .5f, .2f);
  } else if (kind == "burst") {
    P.flare = m.num("time", 2.5f); P.flareLight = m.num("light", 300); shake = .15f; photoOff = 25;
    audio.hiss(600, 4000, .9f, .25f); audio.thump(.6f);
    burst(cx, P.y + P.h * .2f, 40, hex("#c8c0d4"), 260);
    float r = m.num("radius", 170);
    for (auto& e : enemies)
      if (e.alive && std::hypot(e.x + e.w / 2 - cx, e.y + e.h / 2 - cy) < r) damage(e, dmg, type, (e.x > P.x) ? 1.f : -1.f);
  } else if (kind == "leap") {  // magic as movement: a second jump in the air, drawn from mana (or blood)
    P.leaps--; P.vy = -m.num("speed", 720); P.jumping = true; P.jumpBuf = 0;
    burst(cx, P.y + P.h, 18, hex("#8c7aa8"), 120, 60, .7f);
    audio.hiss(1800, 400, .35f, .12f);
  } else if (kind == "dash") {
    int d = P.dir ? P.dir : P.face;
    P.dash = m.num("time", .18f); P.dashDir = float(d); P.face = d;
    P.dashSpeed = m.num("speed", 560) * (P.inWater ? m.num("waterScale", .7f) : 1);
    P.inv = std::max(P.inv, m.num("invuln", .25f));
    audio.hiss(1200, 300, .2f, .1f);
  } else if (kind == "parry") {
    P.parry = m.num("window", .2f); P.parryMove = i;
    audio.hiss(2400, 1200, .1f, .06f);
  }
}

// the item input uses the first kind you carry, in data/items.json order
void World::useItem(const Input& in) {
  Player& P = player;
  const auto& items = content->items.list;
  for (size_t i = 0; i < items.size(); i++) {
    const Record& it = items[i];
    Act a = actByName(it.str("input", "item"));
    if (a == A_COUNT || P.items[i] <= 0 || !in.tap(a)) continue;
    P.items[i]--;
    P.mana = std::min(tun.maxMana, P.mana + it.num("mana"));
    P.hp = std::min(tun.maxHp, P.hp + it.num("heal"));
    P.dust = std::min(100.f, P.dust + it.num("dust"));
    makeNoise(it.num("noise", .3f));
    burst(P.x + P.w / 2, headY() + 10, 24, hex("#b89cff"), 90, -40, 1.2f);
    audio.hiss(900, 2600, .5f, .15f);
    if (P.dust >= 100) die("The dust in the vial was the last of you.");
    return;
  }
}

void World::updateShots(float dt) {
  for (auto& s : shots) {
    s.x += s.vx * dt; s.vx *= std::pow(.6f, dt); s.life -= dt;
    particles.push_back({s.x, s.y + (frand() - .5f) * 6, 0, 0, -20, .4f, .4f, hex("#8c7aa8"), 2});
    if (Box* b = solidHit(s.x, s.y)) {
      s.life = 0; burst(s.x, s.y, 10, hex("#8c7aa8"), 100);
      if (b->seal > 0) hitSeal(*b, s.damageType, s.x, s.y);
    }
    for (auto& e : enemies)
      if (e.alive && s.life > 0 && s.x > e.x && s.x < e.x + e.w && s.y > e.y - 8 && s.y < e.y + e.h + 8) {
        damage(e, s.dmg, s.damageType, s.vx > 0 ? 1.f : -1.f); s.life = 0; burst(s.x, s.y, 14, hex("#8f7448"), 140);
      }
  }
  std::erase_if(shots, [](const Shot& s) { return s.life <= 0; });
}

// each type's "sense" says what it notices you by: your light, your noise, your stardust, or a fixed range
float World::senses(const Record& T) const {
  std::string s = T.str("sense", "light");
  float k = T.num("sight", 1);
  if (s == "fixed") return k;
  if (s == "noise") return k * noise;
  if (s == "dust") return k * player.dust;
  return lightR() * k * (player.flare > 0 ? T.num("flareLure", 1) : 1);
}

// "stalker": drifts, notices you, stalks at a distance, then lunges and recovers (a lurker's lure goes dark).
// "chaser": drifts, notices you, then comes straight at you and backs off after touching you.
void World::updateEnemies(float dt) {
  Player& P = player;
  float cx = P.x + P.w / 2, cy = P.y + P.h / 2, closest = 0;
  for (auto& e : enemies) {
    const Record& T = enemyType(e);
    if (!e.alive) { if ((e.respawn -= dt) <= 0) e = makeEnemy(e.spawn, e.type); continue; }
    e.flash -= dt; e.ph += dt; e.mt -= dt;
    float dx = cx - (e.x + e.w / 2), dy = cy - (e.y + e.h / 2), d = std::max(1.f, std::hypot(dx, dy));
    closest = std::max(closest, clampf(1 - d / 320, 0, 1));
    float sees = senses(T), tx = 0, ty = 0;
    bool chaser = T.str("behaviour", "stalker") == "chaser";
    switch (e.mode) {
      case EnemyMode::Drift:
        tx = ((e.spawn.x - e.x) / 100 + std::cos(e.ph * .5f)) * 40; ty = ((e.spawn.y - e.y) / 100 + std::sin(e.ph * .7f) * .5f) * 40;
        if (d < sees) { e.mode = EnemyMode::Stalk; e.mt = .8f + frand() * 1.6f; }
        break;
      case EnemyMode::Stalk: {
        if (chaser) {
          float v = T.num("speed", 110);
          tx = dx / d * v; ty = dy / d * v;
        } else {
          float k = d > T.num("stalkFar", 160) ? 90 : d < T.num("stalkNear", 110) ? -70 : 0;
          tx = dx / d * k - dy / d * 40; ty = dy / d * k + dx / d * 40;
          if (e.mt <= 0 && d < T.num("lungeRange", 280)) {
            e.mode = EnemyMode::Lunge; e.mt = T.num("lungeTime", .4f); e.lx = dx / d; e.ly = dy / d;
            if (T.num("lungeHiss", 1) > 0) audio.hiss(5000, 1800, .3f, .14f);
          }
        }
        if (e.mode == EnemyMode::Stalk && d > sees * 1.6f && d > 260) e.mode = EnemyMode::Drift;
        break;
      }
      case EnemyMode::Lunge:
        e.vx = e.lx * T.num("lungeSpeed", 520); e.vy = e.ly * T.num("lungeSpeed", 520);
        if (e.mt <= 0) { e.mode = EnemyMode::Recover; e.mt = T.num("recoverTime", 1.2f); }
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
    if (!overlaps(boxOf(e), boxOf(P))) continue;
    if (P.parry > 0 && P.parryMove >= 0) { parried(e); continue; }
    if (P.inv > 0) continue;
    hurt(T.num("contactDamage", 12), P.x >= e.x ? 1.f : -1.f);
    if (float k = T.num("contactDust"); k > 0) {  // it pulls stardust out of your skull
      P.dust = std::min(100.f, P.dust + k);
      burst(cx, headY(), 14, hex("#8c7aa8"), 80, -20, 1);
    }
    if (chaser) { e.mode = EnemyMode::Recover; e.mt = T.num("recoverTime", .8f); }
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
      if (!w.taken && seen(w.x, w.y, w.reveal) && std::hypot(cx - w.x, cy - w.y) < 26) {
        w.taken = true; say("A DROWNED WRAPPER", wrapperBonus(w)); audio.hiss(1200, 3000, .8f, .12f); burst(w.x, w.y, 30, hex("#c8a868"), 120);
      }
    for (auto& it : level.items) {
      int k = content->items.find(it.kind);
      if (k < 0 || it.taken || !seen(it.x, it.y, it.reveal) || std::hypot(cx - it.x, cy - it.y) >= 26) continue;
      const Record& r = content->items.list[k];
      if (P.items[k] >= r.num("max", 3)) continue;  // hands full: it stays for later
      it.taken = true; P.items[k]++;
      say(r.str("title", r.name), r.str("line"));
      audio.hiss(2200, 3400, .4f, .1f); burst(it.x, it.y, 20, hex("#b89cff"), 90);
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

// feeds the character's state machine; the names here, plus each move's "anim", are what characters/*.json can test
void World::animatePlayer(float dt) {
  if (!character) return;
  const Player& P = player;
  AnimParams p{{"inWater", P.inWater},       {"ground", P.ground}, {"focusing", P.focusing},
               {"hurt", P.kb > 0},           {"vx", P.vx},         {"vy", P.vy},
               {"speed", std::fabs(P.vx)}, {"runRatio", std::fabs(P.vx) / tun.runSpeed}};
  for (auto& [k, v] : P.anims.list) p[k] = v;
  float charge = 0;  // how long a move's hold input has been held
  for (auto& m : content->moves.list)
    if (Act a = actByName(m.str("input")); a != A_COUNT && m.num("hold") > 0) charge = std::max(charge, P.holdT[a]);
  p["charge"] = charge;
  player.anim.update(*character, p, dt);
}
