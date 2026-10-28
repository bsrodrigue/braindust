// The simulation: level, player, enemies, moves, items, particles, camera and game flow. Nothing here draws.
#pragma once
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "character.hpp"
#include "content.hpp"
#include "input.hpp"
#include "level.hpp"
#include "synth.hpp"
#include "tunables.hpp"

enum class Outcome { Playing, Dead, Won };
enum class EnemyMode { Drift, Stalk, Lunge, Recover };

// named countdowns (s), set by data: a move's "anim" param, a move group's cooldown
struct Timers {
  std::vector<std::pair<std::string, float>> list;
  float& operator[](const std::string& k) {
    for (auto& [key, v] : list) if (key == k) return v;
    list.push_back({k, 0.f});
    return list.back().second;
  }
  float get(const std::string& k) const {
    for (auto& [key, v] : list) if (key == k) return v;
    return 0;
  }
  void tick(float dt) { for (auto& [k, v] : list) v = std::max(0.f, v - dt); }
};

struct Player {
  float x = 0, y = 0, w = 28, h = 70, vx = 0, vy = 0;  // hitbox: the head rises above it
  int face = 1, dir = 0;  // dir: the held left/right input
  bool ground = false, focusing = false, inWater = true, jumping = false, jumped = false;  // jumped: this frame
  float hp = 0, mana = 0, dust = 0, focus = 0;  // dust: stardust %, the brain turning to stars
  // countdowns (s)
  float coyote = 0, jumpBuf = 0, inv = 0, kb = 0, bleed = 0, strokeCd = 0, portalCd = 0;
  float flare = 0, flareLight = 0;           // the light of the last burst, while it lasts
  float dash = 0, dashDir = 0, dashSpeed = 0;  // a dodge in progress
  float parry = 0;                           // the parry window
  int parryMove = -1, leaps = 0;             // leaps: air leaps left before touching ground or water
  float holdT[A_COUNT]{};                    // how long each input has been held
  Timers cooldowns, anims;                   // per move group; per animation param (characters/*.json reads them)
  std::vector<int> items;                    // carried, per entry of data/items.json
  // body animation: squash, and the countdowns the character's state machine reads
  float squash = 1, stroke = 0, land = 0;
  Animator anim;
};

struct Enemy {
  int type = 0;  // an entry of data/enemies.json
  Vector2 spawn{};
  float x = 0, y = 0, w = 40, h = 18, vx = 0, vy = 0;
  int face = 1;
  bool ground = false, alive = true;
  float hp = 30, respawn = 0, flash = 0, ph = 0, mt = 0, lx = 0, ly = 0;  // mt: time left in the mode; lx, ly: lunge direction
  EnemyMode mode = EnemyMode::Drift;
};

struct Shot { float x, y, vx, life, dmg, r; std::string damageType; };
struct Particle { float x, y, vx, vy, g, life, max; Color c; float size; };
struct Crack { Rectangle r; bool universe, seen; };
struct Banner { std::string title, line; float t = 0; };

// the universe inside the portal crack: a fixed little room
namespace pocket {
inline constexpr float W = 1400, H = 800;
inline constexpr Rectangle exitCrack{70, 560, 24, 80}, altar{1195, 280, 30, 60};
}  // namespace pocket

class World {
 public:
  explicit World(Synth& audio) : audio(audio) {}

  Synth& audio;
  Level level;
  Tunables base, tun;  // base: tunables.json; tun: base plus the wrapper bonuses picked up this run
  const Character* character = nullptr;
  const Content* content = nullptr;  // moves, enemy types and items; set before reset()

  Player player;
  std::vector<Enemy> enemies;
  std::vector<Shot> shots;
  std::vector<Particle> particles;
  Crack cracks[2]{};  // 0: the portal (a universe inside), 1: the empty crack
  bool inPocket = false;
  std::vector<Box>* solids = &level.solids;  // the current area's; seals open in place
  float W = 1000, H = 5200;            // current area size
  Vector2 cam{}, view{600, 540};       // camera top-left and view size, world px (view is set by the renderer)
  float t = 0, shake = 0, fade = 0, photoOff = 0, warp = 0;
  float noise = 0;                     // how loud you have been lately: listeners hear it (0..~2, decays)
  Banner banner;
  Outcome outcome = Outcome::Playing;
  std::string msg;
  bool portalSeen = false;  // the portal's stardust has surged at you once
  bool nearActive = false;  // set by the 3D pass: the leviathan is between you and the camera

  void reset();
  void update(float dt, const Input& in);
  void retune();  // tun = base plus the bonuses of the wrappers taken this run; call after changing base

  void say(std::string title, std::string line);
  void burst(float x, float y, int n, Color c, float speed, float g = 0, float life = .6f);

  const Record& enemyType(const Enemy& e) const { return content->enemies.list[e.type]; }
  float power() const { return 1 + player.dust / 100 * tun.dustPower; }
  float headY() const;  // world y of the top of the player's head
  float lightR() const;
  bool lit(float x, float y) const;  // inside your light
  bool seen(float x, float y, float reveal) const;  // a pickup with this "reveal" (see level.hpp) shows here
  float cheapestSpell() const;       // the lowest mana cost of a spell: below it, casting draws blood
  int depthM() const;
  bool solidAt(float x, float y) const;
  Vector2 portalCentre() const { return {cracks[0].r.x + 30, cracks[0].r.y + 40}; }
  Vector2 playerCentre() const { return {player.x + player.w / 2, player.y + player.h / 2}; }

 private:
  std::vector<Box> pocketSolids_;
  float fear_ = 0, beatT_ = 0, closestEnemy_ = 0;  // closestEnemy_: 0..1, how near the nearest enemy is

  void step(float dt, const Input& in);  // returns early on area changes and death
  void movePlayer(float dt, const Input& in);
  void updateBody(float dt);
  void useMoves(float dt, const Input& in);
  void doMove(int i);
  bool moveAllowed(const Record& m) const;
  void useItem(const Input& in);
  void updateShots(float dt);
  void updateEnemies(float dt);
  float senses(const Record& T) const;  // how far an enemy of this type notices you, px
  bool updateTriggers(float dt);  // true when the player changed area
  void updateFearAndCamera(float dt);
  void animatePlayer(float dt);

  void spawnEnemies(const std::vector<Spawn>& list);
  Enemy makeEnemy(Vector2 s, int type) const;
  void enterSea();
  void enterPocket();
  void leavePocket();
  void die(const char* why);
  void pay(float cost, float bloodRatio);
  bool damage(Enemy& e, float d, const std::string& type, float dir, float knockback = 220);  // false: no effect
  void hurt(float d, float dir);
  void parried(Enemy& e);
  void hitSeal(Box& s, const std::string& type, float x, float y);
  Box* solidHit(float x, float y);
  const char* wrapperBonus(const Wrapper& w);  // the wrapper must already be taken
  void makeNoise(float loud) { noise = std::max(noise, loud); }
};
