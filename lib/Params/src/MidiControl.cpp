#include "MidiControl.h"

#include <cmath>
#include <cstdio>
// By path, like Params.cpp (the framework has a settings.h of its own)
#include "../../../include/Settings.h"

using namespace midicontrol;
namespace s = settings;

namespace
{

const uint8_t USB_MIDI_CIN_CONTROL_CHANGE = 0x0B;
const uint8_t CONTROL_CHANGE = 0xB0;

template <size_t N>
int zone(const int (&values)[N], uint8_t value)
{
  // The 0-127 range in N equal zones
  return values[(value & 0x7F) * N / 128];
}

} // namespace

int midicontrol::controlFor(const uint8_t packet[4])
{
  if (!s::CONTROL_CC_ENABLED || (packet[0] & 0x0F) != USB_MIDI_CIN_CONTROL_CHANGE ||
      packet[1] != (CONTROL_CHANGE | (s::CONTROL_CC_CHANNEL - 1)) || packet[2] > 127 || packet[3] > 127)
    return -1;
  for (int c = 0; c < COUNT; c++)
    if (packet[2] == ccOf(c))
      return c;
  return -1;
}

uint8_t midicontrol::ccOf(int control)
{
  switch (control)
  {
  case STEP_BEATS:
    return s::CC_STEP_BEATS;
  case RATE:
    return s::CC_RATE;
  case FEEDBACK_OFFSET:
    return s::CC_FEEDBACK_OFFSET;
  case TEMPO_MULTIPLIER:
    return s::CC_TEMPO_MULTIPLIER;
  }
  return 0xFF;
}

int midicontrol::stepBeatsFor(uint8_t value)
{
  return zone(s::CC_STEP_BEATS_VALUES, value);
}

int midicontrol::rateFor(uint8_t value)
{
  static const int POSITIONS[] = {-1, 0, 1};
  return zone(POSITIONS, value);
}

float midicontrol::feedbackOffsetFor(uint8_t value)
{
  // Whole tenths, so equal CC values always give exactly the same float
  return std::lround((value & 0x7F) * s::CC_FEEDBACK_OFFSET_MAX_BPM * 10 / 127) / 10.0f;
}

int midicontrol::tempoMultiplierFor(uint8_t value)
{
  return zone(s::CC_TEMPO_MULTIPLIER_VALUES, value);
}

float midicontrol::valueFor(int control, uint8_t value)
{
  switch (control)
  {
  case STEP_BEATS:
    return stepBeatsFor(value);
  case RATE:
    return rateFor(value);
  case FEEDBACK_OFFSET:
    return feedbackOffsetFor(value);
  case TEMPO_MULTIPLIER:
    return tempoMultiplierFor(value);
  }
  return 0;
}

void RateSelect::setSwitch(int pos)
{
  if (switchKnown_ && pos == switch_)
    return;
  if (switchKnown_)
    overridden_ = false; // the switch moved: it wins over an earlier CC
  switch_ = pos;
  switchKnown_ = true;
}

void RateSelect::setOverride(int pos, uint8_t ccValue)
{
  override_ = pos;
  ccValue_ = ccValue;
  overridden_ = true;
}

bool MidiControl::feed(const uint8_t packet[4])
{
  int c = controlFor(packet);
  if (c < 0)
    return false;
  feed(c, packet[3]);
  return true;
}

void MidiControl::feed(int control, uint8_t value)
{
  if (control < 0 || control >= COUNT)
    return;
  pending_[control] = true;
  raw_[control] = value & 0x7F;
}

void MidiControl::apply(Params &params, RateSelect &rate, const Changed &changed, const Rejected &rejected)
{
  for (int c = 0; c < COUNT; c++)
  {
    if (!pending_[c])
      continue;
    pending_[c] = false;
    float value = valueFor(c, raw_[c]);
    // Only a new mapped value counts: a knob moving within one zone does nothing
    if (applied_[c] && value == lastValue_[c])
      continue;
    applied_[c] = true;
    lastValue_[c] = value;

    char source[16];
    std::snprintf(source, sizeof(source), "cc%u=%u", (unsigned)ccOf(c), (unsigned)raw_[c]);
    if (c == RATE)
    {
      rate.setOverride((int)value, raw_[c]);
      continue;
    }
    int id = c == STEP_BEATS ? params::STEP_BEATS : c == FEEDBACK_OFFSET ? params::FEEDBACK_OFFSET_BPM
                                                                          : params::TEMPO_MULTIPLIER;
    float old = params.get(id);
    if (value == old)
      continue;
    // A step length at or below the count-in would be refused: shorten the
    // count-in first (it only cues the LED), so every zone of the knob works
    if (id == params::STEP_BEATS && params.get(params::COUNT_IN_BEATS) >= value)
    {
      float oldCountIn = params.get(params::COUNT_IN_BEATS);
      if (params.set(params::COUNT_IN_BEATS, value - 1) && changed)
        changed(params::COUNT_IN_BEATS, oldCountIn, value - 1, source);
    }
    if (params.set(id, value))
    {
      if (changed)
        changed(id, old, value, source);
    }
    else if (rejected)
      rejected(params.error(), source);
  }
}
