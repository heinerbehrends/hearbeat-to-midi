#include <unity.h>
#include <HeartTempo.h>
#include <Switch3.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

static const int INVALID = -1;

// Simulated clock: one sensor poll every 4 s, tempo updated every second in between.
static uint32_t now;

static void poll(HeartTempo &ht, int hr)
{
  ht.onReading(hr, now);
  for (int i = 0; i < 4; i++)
  {
    now += 1000;
    ht.update(now);
  }
}

static void pollMany(HeartTempo &ht, int hr, int count)
{
  for (int i = 0; i < count; i++)
    poll(ht, hr);
}

void setUp(void)
{
  now = 0;
}

void tearDown(void) {}

void test_starts_at_start_tempo_without_finger(void)
{
  HeartTempo ht;
  TEST_ASSERT_EQUAL(HeartState::NoFinger, ht.state());
  TEST_ASSERT_EQUAL_FLOAT(90, ht.update(now));
}

void test_steady_reading_locks_and_converges(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 2);
  TEST_ASSERT_EQUAL(HeartState::Acquiring, ht.state());
  poll(ht, 70);
  TEST_ASSERT_EQUAL(HeartState::Locked, ht.state());

  pollMany(ht, 70, 45); // 3 minutes
  TEST_ASSERT_FLOAT_WITHIN(0.5, 70, ht.tempo());
}

void test_reacts_slowly(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 5); // 20 s after the first reading, 12 s after locking
  // Less than 60% of the way from the 90 start to 70
  TEST_ASSERT_TRUE(ht.tempo() > 78);
}

void test_disagreeing_readings_do_not_lock(void)
{
  HeartTempo ht;
  for (int i = 0; i < 10; i++)
    poll(ht, i % 2 ? 70 : 100);
  TEST_ASSERT_EQUAL(HeartState::Acquiring, ht.state());
  TEST_ASSERT_EQUAL_FLOAT(90, ht.tempo());
}

void test_single_spike_is_ignored(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 60);
  float before = ht.heartRate();
  poll(ht, 120);
  TEST_ASSERT_FLOAT_WITHIN(0.01, before, ht.heartRate());
  pollMany(ht, 70, 5);
  TEST_ASSERT_FLOAT_WITHIN(0.5, 70, ht.tempo());
}

void test_sustained_change_is_followed(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 60);
  pollMany(ht, 100, 60); // 4 minutes at the new rate
  TEST_ASSERT_EQUAL(HeartState::Locked, ht.state());
  TEST_ASSERT_FLOAT_WITHIN(0.5, 100, ht.tempo());
}

void test_tempo_change_stays_below_perception_threshold(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 60);
  float start = ht.tempo();
  float prev = start;
  for (int i = 0; i < 40; i++)
  {
    ht.onReading(150, now);
    for (int s = 0; s < 4; s++)
    {
      now += 1000;
      float t = ht.update(now);
      float beats = prev / 60; // 1 s worth of beats
      // Holds until 3 readings agree on the jump, then only rises, never too fast
      TEST_ASSERT_TRUE(t >= prev);
      TEST_ASSERT_TRUE((t - prev) / prev <= 0.0015f * beats * 1.001f);
      prev = t;
    }
  }
  TEST_ASSERT_TRUE(prev > start + 10);
}

void test_short_gaps_keep_lock(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 10);
  pollMany(ht, INVALID, 4);
  TEST_ASSERT_EQUAL(HeartState::Locked, ht.state());
}

void test_finger_off_holds_tempo(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 10);
  pollMany(ht, INVALID, 5);
  TEST_ASSERT_EQUAL(HeartState::NoFinger, ht.state());

  float held = ht.tempo();
  pollMany(ht, INVALID, 30);
  TEST_ASSERT_EQUAL_FLOAT(held, ht.tempo());
}

void test_finger_back_needs_agreement_again(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 60);
  pollMany(ht, INVALID, 5);
  poll(ht, 110);
  poll(ht, 75);
  TEST_ASSERT_EQUAL(HeartState::Acquiring, ht.state());
  TEST_ASSERT_FLOAT_WITHIN(0.5, 70, ht.tempo());
}

