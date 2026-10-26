// Control from the DAW: incoming MIDI CCs mapped to runtime settings (CC numbers,
// channel and mappings in include/Settings.h). Pure C++, tested on the board;
// the Arduino side reads the USB-MIDI packets and hands them over.
//
// feed() takes received packets (or values decoded from them) and keeps only the
// latest value per control, so a knob sweep costs one change per control and
// pass, however many CCs arrive.
// apply() then sends the controls whose mapped value changed through
// Params::set, the same validation as the serial `set`, and the rate preset to
// a RateSelect. Nothing is saved: a later `save` stores the values, like `set`.
#pragma once
#include <cstdint>
#include <functional>
#include "Params.h"

namespace midicontrol
{

enum Control : uint8_t
{
  STEP_BEATS,
  RATE,
  FEEDBACK_OFFSET,
  TEMPO_MULTIPLIER,
  COUNT
};

// The control a USB-MIDI event packet is for, or -1: anything but a control
// change on CONTROL_CC_CHANNEL to one of the control CCs (notes, clock, other
// channels, our own tempo CC pair coming back through MIDI-thru, ...).
int controlFor(const uint8_t packet[4]);
// CC number of a control.
uint8_t ccOf(int control);

// The mappings of a CC value 0-127
int stepBeatsFor(uint8_t value);       // 4 / 8 / 16 / 32
int rateFor(uint8_t value);            // -1 slow, 0 medium, +1 fast (rate switch positions)
float feedbackOffsetFor(uint8_t value); // 0 .. CC_FEEDBACK_OFFSET_MAX_BPM in 0.1 bpm steps
int tempoMultiplierFor(uint8_t value); // 1 / 2
// Any of the above as a float
float valueFor(int control, uint8_t value);

} // namespace midicontrol

// The effective rate preset: the rate switch position, or a CC override set
// since the switch last moved (the last change wins).
class RateSelect
{
public:
  // The switch position, called on every pass; a move clears the override.
  void setSwitch(int pos);
  // An override from CC (-1 slow, 0 medium, +1 fast) and the CC value it came from.
  void setOverride(int pos, uint8_t ccValue);
  int effective() const { return overridden_ ? override_ : switch_; }
  bool overridden() const { return overridden_; }
  uint8_t ccValue() const { return ccValue_; }

private:
  int switch_ = 0;
  bool switchKnown_ = false;
  bool overridden_ = false;
  int override_ = 0;
  uint8_t ccValue_ = 0;
};

class MidiControl
{
public:
  // A parameter changed through a CC; `source` names it for the log ("cc21=70").
  using Changed = std::function<void(int id, float oldValue, float newValue, const char *source)>;
  // Params refused a value (a rule between parameters); error() as for `set`.
  using Rejected = std::function<void(const char *error, const char *source)>;

  // Takes one received USB-MIDI event packet. True when it was one of the
  // controls (its value is kept until apply()).
  bool feed(const uint8_t packet[4]);
  // The same for a value already decoded with midicontrol::controlFor().
  void feed(int control, uint8_t value);
  // Applies the controls whose mapped value changed since they last applied.
  // A value equal to the current one changes nothing and isn't reported.
  void apply(Params &params, RateSelect &rate, const Changed &changed, const Rejected &rejected);

private:
  bool pending_[midicontrol::COUNT] = {};
  uint8_t raw_[midicontrol::COUNT] = {};
  bool applied_[midicontrol::COUNT] = {}; // lastValue_ is valid
  float lastValue_[midicontrol::COUNT] = {};
};
