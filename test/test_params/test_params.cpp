// Runtime settings: parameter registry, validation, persistence, serial commands.
#include <unity.h>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <ClockGrid.h>
#include <MidiControl.h>
#include <ParamConsole.h>
#include <Params.h>
#include <Settings.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <ParamNvs.h>
#endif

using namespace params;
namespace mc = midicontrol;

// ParamStore in memory
class MemStore : public ParamStore
{
public:
  std::map<std::string, float> values;
  int version = -1;
  int writes = 0;

  bool load(const char *key, float *value) override
  {
    auto it = values.find(key);
    if (it == values.end())
      return false;
    *value = it->second;
    return true;
  }
  bool store(const char *key, float value) override
  {
    values[key] = value;
    writes++;
    return true;
  }
  bool remove(const char *key) override
  {
    values.erase(key);
    return true;
  }
  int schema() override { return version; }
  bool setSchema(int v) override
  {
    version = v;
    return true;
  }
  bool clear() override
  {
    values.clear();
    version = -1;
    return true;
  }
};

// A console wired to a store, collecting reply lines and change notifications
struct Rig
{
  Params params;
  MemStore store;
  std::vector<std::string> lines;
  std::vector<std::string> changes;
  ParamConsole console{params, store, [this](const char *l) { lines.push_back(l); },
                       [this](int id, float o, float n) {
                         char buf[80];
                         std::snprintf(buf, sizeof(buf), "%s %g -> %g", def(id).name, o, n);
                         changes.push_back(buf);
                       }};

  // Runs a command, returns its last reply line
  std::string run(const char *cmd)
  {
    lines.clear();
    for (const char *p = cmd; *p; p++)
      console.feed(*p);
    console.feed('\n');
    return lines.empty() ? "" : lines.back();
  }
};

static bool startsWith(const std::string &s, const char *prefix)
{
  return s.compare(0, std::strlen(prefix), prefix) == 0;
}

void setUp(void) {}
void tearDown(void) {}

// --- Registry -------------------------------------------------------------------

void test_defaults_come_from_settings(void)
{
  Params p;
  TEST_ASSERT_EQUAL(settings::STEP_BEATS, p.getInt(STEP_BEATS));
  TEST_ASSERT_EQUAL_FLOAT(settings::RATE_SLOW.maxChangePerBeat, p.get(RATE_SLOW_MAX_CHANGE));
  TEST_ASSERT_EQUAL_FLOAT(settings::RATE_FAST.tauSec, p.get(RATE_FAST_TAU_SEC));
  TEST_ASSERT_EQUAL_FLOAT(settings::STEER_HIGH_BPM, p.get(STEER_HIGH_BPM));
  TEST_ASSERT_EQUAL(settings::LED_LEVEL, p.getInt(LED_LEVEL));
  TEST_ASSERT_EQUAL(settings::LED_SHOW_EVERY_MISS, p.getBool(LED_SHOW_EVERY_MISS));
  TEST_ASSERT_EQUAL(settings::LOCK_TIMEOUT_POLLS, p.getInt(LOCK_TIMEOUT_POLLS));
}

