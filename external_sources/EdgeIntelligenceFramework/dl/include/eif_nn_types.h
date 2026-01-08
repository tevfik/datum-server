/**
 * @file eif_nn_types.h
 * @brief Neural Network Core Types
 * 
 * Tensor types, shapes, and fundamental data structures.
 */

#ifndef EIF_NN_TYPES_H
#define EIF_NN_TYPES_H

#include "eif_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Tensor Data Types
// ============================================================================

typedef enum {
    EIF_TENSOR_FLOAT32,
    EIF_TENSOR_INT8,
    EIF_TENSOR_UINT8
} eif_tensor_type_t;

// ============================================================================
// Tensor Structure
// ============================================================================

/**
 * @brief Tensor descriptor
 * 
 * Describes shape, type, and storage of a tensor.
 * - For constants (weights): `data` points directly to weight data
 * - For variables (activations): `data` is NULL, resolved via arena_offset
 */
typedef struct {
    eif_tensor_type_t type;
    int dims[4];           ///< Up to 4 dimensions (N, H, W, C)
    int num_dims;
    void* data;            ///< Constant data, or NULL for arena-managed
    size_t size_bytes;     ///< Size in bytes
    size_t arena_offset;   ///< Offset into tensor arena (for variables)
    bool is_variable;      ///< True if arena-managed
} eif_tensor_t;

// ============================================================================
// Tensor Utilities
// ============================================================================

/**
 * @brief Calculate total number of elements in tensor
 */
static inline int eif_tensor_numel(const eif_tensor_t* t) {
    int n = 1;
    for (int i = 0; i < t->num_dims; i++) n *= t->dims[i];
    return n;
}

/**
 * @brief Get size of tensor type in bytes
 */
static inline size_t eif_tensor_type_size(eif_tensor_type_t type) {
    switch (type) {
        case EIF_TENSOR_FLOAT32: return sizeof(float32_t);
        case EIF_TENSOR_INT8:
        case EIF_TENSOR_UINT8:   return 1;
        default:                  return 0;
    }
}

// ============================================================================
// Tensor Shape (Legacy Compatibility)
// ============================================================================

typedef struct {
    uint16_t dim[4]; ///< N, H, W, C
} eif_tensor_shape_t;

// ============================================================================
// Weight Data
// ============================================================================

typedef struct {
    const float32_t* data;
    size_t count;
} eif_weight_data_t;

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_TYPES_H
