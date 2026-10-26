// Heartbeat to MIDI clock.
//
// Core 0: sensor task polls the SEN0344 every 4 s and runs HeartTempo.
// Core 1: clock task sends 24 PPQN MIDI clock at the current tempo, always.
//         loop() reads the switches and drives both LEDs.
//
// Settings to tweak live in include/Settings.h; the ones marked [runtime] can
// also be changed over the serial port, and a few by MIDI CC from the DAW (both
// applied by the sensor task, see README). Their live values are published as
// atomics: the clock task and loop() read them lock-free, the sensor task is the
// only writer.
#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include <Wire.h>
#include <atomic>
#include <cmath>
#include <esp_log.h>
#include <esp32-hal-tinyusb.h>
#include <ClockGrid.h>
#include <HeartTempo.h>
#include <MidiControl.h>
#include <ParamConsole.h>
#include <ParamNvs.h>
#include <Params.h>
#include <Switch3.h>
#include <Settings.h>

using namespace settings;

// SEN0344 registers (as used by the DFRobot library, read here directly so that
// failed transfers are detected instead of decoding an uninitialised buffer)
static const uint8_t SENSOR_ADDR = 0x57;
static const uint8_t REG_RESULT = 0x0C; // 8 bytes: [0] SpO2, [2..5] heart rate (big endian), 0 = none
static const uint8_t REG_COLLECT = 0x20;
static const uint32_t TEMPO_UPDATE_MS = 100;
// Received MIDI packets read per call at most: more than the receive buffer
// holds (16), so it is always drained, but the USB task never loops unbounded
static const int MIDI_IN_MAX_PER_PASS = 32;

static const int PPQN = 24;
static const uint8_t MIDI_CLOCK = 0xF8;
static const uint8_t MIDI_START = 0xFA;
static const uint8_t MIDI_STOP = 0xFC;
static const uint8_t MIDI_SONG_POSITION = 0xF2;
static const int BOOT_BUTTON_PIN = 0;

// Transport modes, matching the transport switch positions (Switch3: -1 / 0 / +1)
static const int MODE_STOP = -1;
static const int MODE_RUN = 0;
static const int MODE_STEPPED = 1;

USBMIDI DeviceMIDI(DEVICE_NAME);

// Shared between tasks
static std::atomic<uint32_t> clockIntervalUs{0};
static std::atomic<uint32_t> midiDrops{0}; // clock ticks the USB stack refused
static std::atomic<HeartState> heartState{HeartState::NoFinger};
static std::atomic<bool> readingOk{false};
static std::atomic<int> feedbackSwitch{0}; // -1 = −, 0 = follow, +1 = +
static std::atomic<int> rateSwitch{0};     // -1 = slow, 0 = medium, +1 = fast
static std::atomic<int> transportWanted{MODE_STOP}; // from the switch, applied by the clock task
static std::atomic<int> transportMode{MODE_STOP};   // what the clock task has applied
static std::atomic<bool> transportStopSeen{false};  // a stop was wanted since the clock task last looked
static std::atomic<uint32_t> clockSentIntervalUs{0}; // interval the clock actually uses (held in stepped mode)
// For the transport LED
static std::atomic<uint32_t> ticksSinceStart{0}; // clock ticks sent since the last Start
static std::atomic<int> glideDir{0};             // +1 tempo gliding up, -1 down, 0 holding
static std::atomic<int> nextStepDir{0};          // stepped: direction of the coming step
static std::atomic<int> lastStepDir{0};          // stepped: direction of the step just made
// DAW control: the latest CC value + 1 per control, 0 = none since the sensor
// task last took it (so it is valid from power-on, before setup() runs). Written
// by tud_midi_rx_cb (USB task), taken by the sensor task.
static std::atomic<int> ccLatest[midicontrol::COUNT];

// Runtime settings. `runtime` and `nvs` belong to the sensor task (after setup);
// every change is copied to liveParam, which the other tasks read.
static Params runtime;
static ParamNvs nvs;
static std::atomic<float> liveParam[params::COUNT];
// What the clock task has applied (changes wait for a phrase or bar boundary)
static std::atomic<int> appliedStepBeats{STEP_BEATS};
static std::atomic<int> appliedMultiplier{TEMPO_MULTIPLIER};

