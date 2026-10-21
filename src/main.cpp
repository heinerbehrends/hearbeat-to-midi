// Heartbeat to MIDI clock.
//
// Core 0: sensor task polls the SEN0344 every 4 s and runs HeartTempo.
// Core 1: clock task sends 24 PPQN MIDI clock at the current tempo, always.
//         loop() drives the status LED.
#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include <Wire.h>
#include <atomic>
#include <DFRobot_BloodOxygen_S.h>
#include <HeartTempo.h>

// Sensor wiring found with the diag probe
static const int SENSOR_SDA = 17;
static const int SENSOR_SCL = 9;
static const uint8_t SENSOR_ADDR = 0x57;
static const uint32_t SENSOR_POLL_MS = 4000;
static const uint32_t TEMPO_UPDATE_MS = 100;

// Onboard RGB LED: GPIO48 on most DevKitC-1 boards, GPIO38 on v1.1
static const int LED_PIN = 48;
static const uint8_t LED_LEVEL = 32;
static const uint32_t BEAT_FLASH_MS = 80;

static const int PPQN = 24;
static const uint8_t MIDI_CLOCK = 0xF8;

USBMIDI DeviceMIDI;
DFRobot_BloodOxygen_S_I2C sensor(&Wire, SENSOR_ADDR);

// Shared between tasks
static std::atomic<uint32_t> clockIntervalUs{0};
static std::atomic<uint32_t> clockTicks{0};
static std::atomic<HeartState> heartState{HeartState::NoFinger};

static uint32_t intervalFor(float bpm)
{
  return (uint32_t)(60e6f / (bpm * PPQN));
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
  HeartTempo heartTempo;
  bool collecting = false;
  uint32_t lastPoll = millis() - SENSOR_POLL_MS;

  for (;;)
  {
    uint32_t now = millis();
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

      // Serial-plotter format, see diag/sensor_probe.cpp
      Serial.print(">");
      if (hr > 0)
        Serial.printf("Raw:%d,", hr);
      Serial.printf("HR:%.1f,Tempo:%.2f,State:%d\r\n", heartTempo.heartRate(), heartTempo.tempo(),
                    (int)heartTempo.state());
    }

    clockIntervalUs.store(intervalFor(heartTempo.update(now)));
    vTaskDelay(pdMS_TO_TICKS(TEMPO_UPDATE_MS));
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0); // never stall when no serial monitor is attached

  USB.productName("ESP32-S3 Pulse MIDI");
  DeviceMIDI.begin();
  USB.begin();

  Wire.begin(SENSOR_SDA, SENSOR_SCL, 100000);
  Wire.setTimeOut(50);

  clockIntervalUs.store(intervalFor(HeartTempoConfig().startBpm));
  xTaskCreatePinnedToCore(clockTask, "clock", 4096, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
  xTaskCreatePinnedToCore(sensorTask, "sensor", 8192, nullptr, 2, nullptr, 0);
}

// Status LED: red blinking = no sensor, red = no finger, amber = acquiring,
// green flash on each beat = locked and following the heart rate.
void loop()
{
  static uint32_t lastBeatTick = 0, beatAtMs = 0;
  static uint32_t lastColor = 0xFFFFFFFF;

  uint32_t now = millis();
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
    g = now - beatAtMs < BEAT_FLASH_MS ? LED_LEVEL : LED_LEVEL / 8;
    break;
  }

  uint32_t color = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
  if (color != lastColor)
  {
    rgbLedWrite(LED_PIN, r, g, b);
    lastColor = color;
  }
  delay(5);
}
