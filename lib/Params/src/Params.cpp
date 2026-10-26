#include "Params.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
// By path: in library builds the framework's esp_ble_mesh settings.h comes first
// in the include path (and matches on case-insensitive file systems)
#include "../../../include/Settings.h"

using namespace params;
namespace s = settings;

namespace
{

const Def DEFS[COUNT] = {
    // Clock
    {"tempo_multiplier", Type::Int, s::TEMPO_MULTIPLIER, 1, 2, "clock tempo = heart rate tempo x this (2 = double time)"},
    // Stepped mode and bars
    {"step_beats", Type::Int, s::STEP_BEATS, 1, 64, "stepped mode: beats the tempo holds (power of two)"},
    {"count_in_beats", Type::Int, s::COUNT_IN_BEATS, 0, 63, "stepped mode: LED count-in beats before a step (0 = none)"},
    {"beats_per_bar", Type::Int, s::BEATS_PER_BAR, 1, 16, "transport LED: beats per bar (downbeat flash)"},
    // Rate switch presets
    {"rate_slow_max_change", Type::Float, s::RATE_SLOW.maxChangePerBeat, 0.0001f, 0.02f,
     "rate slow: max tempo change per beat (fraction)"},
    {"rate_slow_tau_sec", Type::Float, s::RATE_SLOW.tauSec, 1, 300, "rate slow: heart rate smoothing time (s)"},
    {"rate_medium_max_change", Type::Float, s::RATE_MEDIUM.maxChangePerBeat, 0.0001f, 0.02f,
     "rate medium: max tempo change per beat (fraction)"},
    {"rate_medium_tau_sec", Type::Float, s::RATE_MEDIUM.tauSec, 1, 300, "rate medium: heart rate smoothing time (s)"},
    {"rate_fast_max_change", Type::Float, s::RATE_FAST.maxChangePerBeat, 0.0001f, 0.02f,
     "rate fast: max tempo change per beat (fraction)"},
    {"rate_fast_tau_sec", Type::Float, s::RATE_FAST.tauSec, 1, 300, "rate fast: heart rate smoothing time (s)"},
    // Feedback switch
    {"feedback_offset_bpm", Type::Float, s::FEEDBACK_OFFSET_BPM, 0, 30, "feedback -/+: target offset from the heart rate"},
    {"steer_low_bpm", Type::Float, s::STEER_LOW_BPM, 30, 220, "feedback -: never steer the target below this"},
    {"steer_high_bpm", Type::Float, s::STEER_HIGH_BPM, 30, 220, "feedback +: never steer the target above this"},
    // Heart rate filter
    {"start_bpm", Type::Float, s::START_BPM, s::VALID_MIN_BPM, s::VALID_MAX_BPM,
     "clock tempo at power-on (applies after a restart)"},
    {"caution_above_bpm", Type::Float, s::CAUTION_ABOVE_BPM, 60, 180, "readings above this need a smooth series or agreement"},
    {"caution_step_bpm", Type::Float, s::CAUTION_STEP_BPM, 1, 40, "max step from the last kept reading for a smooth series"},
    {"gate_floor_bpm", Type::Float, s::GATE_FLOOR_BPM, 1, 40, "locked: readings this close to recent ones always pass"},
    {"gate_max_bpm", Type::Float, s::GATE_MAX_BPM, 1, 60, "locked: readings further away never pass directly"},
    {"agree_tol_bpm", Type::Float, s::AGREE_TOL_BPM, 0.5f, 20, "readings within this of their median agree"},
    {"lock_timeout_polls", Type::Int, s::LOCK_TIMEOUT_POLLS, 2, 100, "polls without an accepted reading before a lock drops"},
    {"invalid_to_no_finger", Type::Int, s::INVALID_TO_NO_FINGER, 1, 50, "readings without heart rate before no finger"},
    // LEDs
    {"led_level", Type::Int, s::LED_LEVEL, 3, 255, "sensor LED brightness"},
    {"led_show_every_miss", Type::Bool, s::LED_SHOW_EVERY_MISS, 0, 1, "sensor LED: amber on every missed reading"},
    {"transport_led_level", Type::Int, s::TRANSPORT_LED_LEVEL, 3, 255, "transport LED: downbeat flash brightness"},
    {"stopped_level", Type::Int, s::STOPPED_LEVEL, 1, 255, "transport LED: brightness while stopped"},
};

// Rules between parameters, the runtime version of Settings.h's static_asserts:
// a < b, or a <= b when orEqual
struct Rule
{
  Id a, b;
  bool orEqual;
};

const Rule RULES[] = {
    {COUNT_IN_BEATS, STEP_BEATS, false},
    {STEER_LOW_BPM, STEER_HIGH_BPM, false},
    {GATE_FLOOR_BPM, GATE_MAX_BPM, true},
    {INVALID_TO_NO_FINGER, LOCK_TIMEOUT_POLLS, false},
};

bool isPowerOfTwo(int x)
{
  return x > 0 && (x & (x - 1)) == 0;
}

bool equalsWord(const char *text, const char *word)
{
  for (; *text && *word; text++, word++)
  {
    char c = *text >= 'A' && *text <= 'Z' ? *text - 'A' + 'a' : *text;
    if (c != *word)
      return false;
  }
  return !*text && !*word;
}

} // namespace

