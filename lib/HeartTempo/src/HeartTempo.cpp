#include "HeartTempo.h"

#include <algorithm>
#include <cmath>

void HeartTempo::Buf::push(float x, int cap)
{
  cap = std::max(1, std::min(cap, kMaxBuf));
  // >= rather than ==: after setConfig shrinks agreeCount, a buffer can hold
  // more values than its new capacity; drop the oldest ones
  while (n >= cap)
  {
    std::copy(v + 1, v + n, v);
    n--;
  }
  v[n++] = x;
}

float HeartTempo::Buf::median() const
{
  float s[kMaxBuf];
  std::copy(v, v + n, s);
  std::sort(s, s + n);
  return n % 2 ? s[n / 2] : (s[n / 2 - 1] + s[n / 2]) / 2;
}

int HeartTempo::Buf::agreeing(float tol, float *mean) const
{
  float med = median();
  float sum = 0;
  int count = 0;
  for (int i = 0; i < n; i++)
    if (std::fabs(v[i] - med) <= tol)
    {
      sum += v[i];
      count++;
    }
  *mean = count ? sum / count : med;
  return count;
}

HeartTempo::HeartTempo(const HeartTempoConfig &cfg)
    : cfg_(cfg), estimate_(cfg.startBpm), tempo_(cfg.startBpm) {}

void HeartTempo::onSensorMissing()
{
  handleMissing();
  afterPoll();
}

void HeartTempo::onReading(int hr, uint32_t nowMs)
{
  handleReading(hr, nowMs);
  afterPoll();
}

// A lock that hasn't accepted anything for lockTimeoutPolls (e.g. the sensor
// only produces junk) is no lock: back to acquiring. The tempo holds either way.
void HeartTempo::afterPoll()
{
  sinceAccepted_ = lastAccepted_ ? 0 : sinceAccepted_ + 1;
  if (state_ == HeartState::Locked && sinceAccepted_ >= cfg_.lockTimeoutPolls)
  {
    state_ = HeartState::Acquiring;
    candidates_.clear();
  }
}

void HeartTempo::handleMissing()
{
  lastAccepted_ = false;
  // A failed poll or two (a bus glitch) is a gap like an invalid reading; only a
  // run of them means the sensor is gone
  if (++missingRun_ < cfg_.missingToNoSensor)
  {
    countInvalid();
    return;
  }
  state_ = HeartState::NoSensor;
  candidates_.clear();
  invalidRun_ = 0;
  forgetCautionState();
}

void HeartTempo::countInvalid()
{
  highRun_.clear(); // a gap breaks a run of high readings
  if (++invalidRun_ >= cfg_.invalidToNoFinger)
  {
    state_ = HeartState::NoFinger;
    candidates_.clear();
    forgetCautionState();
  }
}

// After the finger or sensor was gone, earlier readings say nothing about the next ones
void HeartTempo::forgetCautionState()
{
  highRun_.clear();
  haveKept_ = false;
}

void HeartTempo::handleReading(int hr, uint32_t nowMs)
{
  lastAccepted_ = false;
  missingRun_ = 0;
  if (hr < cfg_.minBpm || hr > cfg_.maxBpm)
  {
    if (state_ == HeartState::NoSensor)
      state_ = HeartState::NoFinger; // the sensor answers again, there's just no heart rate
    countInvalid();
    return;
  }
  invalidRun_ = 0; // the sensor sees a signal, even if we discard this value
  if (!passesCaution(hr))
    return;

  if (state_ != HeartState::Locked)
  {
    state_ = HeartState::Acquiring;
    if (tryConsensus(hr, nowMs))
      state_ = HeartState::Locked;
    return;
  }

  // Outlier gate: median +- max(floor, k * MAD scaled to a standard deviation)
  float med = history_.median();
  Buf dev;
  for (int i = 0; i < history_.n; i++)
    dev.push(std::fabs(history_.v[i] - med), kMaxBuf);
  // Capped, so a widely spread history (e.g. after a ramp) can't open the gate wide
  float gate = std::min(std::max(cfg_.gateFloorBpm, cfg_.gateK * 1.4826f * dev.median()),
                        std::max(cfg_.gateMaxBpm, cfg_.gateFloorBpm));

  if (std::fabs(hr - med) <= gate)
  {
    history_.push(hr, cfg_.historySize);
    candidates_.clear();
    smooth(hr, nowMs);
    lastAccepted_ = true;
    return;
  }
  // Rejected. If rejections keep agreeing with each other, or keep climbing or
  // falling together, the heart rate really changed.
  if (!tryConsensus(hr, nowMs))
    acceptRamp(med, nowMs);
}

