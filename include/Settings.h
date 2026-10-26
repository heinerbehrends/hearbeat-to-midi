// ============================================================================
// All the settings to tweak, in one place. Change a value, then build & upload.
//
// Values marked [runtime: <name>] are only defaults: they can also be changed
// while the device runs, over the serial port (`set <name> <value>`, `save`; see
// README "Settings at runtime"), without recompiling. A saved value overrides the
// one here until `reset <name>` or `reset`; values never changed at runtime
// always follow this file. The ranges and rules for runtime changes are in
// lib/Params/src/Params.cpp. Everything unmarked is compile-time only.
// ============================================================================
#pragma once
#include <cstdint>
#include <HeartTempo.h>

namespace settings
{

// Name shown for the USB device and for its MIDI port (max 32 characters).
// Windows may cache the old name: unplug and replug after changing it.
constexpr const char *DEVICE_NAME = "Heart Rate To MIDI";

// --- Pins, per board ---------------------------------------------------------
// The board is chosen by the PlatformIO environment (BOARD_XIAO is set by
// env:xiao-s3-plus). Switches: Velleman 8016 (ON-OFF-ON), middle terminal to GND,
// outer terminals to the two pins; if a switch reads the wrong way round, swap
// its two pin numbers.
#if defined(BOARD_XIAO)
// Seeed XIAO ESP32-S3 Plus, all on the front pins (D-number = silkscreen).
// Left free: D2 (GPIO3, a strapping pin) and D6 (GPIO43, boot messages).
constexpr int SENSOR_SDA = 5;            // D4 (labelled SDA)
constexpr int SENSOR_SCL = 6;            // D5 (labelled SCL)
constexpr int FEEDBACK_PIN_MINUS = 1;    // D0
constexpr int FEEDBACK_PIN_PLUS = 2;     // D1
constexpr int RATE_PIN_SLOW = 7;         // D8
constexpr int RATE_PIN_FAST = 8;         // D9
constexpr int TRANSPORT_PIN_STOP = 9;    // D10
constexpr int TRANSPORT_PIN_STEPPED = 44; // D7
constexpr int LED_PIN = 4;               // D3, external WS2812D-F8 (the onboard LED is a plain one)
constexpr int TRANSPORT_LED_PIN = 3;     // D2, second WS2812D-F8 (GPIO3 is a strapping pin, but
                                         // fine as an output once the board has started)
#else
// ESP32-S3-DevKitC-1 clone (YD-ESP32-S3), breadboard setup
constexpr int SENSOR_SDA = 17;
constexpr int SENSOR_SCL = 9;
constexpr int FEEDBACK_PIN_MINUS = 4;
constexpr int FEEDBACK_PIN_PLUS = 5;
constexpr int RATE_PIN_SLOW = 6;
constexpr int RATE_PIN_FAST = 7;
constexpr int TRANSPORT_PIN_STOP = 15;
constexpr int TRANSPORT_PIN_STEPPED = 16;
// 21 = external WS2812D-F8. 48 = onboard LED (needs the RGB solder jumper bridged).
constexpr int LED_PIN = 21;
constexpr int TRANSPORT_LED_PIN = 10;    // second WS2812D-F8
#endif

// --- Feedback switch: − / 0 / + -------------------------------------------

// How far − and + put the tempo target below/above the heart rate.
constexpr float FEEDBACK_OFFSET_BPM = 5;  // [runtime: feedback_offset_bpm]
// Steering never takes the target past these. They never push against the
// switch: in − with a heart rate below STEER_LOW_BPM the target just follows.
constexpr float STEER_LOW_BPM = 55;       // [runtime: steer_low_bpm]
constexpr float STEER_HIGH_BPM = 130;     // [runtime: steer_high_bpm]

// --- Rate switch: slow / medium / fast -------------------------------------
// Second 8016, wired the same way. Centre = medium.

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

// [runtime: rate_<slow|medium|fast>_max_change, rate_<slow|medium|fast>_tau_sec]
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
// false until the switch is wired (an unwired switch would read "run"): the
// board's BOOT button then cycles stop -> run -> stepped instead. Don't hold BOOT
// while the board resets, that enters download mode.
constexpr bool TRANSPORT_SWITCH_INSTALLED = false;

// Stepped mode: the clock tempo holds for this many beats (counted from Start) and
// then jumps to wherever the glide has got to. 16 beats = 4 bars of 4/4.
// [runtime: step_beats, a power of two 1-64; a change applies on the next phrase
// boundary of both the old and the new length, so the phrase grid stays in phase]
constexpr int STEP_BEATS = 16;

// --- Double time -----------------------------------------------------------------
// Clock tempo = heart rate tempo x TEMPO_MULTIPLIER (1 or 2). Applied in the clock
// only: the filter, feedback and glide limit keep working in heart rate terms;
// beats, phrases, the transport LED and the tempo CC count clock beats. A change
// while playing lands on the next bar line (BEATS_PER_BAR); while stopped at once.
constexpr int TEMPO_MULTIPLIER = 1;      // [runtime: tempo_multiplier]
// Cap on the tempo the clock sends (x2 of a 180 bpm heart rate would be 360). Must
// stay below 256: the tempo CC (bpm x 64, 14 bit) only reaches 255.98 bpm.
constexpr float MAX_CLOCK_BPM = 240;

// --- Tempo as MIDI CC (for REAPER) -----------------------------------------
// REAPER doesn't follow MIDI clock tempo. The device also sends its tempo as a
// 14-bit CC pair (value = bpm x 64: MSB on TEMPO_CC, LSB on TEMPO_CC + 32), read by
// tools/reaper/heart_rate_tempo.lua. Keep these in sync with that script.
constexpr bool TEMPO_CC_ENABLED = true;
constexpr uint8_t TEMPO_CC_CHANNEL = 16; // 1-16
constexpr uint8_t TEMPO_CC = 20;         // 0-31 (20 is undefined in the MIDI spec)
constexpr uint32_t TEMPO_CC_RESEND_MS = 2000; // also resend unchanged, for late listeners

// --- Control from the DAW (incoming MIDI CC) ------------------------------------
// The DAW can change some settings live by sending CCs to the device's MIDI port
// (map a knob or an automation lane to them, see README "DAWs"). They go through
// the same checks and live-apply rules as `set` and, like `set`, are not saved
// until `save`. A CC only acts when its mapped value changes (a knob sends a
// stream of CCs), and other channels, our own tempo CC pair (MIDI-thru in the
// DAW can send it back) and every other message are ignored. The 0-127 range is
// split into equal zones, one per value: 4 zones = 0-31, 32-63, 64-95, 96-127.
constexpr bool CONTROL_CC_ENABLED = true;
constexpr uint8_t CONTROL_CC_CHANNEL = 16;    // 1-16
// step_beats: 0-31 -> 4, 32-63 -> 8, 64-95 -> 16, 96-127 -> 32. A length at or
// below count_in_beats lowers count_in_beats to step_beats - 1 first (logged).
constexpr uint8_t CC_STEP_BEATS = 21;
constexpr int CC_STEP_BEATS_VALUES[] = {4, 8, 16, 32};
// Rate preset: 0-42 slow, 43-85 medium, 86-127 fast. Overrides the rate switch
// until the switch is moved again (the last change wins).
constexpr uint8_t CC_RATE = 22;
// feedback_offset_bpm: 0-127 -> 0 .. CC_FEEDBACK_OFFSET_MAX_BPM, linear, in 0.1 bpm steps.
constexpr uint8_t CC_FEEDBACK_OFFSET = 23;
constexpr float CC_FEEDBACK_OFFSET_MAX_BPM = 10;
// tempo_multiplier: 0-63 -> 1, 64-127 -> 2.
constexpr uint8_t CC_TEMPO_MULTIPLIER = 24;
constexpr int CC_TEMPO_MULTIPLIER_VALUES[] = {1, 2};

// --- Heart rate filter -------------------------------------------------------
constexpr float START_BPM = 70;          // clock tempo at power-on, before the first lock
                                         // [runtime: start_bpm, applies after a restart]
constexpr float VALID_MIN_BPM = 40;      // readings outside this range are ignored
constexpr float VALID_MAX_BPM = 180;
constexpr int INVALID_TO_NO_FINGER = 5;  // readings without heart rate in a row (x 4 s) before "no finger"
                                         // [runtime: invalid_to_no_finger]
constexpr int MISSING_TO_NO_SENSOR = 3;  // polls without an answer in a row before "no sensor";
                                         // fewer are treated as a gap, so a bus glitch keeps the lock
constexpr int AGREE_COUNT = 3;           // readings of the last AGREE_COUNT + 1 that must agree to lock
constexpr float AGREE_TOL_BPM = 4;       // ...within this distance of each other's median [runtime: agree_tol_bpm]
constexpr float GATE_FLOOR_BPM = 8;      // while locked, readings this close to recent ones always pass
                                         // [runtime: gate_floor_bpm, gate_max_bpm]
constexpr float GATE_MAX_BPM = 12;       // ...and readings further away than this never pass directly
                                         // (your resting scatter measured ~2.5 bpm SD)
constexpr int LOCK_TIMEOUT_POLLS = 8;    // a lock without any accepted reading for this many polls
                                         // (x 4 s) drops back to acquiring; the tempo holds
                                         // [runtime: lock_timeout_polls]
// The sensor's typical junk is a high reading out of nowhere: readings above
// CAUTION_ABOVE_BPM only count when within CAUTION_STEP_BPM of the previous kept
// reading (a smooth series), or once AGREE_COUNT of them in a row agree.
// [runtime: caution_above_bpm, caution_step_bpm]
constexpr float CAUTION_ABOVE_BPM = 100;
constexpr float CAUTION_STEP_BPM = 8;

// --- Sensor --------------------------------------------------------------------
// The sensor computes a new value about every 4 s. Polling faster reads the same
// value twice, and repeats would count as "agreeing" readings.
constexpr uint32_t SENSOR_POLL_MS = 4000;
// A sensor that reset unnoticed stops measuring: re-send "start" after this many
// readings in a row without heart rate (keep it below INVALID_TO_NO_FINGER).
constexpr int START_RETRY_AFTER_INVALID = 4;
// Only trust the heart rate while the sensor also reports a valid SpO2 (result
// byte 1). When the sensor is disturbed it drops SpO2 but keeps reporting a
// drifting, too-high heart rate; this treats those readings like "no heart rate"
// (they count toward "no finger", but don't trigger START_RETRY_AFTER_INVALID).
constexpr bool REQUIRE_SPO2_VALID = true;
// Debug: also log the sensor's full 8-byte result as hex on every poll
// (byte 0 = SpO2, 1 = SpO2 valid, 2-5 = heart rate, 7 = heart rate present).
constexpr bool LOG_RAW_SENSOR_BYTES = false;

// --- Status LED (LED 1): sensor state ---------------------------------------------
constexpr uint8_t LED_LEVEL = 16;          // brightness 3-255; the WS2812D-F8 is very bright
                                           // [runtime: led_level]
// true (testing): amber as soon as one reading is missing or rejected, even while
// the tempo keeps following. false (stage): green for as long as the lock holds.
constexpr bool LED_SHOW_EVERY_MISS = true; // [runtime: led_show_every_miss]

// --- Transport LED (LED 2): stopped / beat / tempo direction / count-in -----------
// Stopped: dim white. Playing: a flash on every beat (brighter on beat 1 of each
// bar), white while the tempo holds, orange while it rises, blue while it falls.
// Stepped mode: the last COUNT_IN_BEATS beats before a tempo step flash in the
// step's colour, getting brighter; the step itself gets one long flash.
struct Rgb
{
  uint8_t r, g, b; // full-scale colour, dimmed to the level it's shown at
};
constexpr Rgb COLOR_STEADY = {255, 255, 255};
constexpr Rgb COLOR_FASTER = {255, 90, 0};   // orange
constexpr Rgb COLOR_SLOWER = {0, 60, 255};   // blue
constexpr uint8_t TRANSPORT_LED_LEVEL = 16;  // brightness of a downbeat flash, 3-255 [runtime: transport_led_level]
constexpr uint8_t STOPPED_LEVEL = 3;         // the dim "stopped" glow [runtime: stopped_level]
constexpr int BEATS_PER_BAR = 4;             // which beats count as downbeats [runtime: beats_per_bar]
constexpr uint32_t BEAT_FLASH_MS = 80;
constexpr uint32_t STEP_FLASH_MS = 400;      // the long flash on a tempo step
constexpr int COUNT_IN_BEATS = 4;            // 0 = no count-in [runtime: count_in_beats]
constexpr float DIRECTION_MIN_BPM = 0.3;     // glide: show rising/falling only when the tempo
                                             // still has this far to go to its target
constexpr float STEP_CUE_MIN_BPM = 0.5;      // stepped: no count-in for smaller steps

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
  c.gateMaxBpm = GATE_MAX_BPM;
  c.lockTimeoutPolls = LOCK_TIMEOUT_POLLS;
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
static_assert(GATE_MAX_BPM >= GATE_FLOOR_BPM, "GATE_MAX_BPM must be at least GATE_FLOOR_BPM");
static_assert(LOCK_TIMEOUT_POLLS > INVALID_TO_NO_FINGER, "LOCK_TIMEOUT_POLLS must exceed INVALID_TO_NO_FINGER, "
                                                         "or plain gaps would drop the lock first");
static_assert(START_RETRY_AFTER_INVALID >= 1, "START_RETRY_AFTER_INVALID must be at least 1");
static_assert(SENSOR_POLL_MS >= 1000, "polling faster than 1 s only re-reads the same value");
static_assert(LED_LEVEL >= 3, "LED_LEVEL below 3 makes amber look red");
static_assert(STEP_BEATS >= 1 && STEP_BEATS <= 64 && (STEP_BEATS & (STEP_BEATS - 1)) == 0,
              "STEP_BEATS must be a power of two 1-64 (so phrase grids of different lengths stay in phase)");
static_assert(COUNT_IN_BEATS >= 0 && COUNT_IN_BEATS < STEP_BEATS, "COUNT_IN_BEATS must be 0 .. STEP_BEATS - 1");
static_assert(TEMPO_MULTIPLIER == 1 || TEMPO_MULTIPLIER == 2, "TEMPO_MULTIPLIER must be 1 or 2");
static_assert(BEATS_PER_BAR >= 1, "BEATS_PER_BAR must be at least 1");
static_assert(TRANSPORT_LED_LEVEL >= 3 && STOPPED_LEVEL >= 1, "LED levels too low to show colours");
static_assert(MAX_CLOCK_BPM >= VALID_MAX_BPM && MAX_CLOCK_BPM < 256,
              "MAX_CLOCK_BPM must be VALID_MAX_BPM .. 255: the tempo CC (bpm x 64, 14 bit) only reaches 255.98 bpm");
static_assert(TEMPO_CC_CHANNEL >= 1 && TEMPO_CC_CHANNEL <= 16, "TEMPO_CC_CHANNEL must be 1-16");
static_assert(TEMPO_CC <= 31, "TEMPO_CC must be 0-31 so that TEMPO_CC + 32 is its LSB");
static_assert(CONTROL_CC_CHANNEL >= 1 && CONTROL_CC_CHANNEL <= 16, "CONTROL_CC_CHANNEL must be 1-16");
constexpr bool isControlCc(uint8_t cc)
{
  return cc == CC_STEP_BEATS || cc == CC_RATE || cc == CC_FEEDBACK_OFFSET || cc == CC_TEMPO_MULTIPLIER;
}
static_assert(CC_STEP_BEATS <= 119 && CC_RATE <= 119 && CC_FEEDBACK_OFFSET <= 119 && CC_TEMPO_MULTIPLIER <= 119,
              "control CCs must be 0-119 (120-127 are channel mode messages)");
static_assert(CC_STEP_BEATS != CC_RATE && CC_STEP_BEATS != CC_FEEDBACK_OFFSET && CC_STEP_BEATS != CC_TEMPO_MULTIPLIER &&
                  CC_RATE != CC_FEEDBACK_OFFSET && CC_RATE != CC_TEMPO_MULTIPLIER &&
                  CC_FEEDBACK_OFFSET != CC_TEMPO_MULTIPLIER,
              "control CCs must differ");
static_assert(!isControlCc(TEMPO_CC) && !isControlCc(TEMPO_CC + 32),
              "control CCs must differ from the tempo CC pair, which the DAW may send back");
constexpr bool allPowersOfTwoUpTo64(const int *v, int n)
{
  return n == 0 || (v[0] >= 1 && v[0] <= 64 && (v[0] & (v[0] - 1)) == 0 && allPowersOfTwoUpTo64(v + 1, n - 1));
}
static_assert(allPowersOfTwoUpTo64(CC_STEP_BEATS_VALUES, sizeof(CC_STEP_BEATS_VALUES) / sizeof(int)),
              "CC_STEP_BEATS_VALUES must be powers of two 1-64");
static_assert(CC_FEEDBACK_OFFSET_MAX_BPM > 0 && CC_FEEDBACK_OFFSET_MAX_BPM <= 30,
              "CC_FEEDBACK_OFFSET_MAX_BPM must be within feedback_offset_bpm's range (0-30)");
constexpr bool allOneOrTwo(const int *v, int n)
{
  return n == 0 || ((v[0] == 1 || v[0] == 2) && allOneOrTwo(v + 1, n - 1));
}
static_assert(allOneOrTwo(CC_TEMPO_MULTIPLIER_VALUES, sizeof(CC_TEMPO_MULTIPLIER_VALUES) / sizeof(int)),
              "CC_TEMPO_MULTIPLIER_VALUES must be 1 or 2");

} // namespace settings
