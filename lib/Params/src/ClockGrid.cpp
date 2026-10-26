#include "ClockGrid.h"

#include <numeric>

bool stepChangeDue(uint32_t tick, int oldBeats, int newBeats, int ppqn)
{
  if (oldBeats < 1 || newBeats < 1)
    return true;
  uint32_t common = (uint32_t)std::lcm(oldBeats, newBeats) * (uint32_t)ppqn;
  return tick % common == 0;
}

bool barBoundary(uint32_t tick, int beatsPerBar, int ppqn)
{
  if (beatsPerBar < 1)
    beatsPerBar = 1;
  return tick % ((uint32_t)beatsPerBar * (uint32_t)ppqn) == 0;
}

uint32_t scaleInterval(uint32_t intervalUs, int multiplier, uint32_t minIntervalUs)
{
  if (multiplier < 1)
    multiplier = 1;
  uint32_t scaled = intervalUs / (uint32_t)multiplier;
  return scaled < minIntervalUs ? minIntervalUs : scaled;
}