const Def &params::def(int id)
{
  return DEFS[id];
}

int params::find(const char *name)
{
  for (int i = 0; i < COUNT; i++)
    if (std::strcmp(DEFS[i].name, name) == 0)
      return i;
  return -1;
}

const char *params::typeName(Type t)
{
  return t == Type::Int ? "int" : t == Type::Bool ? "bool" : "float";
}

void params::storageKey(int id, char out[KEY_LEN + 1])
{
  std::strncpy(out, DEFS[id].name, KEY_LEN);
  out[KEY_LEN] = 0;
}

Params::Params()
{
  resetAll();
}

void Params::resetAll()
{
  for (int i = 0; i < COUNT; i++)
    v_[i] = DEFS[i].def;
}

bool Params::checkValue(int id, float value)
{
  const Def &d = DEFS[id];
  if (id == STEP_BEATS && !(value == (int)value && isPowerOfTwo((int)value) && value <= d.max))
  {
    std::snprintf(err_, sizeof(err_), "%s: must be a power of two 1..%d", d.name, (int)d.max);
    return false;
  }
  if (d.type == Type::Bool && value != 0 && value != 1)
  {
    std::snprintf(err_, sizeof(err_), "%s: must be true or false", d.name);
    return false;
  }
  if (d.type == Type::Int && std::isfinite(value) && value != std::floor(value))
  {
    std::snprintf(err_, sizeof(err_), "%s: must be a whole number", d.name);
    return false;
  }
  if (!std::isfinite(value) || value < d.min || value > d.max)
  {
    if (d.type == Type::Float)
      std::snprintf(err_, sizeof(err_), "%s: must be %g..%g", d.name, d.min, d.max);
    else
      std::snprintf(err_, sizeof(err_), "%s: must be %d..%d", d.name, (int)d.min, (int)d.max);
    return false;
  }
  return true;
}

bool Params::checkSet(const float *v, int id)
{
  char va[24], vb[24];
  for (const Rule &r : RULES)
  {
    if (r.orEqual ? v[r.a] <= v[r.b] : v[r.a] < v[r.b])
      continue;
    format(r.a, v[r.a], va, sizeof(va));
    format(r.b, v[r.b], vb, sizeof(vb));
    const char *na = DEFS[r.a].name, *nb = DEFS[r.b].name;
    if (id == r.b)
      std::snprintf(err_, sizeof(err_), "%s: must be %s %s (%s)", nb, r.orEqual ? "at least" : "above", na, va);
    else if (id == r.a)
      std::snprintf(err_, sizeof(err_), "%s: must be %s %s (%s)", na, r.orEqual ? "at most" : "below", nb, vb);
    else
      std::snprintf(err_, sizeof(err_), "%s (%s) must be %s %s (%s)", na, va, r.orEqual ? "at most" : "below", nb, vb);
    return false;
  }
  return true;
}

bool Params::set(int id, float value)
{
  if (id < 0 || id >= COUNT)
  {
    std::snprintf(err_, sizeof(err_), "unknown parameter");
    return false;
  }
  if (!checkValue(id, value))
    return false;
  float next[COUNT];
  std::memcpy(next, v_, sizeof(v_));
  next[id] = value;
  if (!checkSet(next, id))
    return false;
  v_[id] = value;
  err_[0] = 0;
  return true;
}

