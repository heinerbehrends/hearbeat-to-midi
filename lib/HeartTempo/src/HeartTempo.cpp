#include "HeartTempo.h"

#include <algorithm>
#include <cmath>

void HeartTempo::Buf::push(float x, int cap)
{
  cap = std::min(cap, kMaxBuf);
  if (n == cap)
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
  state_ = HeartState::NoSensor;
  candidates_.clear();
  invalidRun_ = 0;
  lastAccepted_ = false;
}

void HeartTempo::onReading(int hr, uint32_t nowMs)
{
  lastAccepted_ = false;
  if (hr < cfg_.minBpm || hr > cfg_.maxBpm)
  {
    if (++invalidRun_ >= cfg_.invalidToNoFinger)
    {
      state_ = HeartState::NoFinger;
      candidates_.clear();
    }
    return;
  }
  invalidRun_ = 0;

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
  float gate = std::max(cfg_.gateFloorBpm, cfg_.gateK * 1.4826f * dev.median());

  if (std::fabs(hr - med) <= gate)
  {
    history_.push(hr, cfg_.historySize);
    candidates_.clear();
    smooth(hr, nowMs);
    lastAccepted_ = true;
    return;
  }
  // Rejected. If rejections keep agreeing with each other, the heart rate really changed.
  tryConsensus(hr, nowMs);
}

bool HeartTempo::tryConsensus(float hr, uint32_t nowMs)
{
  candidates_.push(hr, cfg_.agreeCount + 1);
  float mean;
  if (candidates_.n < cfg_.agreeCount || candidates_.agreeing(cfg_.agreeTolBpm, &mean) < cfg_.agreeCount)
    return false;

  history_.clear();
  for (int i = 0; i < candidates_.n; i++)
    if (std::fabs(candidates_.v[i] - mean) <= 2 * cfg_.agreeTolBpm)
      history_.push(candidates_.v[i], cfg_.historySize);
  candidates_.clear();
  smooth(mean, nowMs);
  lastAccepted_ = true;
  return true;
}

void HeartTempo::smooth(float hr, uint32_t nowMs)
{
  float dt = smoothed_ ? std::min((nowMs - lastSmoothMs_) / 1000.0f, cfg_.maxDtSec) : cfg_.maxDtSec;
  estimate_ += (1 - std::exp(-dt / cfg_.tauSec)) * (hr - estimate_);
  smoothed_ = true;
  lastSmoothMs_ = nowMs;
}

void HeartTempo::setRate(float maxChangePerBeat, float tauSec)
{
  cfg_.maxChangePerBeat = maxChangePerBeat;
  cfg_.tauSec = tauSec;
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
