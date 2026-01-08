/**
 * @file eif_dsp_smooth.c
 * @brief Implementation of smoothing filters
 */

#include "eif_dsp_smooth.h"
#include <string.h>

// =============================================================================
// Median Filter Implementation
// =============================================================================

void eif_median_init(eif_median_t *mf, int size) {
  mf->size = (size < 3) ? 3 : ((size > 7) ? 7 : size);
  // Force odd size
  if (mf->size % 2 == 0)
    mf->size--;
  mf->index = 0;
  mf->count = 0;
  memset(mf->buffer, 0, sizeof(mf->buffer));
}

// Simple insertion sort for small arrays
static void sort_floats(float *arr, int n) {
  for (int i = 1; i < n; i++) {
    float key = arr[i];
    int j = i - 1;
    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }
    arr[j + 1] = key;
  }
}

float eif_median_update(eif_median_t *mf, float input) {
  // Add to circular buffer
  mf->buffer[mf->index] = input;
  mf->index = (mf->index + 1) % mf->size;
  if (mf->count < mf->size)
    mf->count++;

  // Copy and sort
  float sorted[7];
  int n = mf->count;
  for (int i = 0; i < n; i++) {
    sorted[i] = mf->buffer[i];
  }
  sort_floats(sorted, n);

  // Return median
  return sorted[n / 2];
}

void eif_median_reset(eif_median_t *mf) {
  mf->index = 0;
  mf->count = 0;
  memset(mf->buffer, 0, sizeof(mf->buffer));
}

// =============================================================================
// Moving Average Implementation
// =============================================================================

void eif_ma_init(eif_ma_t *ma, int size) {
  ma->size = (size < 1) ? 1 : ((size > 16) ? 16 : size);
  ma->index = 0;
  ma->count = 0;
  ma->sum = 0.0f;
  memset(ma->buffer, 0, sizeof(ma->buffer));
}

float eif_ma_update(eif_ma_t *ma, float input) {
  // Remove oldest from sum if buffer is full
  if (ma->count >= ma->size) {
    ma->sum -= ma->buffer[ma->index];
  }

  // Add new sample
  ma->buffer[ma->index] = input;
  ma->sum += input;
  ma->index = (ma->index + 1) % ma->size;
  if (ma->count < ma->size)
    ma->count++;

  return ma->sum / ma->count;
}

void eif_ma_reset(eif_ma_t *ma) {
  ma->index = 0;
  ma->count = 0;
  ma->sum = 0.0f;
  memset(ma->buffer, 0, sizeof(ma->buffer));
}
