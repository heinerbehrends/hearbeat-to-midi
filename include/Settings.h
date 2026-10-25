// ============================================================================
// All the settings to tweak, in one place. Change a value, then build & upload.
// ============================================================================
#pragma once
#include <cstdint>
#include <HeartTempo.h>

namespace settings
{

// Name shown for the USB device and for its MIDI port (max 32 characters).
// Windows may cache the old name: unplug and replug after changing it.
constexpr const char *DEVICE_NAME = "Heart Rate To MIDI";

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

// --- Transport switch: stop / run / run stepped -------------------------------
// Third 8016, wired like the others. Left = stop, centre = run (tempo glides),
// right = run stepped (tempo changes only every STEP_BEATS beats). Run is in the
// centre so flipping between the two running modes never passes through stop.
// Starting sends Song Position 0 + Start, so the DAW starts from the top in phase;
// stop sends Stop. The MIDI clock keeps running either way.
constexpr int TRANSPORT_PIN_STOP = 15;
constexpr int TRANSPORT_PIN_STEPPED = 16;
// false until the switch is wired (an unwired switch would read "run"): the
// board's BOOT button then cycles stop -> run -> stepped instead. Don't hold BOOT
// while the board resets, that enters download mode.
constexpr bool TRANSPORT_SWITCH_INSTALLED = false;

// Stepped mode: the clock tempo holds for this many beats (counted from Start) and
// then jumps to wherever the glide has got to. 16 beats = 4 bars of 4/4.
constexpr int STEP_BEATS = 16;

// --- Tempo as MIDI CC (for REAPER) -----------------------------------------
// REAPER doesn't follow MIDI clock tempo. The device also sends its tempo as a
// 14-bit CC pair (value = bpm x 64: MSB on TEMPO_CC, LSB on TEMPO_CC + 32), read by
// tools/reaper/heart_rate_tempo.lua. Keep these in sync with that script.
constexpr bool TEMPO_CC_ENABLED = true;
constexpr uint8_t TEMPO_CC_CHANNEL = 16; // 1-16
constexpr uint8_t TEMPO_CC = 20;         // 0-31 (20 is undefined in the MIDI spec)
constexpr uint32_t TEMPO_CC_RESEND_MS = 2000; // also resend unchanged, for late listeners

// --- Heart rate filter -------------------------------------------------------
constexpr float START_BPM = 70;          // clock tempo at power-on, before the first lock
constexpr float VALID_MIN_BPM = 40;      // readings outside this range are ignored
constexpr float VALID_MAX_BPM = 180;
constexpr int INVALID_TO_NO_FINGER = 5;  // readings without heart rate in a row (x 4 s) before "no finger"
constexpr int MISSING_TO_NO_SENSOR = 3;  // polls without an answer in a row before "no sensor";
                                         // fewer are treated as a gap, so a bus glitch keeps the lock
constexpr int AGREE_COUNT = 3;           // readings of the last 4 that must agree to lock
constexpr float AGREE_TOL_BPM = 4;       // ...within this distance of each other's median
constexpr float GATE_FLOOR_BPM = 8;      // while locked, readings this close to recent ones always pass
// The sensor's typical junk is a high reading out of nowhere: readings above
// CAUTION_ABOVE_BPM only count when within CAUTION_STEP_BPM of the previous kept
// reading (a smooth series), or once AGREE_COUNT of them in a row agree.
constexpr float CAUTION_ABOVE_BPM = 100;
constexpr float CAUTION_STEP_BPM = 8;

// --- Sensor --------------------------------------------------------------------
constexpr int SENSOR_SDA = 17;
constexpr int SENSOR_SCL = 9;
// The sensor computes a new value about every 4 s. Polling faster reads the same
// value twice, and repeats would count as "agreeing" readings.
constexpr uint32_t SENSOR_POLL_MS = 4000;
// A sensor that reset unnoticed stops measuring: re-send "start" after this many
// readings in a row without heart rate (keep it below INVALID_TO_NO_FINGER).
constexpr int START_RETRY_AFTER_INVALID = 4;
// Only trust the heart rate while the sensor also reports a valid SpO2 (result
// byte 1). When the sensor is disturbed it drops SpO2 but keeps reporting a
// drifting, too-high heart rate; this treats those readings as missing.
constexpr bool REQUIRE_SPO2_VALID = true;
// Debug: also log the sensor's full 8-byte result as hex on every poll
// (byte 0 = SpO2, 1 = SpO2 valid, 2-5 = heart rate, 7 = heart rate present).
constexpr bool LOG_RAW_SENSOR_BYTES = true;

// --- Status LED ------------------------------------------------------------------
// 21 = external WS2812D-F8. 48 = onboard LED (needs the RGB solder jumper bridged).
constexpr int LED_PIN = 21;
constexpr uint8_t LED_LEVEL = 16;          // brightness 3-255; the WS2812D-F8 is very bright
// true (testing): amber as soon as one reading is missing or rejected, even while
// the tempo keeps following. false (stage): green for as long as the lock holds.
constexpr bool LED_SHOW_EVERY_MISS = true;

// ============================================================================
// Builds the filter configuration from the values above (no need to edit).
inline HeartTempoConfig heartTempoConfig()
{
  HeartTempoConfig c;
  c.startBpm = START_BPM;
  c.minBpm = VALID_MIN_BPM;
  c.maxBpm = VALID_MAX_BPM;
  c.invalidToNoFinger = INVALID_TO_NO_FINGER;
  c.missingToNoSensor = MISSING_TO_NO_SENSOR;
  c.agreeCount = AGREE_COUNT;
  c.agreeTolBpm = AGREE_TOL_BPM;
  c.gateFloorBpm = GATE_FLOOR_BPM;
  c.cautionAboveBpm = CAUTION_ABOVE_BPM;
  c.cautionStepBpm = CAUTION_STEP_BPM;
  c.maxChangePerBeat = RATE_MEDIUM.maxChangePerBeat;
  c.tauSec = RATE_MEDIUM.tauSec;
  c.feedbackOffsetBpm = FEEDBACK_OFFSET_BPM;
  c.steerLowBpm = STEER_LOW_BPM;
  c.steerHighBpm = STEER_HIGH_BPM;
  return c;
}

// Catch values that would silently break the device (the build fails with the message)
static_assert(FEEDBACK_OFFSET_BPM >= 0, "FEEDBACK_OFFSET_BPM must not be negative");
static_assert(STEER_LOW_BPM < STEER_HIGH_BPM, "STEER_LOW_BPM must be below STEER_HIGH_BPM");
static_assert(VALID_MIN_BPM < VALID_MAX_BPM, "VALID_MIN_BPM must be below VALID_MAX_BPM");
static_assert(START_BPM >= VALID_MIN_BPM && START_BPM <= VALID_MAX_BPM, "START_BPM outside the valid range");
static_assert(RATE_SLOW.maxChangePerBeat > 0 && RATE_MEDIUM.maxChangePerBeat > 0 && RATE_FAST.maxChangePerBeat > 0,
              "maxChangePerBeat must be positive");
static_assert(RATE_SLOW.tauSec > 0 && RATE_MEDIUM.tauSec > 0 && RATE_FAST.tauSec > 0, "tauSec must be positive");
static_assert(AGREE_COUNT >= 2 && AGREE_COUNT <= 8, "AGREE_COUNT must be 2-8");
static_assert(INVALID_TO_NO_FINGER >= 1 && MISSING_TO_NO_SENSOR >= 1, "counts must be at least 1");
static_assert(START_RETRY_AFTER_INVALID >= 1, "START_RETRY_AFTER_INVALID must be at least 1");
static_assert(SENSOR_POLL_MS >= 1000, "polling faster than 1 s only re-reads the same value");
static_assert(LED_LEVEL >= 3, "LED_LEVEL below 3 makes amber look red");
static_assert(STEP_BEATS >= 1, "STEP_BEATS must be at least 1");
static_assert(TEMPO_CC_CHANNEL >= 1 && TEMPO_CC_CHANNEL <= 16, "TEMPO_CC_CHANNEL must be 1-16");
static_assert(TEMPO_CC <= 31, "TEMPO_CC must be 0-31 so that TEMPO_CC + 32 is its LSB");

} // namespace settings