void test_sensor_missing_holds_tempo(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 60);
  float held = ht.tempo();
  for (int i = 0; i < 10; i++)
  {
    ht.onSensorMissing();
    now += 4000;
    ht.update(now);
  }
  TEST_ASSERT_EQUAL(HeartState::NoSensor, ht.state());
  TEST_ASSERT_EQUAL_FLOAT(held, ht.tempo());
}

void test_last_reading_flag_follows_each_reading(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 2);
  TEST_ASSERT_FALSE(ht.lastReadingAccepted()); // acquiring
  poll(ht, 70);
  TEST_ASSERT_TRUE(ht.lastReadingAccepted()); // the locking reading counts
  pollMany(ht, 70, 10);

  poll(ht, INVALID);
  TEST_ASSERT_EQUAL(HeartState::Locked, ht.state());
  TEST_ASSERT_FALSE(ht.lastReadingAccepted());
  poll(ht, 71);
  TEST_ASSERT_TRUE(ht.lastReadingAccepted());

  poll(ht, 130); // rejected by the gate
  TEST_ASSERT_EQUAL(HeartState::Locked, ht.state());
  TEST_ASSERT_FALSE(ht.lastReadingAccepted());
  poll(ht, 70);
  TEST_ASSERT_TRUE(ht.lastReadingAccepted());
}

void test_feedback_steers_and_returns(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 60);
  ht.setFeedback(-1);
  pollMany(ht, 70, 30); // 2 minutes
  TEST_ASSERT_FLOAT_WITHIN(0.5, 65, ht.tempo());
  TEST_ASSERT_FLOAT_WITHIN(0.5, 70, ht.heartRate()); // only the target moves

  ht.setFeedback(0);
  pollMany(ht, 70, 30);
  TEST_ASSERT_FLOAT_WITHIN(0.5, 70, ht.tempo());

  ht.setFeedback(1);
  pollMany(ht, 70, 30);
  TEST_ASSERT_FLOAT_WITHIN(0.5, 75, ht.tempo());
}

static float steeredTarget(int hr, int direction)
{
  HeartTempo ht;
  pollMany(ht, hr, 120); // 8 minutes: the estimate has settled on hr
  ht.setFeedback(direction);
  return ht.target();
}

void test_feedback_clamps_without_pushing_against_the_switch(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.1, 55, steeredTarget(58, -1));
  TEST_ASSERT_FLOAT_WITHIN(0.1, 50, steeredTarget(50, -1));
  TEST_ASSERT_FLOAT_WITHIN(0.1, 130, steeredTarget(128, 1));
  TEST_ASSERT_FLOAT_WITHIN(0.1, 135, steeredTarget(135, 1));
}

void test_feedback_glide_stays_below_perception_threshold(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 60);
  ht.setFeedback(1);
  float prev = ht.tempo();
  for (int s = 0; s < 60; s++)
  {
    now += 1000;
    float t = ht.update(now);
    TEST_ASSERT_TRUE((t - prev) / prev <= 0.0015f * (prev / 60) * 1.001f);
    prev = t;
  }
}

void test_feedback_does_nothing_until_locked(void)
{
  HeartTempo ht;
  ht.setFeedback(1);
  pollMany(ht, INVALID, 15);
  TEST_ASSERT_EQUAL_FLOAT(90, ht.tempo());
}

void test_rate_change_is_continuous_and_applies(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 60);
  float before = ht.tempo();
  ht.setRate(0.004f, 12); // fast preset
  TEST_ASSERT_EQUAL_FLOAT(before, ht.tempo());

  // A step to 100 now glides at up to 0.4 %/beat: well past what medium allows in 20 s
  pollMany(ht, 100, 5);
  float fastTempo = ht.tempo();

  HeartTempo medium;
  pollMany(medium, 70, 60);
  pollMany(medium, 100, 5);
  TEST_ASSERT_TRUE(fastTempo > medium.tempo() + 2);
}

