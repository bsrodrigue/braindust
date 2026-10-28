// Gameplay variables: defaults here, overridden by tunables.json, edited live in the F2 tune menu.
#pragma once
#include <string>

struct Tunables {
  float maxHp = 100, maxMana = 100, manaRegen = 3, manaPerHit = 11, bloodRatio = .6f, dustPerBlood = .3f, dustPower = 1.5f, dustDecay = 0,
        lightBase = 160, dustLight = 2.5f, flareCost = 35, flareLight = 300, flareTime = 2.5f, flareDmg = 15, boltCost = 25, boltDmg = 20,
        focusCost = 33, focusHeal = 25, focusTime = .9f, pressureDepth = 300, pressure = .15f, lureSight = 1.2f, lungeSpeed = 520,
        staffDmg = 10, swim = 200, stroke = 260, sink = 180, respawn = 10,
        // land: weighty, Blasphemous-like jump: fast rise, heavier fall, committed air control
        runSpeed = 230, groundAccel = 18, airControl = .35f, jump = 780, landGravity = 2800, fallGravity = 4600, maxFall = 1150,
        jumpCut = .45f, coyoteTime = .08f, jumpBuffer = .1f;
};

struct TunableField { const char* name; float Tunables::*field; float step; const char* group; };  // step = one arrow press
inline const TunableField TUNABLE_FIELDS[] = {
    {"runSpeed", &Tunables::runSpeed, 10, "Land"}, {"groundAccel", &Tunables::groundAccel, 1, "Land"},
    {"airControl", &Tunables::airControl, .05f, "Land"}, {"jump", &Tunables::jump, 20, "Land"},
    {"landGravity", &Tunables::landGravity, 100, "Land"}, {"fallGravity", &Tunables::fallGravity, 100, "Land"},
    {"maxFall", &Tunables::maxFall, 50, "Land"}, {"jumpCut", &Tunables::jumpCut, .05f, "Land"},
    {"coyoteTime", &Tunables::coyoteTime, .01f, "Land"}, {"jumpBuffer", &Tunables::jumpBuffer, .01f, "Land"},
    {"swim", &Tunables::swim, 10, "Water"}, {"stroke", &Tunables::stroke, 10, "Water"}, {"sink", &Tunables::sink, 10, "Water"},
    {"maxHp", &Tunables::maxHp, 5, "Body"}, {"maxMana", &Tunables::maxMana, 5, "Body"}, {"manaRegen", &Tunables::manaRegen, .5f, "Body"},
    {"manaPerHit", &Tunables::manaPerHit, 1, "Body"}, {"focusCost", &Tunables::focusCost, 1, "Body"},
    {"focusHeal", &Tunables::focusHeal, 1, "Body"}, {"focusTime", &Tunables::focusTime, .05f, "Body"},
    {"bloodRatio", &Tunables::bloodRatio, .05f, "Blood & stardust"}, {"dustPerBlood", &Tunables::dustPerBlood, .05f, "Blood & stardust"},
    {"dustPower", &Tunables::dustPower, .1f, "Blood & stardust"}, {"dustDecay", &Tunables::dustDecay, .05f, "Blood & stardust"},
    {"pressureDepth", &Tunables::pressureDepth, 10, "Blood & stardust"}, {"pressure", &Tunables::pressure, .01f, "Blood & stardust"},
    {"staffDmg", &Tunables::staffDmg, 1, "Spells"}, {"boltCost", &Tunables::boltCost, 1, "Spells"}, {"boltDmg", &Tunables::boltDmg, 1, "Spells"},
    {"flareCost", &Tunables::flareCost, 1, "Spells"}, {"flareDmg", &Tunables::flareDmg, 1, "Spells"},
    {"flareTime", &Tunables::flareTime, .1f, "Spells"}, {"flareLight", &Tunables::flareLight, 10, "Light"},
    {"lightBase", &Tunables::lightBase, 10, "Light"}, {"dustLight", &Tunables::dustLight, .1f, "Light"},
    {"lureSight", &Tunables::lureSight, .05f, "Enemies"}, {"lungeSpeed", &Tunables::lungeSpeed, 20, "Enemies"},
    {"respawn", &Tunables::respawn, 1, "Enemies"}};

Tunables loadTunables(const std::string& path);  // defaults for anything missing; a bad file only warns
bool saveTunables(const std::string& path, const Tunables& t);
