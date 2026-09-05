#include <Arduino.h>
#include <unity.h>

void test_example(void)
{
    TEST_ASSERT_EQUAL(1, 1);
}

void setup()
{
    // 1. Initialize the serial hardware pipeline
    Serial.begin(115200);

    // 2. CRITICAL NATIVE USB LOOP: Wait up to 5 seconds for Ubuntu to link up
    // before allowing Unity to print anything to the screen
    unsigned long start_time = millis();
    while (!Serial && (millis() - start_time < 5000))
    {
        delay(10);
    }

    // A small buffer delay to ensure stability
    delay(500);

    UNITY_BEGIN();
    RUN_TEST(test_example);
    UNITY_END();
}

void loop()
{
    // Empty
}