void test_switch_first_read_is_immediate(void)
{
  Switch3 sw(30);
  TEST_ASSERT_TRUE(sw.update(true, false, 0));
  TEST_ASSERT_EQUAL(-1, sw.position());
}

void test_switch_ignores_bounce(void)
{
  Switch3 sw(30);
  sw.update(false, false, 0);
  // Contact chatters while closing
  TEST_ASSERT_FALSE(sw.update(false, true, 5));
  TEST_ASSERT_FALSE(sw.update(false, false, 10));
  TEST_ASSERT_FALSE(sw.update(false, true, 15));
  TEST_ASSERT_FALSE(sw.update(false, true, 40));
  TEST_ASSERT_EQUAL(0, sw.position());
  TEST_ASSERT_TRUE(sw.update(false, true, 45)); // stable for 30 ms
  TEST_ASSERT_EQUAL(1, sw.position());
}

void test_switch_flip_across_centre_reports_only_the_end(void)
{
  Switch3 sw(30);
  sw.update(true, false, 0);
  sw.update(false, false, 100); // passing through the centre for 10 ms
  sw.update(false, true, 110);
  TEST_ASSERT_EQUAL(-1, sw.position());
  TEST_ASSERT_TRUE(sw.update(false, true, 140));
  TEST_ASSERT_EQUAL(1, sw.position());
}

void test_switch_both_closed_reads_centre(void)
{
  Switch3 sw(30);
  sw.update(true, true, 0);
  TEST_ASSERT_EQUAL(0, sw.position());
}

// Real readings captured from the sensor, finger on, 4 s polling
void test_recorded_session_locks_and_ignores_spike(void)
{
  const int rec[] = {95, INVALID, 98, INVALID, 116, INVALID, INVALID, INVALID, INVALID, 99, INVALID};
  HeartTempo ht;
  for (int hr : rec)
    poll(ht, hr);
  TEST_ASSERT_EQUAL(HeartState::Locked, ht.state());
  TEST_ASSERT_TRUE(ht.heartRate() > 90 && ht.heartRate() < 100);
}

static void runTests()
{
  UNITY_BEGIN();
  RUN_TEST(test_starts_at_start_tempo_without_finger);
  RUN_TEST(test_steady_reading_locks_and_converges);
  RUN_TEST(test_reacts_slowly);
  RUN_TEST(test_disagreeing_readings_do_not_lock);
  RUN_TEST(test_single_spike_is_ignored);
  RUN_TEST(test_sustained_change_is_followed);
  RUN_TEST(test_tempo_change_stays_below_perception_threshold);
  RUN_TEST(test_short_gaps_keep_lock);
  RUN_TEST(test_finger_off_holds_tempo);
  RUN_TEST(test_finger_back_needs_agreement_again);
  RUN_TEST(test_sensor_missing_holds_tempo);
  RUN_TEST(test_last_reading_flag_follows_each_reading);
  RUN_TEST(test_feedback_steers_and_returns);
  RUN_TEST(test_feedback_clamps_without_pushing_against_the_switch);
  RUN_TEST(test_feedback_glide_stays_below_perception_threshold);
  RUN_TEST(test_feedback_does_nothing_until_locked);
  RUN_TEST(test_rate_change_is_continuous_and_applies);
  RUN_TEST(test_switch_first_read_is_immediate);
  RUN_TEST(test_switch_ignores_bounce);
  RUN_TEST(test_switch_flip_across_centre_reports_only_the_end);
  RUN_TEST(test_switch_both_closed_reads_centre);
  RUN_TEST(test_recorded_session_locks_and_ignores_spike);
  UNITY_END();
}

#ifdef ARDUINO
void setup()
{
  Serial.begin(115200);
}

// Repeat the run, so a monitor opened at any time catches a complete one.
void loop()
{
  delay(5000);
  runTests();
}
#else
int main()
{
  runTests();
  return 0;
}
#endif
