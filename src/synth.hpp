// Procedural audio: a drone bed plus one-shot voices (groan, thump, hiss), mixed on raylib's audio thread.
#pragma once
#include <cmath>
#include <mutex>
#include <random>
#include <vector>
#include "raylib.h"

class Synth {
 public:
  static constexpr int SR = 44100;

  void start() {
    self() = this;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    SetAudioStreamBufferSizeDefault(1024);
    stream = LoadAudioStream(SR, 32, 1);
    SetAudioStreamCallback(stream, [](void* buf, unsigned int frames) { self()->render(static_cast<float*>(buf), frames); });
    PlayAudioStream(stream);
    running = true;
  }
  void stop() {
    if (running) UnloadAudioStream(stream);
    if (IsAudioDeviceReady()) CloseAudioDevice();
    running = false;
  }
  // drone follows depth: louder and darker the deeper you are
  void drone(float gain, float cutoff) { std::lock_guard l(m); droneGainT = gain; droneCutT = cutoff; }
  void toggleMute() { std::lock_guard l(m); muted = !muted; }
  void groan(bool near) { add({Groan, 0, 6.6f, near ? 58.f : 75.f, near ? 27.f : 40.f, near ? .55f : .16f, near ? 420.f : 170.f}); }
  void thump(float v) {
    add({Thump, 0, .3f, 62, 34, .55f * v});
    add({Thump, -.17f, .3f, 62, 34, .33f * v});  // negative t = delayed start: the second beat
  }
  void hiss(float f0, float f1, float dur, float vol) { add({Hiss, 0, dur, f0, f1, vol}); }

 private:
  enum Kind { Groan, Thump, Hiss };
  struct Voice {
    Kind kind; float t, dur, f0, f1, vol, cut = 0;
    double phase = 0, lfo = 0;
    float lp = 0, bp = 0;  // state-variable filter
  };
  static Synth*& self() { static Synth* s = nullptr; return s; }
  void add(Voice v) { std::lock_guard l(m); if (voices.size() < 48) voices.push_back(v); }
  float noise() { return dist(rng); }
  // Chamberlin state-variable filter step; returns {lowpass, bandpass}
  static void svf(float in, float fc, float q, float& lp, float& bp) {
    float f = 2.f * std::sin(float(M_PI) * std::min(fc, SR / 6.f) / SR);
    lp += f * bp;
    float hp = in - lp - q * bp;
    bp += f * hp;
  }

  void render(float* out, unsigned frames) {
    std::lock_guard l(m);
    const float dt = 1.f / SR;
    for (unsigned n = 0; n < frames; n++) {
      droneGain += (droneGainT - droneGain) * .00005f;
      droneCut += (droneCutT - droneCut) * .00005f;
      // drone: low-passed brown noise and three beating low sines
      brown = (brown + .02f * noise()) / 1.02f;
      float a = 1.f - std::exp(-2.f * float(M_PI) * droneCut / SR);
      droneLp += a * (brown * 3.5f - droneLp);
      float s = (droneLp + .06f * (std::sin(ph[0]) + std::sin(ph[1]) + std::sin(ph[2]))) * droneGain;
      ph[0] += 2 * M_PI * 41.0 / SR; ph[1] += 2 * M_PI * 41.6 / SR; ph[2] += 2 * M_PI * 61.8 / SR;
      for (auto& v : voices) {
        float t = v.t; v.t += dt;
        if (t < 0 || t > v.dur) continue;
        float k = t / v.dur, f;
        switch (v.kind) {
          case Groan: {  // resonant low-passed saw with a slow vibrato
            f = v.f0 * std::pow(v.f1 / v.f0, std::min(1.f, t / 6.f)) + 5.f * std::sin(v.lfo);
            v.lfo += 2 * M_PI * 2.3 / SR;
            v.phase += f / SR; v.phase -= std::floor(v.phase);
            svf(float(v.phase * 2 - 1), v.cut, .12f, v.lp, v.bp);
            float env = t < 1.8f ? t / 1.8f : std::max(0.f, 1.f - (t - 1.8f) / 4.7f);
            s += v.lp * env * v.vol;
            break;
          }
          case Thump: {  // falling sine: a heartbeat
            f = v.f0 * std::pow(v.f1 / v.f0, std::min(1.f, t / .15f));
            v.phase += 2 * M_PI * f / SR;
            float env = t < .02f ? t / .02f : std::exp(-(t - .02f) * 22.f);
            s += float(std::sin(v.phase)) * env * v.vol;
            break;
          }
          case Hiss: {  // band-passed noise sweep
            svf(noise(), v.f0 * std::pow(v.f1 / v.f0, k), .7f, v.lp, v.bp);
            s += v.bp * std::exp(-k * 6.f) * v.vol;
            break;
          }
        }
      }
      out[n] = muted ? 0.f : std::tanh(s * .9f);
    }
    std::erase_if(voices, [](const Voice& v) { return v.t > v.dur; });
  }

  std::mutex m;
  std::vector<Voice> voices;
  std::mt19937 rng{7};
  std::uniform_real_distribution<float> dist{-1.f, 1.f};
  AudioStream stream{};
  bool running = false, muted = false;
  float droneGain = 0, droneGainT = .12f, droneCut = 220, droneCutT = 220, brown = 0, droneLp = 0;
  double ph[3] = {0, 0, 0};
};
