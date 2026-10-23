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

USBMIDI DeviceMIDI(DEVICE_NAME);

// Shared between tasks
static std::atomic<uint32_t> clockIntervalUs{0};
static std::atomic<uint32_t> midiDrops{0}; // clock ticks the USB stack refused
static std::atomic<HeartState> heartState{HeartState::NoFinger};
static std::atomic<bool> readingOk{false};
static std::atomic<int> feedbackSwitch{0}; // -1 = −, 0 = follow, +1 = +
static std::atomic<int> rateSwitch{0};     // -1 = slow, 0 = medium, +1 = fast

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

// Returns false if the transfer failed; *hr is -1 when the sensor has no heart rate.
static bool sensorReadHeartRate(int *hr)
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
  *hr = v == 0 || v > 1000 ? -1 : (int)v;
  return true;
}

static void i2cBegin()
{
  Wire.begin(SENSOR_SDA, SENSOR_SCL, 100000);
  Wire.setTimeOut(50);
}

// Advances a fixed phase by the current interval each tick, so tempo changes
// take effect smoothly between ticks without resetting the beat.
static void clockTask(void *)
{
  int64_t next = esp_timer_get_time();
  for (;;)
  {
    uint32_t interval = clockIntervalUs.load();
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
    if (tud_mounted() && !tud_suspended() && DeviceMIDI.write(MIDI_CLOCK) == 0)
      midiDrops.fetch_add(1);
  }
}

static void sensorTask(void *)
{
  HeartTempo heartTempo(heartTempoConfig());
  HeartState lastState = heartTempo.state();
  int appliedFeedback = 99, appliedRate = 99; // force a log line at boot
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

    if (now - lastPoll >= SENSOR_POLL_MS)
    {
      lastPoll = now;
      int hr = -1;
      if (!sensorPresent() || (!collecting && !sensorStartCollect()) || !sensorReadHeartRate(&hr))
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
        // A sensor that reset without us noticing stops measuring: nudge it again
        invalidPolls = hr > 0 ? 0 : invalidPolls + 1;
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
      // HR = filtered estimate, Target = HR with feedback, Tempo = clock tempo.
      Serial.print(">");
      if (hr > 0)
        Serial.printf("Raw:%d,", hr);
      Serial.printf("HR:%.1f,Target:%.1f,Tempo:%.2f,State:%d,Ok:%d,Fb:%d,Rate:%d,Drops:%u\r\n",
                    heartTempo.heartRate(), heartTempo.target(), heartTempo.tempo(), (int)heartTempo.state(),
                    (int)heartTempo.lastReadingAccepted(), fb, rate, (unsigned)midiDrops.load());
    }

    clockIntervalUs.store(intervalFor(heartTempo.update(now)));
    vTaskDelay(pdMS_TO_TICKS(TEMPO_UPDATE_MS));
  }
}

void setup()
{
  rgbLedWrite(LED_PIN, 0, 0, 0); // the LED keeps its last colour across resets
  Serial.begin(115200);          // hardware UART (COM port), see platformio.ini

  USB.productName(DEVICE_NAME);
  DeviceMIDI.begin();
  USB.begin();

  esp_log_level_set("i2c.master", ESP_LOG_NONE); // keep bus errors out of the plotter stream
  i2cBegin();

  // Switch contacts pull to GND; an unconnected switch reads as centre
  for (int pin : {FEEDBACK_PIN_MINUS, FEEDBACK_PIN_PLUS, RATE_PIN_SLOW, RATE_PIN_FAST})
    pinMode(pin, INPUT_PULLUP);

  clockIntervalUs.store(intervalFor(START_BPM));
  xTaskCreatePinnedToCore(clockTask, "clock", 4096, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
  xTaskCreatePinnedToCore(sensorTask, "sensor", 8192, nullptr, 2, nullptr, 0);
}

static void readSwitches(uint32_t now)
{
  static Switch3 feedback(SWITCH_DEBOUNCE_MS), rate(SWITCH_DEBOUNCE_MS);
  if (feedback.update(digitalRead(FEEDBACK_PIN_MINUS) == LOW, digitalRead(FEEDBACK_PIN_PLUS) == LOW, now))
    feedbackSwitch.store(feedback.position());
  if (rate.update(digitalRead(RATE_PIN_SLOW) == LOW, digitalRead(RATE_PIN_FAST) == LOW, now))
    rateSwitch.store(rate.position());
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
