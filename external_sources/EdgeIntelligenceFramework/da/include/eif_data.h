#ifndef EIF_DATA_H
#define EIF_DATA_H

#include "eif_types.h"
#include "eif_status.h"

// Calculate Mean
eif_status_t eif_data_mean(const float32_t* data, size_t length, float32_t* result);

// Calculate Variance
eif_status_t eif_data_variance(const float32_t* data, size_t length, float32_t* result);

// Find Min/Max
eif_status_t eif_data_minmax(const float32_t* data, size_t length, float32_t* min_val, float32_t* max_val);

#endif // EIF_DATA_H
