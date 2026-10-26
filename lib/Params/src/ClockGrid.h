// Clock-side rules for runtime changes: when a new step length or tempo
// multiplier may take effect, and the clock interval for a multiplier.
// Pure C++, tested on the board. Ticks are 24 PPQN clock ticks counted from
// Start, so beats here are clock beats (doubled with tempo_multiplier 2).
#pragma once
#include <cstdint>

// Stepped mode: a change of the step length takes effect on a beat that is a
// phrase boundary of both the old and the new length, so the phrase grid stays
// in phase. True when `tick` is such a boundary. With powers of two this is
// every max(old, new) beats.
bool stepChangeDue(uint32_t tick, int oldBeats, int newBeats, int ppqn);

// True when `tick` is the first tick of a bar: a tempo multiplier change (an
// audible change of feel) lands on a bar line.
bool barBoundary(uint32_t tick, int beatsPerBar, int ppqn);

// Clock interval for a heart rate tempo interval and a multiplier (1 or 2),
// never shorter than minIntervalUs (the clock's tempo cap).
uint32_t scaleInterval(uint32_t intervalUs, int multiplier, uint32_t minIntervalUs);
