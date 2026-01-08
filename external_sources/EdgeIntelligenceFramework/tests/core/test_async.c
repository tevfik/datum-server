/**
 * @file test_async.c
 * @brief Unit tests for eif_async.h async processing
 */

#include "../framework/eif_test_runner.h"
#include "eif_async.h"
#include <string.h>

// =============================================================================
// Async Handle Tests
// =============================================================================

bool test_async_init(void) {
  eif_async_handle_t handle;
  eif_async_init(&handle);

  TEST_ASSERT_EQUAL_INT(EIF_ASYNC_IDLE, handle.state);
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, handle.result);
  TEST_ASSERT_TRUE(handle.context == NULL);
  TEST_ASSERT_TRUE(handle.on_complete == NULL);
  return true;
}

bool test_async_done_idle(void) {
  eif_async_handle_t handle;
  eif_async_init(&handle);

  TEST_ASSERT_TRUE(!eif_async_done(&handle));
  return true;
}

bool test_async_done_complete(void) {
  eif_async_handle_t handle;
  eif_async_init(&handle);

  eif_async_complete(&handle, EIF_STATUS_OK);

  TEST_ASSERT_TRUE(eif_async_done(&handle));
  TEST_ASSERT_EQUAL_INT(EIF_ASYNC_COMPLETE, handle.state);
  return true;
}

bool test_async_done_error(void) {
  eif_async_handle_t handle;
  eif_async_init(&handle);

  eif_async_complete(&handle, EIF_STATUS_ERROR);

  TEST_ASSERT_TRUE(eif_async_done(&handle));
  TEST_ASSERT_EQUAL_INT(EIF_ASYNC_ERROR, handle.state);
  return true;
}

// Callback test helper
static bool callback_called = false;
static eif_status_t callback_status = EIF_STATUS_OK;

static void test_callback(void *context, eif_status_t status) {
  callback_called = true;
  callback_status = status;
}

bool test_async_callback_invoked(void) {
  callback_called = false;
  callback_status = EIF_STATUS_OK;

  eif_async_handle_t handle;
  eif_async_init(&handle);
  handle.on_complete = test_callback;

  eif_async_complete(&handle, EIF_STATUS_ERROR);

  TEST_ASSERT_TRUE(callback_called);
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_ERROR, callback_status);
  return true;
}

// =============================================================================
// Double Buffer Tests
// =============================================================================

bool test_double_buffer_init(void) {
  float buf_a[64];
  float buf_b[64];
  eif_double_buffer_t db;

  eif_status_t status =
      eif_double_buffer_init(&db, buf_a, buf_b, sizeof(buf_a));

  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
  TEST_ASSERT_TRUE(db.buffer_a == buf_a);
  TEST_ASSERT_TRUE(db.buffer_b == buf_b);
  TEST_ASSERT_EQUAL_INT(sizeof(buf_a), db.buffer_size);
  TEST_ASSERT_EQUAL_INT(0, db.active_buffer);
  return true;
}

bool test_double_buffer_init_null(void) {
  eif_double_buffer_t db;

  TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT,
                        eif_double_buffer_init(NULL, NULL, NULL, 0));
  return true;
}

bool test_double_buffer_swap(void) {
  float buf_a[64];
  float buf_b[64];
  eif_double_buffer_t db;

  eif_double_buffer_init(&db, buf_a, buf_b, sizeof(buf_a));

  TEST_ASSERT_EQUAL_INT(0, db.active_buffer);

  eif_double_buffer_swap(&db);
  TEST_ASSERT_EQUAL_INT(1, db.active_buffer);

  eif_double_buffer_swap(&db);
  TEST_ASSERT_EQUAL_INT(0, db.active_buffer);
  return true;
}

bool test_double_buffer_read_write(void) {
  float buf_a[4] = {1, 2, 3, 4};
  float buf_b[4] = {5, 6, 7, 8};
  eif_double_buffer_t db;

  eif_double_buffer_init(&db, buf_a, buf_b, sizeof(buf_a));

  // Initially: read from A, write to B
  void *read_buf = eif_double_buffer_get_read(&db);
  void *write_buf = eif_double_buffer_get_write(&db);

  TEST_ASSERT_TRUE(read_buf == buf_a);
  TEST_ASSERT_TRUE(write_buf == buf_b);

  // After swap: read from B, write to A
  eif_double_buffer_swap(&db);
  read_buf = eif_double_buffer_get_read(&db);
  write_buf = eif_double_buffer_get_write(&db);

  TEST_ASSERT_TRUE(read_buf == buf_b);
  TEST_ASSERT_TRUE(write_buf == buf_a);
  return true;
}

// =============================================================================
// DMA Tests
// =============================================================================

bool test_dma_memcpy_async(void) {
  uint8_t src[64];
  uint8_t dst[64];

  memset(src, 0xAB, sizeof(src));
  memset(dst, 0x00, sizeof(dst));

  eif_async_handle_t handle;
  eif_async_init(&handle);

  eif_status_t status = eif_dma_memcpy_async(dst, src, sizeof(src), &handle);

  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
  TEST_ASSERT_TRUE(eif_async_done(&handle));
  TEST_ASSERT_EQUAL_INT(0, memcmp(src, dst, sizeof(src)));
  return true;
}

bool test_dma_wait(void) {
  uint8_t src[32] = {1, 2, 3};
  uint8_t dst[32] = {0};

  eif_async_handle_t handle;
  eif_async_init(&handle);

  eif_dma_memcpy_async(dst, src, sizeof(src), &handle);
  eif_status_t status = eif_dma_wait(&handle, 1000);

  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
  return true;
}

// =============================================================================
// Test Suite
// =============================================================================

BEGIN_TEST_SUITE(run_async_tests)
RUN_TEST(test_async_init);
RUN_TEST(test_async_done_idle);
RUN_TEST(test_async_done_complete);
RUN_TEST(test_async_done_error);
RUN_TEST(test_async_callback_invoked);
RUN_TEST(test_double_buffer_init);
RUN_TEST(test_double_buffer_init_null);
RUN_TEST(test_double_buffer_swap);
RUN_TEST(test_double_buffer_read_write);
RUN_TEST(test_dma_memcpy_async);
RUN_TEST(test_dma_wait);
END_TEST_SUITE()
