// Heartbeat to MIDI clock.
//
// Core 0: sensor task polls the SEN0344 every 4 s and runs HeartTempo.
// Core 1: clock task sends 24 PPQN MIDI clock at the current tempo, always.
//         loop() reads the switches and drives the status LED.
//
// Settings to tweak live in include/Settings.h.
#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include <Wire.h>
#include <atomic>
#include <cmath>
#include <esp_log.h>
#include <esp32-hal-tinyusb.h>
#include <HeartTempo.h>
#include <Switch3.h>
#include <Settings.h>

using namespace settings;

// SEN0344 registers (as used by the DFRobot library, read here directly so that
// failed transfers are detected instead of decoding an uninitialised buffer)
static const uint8_t SENSOR_ADDR = 0x57;
static const uint8_t REG_RESULT = 0x0C; // 8 bytes: [0] SpO2, [2..5] heart rate (big endian), 0 = none
static const uint8_t REG_COLLECT = 0x20;
static const uint32_t TEMPO_UPDATE_MS = 100;

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

static const RatePreset &ratePreset(int pos)
{
  return pos < 0 ? RATE_SLOW : pos > 0 ? RATE_FAST : RATE_MEDIUM;
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

// Advances a fixed phase by the current interval each tick, so tempo changes
// take effect smoothly between ticks without resetting the beat. In stepped mode
// the interval is held and only refreshed on every STEP_BEATS-th beat after Start.
static void clockTask(void *)
{
  const uint32_t ticksPerStep = STEP_BEATS * PPQN;
  int64_t next = esp_timer_get_time();
  uint32_t held = clockIntervalUs.load();
  bool gliding = true;
  uint32_t tick = 0;     // index of the next tick since Start (0 = beat 1); counts
                         // only ticks that went out, so it matches the DAW's count
  uint32_t failRun = 0;  // ticks in a row that couldn't be sent
  for (;;)
  {
    int mode = transportMode.load();
    if (mode == MODE_STEPPED)
      gliding = false;
    else if (mode == MODE_STOP)
      gliding = true;
    // (stepped -> run keeps holding until the next phrase boundary, where the
    // held tempo meets the glide, so leaving stepped mode never jumps)
    if (gliding)
      held = clockIntervalUs.load();
    uint32_t interval = held;
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
        // Step boundary: the tick just sent starts a new phrase, take the glide's
        // current tempo for the intervals that follow
        if (tick % ticksPerStep == 0)
        {
          held = clockIntervalUs.load();
          if (mode == MODE_RUN)
            gliding = true;
        }
        tick++;
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
      tick = 0;
    sendTempoCc(held);
  }
}

static void sensorTask(void *)
{
  HeartTempo heartTempo(heartTempoConfig());
  HeartState lastState = heartTempo.state();
  int appliedFeedback = 99, appliedRate = 99; // force a log line at boot
  int loggedMode = MODE_STOP;
  bool collecting = false;
  int failedPolls = 0, invalidPolls = 0;
  uint32_t lastPoll = millis() - SENSOR_POLL_MS;

  for (;;)
  {
    uint32_t now = millis();

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
    int rate = rateSwitch.load();
    if (rate != appliedRate)
    {
      const RatePreset &p = ratePreset(rate);
      if (appliedRate == 99)
        Serial.printf("rate: %s", p.name);
      else
        Serial.printf("rate: %s -> %s", ratePreset(appliedRate).name, p.name);
      Serial.printf(" (%.3f %%/beat, tau %.0f s)\r\n", p.maxChangePerBeat * 100, p.tauSec);
      heartTempo.setRate(p.maxChangePerBeat, p.tauSec);
      appliedRate = rate;
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
    vTaskDelay(pdMS_TO_TICKS(TEMPO_UPDATE_MS));
  }
}

void setup()
{
  rgbLedWrite(LED_PIN, 0, 0, 0); // the LED keeps its last colour across resets
  Serial.begin(115200); // DevKit: hardware UART (COM port); XIAO: USB CDC next to MIDI
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0); // never stall the sensor task when no serial monitor is open
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

  clockIntervalUs.store(intervalFor(START_BPM));
  clockSentIntervalUs.store(intervalFor(START_BPM));
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

// Status LED: red blinking = no sensor, red = no finger (tempo held, clock still
// running), amber = acquiring (or, with LED_SHOW_EVERY_MISS, last reading
// missing/rejected), green = locked and following the heart rate.
static void updateLed(uint32_t now)
{
  static uint32_t lastColor = 0xFFFFFFFF;
  const uint8_t amberGreen = std::max(1, LED_LEVEL / 3);

  uint8_t r = 0, g = 0, b = 0;
  switch (heartState.load())
  {
  case HeartState::NoSensor:
    r = (now / 250) % 2 ? LED_LEVEL : 0;
    break;
  case HeartState::NoFinger:
    r = LED_LEVEL;
    break;
  case HeartState::Acquiring:
    r = LED_LEVEL;
    g = amberGreen;
    break;
  case HeartState::Locked:
    if (readingOk.load() || !LED_SHOW_EVERY_MISS)
      g = LED_LEVEL;
    else
    {
      r = LED_LEVEL;
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

void loop()
{
  uint32_t now = millis();
  readSwitches(now);
  updateLed(now);
  delay(5);
}