void test_every_param_is_well_formed(void)
{
  Params p;
  char keys[COUNT][KEY_LEN + 1];
  for (int i = 0; i < COUNT; i++)
  {
    const Def &d = def(i);
    TEST_ASSERT_NOT_NULL(d.name);
    TEST_ASSERT_EQUAL(i, find(d.name));
    for (const char *c = d.name; *c; c++)
      TEST_ASSERT_TRUE_MESSAGE((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_', d.name);
    TEST_ASSERT_NULL_MESSAGE(std::strchr(d.help, '"'), d.name);
    TEST_ASSERT_TRUE_MESSAGE(d.min <= d.def && d.def <= d.max, d.name);
    // The default passes its own validation
    TEST_ASSERT_TRUE_MESSAGE(p.set(i, d.def), d.name);
    // Storage keys are unique after cutting to the NVS limit
    storageKey(i, keys[i]);
    for (int j = 0; j < i; j++)
      TEST_ASSERT_TRUE_MESSAGE(std::strcmp(keys[i], keys[j]) != 0, d.name);
  }
  TEST_ASSERT_EQUAL(-1, find("no_such_param"));
}

void test_default_filter_config_matches_settings(void)
{
  Params p;
  HeartTempoConfig a = p.heartTempoConfig(0), b = settings::heartTempoConfig();
  TEST_ASSERT_EQUAL(0, std::memcmp(&a, &b, sizeof(a)));
  HeartTempoConfig slow = p.heartTempoConfig(-1), fast = p.heartTempoConfig(1);
  TEST_ASSERT_EQUAL_FLOAT(settings::RATE_SLOW.maxChangePerBeat, slow.maxChangePerBeat);
  TEST_ASSERT_EQUAL_FLOAT(settings::RATE_SLOW.tauSec, slow.tauSec);
  TEST_ASSERT_EQUAL_FLOAT(settings::RATE_FAST.maxChangePerBeat, fast.maxChangePerBeat);
}

void test_changed_values_reach_the_filter_config(void)
{
  Params p;
  TEST_ASSERT_TRUE(p.set(RATE_FAST_MAX_CHANGE, 0.008f));
  TEST_ASSERT_TRUE(p.set(GATE_MAX_BPM, 20));
  TEST_ASSERT_TRUE(p.set(FEEDBACK_OFFSET_BPM, 10));
  TEST_ASSERT_EQUAL_FLOAT(0.008f, p.heartTempoConfig(1).maxChangePerBeat);
  TEST_ASSERT_EQUAL_FLOAT(settings::RATE_MEDIUM.maxChangePerBeat, p.heartTempoConfig(0).maxChangePerBeat);
  TEST_ASSERT_EQUAL_FLOAT(20, p.heartTempoConfig(0).gateMaxBpm);
  TEST_ASSERT_EQUAL_FLOAT(10, p.heartTempoConfig(-1).feedbackOffsetBpm);
}

// --- Validation -----------------------------------------------------------------

void test_step_beats_must_be_a_power_of_two(void)
{
  Params p;
  TEST_ASSERT_TRUE(p.set(COUNT_IN_BEATS, 0)); // so step_beats 1 is allowed
  for (int ok : {1, 2, 4, 8, 16, 32, 64})
    TEST_ASSERT_TRUE(p.set(STEP_BEATS, ok));
  for (int bad : {0, 3, 12, 48, 128, -16})
  {
    TEST_ASSERT_FALSE(p.set(STEP_BEATS, bad));
    TEST_ASSERT_EQUAL_STRING("step_beats: must be a power of two 1..64", p.error());
  }
  TEST_ASSERT_EQUAL(64, p.getInt(STEP_BEATS)); // unchanged by the failures
}

void test_range_and_type_are_checked(void)
{
  Params p;
  TEST_ASSERT_FALSE(p.set(LED_LEVEL, 2));
  TEST_ASSERT_EQUAL_STRING("led_level: must be 3..255", p.error());
  TEST_ASSERT_FALSE(p.set(LED_LEVEL, 10.5f));
  TEST_ASSERT_EQUAL_STRING("led_level: must be a whole number", p.error());
  TEST_ASSERT_FALSE(p.set(RATE_SLOW_TAU_SEC, 0));
  TEST_ASSERT_EQUAL_STRING("rate_slow_tau_sec: must be 1..300", p.error());
  TEST_ASSERT_FALSE(p.set(FEEDBACK_OFFSET_BPM, NAN));
  TEST_ASSERT_FALSE(p.set(LED_SHOW_EVERY_MISS, 2));
  TEST_ASSERT_EQUAL(settings::LED_LEVEL, p.getInt(LED_LEVEL));
}

void test_rules_between_params_are_enforced(void)
{
  Params p;
  TEST_ASSERT_FALSE(p.set(STEER_LOW_BPM, 130));
  TEST_ASSERT_EQUAL_STRING("steer_low_bpm: must be below steer_high_bpm (130)", p.error());
  TEST_ASSERT_FALSE(p.set(STEER_HIGH_BPM, 50));
  TEST_ASSERT_EQUAL_STRING("steer_high_bpm: must be above steer_low_bpm (55)", p.error());
  TEST_ASSERT_FALSE(p.set(GATE_FLOOR_BPM, 13));
  TEST_ASSERT_EQUAL_STRING("gate_floor_bpm: must be at most gate_max_bpm (12)", p.error());
  TEST_ASSERT_TRUE(p.set(GATE_FLOOR_BPM, 12)); // equal is fine
  TEST_ASSERT_FALSE(p.set(COUNT_IN_BEATS, 16));
  TEST_ASSERT_EQUAL_STRING("count_in_beats: must be below step_beats (16)", p.error());
  TEST_ASSERT_FALSE(p.set(STEP_BEATS, 4));
  TEST_ASSERT_EQUAL_STRING("step_beats: must be above count_in_beats (4)", p.error());
  TEST_ASSERT_FALSE(p.set(LOCK_TIMEOUT_POLLS, 5));
  TEST_ASSERT_EQUAL_STRING("lock_timeout_polls: must be above invalid_to_no_finger (5)", p.error());
  TEST_ASSERT_FALSE(p.set(INVALID_TO_NO_FINGER, 8));
}

void test_parse_by_type(void)
{
  Params p;
  float v;
  TEST_ASSERT_TRUE(p.parse(STEP_BEATS, "32", &v));
  TEST_ASSERT_EQUAL_FLOAT(32, v);
  TEST_ASSERT_FALSE(p.parse(STEP_BEATS, "32.5", &v));
  TEST_ASSERT_EQUAL_STRING("step_beats: must be a whole number", p.error());
  TEST_ASSERT_FALSE(p.parse(STEP_BEATS, "abc", &v));
  TEST_ASSERT_TRUE(p.parse(RATE_SLOW_MAX_CHANGE, "0.001", &v));
  TEST_ASSERT_EQUAL_FLOAT(0.001f, v);
  TEST_ASSERT_TRUE(p.parse(RATE_SLOW_MAX_CHANGE, "1e-3", &v));
  TEST_ASSERT_FALSE(p.parse(RATE_SLOW_MAX_CHANGE, "nan", &v));
  TEST_ASSERT_FALSE(p.parse(RATE_SLOW_MAX_CHANGE, "0.001x", &v));
  for (const char *t : {"true", "on", "1", "TRUE"})
  {
    TEST_ASSERT_TRUE(p.parse(LED_SHOW_EVERY_MISS, t, &v));
    TEST_ASSERT_EQUAL_FLOAT(1, v);
  }
  for (const char *f : {"false", "off", "0"})
  {
    TEST_ASSERT_TRUE(p.parse(LED_SHOW_EVERY_MISS, f, &v));
    TEST_ASSERT_EQUAL_FLOAT(0, v);
  }
  TEST_ASSERT_FALSE(p.parse(LED_SHOW_EVERY_MISS, "yes", &v));
}

void test_format_round_trips(void)
{
  Params p;
  char buf[24];
  for (int i = 0; i < COUNT; i++)
  {
    float v;
    p.format(i, p.get(i), buf, sizeof(buf));
    TEST_ASSERT_TRUE_MESSAGE(p.parse(i, buf, &v), def(i).name);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(p.get(i), v, def(i).name);
  }
  p.format(RATE_MEDIUM_MAX_CHANGE, 0.0015f, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("0.0015", buf);
  p.format(LED_SHOW_EVERY_MISS, 1, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("true", buf);
}

// --- Persistence ------------------------------------------------------------------

void test_save_and_load_round_trip(void)
{
  MemStore store;
  Params a;
  TEST_ASSERT_TRUE(a.set(STEP_BEATS, 32));
  TEST_ASSERT_TRUE(a.set(RATE_SLOW_TAU_SEC, 60));
  TEST_ASSERT_TRUE(a.set(LED_SHOW_EVERY_MISS, 0));
  TEST_ASSERT_TRUE(a.save(store));
  TEST_ASSERT_EQUAL(SCHEMA_VERSION, store.version);
  // Only values that differ from their default are stored
  TEST_ASSERT_EQUAL(3, (int)store.values.size());

  Params b;
  TEST_ASSERT_EQUAL(3, b.load(store));
  TEST_ASSERT_EQUAL(32, b.getInt(STEP_BEATS));
  TEST_ASSERT_EQUAL_FLOAT(60, b.get(RATE_SLOW_TAU_SEC));
  TEST_ASSERT_FALSE(b.getBool(LED_SHOW_EVERY_MISS));
  TEST_ASSERT_EQUAL(settings::LED_LEVEL, b.getInt(LED_LEVEL));

  // Back to the default: the stored value is removed
  TEST_ASSERT_TRUE(b.set(STEP_BEATS, settings::STEP_BEATS));
  TEST_ASSERT_TRUE(b.save(store));
  TEST_ASSERT_EQUAL(2, (int)store.values.size());
}

void test_load_without_saved_values_gives_defaults(void)
{
  MemStore store;
  Params p;
  p.set(STEP_BEATS, 64);
  TEST_ASSERT_EQUAL(0, p.load(store));
  TEST_ASSERT_EQUAL(settings::STEP_BEATS, p.getInt(STEP_BEATS));
}

void test_load_ignores_other_schema_version(void)
{
  MemStore store;
  store.version = SCHEMA_VERSION + 1;
  store.values["step_beats"] = 32;
  Params p;
  TEST_ASSERT_EQUAL(0, p.load(store));
  TEST_ASSERT_EQUAL(settings::STEP_BEATS, p.getInt(STEP_BEATS));
}

void test_load_skips_invalid_values(void)
{
  MemStore store;
  store.version = SCHEMA_VERSION;
  store.values["step_beats"] = 12;  // not a power of two
  store.values["led_level"] = 1000; // out of range
  store.values["gate_max_bpm"] = 20;
  Params p;
  TEST_ASSERT_EQUAL(1, p.load(store));
  TEST_ASSERT_EQUAL(settings::STEP_BEATS, p.getInt(STEP_BEATS));
  TEST_ASSERT_EQUAL(settings::LED_LEVEL, p.getInt(LED_LEVEL));
  TEST_ASSERT_EQUAL_FLOAT(20, p.get(GATE_MAX_BPM));
}

void test_load_of_inconsistent_set_falls_back_to_defaults(void)
{
  MemStore store;
  store.version = SCHEMA_VERSION;
  store.values["steer_low_bpm"] = 140; // above the default steer_high_bpm
  Params p;
  TEST_ASSERT_EQUAL(0, p.load(store));
  TEST_ASSERT_EQUAL_FLOAT(settings::STEER_LOW_BPM, p.get(STEER_LOW_BPM));
}

void test_long_names_get_cut_storage_keys(void)
{
  char key[KEY_LEN + 1];
  storageKey(RATE_MEDIUM_MAX_CHANGE, key); // 22 characters
  TEST_ASSERT_EQUAL_STRING("rate_medium_max", key);
  MemStore store;
  Params a, b;
  TEST_ASSERT_TRUE(a.set(RATE_MEDIUM_MAX_CHANGE, 0.002f));
  a.save(store);
  b.load(store);
  TEST_ASSERT_EQUAL_FLOAT(0.002f, b.get(RATE_MEDIUM_MAX_CHANGE));
}

// --- Commands ---------------------------------------------------------------------

void test_cmd_set_replies_and_reports_change(void)
{
  Rig r;
  TEST_ASSERT_EQUAL_STRING("ok step_beats=32", r.run("set step_beats 32").c_str());
  TEST_ASSERT_EQUAL(32, r.params.getInt(STEP_BEATS));
  TEST_ASSERT_EQUAL(1, (int)r.changes.size());
  TEST_ASSERT_EQUAL_STRING("step_beats 16 -> 32", r.changes[0].c_str());
  // Setting the same value again: ok, but no change
  TEST_ASSERT_EQUAL_STRING("ok step_beats=32", r.run("set step_beats 32").c_str());
  TEST_ASSERT_EQUAL(1, (int)r.changes.size());
  // Not saved until asked
  TEST_ASSERT_EQUAL(0, (int)r.store.values.size());
}

void test_cmd_set_errors(void)
{
  Rig r;
  TEST_ASSERT_EQUAL_STRING("err step_beats: must be a power of two 1..64", r.run("set step_beats 12").c_str());
  TEST_ASSERT_EQUAL_STRING("err step_beats: must be a whole number", r.run("set step_beats x").c_str());
  TEST_ASSERT_EQUAL_STRING("err unknown parameter nope (try get)", r.run("set nope 1").c_str());
  TEST_ASSERT_EQUAL_STRING("err set: wrong arguments (try help)", r.run("set step_beats").c_str());
  TEST_ASSERT_EQUAL_STRING("err set: wrong arguments (try help)", r.run("set step_beats 32 64").c_str());
  TEST_ASSERT_EQUAL_STRING("err steer_low_bpm: must be below steer_high_bpm (130)",
                           r.run("set steer_low_bpm 140").c_str());
  TEST_ASSERT_EQUAL_STRING("err unknown command fly (try help)", r.run("fly").c_str());
  TEST_ASSERT_EQUAL(0, (int)r.changes.size());
  TEST_ASSERT_EQUAL(settings::STEP_BEATS, r.params.getInt(STEP_BEATS));
}

void test_cmd_get_lists_everything(void)
{
  Rig r;
  TEST_ASSERT_EQUAL_STRING("ok get count=25", r.run("get").c_str());
  TEST_ASSERT_EQUAL(COUNT + 1, (int)r.lines.size());
  TEST_ASSERT_EQUAL_STRING(
      "param step_beats int 16 min=1 max=64 default=16 \"stepped mode: beats the tempo holds (power of two)\"",
      r.lines[STEP_BEATS].c_str());
  for (int i = 0; i < COUNT; i++)
    TEST_ASSERT_TRUE(startsWith(r.lines[i], "param "));
}

void test_cmd_get_one(void)
{
  Rig r;
  r.run("set led_show_every_miss off");
  TEST_ASSERT_EQUAL_STRING("ok get led_show_every_miss", r.run("get led_show_every_miss").c_str());
  TEST_ASSERT_EQUAL(2, (int)r.lines.size());
  TEST_ASSERT_EQUAL_STRING("param led_show_every_miss bool false min=false max=true default=true "
                           "\"sensor LED: amber on every missed reading\"",
                           r.lines[0].c_str());
  r.run("get rate_medium_max_change");
  TEST_ASSERT_TRUE(startsWith(r.lines[0], "param rate_medium_max_change float 0.0015 min=0.0001 max=0.02 default=0.0015 "));
  TEST_ASSERT_EQUAL_STRING("err unknown parameter nope (try get)", r.run("get nope").c_str());
}

void test_cmd_save_and_reset(void)
{
  Rig r;
  r.run("set step_beats 32");
  r.run("set led_level 40");
  TEST_ASSERT_EQUAL_STRING("ok saved", r.run("save").c_str());
  TEST_ASSERT_EQUAL(2, (int)r.store.values.size());

  // reset <name>: back to the default, not saved yet
  TEST_ASSERT_EQUAL_STRING("ok led_level=16", r.run("reset led_level").c_str());
  TEST_ASSERT_EQUAL(2, (int)r.store.values.size());

  // reset: everything back at once, but like set only stored by save
  r.changes.clear();
  TEST_ASSERT_EQUAL_STRING("ok reset", r.run("reset").c_str());
  TEST_ASSERT_EQUAL(settings::STEP_BEATS, r.params.getInt(STEP_BEATS));
  TEST_ASSERT_EQUAL(1, (int)r.changes.size()); // step_beats; led_level was already back
  TEST_ASSERT_EQUAL(2, (int)r.store.values.size()); // the saved values survive a restart
  TEST_ASSERT_EQUAL_STRING("ok saved", r.run("save").c_str());
  TEST_ASSERT_EQUAL(0, (int)r.store.values.size()); // defaults aren't stored
  TEST_ASSERT_EQUAL(SCHEMA_VERSION, r.store.version);
}

void test_cmd_help_and_reply_format(void)
{
  Rig r;
  TEST_ASSERT_EQUAL_STRING("ok help", r.run("help").c_str());
  TEST_ASSERT_TRUE(r.lines.size() >= 7);
  for (size_t i = 0; i + 1 < r.lines.size(); i++)
    TEST_ASSERT_TRUE(startsWith(r.lines[i], "help "));
  // No reply may look like a plotter line
  for (const char *cmd : {"help", "get", "set step_beats 3", "bogus", "save", "reset", "get x"})
  {
    r.run(cmd);
    for (const std::string &l : r.lines)
      TEST_ASSERT_TRUE(!l.empty() && l[0] != '>');
  }
}

void test_cmd_line_endings_and_whitespace(void)
{
  Rig r;
  for (char c : std::string("set  step_beats   8\r\n\r\nget step_beats\r"))
    r.console.feed(c);
  TEST_ASSERT_EQUAL(8, r.params.getInt(STEP_BEATS));
  TEST_ASSERT_EQUAL(3, (int)r.lines.size()); // ok, param, ok: blank lines are ignored
  TEST_ASSERT_EQUAL_STRING("ok step_beats=8", r.lines[0].c_str());
}

void test_cmd_overlong_line_is_rejected(void)
{
  Rig r;
  std::string longLine = "set step_beats 32" + std::string(200, ' ') + "x";
  TEST_ASSERT_EQUAL_STRING("err line too long (max 96 characters)", r.run(longLine.c_str()).c_str());
  TEST_ASSERT_EQUAL(settings::STEP_BEATS, r.params.getInt(STEP_BEATS));
  TEST_ASSERT_EQUAL_STRING("ok step_beats=32", r.run("set step_beats 32").c_str());
}

// --- Step length change on a common phrase boundary ---------------------------------

void test_step_change_waits_for_common_boundary(void)
{
  const int ppqn = 24;
  // 16 -> 32: only every 32 beats
  TEST_ASSERT_TRUE(stepChangeDue(0, 16, 32, ppqn));
  TEST_ASSERT_FALSE(stepChangeDue(16 * ppqn, 16, 32, ppqn));
  TEST_ASSERT_TRUE(stepChangeDue(32 * ppqn, 16, 32, ppqn));
  // 32 -> 8: also every 32 beats (a boundary of the old phrase)
  TEST_ASSERT_FALSE(stepChangeDue(8 * ppqn, 32, 8, ppqn));
  TEST_ASSERT_TRUE(stepChangeDue(64 * ppqn, 32, 8, ppqn));
  // Never between beats
  TEST_ASSERT_FALSE(stepChangeDue(32 * ppqn + 1, 16, 32, ppqn));
}

void test_tempo_multiplier_is_1_or_2(void)
{
  Rig r;
  TEST_ASSERT_EQUAL(1, r.params.getInt(TEMPO_MULTIPLIER));
  TEST_ASSERT_EQUAL_STRING("ok tempo_multiplier=2", r.run("set tempo_multiplier 2").c_str());
  TEST_ASSERT_EQUAL_STRING("err tempo_multiplier: must be 1..2", r.run("set tempo_multiplier 3").c_str());
  TEST_ASSERT_EQUAL_STRING("err tempo_multiplier: must be a whole number", r.run("set tempo_multiplier 1.5").c_str());
  // Not part of the filter: HeartTempo keeps working in heart rate terms
  HeartTempoConfig a = r.params.heartTempoConfig(0), b = settings::heartTempoConfig();
  TEST_ASSERT_EQUAL(0, std::memcmp(&a, &b, sizeof(a)));
}

void test_multiplier_change_lands_on_a_bar_line(void)
{
  const int ppqn = 24;
  TEST_ASSERT_TRUE(barBoundary(0, 4, ppqn));
  TEST_ASSERT_FALSE(barBoundary(1 * ppqn, 4, ppqn));
  TEST_ASSERT_FALSE(barBoundary(4 * ppqn - 1, 4, ppqn));
  TEST_ASSERT_TRUE(barBoundary(4 * ppqn, 4, ppqn));
  TEST_ASSERT_TRUE(barBoundary(3 * ppqn, 3, ppqn));
}

void test_multiplier_scales_interval_with_cap(void)
{
  const uint32_t at90 = 60000000 / (90 * 24), at150 = 60000000 / (150 * 24);
  const uint32_t cap240 = 60000000 / (240 * 24);
  TEST_ASSERT_EQUAL_UINT32(at90, scaleInterval(at90, 1, cap240));
  TEST_ASSERT_EQUAL_UINT32(at90 / 2, scaleInterval(at90, 2, cap240)); // 180 bpm
  TEST_ASSERT_EQUAL_UINT32(cap240, scaleInterval(at150, 2, cap240));  // 300 bpm capped to 240
  // The cap keeps the tempo CC (bpm x 64, 14 bit) in range
  TEST_ASSERT_TRUE(settings::MAX_CLOCK_BPM * 64 < 16384);
}

// --- Control from the DAW (incoming MIDI CC) -----------------------------------------

// A USB-MIDI event packet: control change on a channel (1-16)
struct Packet
{
  uint8_t b[4];
};

static Packet cc(uint8_t number, uint8_t value, uint8_t channel = settings::CONTROL_CC_CHANNEL)
{
  return {{0x0B, (uint8_t)(0xB0 | (channel - 1)), number, value}};
}

// MidiControl + RateSelect + Params, collecting the log lines main.cpp would print
struct CcRig
{
  Params params;
  MidiControl control;
  RateSelect rate;
  std::vector<std::string> changes;
  std::vector<std::string> refused;

  // Feeds packets as one pass and applies them
  void pass(std::initializer_list<Packet> packets)
  {
    for (const Packet &p : packets)
      control.feed(p.b);
    control.apply(
        params, rate,
        [this](int id, float o, float n, const char *source) {
          char buf[80];
          std::snprintf(buf, sizeof(buf), "%s %g -> %g (%s)", def(id).name, o, n, source);
          changes.push_back(buf);
        },
        [this](const char *error, const char *source) { refused.push_back(std::string(source) + ": " + error); });
  }
};

void test_cc_mappings(void)
{
  // rate: 0-42 slow, 43-85 medium, 86-127 fast
  const int rates[][2] = {{0, -1}, {42, -1}, {43, 0}, {85, 0}, {86, 1}, {127, 1}};
  for (const auto &r : rates)
    TEST_ASSERT_EQUAL(r[1], mc::rateFor(r[0]));
  // feedback offset: linear 0-10 bpm in tenths
  TEST_ASSERT_EQUAL_FLOAT(0, mc::feedbackOffsetFor(0));
  TEST_ASSERT_EQUAL_FLOAT(10, mc::feedbackOffsetFor(127));
  TEST_ASSERT_EQUAL_FLOAT(5.0f, mc::feedbackOffsetFor(64)); // 5.04 -> 5.0
  TEST_ASSERT_EQUAL_FLOAT(0.1f, mc::feedbackOffsetFor(1));  // 0.079 -> 0.1
  for (int v = 1; v < 128; v++)
    TEST_ASSERT_TRUE(mc::feedbackOffsetFor(v) >= mc::feedbackOffsetFor(v - 1)); // never goes back
  // tempo multiplier: two halves
  TEST_ASSERT_EQUAL(1, mc::tempoMultiplierFor(0));
  TEST_ASSERT_EQUAL(1, mc::tempoMultiplierFor(63));
  TEST_ASSERT_EQUAL(2, mc::tempoMultiplierFor(64));
  TEST_ASSERT_EQUAL(2, mc::tempoMultiplierFor(127));
  // Every mapped value passes the registry's own checks
  for (int v = 0; v < 128; v++)
  {
    Params p;
    TEST_ASSERT_TRUE(p.set(FEEDBACK_OFFSET_BPM, mc::feedbackOffsetFor(v)));
    TEST_ASSERT_TRUE(p.set(TEMPO_MULTIPLIER, mc::tempoMultiplierFor(v)));
  }
}

void test_cc_filtering(void)
{
  TEST_ASSERT_EQUAL(mc::RATE, mc::controlFor(cc(22, 0).b));
  TEST_ASSERT_EQUAL(mc::FEEDBACK_OFFSET, mc::controlFor(cc(23, 0).b));
  TEST_ASSERT_EQUAL(mc::TEMPO_MULTIPLIER, mc::controlFor(cc(24, 0).b));
  // The step length is set through the settings page, not by CC
  TEST_ASSERT_EQUAL(-1, mc::controlFor(cc(21, 100).b));
  // Our own tempo CC pair coming back through MIDI-thru
  TEST_ASSERT_EQUAL(-1, mc::controlFor(cc(settings::TEMPO_CC, 10).b));
  TEST_ASSERT_EQUAL(-1, mc::controlFor(cc(settings::TEMPO_CC + 32, 10).b));
  // Other channels, other CCs
  TEST_ASSERT_EQUAL(-1, mc::controlFor(cc(22, 100, 1).b));
  TEST_ASSERT_EQUAL(-1, mc::controlFor(cc(22, 100, 15).b));
  TEST_ASSERT_EQUAL(-1, mc::controlFor(cc(7, 100).b));
  // Note on (same bytes otherwise), clock, a malformed value
  const uint8_t noteOn[4] = {0x09, 0x9F, 22, 100};
  const uint8_t clock[4] = {0x0F, 0xF8, 0, 0};
  const uint8_t badValue[4] = {0x0B, 0xBF, 22, 0x80};
  TEST_ASSERT_EQUAL(-1, mc::controlFor(noteOn));
  TEST_ASSERT_EQUAL(-1, mc::controlFor(clock));
  TEST_ASSERT_EQUAL(-1, mc::controlFor(badValue));

  // Ignored messages change nothing
  CcRig r;
  r.pass({cc(20, 0), cc(52, 127), cc(21, 127), cc(22, 127, 1), {{0x09, 0x9F, 22, 127}}});
  TEST_ASSERT_EQUAL(0, (int)r.changes.size());
  TEST_ASSERT_EQUAL(settings::STEP_BEATS, r.params.getInt(STEP_BEATS));
  TEST_ASSERT_FALSE(r.rate.overridden());
}

void test_cc_changes_params_like_set(void)
{
  CcRig r;
  r.pass({cc(24, 100)});
  TEST_ASSERT_EQUAL(2, r.params.getInt(TEMPO_MULTIPLIER));
  TEST_ASSERT_EQUAL(1, (int)r.changes.size());
  TEST_ASSERT_EQUAL_STRING("tempo_multiplier 1 -> 2 (cc24=100)", r.changes[0].c_str());
  r.pass({cc(23, 127)});
  TEST_ASSERT_EQUAL_FLOAT(10, r.params.get(FEEDBACK_OFFSET_BPM));
  TEST_ASSERT_EQUAL_STRING("feedback_offset_bpm 5 -> 10 (cc23=127)", r.changes[1].c_str());
  // Not saved: a later save stores them, like set
  MemStore store;
  TEST_ASSERT_EQUAL(0, (int)store.values.size());
  TEST_ASSERT_TRUE(r.params.save(store));
  TEST_ASSERT_EQUAL(2, (int)store.values.size());
}

void test_cc_only_acts_on_a_changed_value(void)
{
  CcRig r;
  // A knob stream within one zone: one change
  r.pass({cc(24, 96), cc(24, 100), cc(24, 110)});
  r.pass({cc(24, 120), cc(24, 127)});
  TEST_ASSERT_EQUAL(1, (int)r.changes.size());
  // A burst is coalesced to its last value per control
  r.pass({cc(24, 70), cc(24, 40), cc(24, 20)});
  TEST_ASSERT_EQUAL(2, (int)r.changes.size());
  TEST_ASSERT_EQUAL_STRING("tempo_multiplier 2 -> 1 (cc24=20)", r.changes[1].c_str());
  // There and back within one pass: nothing
  r.pass({cc(24, 100), cc(24, 10)});
  TEST_ASSERT_EQUAL(2, (int)r.changes.size());
  // A value equal to the current one isn't reported (feedback 5 = cc 64)
  r.pass({cc(23, 64)});
  TEST_ASSERT_EQUAL(2, (int)r.changes.size());
  // Last change wins: after a serial set the knob acts again once its zone changes
  TEST_ASSERT_TRUE(r.params.set(TEMPO_MULTIPLIER, 2));
  r.pass({cc(24, 30)}); // same zone as the knob's last value: nothing
  TEST_ASSERT_EQUAL(2, r.params.getInt(TEMPO_MULTIPLIER));
  r.pass({cc(24, 90)});
  r.pass({cc(24, 10)});
  TEST_ASSERT_EQUAL(1, r.params.getInt(TEMPO_MULTIPLIER));
}

void test_cc_rate_override(void)
{
  RateSelect rs;
  rs.setSwitch(0);
  TEST_ASSERT_EQUAL(0, rs.effective());
  TEST_ASSERT_FALSE(rs.overridden());
  // CC overrides the switch
  rs.setOverride(1, 100);
  TEST_ASSERT_EQUAL(1, rs.effective());
  TEST_ASSERT_TRUE(rs.overridden());
  // The switch staying where it is keeps the override
  rs.setSwitch(0);
  TEST_ASSERT_EQUAL(1, rs.effective());
  // Moving it wins
  rs.setSwitch(-1);
  TEST_ASSERT_EQUAL(-1, rs.effective());
  TEST_ASSERT_FALSE(rs.overridden());
  // ...until the next CC
  rs.setOverride(0, 60);
  TEST_ASSERT_EQUAL(0, rs.effective());
  TEST_ASSERT_EQUAL(60, rs.ccValue());

  // Through MidiControl: zones, only on change, no parameter touched
  CcRig r;
  r.rate.setSwitch(0);
  r.pass({cc(22, 127)});
  TEST_ASSERT_EQUAL(1, r.rate.effective());
  r.rate.setSwitch(-1); // switch moved: slow
  r.pass({cc(22, 120)}); // same zone as before: no change, the switch keeps it
  TEST_ASSERT_EQUAL(-1, r.rate.effective());
  r.pass({cc(22, 50)});
  TEST_ASSERT_EQUAL(0, r.rate.effective());
  TEST_ASSERT_EQUAL(0, (int)r.changes.size());
}

#ifdef ARDUINO
void test_parse_int_edge_cases(void)
{
  Params p;
  float v;
  TEST_ASSERT_TRUE(p.parse(LED_LEVEL, "+40", &v));
  TEST_ASSERT_EQUAL_FLOAT(40, v);
  TEST_ASSERT_TRUE(p.parse(LED_LEVEL, "-5", &v)); // parses, then fails the range check
  TEST_ASSERT_FALSE(p.set(LED_LEVEL, v));
  TEST_ASSERT_TRUE(p.parse(LED_LEVEL, "99999999999", &v)); // beyond long: clamped, then out of range
  TEST_ASSERT_FALSE(p.set(LED_LEVEL, v));
  TEST_ASSERT_FALSE(p.parse(LED_LEVEL, "0x10", &v));
  TEST_ASSERT_FALSE(p.parse(LED_LEVEL, "", &v));
  TEST_ASSERT_FALSE(p.parse(RATE_SLOW_MAX_CHANGE, "inf", &v));
  TEST_ASSERT_FALSE(p.parse(RATE_SLOW_MAX_CHANGE, "", &v));
  TEST_ASSERT_EQUAL(settings::LED_LEVEL, p.getInt(LED_LEVEL));
}

void test_load_of_inconsistent_set_names_the_rule(void)
{
  MemStore store;
  store.version = SCHEMA_VERSION;
  store.values["steer_low_bpm"] = 140; // above the default steer_high_bpm: as if that default moved
  Params p;
  TEST_ASSERT_EQUAL(0, p.load(store));
  TEST_ASSERT_EQUAL_STRING("steer_low_bpm (140) must be below steer_high_bpm (130)", p.error());
  // A load that works clears the message
  store.values.clear();
  TEST_ASSERT_EQUAL(0, p.load(store));
  TEST_ASSERT_EQUAL_STRING("", p.error());
}

// A store whose writes fail (NVS full or unavailable)
class FailingStore : public MemStore
{
public:
  bool store(const char *, float) override { return false; }
};

void test_cmd_save_reports_a_store_failure(void)
{
  Params params;
  FailingStore store;
  std::vector<std::string> lines;
  ParamConsole console(params, store, [&](const char *l) { lines.push_back(l); });
  console.handleLine("set led_level 40");
  console.handleLine("save");
  TEST_ASSERT_EQUAL_STRING("err save failed", lines.back().c_str());
  TEST_ASSERT_EQUAL(40, params.getInt(LED_LEVEL)); // the running value is kept
}

void test_cmd_reset_one_is_checked_like_set(void)
{
  Rig r;
  r.run("set step_beats 32");
  r.run("set count_in_beats 16");
  // The default step length would not be above the count-in: refused, nothing changes
  TEST_ASSERT_EQUAL_STRING("err step_beats: must be above count_in_beats (16)", r.run("reset step_beats").c_str());
  TEST_ASSERT_EQUAL(32, r.params.getInt(STEP_BEATS));
  r.changes.clear();
  TEST_ASSERT_TRUE(startsWith(r.run("reset count_in_beats"), "ok count_in_beats="));
  TEST_ASSERT_TRUE(startsWith(r.run("reset step_beats"), "ok step_beats="));
  TEST_ASSERT_EQUAL(settings::STEP_BEATS, r.params.getInt(STEP_BEATS));
  TEST_ASSERT_EQUAL(2, (int)r.changes.size());
}

void test_nvs_round_trip(void);
#endif

static void runTests()
{
  UNITY_BEGIN();
  RUN_TEST(test_defaults_come_from_settings);
  RUN_TEST(test_every_param_is_well_formed);
  RUN_TEST(test_default_filter_config_matches_settings);
  RUN_TEST(test_changed_values_reach_the_filter_config);
  RUN_TEST(test_step_beats_must_be_a_power_of_two);
  RUN_TEST(test_range_and_type_are_checked);
  RUN_TEST(test_rules_between_params_are_enforced);
  RUN_TEST(test_parse_by_type);
  RUN_TEST(test_format_round_trips);
  RUN_TEST(test_save_and_load_round_trip);
  RUN_TEST(test_load_without_saved_values_gives_defaults);
  RUN_TEST(test_load_ignores_other_schema_version);
  RUN_TEST(test_load_skips_invalid_values);
  RUN_TEST(test_load_of_inconsistent_set_falls_back_to_defaults);
  RUN_TEST(test_long_names_get_cut_storage_keys);
  RUN_TEST(test_cmd_set_replies_and_reports_change);
  RUN_TEST(test_cmd_set_errors);
  RUN_TEST(test_cmd_get_lists_everything);
  RUN_TEST(test_cmd_get_one);
  RUN_TEST(test_cmd_save_and_reset);
  RUN_TEST(test_cmd_help_and_reply_format);
  RUN_TEST(test_cmd_line_endings_and_whitespace);
  RUN_TEST(test_cmd_overlong_line_is_rejected);
  RUN_TEST(test_parse_int_edge_cases);
  RUN_TEST(test_load_of_inconsistent_set_names_the_rule);
  RUN_TEST(test_cmd_save_reports_a_store_failure);
  RUN_TEST(test_cmd_reset_one_is_checked_like_set);
  RUN_TEST(test_step_change_waits_for_common_boundary);
  RUN_TEST(test_tempo_multiplier_is_1_or_2);
  RUN_TEST(test_multiplier_change_lands_on_a_bar_line);
  RUN_TEST(test_multiplier_scales_interval_with_cap);
  RUN_TEST(test_cc_mappings);
  RUN_TEST(test_cc_filtering);
  RUN_TEST(test_cc_changes_params_like_set);
  RUN_TEST(test_cc_only_acts_on_a_changed_value);
  RUN_TEST(test_cc_rate_override);
#ifdef ARDUINO
  RUN_TEST(test_nvs_round_trip);
#endif
  UNITY_END();
}

#ifdef ARDUINO
// The real NVS store, in its own namespace so the device's saved settings stay untouched
void test_nvs_round_trip(void)
{
  ParamNvs nvs;
  TEST_ASSERT_TRUE(nvs.begin("hr2midi_test"));
  TEST_ASSERT_TRUE(nvs.clear());
  TEST_ASSERT_EQUAL(-1, nvs.schema());
  float v;
  TEST_ASSERT_FALSE(nvs.load("step_beats", &v));
  TEST_ASSERT_TRUE(nvs.remove("step_beats")); // removing a missing key is fine

  Params a, b;
  TEST_ASSERT_TRUE(a.set(STEP_BEATS, 64));
  TEST_ASSERT_TRUE(a.set(RATE_MEDIUM_MAX_CHANGE, 0.002f));
  TEST_ASSERT_TRUE(a.save(nvs));
  nvs.end();

  TEST_ASSERT_TRUE(nvs.begin("hr2midi_test"));
  TEST_ASSERT_EQUAL(2, b.load(nvs));
  TEST_ASSERT_EQUAL(64, b.getInt(STEP_BEATS));
  TEST_ASSERT_EQUAL_FLOAT(0.002f, b.get(RATE_MEDIUM_MAX_CHANGE));
  TEST_ASSERT_TRUE(nvs.clear());
  nvs.end();
}
#endif

#ifdef ARDUINO
void setup()
{
  Serial.begin(115200);
}

// Repeat the run, so a monitor opened at any time catches a complete one.
void loop()
{
  delay(5000);
  runTests();
}
#else
int main()
{
  runTests();
  return 0;
}
#endif
