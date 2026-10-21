#include <unity.h>
#include <HeartTempo.h>

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

void test_tempo_never_moves_faster_than_slew_limit(void)
{
  HeartTempo ht;
  pollMany(ht, 70, 60);
  float prev = ht.tempo();
  for (int i = 0; i < 40; i++)
  {
    ht.onReading(150, now);
    for (int s = 0; s < 4; s++)
    {
      now += 1000;
      float t = ht.update(now);
      TEST_ASSERT_TRUE(t - prev <= 1.0f + 1e-4f);
      prev = t;
    }
  }
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
  RUN_TEST(test_tempo_never_moves_faster_than_slew_limit);
  RUN_TEST(test_short_gaps_keep_lock);
  RUN_TEST(test_finger_off_holds_tempo);
  RUN_TEST(test_finger_back_needs_agreement_again);
  RUN_TEST(test_sensor_missing_holds_tempo);
  RUN_TEST(test_recorded_session_locks_and_ignores_spike);
  UNITY_END();
}

#ifdef ARDUINO
void setup()
{
  Serial.begin(115200);
  delay(2000);
  runTests();
}

void loop() {}
#else
int main()
{
  runTests();
  return 0;
}
#endif
