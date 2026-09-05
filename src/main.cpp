#include <Arduino.h>

const int SENSOR_PIN = 1;

// Dynamic filtering metrics
int signalMin = 4095;
int signalMax = 0;
int threshold = 2000;
unsigned long lastBeatTime = 0;
int bpm = 0;
bool pulseDetected = false;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }
  delay(1000);

  analogSetPinAttenuation(SENSOR_PIN, ADC_11db);
}

void loop()
{
  int rawValue = analogRead(SENSOR_PIN);
  unsigned long currentTime = millis();

  // Dynamically calibrate baseline threshold envelopes every 2 seconds
  static unsigned long lastCalibration = 0;
  if (currentTime - lastCalibration > 2000)
  {
    threshold = (signalMax + signalMin) / 2;
    signalMax = rawValue;
    signalMin = rawValue;
    lastCalibration = currentTime;
  }

  if (rawValue > signalMax)
    signalMax = rawValue;
  if (rawValue < signalMin)
    signalMin = rawValue;

  // Pulse Detection Logic
  if (rawValue > (threshold + 15) && !pulseDetected)
  { // Added a 15-unit dead-band to prevent noise triggers
    if (currentTime - lastBeatTime > 280)
    {
      pulseDetected = true;
      unsigned long timeBetweenBeats = currentTime - lastBeatTime;
      lastBeatTime = currentTime;
      bpm = 60000 / timeBetweenBeats;
    }
  }

  if (rawValue < threshold && pulseDetected)
  {
    pulseDetected = false;
  }

  // CRITICAL FIX: Only print to the screen every 40ms to completely clear the USB interface bottleneck
  static unsigned long lastPrintTime = 0;
  if (currentTime - lastPrintTime >= 40)
  {
    lastPrintTime = currentTime;

    // Stream raw CSV integers (Ultra-lightweight formatting)
    Serial.print(rawValue);
    Serial.print(",");
    Serial.print(threshold);
    Serial.print(",");
    Serial.println(bpm);
  }

  delay(5); // Internal oversampling speed
}
