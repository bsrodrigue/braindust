// The simulation: level, player, enemies, spells, particles, camera and game flow. Nothing here draws.
#pragma once
#include <string>
#include <vector>

#include "character.hpp"
#include "input.hpp"
#include "level.hpp"
#include "synth.hpp"
#include "tunables.hpp"

enum class Outcome { Playing, Dead, Won };
enum class EnemyMode { Drift, Stalk, Lunge, Recover };

struct Player {
  float x = 0, y = 0, w = 28, h = 70, vx = 0, vy = 0;  // hitbox: the head rises above it
  int face = 1;
  bool ground = false, focusing = false, inWater = true, jumping = false;
  float hp = 0, mana = 0, dust = 0, focus = 0;  // dust: stardust %, the brain turning to stars
  // countdowns (s)
  float coyote = 0, jumpBuf = 0, inv = 0, kb = 0, atk = 0, atkCd = 0, cast = 0, bleed = 0, strokeCd = 0, flare = 0, portalCd = 0;
  // body animation: squash, and the countdowns the character's state machine reads
  float squash = 1, stroke = 0, castHold = 0, strike = 0, land = 0;
  Animator anim;
};

struct Enemy {
  bool thought = false;  // a creature that escaped from a cranium; otherwise a lurker
  Vector2 spawn{};
  float x = 0, y = 0, w = 40, h = 18, vx = 0, vy = 0;
  int face = 1;
  bool ground = false, alive = true;
  float hp = 30, respawn = 0, flash = 0, ph = 0, mt = 0, lx = 0, ly = 0;  // mt: time left in the mode; lx, ly: lunge direction
  EnemyMode mode = EnemyMode::Drift;
};

struct Shot { float x, y, vx, life, dmg, r; };
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

  Player player;
  std::vector<Enemy> enemies;
  std::vector<Shot> shots;
  std::vector<Particle> particles;
  Crack cracks[2]{};  // 0: the portal (a universe inside), 1: the empty crack
  bool inPocket = false;
  const std::vector<Box>* solids = &level.solids;
  float W = 1000, H = 5200;            // current area size
  Vector2 cam{}, view{600, 540};       // camera top-left and view size, world px (view is set by the renderer)
  float t = 0, shake = 0, fade = 0, photoOff = 0, warp = 0;
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

  float power() const { return 1 + player.dust / 100 * tun.dustPower; }
  float headY() const;  // world y of the top of the player's head
  float lightR() const;
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
  void castSpells(const Input& in);
  void updateShots(float dt);
  void updateEnemies(float dt);
  bool updateTriggers(float dt);  // true when the player changed area
  void updateFearAndCamera(float dt);
  void animatePlayer(float dt);

  void enterSea();
  void enterPocket();
  void leavePocket();
  void die(const char* why);
  void pay(float cost);
  void damage(Enemy& e, float d, float dir);
  void hurt(float d, float dir);
  const char* wrapperBonus(const Wrapper& w);  // the wrapper must already be taken
};
