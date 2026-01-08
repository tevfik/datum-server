/**
 * @file eif_dsp_smooth_fixed.c
 * @brief Fixed-Point Smoothing Implementation
 */

#include "eif_dsp_smooth_fixed.h"
#include <string.h>

// --- Median Filter ---
void eif_median_q15_init(eif_median_q15_t *mf, int size) {
  if (!mf)
    return;
  mf->size = (size < 3) ? 3 : ((size > 7) ? 7 : size);
  if (mf->size % 2 == 0)
    mf->size--; // Force odd
  eif_median_q15_reset(mf);
}

void eif_median_q15_reset(eif_median_q15_t *mf) {
  if (!mf)
    return;
  mf->index = 0;
  mf->count = 0;
  memset(mf->buffer, 0, sizeof(mf->buffer));
}

// Insertion sort
static void sort_q15(q15_t *arr, int n) {
  for (int i = 1; i < n; i++) {
    q15_t key = arr[i];
    int j = i - 1;
    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }
    arr[j + 1] = key;
  }
}

q15_t eif_median_q15_update(eif_median_q15_t *mf, q15_t input) {
  if (!mf)
    return 0;

  mf->buffer[mf->index] = input;
  mf->index = (mf->index + 1) % mf->size;
  if (mf->count < mf->size)
    mf->count++;

  q15_t sorted[7];
  int n = mf->count;
  for (int i = 0; i < n; i++)
    sorted[i] = mf->buffer[i];

  sort_q15(sorted, n);

  return sorted[n / 2];
}

// --- Moving Average ---
void eif_ma_q15_init(eif_ma_q15_t *ma, int size) {
  if (!ma)
    return;
  ma->size = (size < 1) ? 1 : ((size > 16) ? 16 : size);
  eif_ma_q15_reset(ma);
}

void eif_ma_q15_reset(eif_ma_q15_t *ma) {
  if (!ma)
    return;
  ma->index = 0;
  ma->count = 0;
  ma->sum = 0;
  memset(ma->buffer, 0, sizeof(ma->buffer));
}

q15_t eif_ma_q15_update(eif_ma_q15_t *ma, q15_t input) {
  if (!ma)
    return 0;

  if (ma->count >= ma->size) {
    ma->sum -= ma->buffer[ma->index];
  }

  ma->buffer[ma->index] = input;
  ma->sum += input;
  ma->index = (ma->index + 1) % ma->size;
  if (ma->count < ma->size)
    ma->count++;

  return (q15_t)(ma->sum / ma->count);
}
