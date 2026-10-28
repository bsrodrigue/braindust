// Gameplay variables: defaults here, overridden by tunables.json, edited live in the F2 tune menu.
#pragma once
#include <string>

// Body, movement, light and stardust rules. Spells, physical moves, enemies and items live in data/*.json.
struct Tunables {
  float maxHp = 100, maxMana = 100, manaRegen = 3, dustPerBlood = .3f, dustPower = 1.5f, dustDecay = 0, lightBase = 160, dustLight = 2.5f,
        focusCost = 33, focusHeal = 25, focusTime = .9f, pressureDepth = 300, pressure = .15f, swim = 200, stroke = 260, sink = 180,
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
    {"focusCost", &Tunables::focusCost, 1, "Body"},
    {"focusHeal", &Tunables::focusHeal, 1, "Body"}, {"focusTime", &Tunables::focusTime, .05f, "Body"},
    {"dustPerBlood", &Tunables::dustPerBlood, .05f, "Blood & stardust"},
    {"dustPower", &Tunables::dustPower, .1f, "Blood & stardust"}, {"dustDecay", &Tunables::dustDecay, .05f, "Blood & stardust"},
    {"pressureDepth", &Tunables::pressureDepth, 10, "Blood & stardust"}, {"pressure", &Tunables::pressure, .01f, "Blood & stardust"},
    {"lightBase", &Tunables::lightBase, 10, "Light"}, {"dustLight", &Tunables::dustLight, .1f, "Light"}};

Tunables loadTunables(const std::string& path);  // defaults for anything missing; a bad file only warns
bool saveTunables(const std::string& path, const Tunables& t);