bool Params::parse(int id, const char *text, float *value)
{
  const Def &d = DEFS[id];
  if (d.type == Type::Bool)
  {
    if (equalsWord(text, "true") || equalsWord(text, "on") || equalsWord(text, "1"))
      *value = 1;
    else if (equalsWord(text, "false") || equalsWord(text, "off") || equalsWord(text, "0"))
      *value = 0;
    else
    {
      std::snprintf(err_, sizeof(err_), "%s: must be true or false", d.name);
      return false;
    }
    return true;
  }
  char *end = nullptr;
  if (d.type == Type::Int)
  {
    long x = std::strtol(text, &end, 10);
    if (end == text || *end)
    {
      std::snprintf(err_, sizeof(err_), "%s: must be a whole number", d.name);
      return false;
    }
    // Out-of-range longs are caught by the range check
    *value = x < -1000000 ? -1e6f : x > 1000000 ? 1e6f : (float)x;
    return true;
  }
  float x = std::strtof(text, &end);
  if (end == text || *end || !std::isfinite(x))
  {
    std::snprintf(err_, sizeof(err_), "%s: must be a number", d.name);
    return false;
  }
  *value = x;
  return true;
}

void Params::format(int id, float value, char *out, size_t size) const
{
  switch (DEFS[id].type)
  {
  case Type::Bool:
    std::snprintf(out, size, "%s", value != 0 ? "true" : "false");
    break;
  case Type::Int:
    std::snprintf(out, size, "%d", (int)value);
    break;
  case Type::Float:
    std::snprintf(out, size, "%g", value);
    break;
  }
}

int Params::load(ParamStore &store)
{
  resetAll();
  err_[0] = 0;
  if (store.schema() != SCHEMA_VERSION)
    return 0;
  int loaded = 0;
  char key[KEY_LEN + 1];
  for (int i = 0; i < COUNT; i++)
  {
    float x;
    storageKey(i, key);
    if (store.load(key, &x) && checkValue(i, x))
    {
      v_[i] = x;
      loaded++;
    }
  }
  if (!checkSet(v_, -1)) // error() keeps the rule that failed
  {
    resetAll();
    return 0;
  }
  return loaded;
}

bool Params::save(ParamStore &store)
{
  bool ok = store.setSchema(SCHEMA_VERSION);
  char key[KEY_LEN + 1];
  for (int i = 0; i < COUNT; i++)
  {
    storageKey(i, key);
    ok = (v_[i] != DEFS[i].def ? store.store(key, v_[i]) : store.remove(key)) && ok;
  }
  return ok;
}

float Params::maxChangePerBeat(int ratePos) const
{
  return v_[ratePos < 0 ? RATE_SLOW_MAX_CHANGE : ratePos > 0 ? RATE_FAST_MAX_CHANGE : RATE_MEDIUM_MAX_CHANGE];
}

float Params::tauSec(int ratePos) const
{
  return v_[ratePos < 0 ? RATE_SLOW_TAU_SEC : ratePos > 0 ? RATE_FAST_TAU_SEC : RATE_MEDIUM_TAU_SEC];
}

HeartTempoConfig Params::heartTempoConfig(int ratePos) const
{
  HeartTempoConfig c = settings::heartTempoConfig(); // the compile-time-only values
  c.startBpm = v_[START_BPM];
  c.invalidToNoFinger = getInt(INVALID_TO_NO_FINGER);
  c.agreeTolBpm = v_[AGREE_TOL_BPM];
  c.gateFloorBpm = v_[GATE_FLOOR_BPM];
  c.gateMaxBpm = v_[GATE_MAX_BPM];
  c.lockTimeoutPolls = getInt(LOCK_TIMEOUT_POLLS);
  c.cautionAboveBpm = v_[CAUTION_ABOVE_BPM];
  c.cautionStepBpm = v_[CAUTION_STEP_BPM];
  c.maxChangePerBeat = maxChangePerBeat(ratePos);
  c.tauSec = tauSec(ratePos);
  c.feedbackOffsetBpm = v_[FEEDBACK_OFFSET_BPM];
  c.steerLowBpm = v_[STEER_LOW_BPM];
  c.steerHighBpm = v_[STEER_HIGH_BPM];
  return c;
}
