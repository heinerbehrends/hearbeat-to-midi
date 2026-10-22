// Wiring diagnostic for the DFRobot heart rate / SpO2 sensor (SEN0344).
// Finds which GPIOs the sensor is on, whether it answers over I2C (0x57)
// or UART (Modbus, 9600 baud), and prints readings once found.
// Build/flash: pio run -e diag -t upload, then: pio device monitor
#include <Arduino.h>
#include <Wire.h>
#include <DFRobot_BloodOxygen_S.h>

// Pins safe to probe on the N16R8 (skips USB 19/20, flash/PSRAM 26-37, strapping 0/3/45/46,
// and 43/44, the UART that Serial uses)
static const uint8_t PINS[] = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                               21, 38, 39, 40, 41, 42, 47, 48};

static int foundSda = -1, foundScl = -1, foundRx = -1, foundTx = -1;

// A pin that reads HIGH with the internal pull-down enabled is being driven or
// pulled up externally: the sensor's I2C pull-ups or its UART TX idle level.
static int findPulledUpPins(uint8_t *out)
{
  int n = 0;
  for (uint8_t p : PINS)
  {
    pinMode(p, INPUT_PULLDOWN);
    delay(2);
    if (digitalRead(p) == HIGH)
      out[n++] = p;
    pinMode(p, INPUT);
  }
  return n;
}

static bool probeI2C(const uint8_t *cand, int n)
{
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
    {
      if (i == j)
        continue;
      Wire.end();
      Wire.begin(cand[i], cand[j], 100000);
      Wire.setTimeOut(20);
      Wire.beginTransmission(0x57);
      if (Wire.endTransmission() == 0)
      {
        foundSda = cand[i];
        foundScl = cand[j];
        return true;
      }
    }
  Wire.end();
  return false;
}

static bool probeUART(const uint8_t *cand, int n)
{
  // Sensor TX idles high, so RX (ESP side) must be one of the pulled-up pins;
  // the ESP TX pin can be any probed pin.
  for (int i = 0; i < n; i++)
    for (uint8_t tx : PINS)
    {
      if (tx == cand[i])
        continue;
      Serial1.end();
      Serial1.begin(9600, SERIAL_8N1, cand[i], tx);
      Serial1.setTimeout(300);
      DFRobot_BloodOxygen_S_HardWareUart s(&Serial1, 9600);
      if (s.begin())
      {
        foundRx = cand[i];
        foundTx = tx;
        return true;
      }
    }
  Serial1.end();
  return false;
}

void setup()
{
  Serial.begin(115200);
  delay(3000); // time to open the serial monitor after a reset
}

void loop()
{
  if (foundSda < 0 && foundRx < 0)
  {
    uint8_t high[sizeof(PINS)];
    int n = findPulledUpPins(high);
    Serial.printf("\n--- scan ---\nPins pulled HIGH externally (%d):", n);
    for (int i = 0; i < n; i++)
      Serial.printf(" %d", high[i]);
    Serial.println(n ? "" : " none -> sensor unpowered, or signal wires not on probed pins");

    if (n >= 2 && probeI2C(high, n))
      Serial.printf("I2C sensor found: SDA=GPIO%d SCL=GPIO%d\n", foundSda, foundScl);
    else if (n >= 1 && probeUART(high, n))
      Serial.printf("UART sensor found: ESP RX=GPIO%d (sensor TX), ESP TX=GPIO%d (sensor RX)\n", foundRx, foundTx);
    else
      Serial.println("No sensor answered over I2C or UART. Retrying in 5 s.");
    delay(5000);
    return;
  }

  static DFRobot_BloodOxygen_S_I2C i2cSensor(&Wire, 0x57);
  static DFRobot_BloodOxygen_S_HardWareUart uartSensor(&Serial1, 9600);
  DFRobot_BloodOxygen_S &sensor = foundSda >= 0 ? (DFRobot_BloodOxygen_S &)i2cSensor : (DFRobot_BloodOxygen_S &)uartSensor;

  static bool started = false;
  if (!started)
  {
    sensor.sensorStartCollect();
    started = true;
    Serial.println("Collecting. Put a finger on the sensor; first values take ~4 s.");
  }

  // Serial-plotter format (">name:value,..."), works with Arduino IDE's plotter and
  // the VS Code "Serial Plotter" extension. Invalid (-1) values are left out so the
  // graph shows gaps instead of dips to -1.
  sensor.getHeartbeatSPO2();
  int hr = sensor._sHeartbeatSPO2.Heartbeat;
  int spo2 = sensor._sHeartbeatSPO2.SPO2;
  Serial.print(">");
  if (hr > 0)
    Serial.printf("HR:%d,", hr);
  if (spo2 > 0)
    Serial.printf("SpO2:%d,", spo2);
  Serial.printf("Temp:%.1f\r\n", sensor.getTemperature_C());
  delay(1000);
}
