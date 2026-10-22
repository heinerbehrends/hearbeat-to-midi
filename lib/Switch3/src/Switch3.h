// Debounced 3-position switch (ON-OFF-ON): two contacts, centre = neither closed.
// Pure C++ (no Arduino dependencies) so it can be tested off the board.
#pragma once
#include <cstdint>

class Switch3
{
public:
  explicit Switch3(uint32_t debounceMs) : debounceMs_(debounceMs) {}

  // Feed the raw contact states (true = closed). Returns true when the debounced
  // position changed. The first call sets the position immediately.
  bool update(bool leftClosed, bool rightClosed, uint32_t nowMs);

  // -1 = left contact, 0 = centre, +1 = right contact
  int position() const { return stable_; }

private:
  uint32_t debounceMs_;
  bool started_ = false;
  int stable_ = 0;
  int candidate_ = 0;
  uint32_t candidateSinceMs_ = 0;
};
