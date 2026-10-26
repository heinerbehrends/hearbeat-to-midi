// Runtime settings: a registry of the tunable values from include/Settings.h that
// can be changed without recompiling (serial commands, MIDI CC from the DAW via
// MidiControl.h; the Web Serial page in docs/), with validation and persistence.
// Pure C++ (no Arduino dependencies) so it can be tested off the board; the
// Arduino side only supplies a ParamStore (NVS, see lib/ParamNvs).
//
// The defaults are the constants in Settings.h. All values are held as float
// (every int in range is exact), and the type only decides parsing and printing.
#pragma once
#include <cstddef>
#include <cstdint>
#include <HeartTempo.h>

namespace params
{

// Order = order of the `get` listing. Appending or reordering is safe (values
// are stored under their names); renaming one forgets its saved value.
enum Id : uint8_t
{
  TEMPO_MULTIPLIER,
  STEP_BEATS,
  COUNT_IN_BEATS,
  BEATS_PER_BAR,
  RATE_SLOW_MAX_CHANGE,
  RATE_SLOW_TAU_SEC,
  RATE_MEDIUM_MAX_CHANGE,
  RATE_MEDIUM_TAU_SEC,
  RATE_FAST_MAX_CHANGE,
  RATE_FAST_TAU_SEC,
  FEEDBACK_OFFSET_BPM,
  STEER_LOW_BPM,
  STEER_HIGH_BPM,
  START_BPM,
  CAUTION_ABOVE_BPM,
  CAUTION_STEP_BPM,
  GATE_FLOOR_BPM,
  GATE_MAX_BPM,
  AGREE_TOL_BPM,
  LOCK_TIMEOUT_POLLS,
  INVALID_TO_NO_FINGER,
  LED_LEVEL,
  LED_SHOW_EVERY_MISS,
  TRANSPORT_LED_LEVEL,
  STOPPED_LEVEL,
  COUNT
};

enum class Type : uint8_t
{
  Int,
  Float,
  Bool,
};

struct Def
{
  const char *name; // snake_case, used by the commands and as the storage key
  Type type;
  float def;        // from Settings.h
  float min;
  float max;
  const char *help; // short, no double quotes (it is printed in quotes)
};

const Def &def(int id);
// Index of the parameter with this name, or -1.
int find(const char *name);
const char *typeName(Type t);

// Bumped when a stored value would mean something different to this firmware
// (a unit or meaning changed). A stored set with another version is ignored.
// Adding or removing parameters needs no bump: missing ones use their default.
constexpr uint16_t SCHEMA_VERSION = 1;
constexpr size_t KEY_LEN = 15; // NVS key limit
// Storage key: the name, cut to KEY_LEN characters (a test checks they stay unique).
void storageKey(int id, char out[KEY_LEN + 1]);

} // namespace params

// Where values are persisted. On the board: NVS (lib/ParamNvs); in tests: memory.
class ParamStore
{
public:
  virtual ~ParamStore() = default;
  virtual bool load(const char *key, float *value) = 0; // false = not stored
  virtual bool store(const char *key, float value) = 0;
  virtual bool remove(const char *key) = 0;             // true also when not stored
  virtual int schema() = 0;                              // -1 = none stored
  virtual bool setSchema(int version) = 0;
  virtual bool clear() = 0;
};

class Params
{
public:
  Params(); // all defaults

  float get(int id) const { return v_[id]; }
  int getInt(int id) const { return (int)v_[id]; }
  bool getBool(int id) const { return v_[id] != 0; }

  // Validates against the range, the type and the rules between parameters
  // (the ones Settings.h checks with static_assert). On failure nothing changes
  // and error() says why, e.g. "step_beats: must be a power of two 1..64".
  bool set(int id, float value);
  // Parses text for this parameter's type (int: whole number; bool: true/false,
  // on/off, 1/0). On failure error() says why.
  bool parse(int id, const char *text, float *value);
  // Prints a value the way parse() reads it back: "16", "0.0015", "true".
  void format(int id, float value, char *out, size_t size) const;
  const char *error() const { return err_; }

  // Back to the defaults from Settings.h (not persisted by itself).
  void resetAll();

  // Loads saved values. Values that are missing, out of range or from another
  // schema version keep their default; if the loaded set as a whole breaks a
  // rule between parameters (a default moved in Settings.h since the save),
  // all defaults are used and error() names the rule. Returns how many values
  // were loaded (0 = all defaults).
  int load(ParamStore &store);
  // Saves the values that differ from their default and removes the others, so
  // a changed default in Settings.h still applies to values never set.
  bool save(ParamStore &store);

  // The filter configuration for a rate switch position (-1 slow, 0 medium, +1 fast).
  HeartTempoConfig heartTempoConfig(int ratePos) const;
  // Rate preset values for a switch position.
  float maxChangePerBeat(int ratePos) const;
  float tauSec(int ratePos) const;

private:
  // Range, type and single-value rules; false sets err_.
  bool checkValue(int id, float value);
  // Rules between parameters for the whole set; false sets err_ (named after `id`).
  bool checkSet(const float *v, int id);

  float v_[params::COUNT];
  char err_[96] = "";
};
