#ifndef EIF_NLP_EMBEDDING_FIXED_H
#define EIF_NLP_EMBEDDING_FIXED_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_fixedpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

// Limits
#define EIF_EMBEDDING_MAX_VOCAB 1000
#define EIF_EMBEDDING_DIM 50 

/**
 * @brief Simple Word Embedding Model (Q7 Quantized)
 * 
 * Stores words and associated 8-bit vectors.
 */
typedef struct {
    char** vocab;           ///< Array of strings
    q7_t* vectors;          ///< Flat array: [vocab_size][dim]
    int vocab_size;
    int dim;
} eif_embedding_fixed_t;

/**
 * @brief Initialize empty fixed-point embedding model
 */
eif_status_t eif_word_embedding_init_fixed(eif_embedding_fixed_t* model, int vocab_capacity, int dim);

/**
 * @brief Add word vector (converts float inputs to Q7)
 */
eif_status_t eif_embedding_add_fixed(eif_embedding_fixed_t* model, const char* word, const float* vector);

/**
 * @brief Add word vector (direct Q7 input)
 */
eif_status_t eif_embedding_add_q7(eif_embedding_fixed_t* model, const char* word, const q7_t* vector);

/**
 * @brief Get Q7 vector for word
 */
const q7_t* eif_embedding_get_fixed(const eif_embedding_fixed_t* model, const char* word);

/**
 * @brief Calculate Cosine Similarity between two words (returns float for ease of use)
 */
float eif_embedding_similarity_fixed(const eif_embedding_fixed_t* model, const char* word1, const char* word2);

/**
 * @brief Convert float vector to Q7
 */
void eif_embedding_quantize_vec(const float* src, q7_t* dst, int dim);

/**
 * @brief Free resources
 */
void eif_word_embedding_free_fixed(eif_embedding_fixed_t* model);

#ifdef __cplusplus
}
#endif

#endif // EIF_NLP_EMBEDDING_FIXED_H
