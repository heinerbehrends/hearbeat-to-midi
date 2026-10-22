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
#include <DFRobot_BloodOxygen_S.h>
#include <HeartTempo.h>
#include <Switch3.h>
#include <Settings.h>

using namespace settings;

static const uint8_t SENSOR_ADDR = 0x57;
static const uint32_t TEMPO_UPDATE_MS = 100;

static const int PPQN = 24;
static const uint8_t MIDI_CLOCK = 0xF8;

USBMIDI DeviceMIDI;
DFRobot_BloodOxygen_S_I2C sensor(&Wire, SENSOR_ADDR);

// Shared between tasks
static std::atomic<uint32_t> clockIntervalUs{0};
static std::atomic<uint32_t> clockTicks{0};
static std::atomic<HeartState> heartState{HeartState::NoFinger};
static std::atomic<bool> readingOk{false};
static std::atomic<int> feedbackSwitch{0}; // -1 = −, 0 = follow, +1 = +
static std::atomic<int> rateSwitch{0};     // -1 = slow, 0 = medium, +1 = fast

static uint32_t intervalFor(float bpm)
{
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

// Advances a fixed phase by the current interval each tick, so tempo changes
// take effect smoothly between ticks without resetting the beat.
static void clockTask(void *)
{
  int64_t next = esp_timer_get_time();
  for (;;)
  {
    next += clockIntervalUs.load();
    int64_t wait;
    while ((wait = next - esp_timer_get_time()) > 0)
    {
      if (wait > 2000)
        vTaskDelay(1); // coarse sleep, then spin the last ~2 ms for accuracy
    }
    if (-wait > 100000)
      next = esp_timer_get_time(); // fell far behind (should not happen): resync

    DeviceMIDI.write(MIDI_CLOCK);
    clockTicks.fetch_add(1);
  }
}

static void sensorTask(void *)
{
  HeartTempo heartTempo(heartTempoConfig());
  HeartState lastState = heartTempo.state();
  int appliedFeedback = 99, appliedRate = 99; // force a log line at boot
  bool collecting = false;
  uint32_t lastPoll = millis() - SENSOR_POLL_MS;

  for (;;)
  {
    uint32_t now = millis();

    // Plain text lines are shown as-is by serial plotters
    int fb = feedbackSwitch.load();
    if (fb != appliedFeedback)
    {
      Serial.printf("feedback: %s\r\n", feedbackName(fb));
      heartTempo.setFeedback(fb);
      appliedFeedback = fb;
    }
    int rate = rateSwitch.load();
    if (rate != appliedRate)
    {
      const RatePreset &p = ratePreset(rate);
      Serial.printf("rate: %s (%.3f %%/beat, tau %.0f s)\r\n", p.name, p.maxChangePerBeat * 100, p.tauSec);
      heartTempo.setRate(p.maxChangePerBeat, p.tauSec);
      appliedRate = rate;
    }

    if (now - lastPoll >= SENSOR_POLL_MS)
    {
      lastPoll = now;
      int hr = -1;
      if (!sensorPresent())
      {
        heartTempo.onSensorMissing();
        collecting = false; // sensor may have lost power; restart it when it is back
      }
      else
      {
        if (!collecting)
        {
          sensor.sensorStartCollect();
          collecting = true;
        }
        sensor.getHeartbeatSPO2();
        hr = sensor._sHeartbeatSPO2.Heartbeat;
        heartTempo.onReading(hr, now);
      }
      heartState.store(heartTempo.state());
      readingOk.store(heartTempo.lastReadingAccepted());

      if (heartTempo.state() != lastState)
      {
        Serial.printf("state: %s -> %s\r\n", stateName(lastState), stateName(heartTempo.state()));
        lastState = heartTempo.state();
      }

      // Serial-plotter format, see diag/sensor_probe.cpp
      Serial.print(">");
      if (hr > 0)
        Serial.printf("Raw:%d,", hr);
      Serial.printf("HR:%.1f,Target:%.1f,Tempo:%.2f,State:%d,Ok:%d,Fb:%d,Rate:%d\r\n", heartTempo.heartRate(),
                    heartTempo.target(), heartTempo.tempo(), (int)heartTempo.state(),
                    (int)heartTempo.lastReadingAccepted(), fb, rate);
    }

    clockIntervalUs.store(intervalFor(heartTempo.update(now)));
    vTaskDelay(pdMS_TO_TICKS(TEMPO_UPDATE_MS));
  }
}

void setup()
{
  Serial.begin(115200); // hardware UART (COM port), see platformio.ini

  USB.productName("ESP32-S3 Pulse MIDI");
  DeviceMIDI.begin();
  USB.begin();

  Wire.begin(SENSOR_SDA, SENSOR_SCL, 100000);
  Wire.setTimeOut(50);

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

// Status LED: red blinking = no sensor, red = no finger, amber = acquiring or
// last reading missing/rejected, green flash on each beat = locked and the last
// reading was accepted.
static void updateLed(uint32_t now)
{
  static uint32_t lastBeatTick = 0, beatAtMs = 0;
  static uint32_t lastColor = 0xFFFFFFFF;

  uint32_t ticks = clockTicks.load();
  if (ticks / PPQN != lastBeatTick / PPQN)
    beatAtMs = now;
  lastBeatTick = ticks;

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
    g = LED_LEVEL / 3;
    break;
  case HeartState::Locked:
    if (readingOk.load())
      g = now - beatAtMs < BEAT_FLASH_MS ? LED_LEVEL : LED_LEVEL / 8;
    else
    {
      r = LED_LEVEL;
      g = LED_LEVEL / 3;
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