bool HeartTempo::passesCaution(float hr)
{
  bool smooth = haveKept_ && std::fabs(hr - lastKept_) <= cfg_.cautionStepBpm;
  if (hr > cfg_.cautionAboveBpm && !smooth)
  {
    highRun_.push(hr, cfg_.agreeCount + 1);
    float mean;
    // Enough of them agree, and this reading is one of those that agree
    if (highRun_.n < cfg_.agreeCount || highRun_.agreeing(cfg_.agreeTolBpm, &mean) < cfg_.agreeCount ||
        std::fabs(hr - highRun_.median()) > cfg_.agreeTolBpm)
      return false;
  }
  highRun_.clear();
  haveKept_ = true;
  lastKept_ = hr;
  return true;
}

bool HeartTempo::tryConsensus(float hr, uint32_t nowMs)
{
  candidates_.push(hr, cfg_.agreeCount + 1);
  float mean;
  if (candidates_.n < cfg_.agreeCount || candidates_.agreeing(cfg_.agreeTolBpm, &mean) < cfg_.agreeCount)
    return false;

  // Seed the history with exactly the readings that agreed
  float med = candidates_.median();
  history_.clear();
  for (int i = 0; i < candidates_.n; i++)
    if (std::fabs(candidates_.v[i] - med) <= cfg_.agreeTolBpm)
      history_.push(candidates_.v[i], cfg_.historySize);
  candidates_.clear();
  smooth(mean, nowMs);
  lastAccepted_ = true;
  return true;
}

// A steady climb (e.g. exercise onset) outruns the gate's median, and its
// readings are too spread out to agree. Accept it once the last agreeCount
// rejections all lie beyond the median and each one continues the same way.
bool HeartTempo::acceptRamp(float med, uint32_t nowMs)
{
  int k = cfg_.agreeCount;
  if (k < 2 || candidates_.n < k)
    return false;
  const float *run = candidates_.v + candidates_.n - k;
  float dir = run[k - 1] > run[0] ? 1 : -1;
  for (int i = 0; i < k; i++)
  {
    if ((run[i] - med) * dir <= 0)
      return false;
    if (i > 0 && (run[i] - run[i - 1]) * dir <= 0)
      return false;
  }

  // Move toward the middle of the run, not its end: a ramp is accepted on little
  // evidence, so take the cautious value
  Buf ramp;
  for (int i = 0; i < k; i++)
    ramp.push(run[i], kMaxBuf);
  history_.clear();
  for (int i = 0; i < k; i++)
    history_.push(run[i], cfg_.historySize);
  candidates_.clear();
  smooth(ramp.median(), nowMs);
  lastAccepted_ = true;
  return true;
}

void HeartTempo::smooth(float hr, uint32_t nowMs)
{
  // The first lock is a measured value: start the estimate there instead of
  // easing in from startBpm (the tempo still glides there at the glide limit)
  if (!smoothed_)
    estimate_ = hr;
  else
  {
    float dt = std::min((nowMs - lastSmoothMs_) / 1000.0f, cfg_.maxDtSec);
    estimate_ += (1 - std::exp(-dt / cfg_.tauSec)) * (hr - estimate_);
  }
  smoothed_ = true;
  lastSmoothMs_ = nowMs;
}

void HeartTempo::setRate(float maxChangePerBeat, float tauSec)
{
  cfg_.maxChangePerBeat = maxChangePerBeat;
  cfg_.tauSec = tauSec;
}

void HeartTempo::setConfig(const HeartTempoConfig &cfg)
{
  cfg_ = cfg;
}

float HeartTempo::target() const
{
  if (feedback_ < 0)
    return std::min(estimate_, std::max(estimate_ - cfg_.feedbackOffsetBpm, cfg_.steerLowBpm));
  if (feedback_ > 0)
    return std::max(estimate_, std::min(estimate_ + cfg_.feedbackOffsetBpm, cfg_.steerHighBpm));
  return estimate_;
}

float HeartTempo::update(uint32_t nowMs)
{
  float dt = updated_ ? (nowMs - lastUpdateMs_) / 1000.0f : 0;
  updated_ = true;
  lastUpdateMs_ = nowMs;

  if (state_ == HeartState::Locked)
  {
    float beats = tempo_ / 60 * dt;
    float step = tempo_ * cfg_.maxChangePerBeat * beats;
    tempo_ += std::max(-step, std::min(step, target() - tempo_));
  }
  return tempo_;
}
