#include "../../dsp/include/eif_opus.h"
#include "../framework/eif_test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define assertion macros if missing
#ifndef TEST_ASSERT_TRUE
#define TEST_ASSERT_TRUE(condition) TEST_ASSERT(condition)
#endif
#ifndef TEST_ASSERT_FALSE
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))
#endif
#ifndef TEST_ASSERT_GREATER_THAN
#define TEST_ASSERT_GREATER_THAN(threshold, value)                             \
  TEST_ASSERT((value) > (threshold))
#endif

#ifndef TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL(expected, actual)                                    \
  TEST_ASSERT_EQUAL_INT(expected, actual)
#endif

bool test_opus_lifecycle(void) {
  eif_opus_t opus;
  eif_status_t status = eif_opus_init(&opus, 16000, 1, EIF_OPUS_APP_VOIP);

  TEST_ASSERT_EQUAL(EIF_STATUS_OK, status);
  TEST_ASSERT_TRUE(opus.initialized);

  eif_opus_destroy(&opus);
  TEST_ASSERT_FALSE(opus.initialized);
  return true;
}

bool test_opus_encode_decode(void) {
  eif_opus_t opus;
  eif_opus_init(&opus, 16000, 1, EIF_OPUS_APP_VOIP);

  // Set complexity
  eif_status_t status =
      eif_opus_set_complexity(&opus, 0); // Low complexity for test
  TEST_ASSERT_EQUAL(EIF_STATUS_OK, status);

  // Dummy PCM (16kHz, 20ms frame = 320 samples)
  int frame_size = 320;
  int16_t input_pcm[320];
  for (int i = 0; i < frame_size; i++)
    input_pcm[i] = i * 10;

  uint8_t packet[100];
  int bytes = eif_opus_encode(&opus, input_pcm, frame_size, packet, 100);

  // Mock should return > 0 (our mock returns 10)
  TEST_ASSERT_GREATER_THAN(0, bytes);

  int16_t output_pcm[320];
  int samples = eif_opus_decode(&opus, packet, bytes, output_pcm, frame_size);

  // Mock should return frame_size
  TEST_ASSERT_EQUAL(frame_size, samples);

  eif_opus_destroy(&opus);
  return true;
}

int run_opus_integration_tests(void) {
  int failed = 0;
  if (!test_opus_lifecycle()) {
    printf("test_opus_lifecycle FAILED\n");
    failed++;
  }
  if (!test_opus_encode_decode()) {
    printf("test_opus_encode_decode FAILED\n");
    failed++;
  }
  return failed;
}

int main(void) {
  printf("Running Opus Integration Tests...\n");
  int failed = run_opus_integration_tests();
  if (failed == 0) {
    printf("ALL TESTS PASSED\n");
    return 0;
  } else {
    printf("%d TESTS FAILED\n", failed);
    return 1;
  }
}
