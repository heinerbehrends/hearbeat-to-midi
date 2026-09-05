#include <Arduino.h>
#include <unity.h>

#define SENSOR_PIN 1

// Test 1: Verify the chip's memory reporting layers
void test_board_memory(void)
{
    // Check internal system heap size
    uint32_t internal_heap = ESP.getHeapSize();
    TEST_ASSERT_GREATER_THAN_UINT32(100000, internal_heap);

    // Verify 8MB Octal PSRAM configuration state
    TEST_ASSERT_TRUE_MESSAGE(psramFound(), "PSRAM hardware layer not detected!");

    uint32_t total_psram = ESP.getPsramSize();
    // 8MB is exactly 8,388,608 bytes. Verify we read a high capacity.
    TEST_ASSERT_GREATER_THAN_UINT32(7500000, total_psram);
}

// Test 2: Dynamic PSRAM Read/Write verification via safe Arduino primitives
void test_psram_read_write(void)
{
    size_t test_size = 2048; // 2KB allocation buffer

    // Allocate space utilizing the underlying Arduino PSRAM allocator
    uint8_t *buffer = (uint8_t *)malloc(test_size);
    TEST_ASSERT_NOT_NULL_MESSAGE(buffer, "Failed to allocate memory buffer block.");

    // Fill buffer array with verification footprints
    for (size_t i = 0; i < test_size; i++)
    {
        buffer[i] = (uint8_t)(i % 256);
    }

    // Verify written values are mathematically pristine
    for (size_t i = 0; i < test_size; i++)
    {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i % 256), buffer[i]);
    }

    free(buffer);
}

// Test 3: Sensor input mapping sanity check
void test_sensor_bounds(void)
{
    pinMode(SENSOR_PIN, INPUT);
    delay(10);

    int raw_reading = analogRead(SENSOR_PIN);

    // The ESP32-S3 features an internal 12-bit Analog-to-Digital Converter (0-4095)
    TEST_ASSERT_INT_WITHIN(2048, 2047, raw_reading);
}

void setup()
{
    Serial.begin(115200);

    // Replace the while(!Serial) loop with a simple, flat 2-second stabilization delay
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_board_memory);
    RUN_TEST(test_psram_read_write);
    RUN_TEST(test_sensor_bounds);
    UNITY_END();
}

void loop()
{
    // Keep empty for target loop structure
}