static float live(int id)
{
  return liveParam[id].load(std::memory_order_relaxed);
}

static int liveInt(int id)
{
  return (int)live(id);
}

static void publishParams()
{
  for (int i = 0; i < params::COUNT; i++)
    liveParam[i].store(runtime.get(i));
}

static uint32_t intervalFor(float bpm)
{
  // Last-resort guard: a bad value must never stop or flood the clock
  if (!std::isfinite(bpm))
    bpm = START_BPM;
  bpm = std::min(std::max(bpm, 20.0f), 300.0f);
  return (uint32_t)(60e6f / (bpm * PPQN));
}

static const char *stateName(HeartState s)
{
  switch (s)
  {
  case HeartState::NoSensor:
    return "NoSensor";
  case HeartState::NoFinger:
    return "NoFinger";
  case HeartState::Acquiring:
    return "Acquiring";
  case HeartState::Locked:
    return "Locked";
  }
  return "?";
}

static const char *feedbackName(int pos)
{
  return pos < 0 ? "minus" : pos > 0 ? "plus" : "follow";
}

static const char *modeName(int mode)
{
  return mode == MODE_STOP ? "stop" : mode == MODE_RUN ? "run" : "stepped";
}

static const char *rateName(int pos)
{
  return pos < 0 ? RATE_SLOW.name : pos > 0 ? RATE_FAST.name : RATE_MEDIUM.name;
}

static bool sensorPresent()
{
  Wire.beginTransmission(SENSOR_ADDR);
  return Wire.endTransmission() == 0;
}

static bool sensorStartCollect()
{
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(REG_COLLECT);
  Wire.write(0);
  Wire.write(1);
  return Wire.endTransmission() == 0;
}

