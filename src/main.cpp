#include <Arduino.h>
#include <PulseSensorPlayground.h>

// 1 = SERIAL_PLOTTER (Visual graph to see the waveform)
// 0 = SERIAL_MONITOR (Text metrics for debugging values)
#define USE_SERIAL_PLOTTER 1

const int PULSE_INPUT = 1; // Connect PulseSensor Signal (Purple) to GPIO 1 (ADC1_CH0)
const int THRESHOLD = 550; // 550 works well since the library scales ESP32 input down to 10-bit internally

PulseSensorPlayground pulseSensor;
unsigned long ledOffTime = 0; // Tracks when to turn off the RGB LED
bool ledIsOn = false;         // Tracks current LED state

void setup()
{
  Serial.begin(115200);

  // Initialize the sensor configuration
  pulseSensor.analogInput(PULSE_INPUT);
  pulseSensor.setThreshold(THRESHOLD);
  pulseSensor.setSerial(Serial);

  // Set output format conditionally
  if (USE_SERIAL_PLOTTER)
  {
    pulseSensor.setOutputType(SERIAL_PLOTTER);
  }

  if (pulseSensor.begin())
  {
    Serial.println("System Active.");
  }
  else
  {
    Serial.println("Sensor Init Failed!");
    while (1)
      ;
  }
}

void loop()
{
  // Check for software sample timing
  if (pulseSensor.sawNewSample())
  {

    // 1. Plotter Debugging (Visualizes the pulse wave in VS Code)
    if (USE_SERIAL_PLOTTER)
    {
      pulseSensor.outputSample();
    }

    // 2. Heartbeat Evaluation
    if (pulseSensor.sawStartOfBeat())
    {
      int myBPM = pulseSensor.getBeatsPerMinute();
      int myIBI = pulseSensor.getInterBeatIntervalMs();

      // Onboard LED software flash: Turns bright GREEN on heartbeat
      neopixelWrite(RGB_BUILTIN, 0, 150, 0);
      ledIsOn = true;
      ledOffTime = millis() + 250; // Set LED to turn off after 250ms

      // Textual Debugging (Switch USE_SERIAL_PLOTTER to 0 to view this cleanly)
      if (!USE_SERIAL_PLOTTER)
      {
        Serial.printf("♥ Beat! BPM: %d | IBI: %d ms | Threshold Baseline: %d\n", myBPM, myIBI, THRESHOLD);
      }
    }
  }

  // 3. Handle turning the LED off after the pulse flash duration has elapsed
  if (ledIsOn && millis() > ledOffTime)
  {
    neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Off
    ledIsOn = false;
  }
}
