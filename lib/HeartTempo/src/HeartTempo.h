// Turns noisy heart rate readings into a slowly gliding clock tempo.
// Pure C++ (no Arduino dependencies) so it can be tested off the board.
//
//   reading -> validate -> outlier gate -> smoothing -> slew limit -> tempo
//
// The tempo never stops: it only moves while the reading is locked, and holds
// its last value through gaps, finger-off and sensor loss.
#pragma once
#include <cstdint>

enum class HeartState : uint8_t
{
  NoSensor,  // sensor did not answer
  NoFinger,  // sensor answers but no valid heart rate for a while
  Acquiring, // valid readings, but not enough of them agree yet
  Locked,    // tempo is following the heart rate
};

struct HeartTempoConfig
{
  float minBpm = 40;          // readings outside [minBpm, maxBpm] count as invalid
  float maxBpm = 180;
  float startBpm = 90;        // clock tempo before the first lock
  int historySize = 5;        // accepted readings the outlier gate takes its median over
  float gateK = 3;            // reject readings further than gateK robust sigmas from the median...
  float gateFloorBpm = 8;     // ...but never use a gate narrower than this
  int agreeCount = 3;         // readings out of the last agreeCount + 1 that must agree to
                              // lock, or to accept a sustained change the gate rejected
  float agreeTolBpm = 4;      // "agree" = within this distance of their median
  int invalidToNoFinger = 5;  // consecutive invalid readings before declaring finger-off
  float tauSec = 25;          // smoothing time constant of the heart rate estimate
  float maxDtSec = 10;        // cap on the smoothing step, so long gaps don't cause a jump
  float maxSlewBpmPerSec = 1; // hard cap on how fast the clock tempo moves
};

class HeartTempo
{
public:
  explicit HeartTempo(const HeartTempoConfig &cfg = HeartTempoConfig());

  // Feed one sensor poll. hr <= 0 (the sensor reports -1) or out of range is invalid.
  void onReading(int hr, uint32_t nowMs);
  // The sensor did not answer on the bus.
  void onSensorMissing();
  // Advance the tempo glide; call regularly. Returns the clock tempo in BPM.
  float update(uint32_t nowMs);

  float tempo() const { return tempo_; }
  float heartRate() const { return estimate_; }
  HeartState state() const { return state_; }

private:
  static const int kMaxBuf = 9;

  struct Buf
  {
    float v[kMaxBuf];
    int n = 0;
    void push(float x, int cap);
    void clear() { n = 0; }
    float median() const;
    // Number of values within tol of the median; their mean goes to *mean.
    int agreeing(float tol, float *mean) const;
  };

  void smooth(float hr, uint32_t nowMs);
  // Push to candidates_ and, if enough of them agree, make them the new history.
  bool tryConsensus(float hr, uint32_t nowMs);

  HeartTempoConfig cfg_;
  HeartState state_ = HeartState::NoFinger;
  Buf history_;    // accepted readings
  Buf candidates_; // readings waiting for agreement (acquiring, or rejected while locked)
  int invalidRun_ = 0;
  float estimate_;
  float tempo_;
  bool smoothed_ = false;
  uint32_t lastSmoothMs_ = 0;
  bool updated_ = false;
  uint32_t lastUpdateMs_ = 0;
};
