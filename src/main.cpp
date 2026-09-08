#include <Arduino.h>
#include <USB.h>     // Found via path mapping
#include <USBMIDI.h> // Found via path mapping

// Initialize the native Espressif MIDI interface
USBMIDI DeviceMIDI;

void setup()
{
  Serial.begin(115200);

  // Configure the USB hardware device properties
  USB.productName("ESP32-S3 Pulse MIDI");

  // Initialize the native MIDI protocol handler
  DeviceMIDI.begin();

  // Start up the core hardware USB controller
  USB.begin();
}

void loop()
{
  // Simple Hello World MIDI event sequence
  DeviceMIDI.noteOn(60, 100, 1); // Play Middle C
  delay(200);

  DeviceMIDI.noteOff(60, 100, 1);
  delay(800);
}