// Returns false if the transfer failed. *hr is -1 when there is no usable heart
// rate; *measuring is false when the sensor reports no heart rate at all (as
// opposed to one discarded for bad quality).
static bool sensorReadHeartRate(int *hr, bool *measuring)
{
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(REG_RESULT);
  if (Wire.endTransmission() != 0)
    return false;
  uint8_t b[8] = {};
  if (Wire.requestFrom(SENSOR_ADDR, (uint8_t)sizeof(b)) != sizeof(b))
    return false;
  for (uint8_t &x : b)
    x = Wire.read();
  uint32_t v = (uint32_t)b[2] << 24 | (uint32_t)b[3] << 16 | (uint32_t)b[4] << 8 | b[5];
  // Byte 1 is the SpO2-valid flag; the heart rate it reports while that is 0 has
  // proven to be junk (a drifting series after the sensor is disturbed)
  bool quality = !REQUIRE_SPO2_VALID || b[1] != 0;
  *hr = v == 0 || v > 1000 || !quality ? -1 : (int)v;
  *measuring = v != 0;
  if (LOG_RAW_SENSOR_BYTES)
    Serial.printf("raw: %02X %02X %02X %02X %02X %02X %02X %02X\r\n", b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
  return true;
}

static void i2cBegin()
{
  Wire.begin(SENSOR_SDA, SENSOR_SCL, 100000);
  Wire.setTimeOut(50);
}

static bool hostListening()
{
  return tud_mounted() && !tud_suspended();
}

// All MIDI output happens in the clock task. Writes fail (return false) when the
// 64-byte USB TX buffer is full because the host isn't draining it for a moment.

// One clock tick. A full buffer gets ~1 ms of retries: the tick is a little late
// rather than lost.
static bool sendClockTick()
{
  int64_t giveUp = esp_timer_get_time() + 1000;
  while (DeviceMIDI.write(MIDI_CLOCK) == 0)
    if (esp_timer_get_time() > giveUp)
      return false;
  return true;
}

// Song Position Pointer is a 3-byte System Common message (USB-MIDI CIN 0x3)
static bool sendSongPositionZero()
{
  midiEventPacket_t p = {0x03, MIDI_SONG_POSITION, 0, 0};
  return DeviceMIDI.writePacket(&p);
}

// Control change (USB-MIDI CIN 0xB); USBMIDI::controlChange doesn't report failure
static bool sendCc(uint8_t cc, uint8_t value)
{
  midiEventPacket_t p = {0x0B, (uint8_t)(0xB0 | (TEMPO_CC_CHANNEL - 1)), cc, value};
  return DeviceMIDI.writePacket(&p);
}

// Called right after a clock tick, so the next tick (a full interval later) is
// beat 1. Stop first: Song Position is only honoured while stopped, and after a
// reconnect or a missed switch flick the DAW may still think it's playing.
static bool sendStartFromTop()
{
  return DeviceMIDI.write(MIDI_STOP) && sendSongPositionZero() && DeviceMIDI.write(MIDI_START);
}

// Applies the wanted transport mode. The mode (and so the tempo behaviour)
// changes at once; Start/Stop that couldn't be sent stay pending and are retried
// after the next tick. Returns true when a Start went out: the next tick is beat 1.
static bool handleTransport(bool resync)
{
  static bool wasListening = false, pendingStart = false, pendingStop = false;
  bool listening = hostListening();
  bool reconnected = listening && !wasListening;
  wasListening = listening;
  bool wasRunning = transportMode.load() != MODE_STOP;
  int wanted = transportWanted.load();
  bool run = wanted != MODE_STOP;
  // A run -> stop -> run flick shorter than one tick still restarts from the top
  bool flicked = transportStopSeen.exchange(false) && run && wasRunning;

  if (run && (!wasRunning || reconnected || flicked || resync))
  {
    pendingStart = true;
    pendingStop = false;
  }
  else if (!run && (wasRunning || reconnected))
  {
    pendingStop = true; // also when reconnecting: the DAW may have missed our Stop
    pendingStart = false;
  }
  // run <-> stepped changes only the tempo mode, no transport messages
  transportMode.store(wanted);

  if (!listening)
    return false;
  if (pendingStart)
  {
    pendingStart = !sendStartFromTop();
    return !pendingStart;
  }
  if (pendingStop)
    pendingStop = !DeviceMIDI.write(MIDI_STOP);
  return false;
}

static uint16_t tempoToCc(float bpm)
{
  return (uint16_t)std::min(std::max(std::lround(bpm * 64), 0L), 16383L);
}

// For DAWs that don't follow clock tempo (REAPER): send the tempo the clock is
// actually using as a 14-bit CC pair when it changes, every TEMPO_CC_RESEND_MS,
// and right after the host (re)connects. A pair that couldn't be sent is retried.
static void sendTempoCc(uint32_t intervalUs)
{
  static uint16_t lastSent = 0xFFFF;
  static int64_t lastSentUs = 0;
  static bool wasListening = false;
  bool listening = hostListening();
  if (listening && !wasListening)
    lastSent = 0xFFFF;
  wasListening = listening;

  uint16_t v = tempoToCc(60e6f / ((float)intervalUs * PPQN));
  int64_t now = esp_timer_get_time();
  if (!TEMPO_CC_ENABLED || !listening || (v == lastSent && now - lastSentUs < TEMPO_CC_RESEND_MS * 1000LL))
    return;
  // MSB then LSB; the receiver applies the value on the LSB
  if (sendCc(TEMPO_CC, v >> 7) && sendCc(TEMPO_CC + 32, v & 0x7F))
  {
    lastSent = v;
    lastSentUs = now;
  }
}

static float bpmOf(uint32_t intervalUs)
{
  return 60e6f / ((float)intervalUs * PPQN);
}

// -1 / 0 / +1, with differences smaller than minimum counting as 0
static int direction(float delta, float minimum)
{
  return delta >= minimum ? 1 : delta <= -minimum ? -1 : 0;
}

// Advances a fixed phase by the current interval each tick, so tempo changes
// take effect smoothly between ticks without resetting the beat. In stepped mode
// the interval is held and only refreshed on every step_beats-th beat after Start.
// The tempo multiplier scales the interval actually sent (held stays in heart
// rate terms). Runtime changes of step_beats and tempo_multiplier wait for a
// boundary that keeps the DAW in phase; while stopped they apply at once.
static void clockTask(void *)
{
  const uint32_t minIntervalUs = intervalFor(MAX_CLOCK_BPM);
  int stepBeats = appliedStepBeats.load(), multiplier = appliedMultiplier.load();
  int64_t next = esp_timer_get_time();
  uint32_t held = clockIntervalUs.load(); // heart rate tempo, before the multiplier
  bool gliding = true;
  uint32_t tick = 0;     // index of the next tick since Start (0 = beat 1); counts
                         // only ticks that went out, so it matches the DAW's count
  uint32_t failRun = 0;  // ticks in a row that couldn't be sent
  for (;;)
  {
    int mode = transportMode.load();
    int wantedStep = liveInt(params::STEP_BEATS), wantedMultiplier = liveInt(params::TEMPO_MULTIPLIER);
    if (mode == MODE_STOP)
    {
      stepBeats = wantedStep;
      multiplier = wantedMultiplier;
      appliedStepBeats.store(stepBeats);
      appliedMultiplier.store(multiplier);
    }
    if (mode == MODE_STEPPED)
      gliding = false;
    else if (mode == MODE_STOP)
      gliding = true;
    // (stepped -> run keeps holding until the next phrase boundary, where the
    // held tempo meets the glide, so leaving stepped mode never jumps)
    if (gliding)
      held = clockIntervalUs.load();
    nextStepDir.store(mode == MODE_STEPPED
                          ? direction(bpmOf(clockIntervalUs.load()) - bpmOf(held), STEP_CUE_MIN_BPM)
                          : 0);
    uint32_t interval = scaleInterval(held, multiplier, minIntervalUs);
    clockSentIntervalUs.store(interval);
    next += interval;
    int64_t wait;
    while ((wait = next - esp_timer_get_time()) > 0)
    {
      if (wait > 2000)
        vTaskDelay(1); // coarse sleep, then spin the last ~2 ms for accuracy
    }
    // Late by a whole interval or more (a stall): skip the missed ticks instead
    // of sending them back to back
    if (-wait >= interval)
      next = esp_timer_get_time();

    // While nobody is listening (unplugged, host asleep) don't queue ticks: they
    // would all arrive at once when the host comes back
    bool resync = false;
    if (hostListening())
    {
      if (sendClockTick())
      {
        // A new step length starts on a phrase boundary of both lengths, so the
        // phrase grid continues; double time starts on a bar line
        if (wantedStep != stepBeats && stepChangeDue(tick, stepBeats, wantedStep, PPQN))
        {
          stepBeats = wantedStep;
          appliedStepBeats.store(stepBeats);
        }
        if (wantedMultiplier != multiplier && barBoundary(tick, liveInt(params::BEATS_PER_BAR), PPQN))
        {
          multiplier = wantedMultiplier;
          appliedMultiplier.store(multiplier);
        }
        // Step boundary: the tick just sent starts a new phrase, take the glide's
        // current tempo for the intervals that follow
        if (tick % ((uint32_t)stepBeats * PPQN) == 0)
        {
          uint32_t before = held;
          held = clockIntervalUs.load();
          lastStepDir.store(direction(bpmOf(held) - bpmOf(before), STEP_CUE_MIN_BPM));
          if (mode == MODE_RUN)
            gliding = true;
        }
        tick++;
        ticksSinceStart.store(tick);
        // After a beat or more of lost ticks the DAW's position is unknown:
        // restart it from the top so it's back in phase
        resync = failRun >= (uint32_t)PPQN;
        failRun = 0;
      }
      else
      {
        midiDrops.fetch_add(1);
        failRun++;
      }
    }

    if (handleTransport(resync))
    {
      tick = 0;
      ticksSinceStart.store(0);
    }
    sendTempoCc(interval); // the tempo actually sent, multiplier included
  }
}

// Incoming MIDI from the DAW. TinyUSB calls this (in its own task) whenever a
// transfer from the host arrived. Its receive buffer holds only 16 packets and
// the host can send the next transfer only once it's drained, so it is read
// right here: polling from a task would hold every message up until the next
// poll (at the sensor task's 100 ms, a knob sweep backs up for seconds).
// Non-blocking; received and sent packets have separate FIFOs with their own
// locks, so this doesn't hold up the clock task's writes. Only the latest value
// per control is handed over; the sensor task, the registry's only writer,
// applies it.
extern "C" void tud_midi_rx_cb(uint8_t itf)
{
  midiEventPacket_t p;
  for (int n = 0; n < MIDI_IN_MAX_PER_PASS && DeviceMIDI.readPacket(&p); n++)
  {
    int c = midicontrol::controlFor((const uint8_t *)&p);
    if (c >= 0)
      ccLatest[c].store(p.byte3 + 1);
  }
}

static void printLine(const char *line)
{
  Serial.printf("%s\r\n", line);
}

static void sensorTask(void *)
{
  HeartTempo heartTempo(runtime.heartTempoConfig(0));
  HeartState lastState = heartTempo.state();
  int appliedFeedback = 99, appliedRate = 99; // force a log line at boot
  int loggedMode = MODE_STOP;
  int loggedStepBeats = appliedStepBeats.load(), loggedMultiplier = appliedMultiplier.load();
  bool configChanged = false;
  // Every change, from a serial command or a DAW CC (`source`, e.g. "cc23=70"),
  // is logged and published the same way
  auto paramChanged = [&](int id, float oldValue, float newValue, const char *source) {
    char from[24], to[24];
    runtime.format(id, oldValue, from, sizeof(from));
    runtime.format(id, newValue, to, sizeof(to));
    if (source)
      Serial.printf("param: %s %s -> %s (%s)\r\n", params::def(id).name, from, to, source);
    else
      Serial.printf("param: %s %s -> %s\r\n", params::def(id).name, from, to);
    liveParam[id].store(newValue);
    configChanged = true;
  };
  // Serial commands (set, get, save, ...). `save` writes flash, which can delay
  // a clock tick by a few ms.
  ParamConsole console(runtime, nvs, printLine,
                       [&](int id, float oldValue, float newValue) { paramChanged(id, oldValue, newValue, nullptr); });
  // CCs from the DAW: same checks and live rules as `set`, not saved
  MidiControl midiControl;
  RateSelect rateSelect; // rate switch, or the CC override since it last moved
  const MidiControl::Changed ccChanged = paramChanged;
  const MidiControl::Rejected ccRejected = [](const char *error, const char *source) {
    Serial.printf("param: %s refused: %s\r\n", source, error);
  };
  bool collecting = false;
  int failedPolls = 0, invalidPolls = 0;
  uint32_t lastPoll = millis() - SENSOR_POLL_MS;

  for (;;)
  {
    uint32_t now = millis();

    while (Serial.available() > 0)
      console.feed((char)Serial.read());

    // The switch first, so a CC in the same pass wins (it came later)
    rateSelect.setSwitch(rateSwitch.load());
    for (int c = 0; c < midicontrol::COUNT; c++)
    {
      int v = ccLatest[c].exchange(0);
      if (v > 0)
        midiControl.feed(c, (uint8_t)(v - 1));
    }
    midiControl.apply(runtime, rateSelect, ccChanged, ccRejected);

    // Plain text lines are shown as-is by serial plotters
    int fb = feedbackSwitch.load();
    if (fb != appliedFeedback)
    {
      if (appliedFeedback == 99)
        Serial.printf("feedback: %s\r\n", feedbackName(fb));
      else
        Serial.printf("feedback: %s -> %s\r\n", feedbackName(appliedFeedback), feedbackName(fb));
      heartTempo.setFeedback(fb);
      appliedFeedback = fb;
    }
    int rate = rateSelect.effective();
    if (rate != appliedRate)
    {
      if (appliedRate == 99)
        Serial.printf("rate: %s", rateName(rate));
      else
        Serial.printf("rate: %s -> %s", rateName(appliedRate), rateName(rate));
      if (rateSelect.overridden())
        Serial.printf(" (cc%u=%u)", (unsigned)CC_RATE, (unsigned)rateSelect.ccValue());
      Serial.printf(" (%.3f %%/beat, tau %.0f s)\r\n", runtime.maxChangePerBeat(rate) * 100, runtime.tauSec(rate));
      appliedRate = rate;
      configChanged = true;
    }
    // Rate switch or runtime settings: the glide continues from where it is
    if (configChanged)
    {
      heartTempo.setConfig(runtime.heartTempoConfig(appliedRate));
      configChanged = false;
    }
    if (appliedStepBeats.load() != loggedStepBeats || appliedMultiplier.load() != loggedMultiplier)
    {
      loggedStepBeats = appliedStepBeats.load();
      loggedMultiplier = appliedMultiplier.load();
      Serial.printf("clock: step_beats %d, tempo_multiplier %d in effect\r\n", loggedStepBeats, loggedMultiplier);
    }
    int mode = transportMode.load();
    if (mode != loggedMode)
    {
      Serial.printf("transport: %s -> %s\r\n", modeName(loggedMode), modeName(mode));
      loggedMode = mode;
    }

    if (now - lastPoll >= SENSOR_POLL_MS)
    {
      lastPoll = now;
      int hr = -1;
      bool measuring = false;
      if (!sensorPresent() || (!collecting && !sensorStartCollect()) || !sensorReadHeartRate(&hr, &measuring))
      {
        hr = -1;
        heartTempo.onSensorMissing();
        collecting = false; // the sensor may have lost power; restart it when it answers
        // A sensor reset mid-transfer can leave the bus stuck: restart the I2C driver
        if (++failedPolls % MISSING_TO_NO_SENSOR == 0)
        {
          Wire.end();
          i2cBegin();
        }
      }
      else
      {
        collecting = true;
        failedPolls = 0;
        heartTempo.onReading(hr, now);
        // A sensor that reset without us noticing stops measuring: nudge it again.
        // Only for "no heart rate at all": restarting during a quality dip would
        // reset its averaging and stretch the disturbance.
        invalidPolls = measuring ? 0 : invalidPolls + 1;
        if (invalidPolls > 0 && invalidPolls % START_RETRY_AFTER_INVALID == 0)
          sensorStartCollect();
      }
      heartState.store(heartTempo.state());
      readingOk.store(heartTempo.lastReadingAccepted());

      if (heartTempo.state() != lastState)
      {
        Serial.printf("state: %s -> %s\r\n", stateName(lastState), stateName(heartTempo.state()));
        lastState = heartTempo.state();
      }

      // Serial-plotter format, see diag/sensor_probe.cpp. Raw = sensor value,
      // HR = filtered estimate, Target = HR with feedback, Tempo = glide tempo,
      // Clock = tempo the clock sends (= Tempo, or held between steps in stepped mode).
      Serial.print(">");
      if (hr > 0)
        Serial.printf("Raw:%d,", hr);
      Serial.printf("HR:%.1f,Target:%.1f,Tempo:%.2f,Clock:%.2f,State:%d,Ok:%d,Fb:%d,Rate:%d,Mode:%d,Drops:%u\r\n",
                    heartTempo.heartRate(), heartTempo.target(), heartTempo.tempo(),
                    60e6f / ((float)clockSentIntervalUs.load() * PPQN), (int)heartTempo.state(),
                    (int)heartTempo.lastReadingAccepted(), fb, rate, mode, (unsigned)midiDrops.load());
    }

    clockIntervalUs.store(intervalFor(heartTempo.update(now)));
    // The tempo only moves while locked, toward the target
    glideDir.store(heartTempo.state() == HeartState::Locked
                       ? direction(heartTempo.target() - heartTempo.tempo(), DIRECTION_MIN_BPM)
                       : 0);
    vTaskDelay(pdMS_TO_TICKS(TEMPO_UPDATE_MS));
  }
}

void setup()
{
  rgbLedWrite(LED_PIN, 0, 0, 0); // the LEDs keep their last colour across resets
  rgbLedWrite(TRANSPORT_LED_PIN, 0, 0, 0);
  Serial.setRxBufferSize(512); // room for a few setting commands between reads
  Serial.begin(115200); // DevKit: hardware UART (COM port); XIAO: USB CDC next to MIDI
#if ARDUINO_USB_CDC_ON_BOOT
  // USB CDC drops what it can't send: nothing at all while the host has DTR clear
  // (no monitor open), and with a 0 ms timeout also the tail of a line when the
  // 64-byte FIFO is full. A short timeout lets a burst like `get` drain.
  Serial.setTxTimeoutMs(20);
#endif

  USB.productName(DEVICE_NAME);
  DeviceMIDI.begin();
  USB.begin();

  esp_log_level_set("i2c.master", ESP_LOG_NONE); // keep bus errors out of the plotter stream
  i2cBegin();

  // Switch contacts pull to GND; an unconnected switch reads as centre
  for (int pin : {FEEDBACK_PIN_MINUS, FEEDBACK_PIN_PLUS, RATE_PIN_SLOW, RATE_PIN_FAST, TRANSPORT_PIN_STOP,
                  TRANSPORT_PIN_STEPPED, BOOT_BUTTON_PIN})
    pinMode(pin, INPUT_PULLUP);

  // Runtime settings: saved values over the Settings.h defaults
  if (!nvs.begin("hr2midi"))
    Serial.printf("params: NVS unavailable, using defaults\r\n");
  int loaded = runtime.load(nvs);
  if (runtime.error()[0]) // the saved set breaks a rule (a default moved since the save)
    Serial.printf("params: saved values ignored: %s\r\n", runtime.error());
  Serial.printf("params: %d saved value(s) loaded, type help for commands\r\n", loaded);
  publishParams();
  appliedStepBeats.store(runtime.getInt(params::STEP_BEATS));
  appliedMultiplier.store(runtime.getInt(params::TEMPO_MULTIPLIER));

  uint32_t startInterval = intervalFor(runtime.get(params::START_BPM));
  clockIntervalUs.store(startInterval);
  clockSentIntervalUs.store(scaleInterval(startInterval, appliedMultiplier.load(), intervalFor(MAX_CLOCK_BPM)));
  xTaskCreatePinnedToCore(clockTask, "clock", 4096, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
  xTaskCreatePinnedToCore(sensorTask, "sensor", 8192, nullptr, 2, nullptr, 0);
}

static void setTransport(int mode)
{
  if (mode == MODE_STOP)
    transportStopSeen.store(true); // so a quick stop-and-back isn't missed
  transportWanted.store(mode);
}

static void readSwitches(uint32_t now)
{
  static Switch3 feedback(SWITCH_DEBOUNCE_MS), rate(SWITCH_DEBOUNCE_MS);
  if (feedback.update(digitalRead(FEEDBACK_PIN_MINUS) == LOW, digitalRead(FEEDBACK_PIN_PLUS) == LOW, now))
    feedbackSwitch.store(feedback.position());
  if (rate.update(digitalRead(RATE_PIN_SLOW) == LOW, digitalRead(RATE_PIN_FAST) == LOW, now))
    rateSwitch.store(rate.position());

  if (TRANSPORT_SWITCH_INSTALLED)
  {
    static Switch3 transport(SWITCH_DEBOUNCE_MS);
    if (transport.update(digitalRead(TRANSPORT_PIN_STOP) == LOW, digitalRead(TRANSPORT_PIN_STEPPED) == LOW, now))
      setTransport(transport.position()); // -1 stop, 0 run, +1 stepped
  }
  else
  {
    // Stand-in until the switch is wired: each BOOT press cycles stop -> run -> stepped.
    // A single contact uses Switch3's left input only: position -1 = pressed.
    static Switch3 bootButton(SWITCH_DEBOUNCE_MS);
    if (bootButton.update(digitalRead(BOOT_BUTTON_PIN) == LOW, false, now) && bootButton.position() < 0)
    {
      int m = transportWanted.load();
      setTransport(m == MODE_STOP ? MODE_RUN : m == MODE_RUN ? MODE_STEPPED : MODE_STOP);
    }
  }
}

// Sensor-state LED (colours: README "Controls and LED", levels: Settings.h)
static void updateLed(uint32_t now)
{
  static uint32_t lastColor = 0xFFFFFFFF;
  const int level = liveInt(params::LED_LEVEL);
  const uint8_t amberGreen = std::max(1, level / 3);

  uint8_t r = 0, g = 0, b = 0;
  switch (heartState.load())
  {
  case HeartState::NoSensor:
    r = (now / 250) % 2 ? level : 0;
    break;
  case HeartState::NoFinger:
    r = level;
    break;
  case HeartState::Acquiring:
    r = level;
    g = amberGreen;
    break;
  case HeartState::Locked:
    if (readingOk.load() || !live(params::LED_SHOW_EVERY_MISS))
      g = level;
    else
    {
      r = level;
      g = amberGreen;
    }
    break;
  }

  uint32_t color = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
  if (color != lastColor)
  {
    rgbLedWrite(LED_PIN, r, g, b);
    lastColor = color;
  }
}

// Transport LED (behaviour: README "Controls and LED", colours and timing: Settings.h)
static void updateTransportLed(uint32_t now)
{
  static uint32_t lastBeat = UINT32_MAX, beatAtMs = 0, lastColor = 0xFFFFFFFF;
  auto dirColor = [](int dir) { return dir > 0 ? COLOR_FASTER : dir < 0 ? COLOR_SLOWER : COLOR_STEADY; };

  Rgb c = COLOR_STEADY;
  int level = 0;
  int mode = transportMode.load();
  uint32_t ticks = ticksSinceStart.load();
  // Live settings; the step length is the one the clock has applied
  const int stepBeats = appliedStepBeats.load();
  const int countInBeats = std::min(liveInt(params::COUNT_IN_BEATS), stepBeats - 1);
  const int beatsPerBar = liveInt(params::BEATS_PER_BAR);
  const int flashLevel = liveInt(params::TRANSPORT_LED_LEVEL);
  if (mode == MODE_STOP)
  {
    level = liveInt(params::STOPPED_LEVEL);
    lastBeat = UINT32_MAX;
  }
  else if (ticks > 0) // the first tick after Start is beat 1
  {
    uint32_t beat = (ticks - 1) / PPQN;
    if (beat != lastBeat)
    {
      lastBeat = beat;
      beatAtMs = now;
    }
    uint32_t sinceBeat = now - beatAtMs;
    int beatInPhrase = beat % stepBeats;
    bool downbeat = beat % beatsPerBar == 0;
    int normal = downbeat ? flashLevel : std::max(1, flashLevel / 2);

    if (mode == MODE_STEPPED)
    {
      int countIn = beatInPhrase - (stepBeats - countInBeats) + 1; // 1 .. countInBeats
      int stepDir = lastStepDir.load(), cueDir = nextStepDir.load();
      if (beatInPhrase == 0 && beat > 0 && stepDir != 0)
      {
        c = dirColor(stepDir); // the step just happened: one long flash
        level = sinceBeat < STEP_FLASH_MS ? flashLevel : 0;
      }
      else if (countIn >= 1 && cueDir != 0)
      {
        c = dirColor(cueDir); // count-in, getting brighter toward the step
        level = sinceBeat < BEAT_FLASH_MS ? std::max(1, flashLevel * countIn / countInBeats) : 0;
      }
      else
        level = sinceBeat < BEAT_FLASH_MS ? normal : 0;
    }
    else
    {
      c = dirColor(glideDir.load());
      level = sinceBeat < BEAT_FLASH_MS ? normal : 0;
    }
  }

  // Dim the full-scale colour to the level, keeping a lit channel at least 1
  auto dim = [&](uint8_t v) { return (uint8_t)(v ? std::max(1, (v * level + 254) / 255) : 0); };
  uint8_t r = level ? dim(c.r) : 0, g = level ? dim(c.g) : 0, b = level ? dim(c.b) : 0;
  uint32_t color = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
  if (color != lastColor)
  {
    rgbLedWrite(TRANSPORT_LED_PIN, r, g, b);
    lastColor = color;
  }
}

void loop()
{
  uint32_t now = millis();
  readSwitches(now);
  updateLed(now);
  updateTransportLed(now);
  delay(5);
}
