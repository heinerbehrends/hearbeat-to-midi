#include "Switch3.h"

bool Switch3::update(bool leftClosed, bool rightClosed, uint32_t nowMs)
{
  // Both closed can only be a wiring fault: treat it as centre
  int raw = leftClosed == rightClosed ? 0 : leftClosed ? -1 : 1;

  if (!started_)
  {
    started_ = true;
    stable_ = candidate_ = raw;
    candidateSinceMs_ = nowMs;
    return true;
  }
  if (raw != candidate_)
  {
    candidate_ = raw;
    candidateSinceMs_ = nowMs;
  }
  if (candidate_ != stable_ && nowMs - candidateSinceMs_ >= debounceMs_)
  {
    stable_ = candidate_;
    return true;
  }
  return false;
}
