// The rules behind data/*.json: blood costs, moves, senses, resistances, parry, seals, items and hidden pickups.
// Expected values come from the data files, so tuning them keeps this passing. Run from the project root:
// ./build/gameplay_test
#undef NDEBUG  // asserts must run in Release builds too
#include <cassert>
#include <cmath>
#include <cstdio>
#include <functional>

#include "../src/json.hpp"
#include "../src/util.hpp"
#include "../src/world.hpp"

namespace {

const float DT = 1 / 60.f;
Content content;

// a world of open water (or the given solids); the player starts at (500, 1000), facing right
struct Game {
  Synth audio;  // never started: sounds are queued and dropped
  World w{audio};
  Input in;
  explicit Game(std::vector<Spawn> enemies = {}, std::vector<Box> solids = {}, Vector2 spawn = {500, 1000}) {
    w.content = &content;
    w.level.width = 2000; w.level.height = 5200; w.level.top = -1000; w.level.spawn = spawn;
    w.level.enemies = std::move(enemies);
    w.level.solids = std::move(solids);
    w.reset();
  }
  void frame(std::initializer_list<Act> down = {}) { in.feed(down); w.update(DT, in); }
  Player& P() { return w.player; }
};

const Record& move(const char* name) { return content.moves.list[content.moves.find(name)]; }
const Record& enemy(const char* name) { return content.enemies.list[content.enemies.find(name)]; }
bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

void bloodCost() {  // what mana cannot pay is paid in blood, at the spell's own rate, and turns to stardust
  Game g;
  const Record& bolt = move("bolt");
  g.P().mana = 10;
  float mana = 10 + g.w.tun.manaRegen * DT, owed = bolt.num("mana") - mana;
  g.frame({A_BOLT});
  assert(near(g.P().hp, g.w.tun.maxHp - owed * bolt.num("bloodRatio")));
  assert(near(g.P().dust, owed * g.w.tun.dustPerBlood));
  assert(g.P().mana == 0 && g.w.shots.size() == 1);
}

void resistances() {  // the isopod's shell turns the wand (immune: no damage, no mana); a lurker takes it
  for (auto [type, harmed] : {std::pair{"isopod", false}, {"lurker", true}}) {
    Game g({{type, 486 + 28 + 4, 1010}});
    float hp = g.w.enemies[0].hp, mana = g.P().mana;
    g.frame({A_STAFF});
    const Record& wand = move("wand");
    float regen = g.w.tun.manaRegen * DT;
    if (harmed) {
      assert(near(g.w.enemies[0].hp, hp - wand.num("damage") * enemy(type).num("resist.slash", 1)));
      assert(near(g.P().mana, mana + regen + wand.num("manaGain")));
    } else {
      assert(g.w.enemies[0].hp == hp);
      assert(near(g.P().mana, mana + regen));
    }
  }
  // the leech feeds on star bolts: they heal it, never past twice its health
  Game g({{"leech", 486 + 80, 1012}});
  g.w.enemies[0].hp = 10;
  g.P().mana = 100;
  g.frame({A_BOLT});
  for (int i = 0; i < 30 && !g.w.shots.empty(); i++) g.frame();
  assert(g.w.enemies[0].hp > 10 && g.w.enemies[0].hp <= enemy("leech").num("hp") * 2);
}

void thrust() {  // a tap strikes; holding the staff past "hold" and letting go thrusts
  const Record& t = move("thrust");
  Game g;
  g.frame({A_STAFF});
  assert(g.P().anims.get("strike") > 0);
  g.frame();
  assert(g.P().anims.get("thrust") == 0);
  for (int i = 0; i < 60; i++) g.frame();
  int frames = int(std::ceil(t.num("hold") / DT)) + 1;
  for (int i = 0; i < frames; i++) g.frame({A_STAFF});
  g.frame();
  assert(g.P().anims.get("thrust") > 0);
}

void leap() {  // on land, Space jumps; Space again in the air is a star leap, paid in mana, once per airtime
  const Record& l = move("leap");
  Game g({}, {{0, -100, 2000, 60}}, {500, -171});
  for (int i = 0; i < 10; i++) g.frame();
  assert(g.P().ground && !g.P().inWater);
  float mana = g.P().mana;
  g.frame({A_UP});
  assert(g.P().vy < 0 && g.P().leaps == int(l.num("charges", 1)) && g.P().mana > mana);  // a plain jump: no mana spent
  for (int i = 0; i < 8; i++) g.frame({A_UP});
  mana = g.P().mana;
  g.frame();
  g.frame({A_UP});
  assert(near(g.P().vy, -l.num("speed")) && near(g.P().mana, mana + 2 * g.w.tun.manaRegen * DT - l.num("mana")));
  for (int i = 0; i < 6; i++) g.frame({A_UP});
  float vy = g.P().vy;
  g.frame();
  g.frame({A_UP});
  assert(g.P().vy > vy);  // no charge left: gravity, not another leap
}

void dodge() {  // a dash with a moment of invulnerability, slower in the water
  const Record& d = move("dodge");
  Game g;
  g.frame({A_DODGE});
  assert(g.P().dash > 0 && g.P().inv >= d.num("invuln") - DT);
  g.frame();
  assert(near(g.P().vx, d.num("speed") * d.num("waterScale")));
}

void parry() {  // a touch inside the parry window: no harm, mana from the clash, the enemy reels
  const Record& p = move("parry");
  {
    Game g({{"lurker", 486, 1010}});
    g.P().mana = 50;
    g.frame({A_PARRY});
    assert(g.P().hp == g.w.tun.maxHp);
    assert(near(g.P().mana, 50 + g.w.tun.manaRegen * DT + p.num("manaGain")));
    assert(g.w.enemies[0].mode == EnemyMode::Recover);
  }
  Game g({{"lurker", 486, 1010}});
  g.frame();
  assert(near(g.P().hp, g.w.tun.maxHp - enemy("lurker").num("contactDamage")));
}

void senses() {
  {  // the blind isopod ignores a quiet swimmer and hears a flare
    Game g({{"isopod", 486 + 330, 1000}});
    for (int i = 0; i < 20; i++) g.frame();
    assert(g.w.enemies[0].mode == EnemyMode::Drift);
    g.P().mana = 100;
    g.frame({A_FLARE});
    g.frame();
    assert(g.w.enemies[0].mode == EnemyMode::Stalk);
  }
  // the leech smells stardust: none, and it drifts; enough, and it comes
  Game g({{"leech", 486 + 250, 1000}});
  for (int i = 0; i < 20; i++) g.frame();
  assert(g.w.enemies[0].mode == EnemyMode::Drift);
  g.P().dust = 300 / enemy("leech").num("sight");
  g.frame();
  assert(g.w.enemies[0].mode == EnemyMode::Stalk);
}

void seals() {  // a star bolt opens a seal only with enough stardust
  Game g({}, {{600, 700, 60, 800, "", 30}});
  g.P().mana = 100;
  g.frame({A_BOLT});
  for (int i = 0; i < 40; i++) g.frame();
  assert(!g.w.level.solids[0].open && g.w.level.solids[0].tried);
  g.P().dust = 30; g.P().mana = 100;
  g.frame({A_BOLT});
  for (int i = 0; i < 40; i++) g.frame();
  assert(g.w.level.solids[0].open);
  g.w.reset();
  assert(!g.w.level.solids[0].open);  // shut again on a new run
}

void items() {  // a dust vial: picked up, carried, drunk for mana at the price of stardust; hidden ones need a loosened mind
  const Record& v = content.items.list[content.items.find("dustVial")];
  Game g;
  g.w.level.items = {{500, 1035, "dustVial"}, {500, 1035, "dustVial", 20}};
  g.w.reset();
  g.frame();
  assert(g.P().items[0] == 1 && !g.w.level.items[1].taken);
  g.P().mana = 0;
  g.frame({A_ITEM});
  assert(g.P().items[0] == 0 && near(g.P().dust, v.num("dust")) && near(g.P().mana, std::min(g.w.tun.maxMana, v.num("mana")) + g.w.tun.manaRegen * DT));
  g.P().dust = 20;
  g.frame();
  assert(g.P().items[0] == 1 && g.w.level.items[1].taken);
}

void roundTrip() {  // the tune pages save numbers back into the file, keeping its strings and nesting
  std::string path = "build/roundtrip_enemies.json";
  SaveFileText(path.c_str(), readFile("data/enemies.json").data());
  RecordSet s = loadRecords(path);
  for (auto& [k, val] : s.list[0].nums) if (k == "resist.slash") val = .25f;
  assert(saveRecords(s));
  RecordSet r = loadRecords(path);
  assert(r.list[0].num("resist.slash") == .25f && r.list[0].str("draw") == s.list[0].str("draw") && r.list.size() == s.list.size());
}

void leechDrainsTheLastOfYou() {  // its touch can be the stardust that kills you, and says so
  Game g({{"leech", 486, 1010}});
  g.P().dust = 100 - enemy("leech").num("contactDust") / 2;
  g.frame();
  assert(g.w.outcome == Outcome::Dead && g.w.msg == "Something drank the last of your mind.");
}

bool throws(const std::function<void()>& f) {
  try { f(); } catch (const std::exception&) { return true; }
  return false;
}

void badData() {  // malformed files fail with an error instead of reading garbage
  assert(Json::parse(R"("\u00e9")").str == "\xc3\xa9");
  assert(throws([] { Json::parse(R"("\u12zz")"); }));
  SaveFileText("build/bad_atlas.json", (char*)R"({ "cell": [10, 10], "anchor": [5, 10], "anims": { "idle": { "row": 0, "frames": 1, "head": [[0]] } } })");
  SaveFileText("build/bad_character.json", (char*)R"({ "atlas": "build/bad_atlas.json", "states": { "idle": { "anim": "idle" } } })");
  assert(throws([] { parseCharacter("build/bad_character.json"); }));
}

}  // namespace

int main() {
  SetTraceLogLevel(LOG_WARNING);
  content = loadContent("data");
  bloodCost();
  resistances();
  thrust();
  leap();
  dodge();
  parry();
  senses();
  seals();
  items();
  roundTrip();
  leechDrainsTheLastOfYou();
  badData();
  std::puts("gameplay_test: ok");
}
