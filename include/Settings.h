// ============================================================================
// All the settings to tweak, in one place. Change a value, then build & upload.
// ============================================================================
#pragma once
#include <cstdint>
#include <HeartTempo.h>

namespace settings
{

// --- Feedback switch: − / 0 / + -------------------------------------------
// Velleman 8016 (ON-OFF-ON). Middle terminal to GND, outer terminals to these
// pins. If − and + come out the wrong way round, swap the two pin numbers.
constexpr int FEEDBACK_PIN_MINUS = 4;
constexpr int FEEDBACK_PIN_PLUS = 5;

// How far − and + put the tempo target below/above the heart rate.
constexpr float FEEDBACK_OFFSET_BPM = 5;
// Steering never takes the target past these. They never push against the
// switch: in − with a heart rate below STEER_LOW_BPM the target just follows.
constexpr float STEER_LOW_BPM = 55;
constexpr float STEER_HIGH_BPM = 130;

// --- Rate switch: slow / medium / fast -------------------------------------
// Second 8016, wired the same way. Centre = medium.
constexpr int RATE_PIN_SLOW = 6;
constexpr int RATE_PIN_FAST = 7;

struct RatePreset
{
  const char *name;
  // Fastest the clock tempo may move, as a fraction of the tempo per beat.
  // Listeners start noticing a continuous drift at ~0.0017-0.0027 (0.17-0.27 %).
  // At 90 bpm: 0.00075 = 0.1 bpm/s, 0.0015 = 0.2 bpm/s, 0.004 = 0.54 bpm/s.
  float maxChangePerBeat;
  // Smoothing time of the heart rate estimate: how long it takes to cover ~63 %
  // of a change in heart rate. Larger = calmer, slower to react.
  float tauSec;
};

constexpr RatePreset RATE_SLOW = {"slow", 0.00075f, 40};
constexpr RatePreset RATE_MEDIUM = {"medium", 0.0015f, 25};
constexpr RatePreset RATE_FAST = {"fast", 0.004f, 12};

// Ignore contact bounce shorter than this when a switch is flipped.
constexpr uint32_t SWITCH_DEBOUNCE_MS = 30;

// --- Heart rate filter -------------------------------------------------------
constexpr float START_BPM = 90;          // clock tempo at power-on, before the first lock
constexpr float VALID_MIN_BPM = 40;      // readings outside this range are ignored
constexpr float VALID_MAX_BPM = 180;
constexpr int INVALID_TO_NO_FINGER = 5;  // missing readings in a row (x 4 s) before "no finger"
constexpr int AGREE_COUNT = 3;           // readings of the last 4 that must agree to lock
constexpr float AGREE_TOL_BPM = 4;       // ...within this distance of each other's median
constexpr float GATE_FLOOR_BPM = 8;      // while locked, readings this close to recent ones always pass

// --- Sensor --------------------------------------------------------------------
constexpr int SENSOR_SDA = 17;
constexpr int SENSOR_SCL = 9;
constexpr uint32_t SENSOR_POLL_MS = 4000; // the sensor computes a new value about every 4 s

// --- Status LED ------------------------------------------------------------------
// 21 = external WS2812D-F8. 48 = onboard LED (needs the RGB solder jumper bridged).
constexpr int LED_PIN = 21;
constexpr uint8_t LED_LEVEL = 16;          // brightness 0-255; the WS2812D-F8 is very bright
constexpr uint32_t BEAT_FLASH_MS = 80;     // length of the green flash on each beat

// ============================================================================
// Builds the filter configuration from the values above (no need to edit).
inline HeartTempoConfig heartTempoConfig()
{
  HeartTempoConfig c;
  c.startBpm = START_BPM;
  c.minBpm = VALID_MIN_BPM;
  c.maxBpm = VALID_MAX_BPM;
  c.invalidToNoFinger = INVALID_TO_NO_FINGER;
  c.agreeCount = AGREE_COUNT;
  c.agreeTolBpm = AGREE_TOL_BPM;
  c.gateFloorBpm = GATE_FLOOR_BPM;
  c.maxChangePerBeat = RATE_MEDIUM.maxChangePerBeat;
  c.tauSec = RATE_MEDIUM.tauSec;
  c.feedbackOffsetBpm = FEEDBACK_OFFSET_BPM;
  c.steerLowBpm = STEER_LOW_BPM;
  c.steerHighBpm = STEER_HIGH_BPM;
  return c;
}

} // namespace settings
