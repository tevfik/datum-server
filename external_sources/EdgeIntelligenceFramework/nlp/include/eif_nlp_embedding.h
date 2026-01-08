#ifndef EIF_NLP_EMBEDDING_H
#define EIF_NLP_EMBEDDING_H

#include "eif_types.h"
#include "eif_status.h"

#ifdef __cplusplus
extern "C" {
#endif

// Limits
#define EIF_EMBEDDING_MAX_VOCAB 1000
#define EIF_EMBEDDING_DIM 50 

/**
 * @brief Simple Word Embedding Model
 * 
 * Stores a small dictionary and associated vectors.
 * Designed for command-word classification or few-shot intent detection.
 */
typedef struct {
    char** vocab;           ///< Array of strings
    float* vectors;         ///< Flat array: [vocab_size][dim]
    int vocab_size;
    int dim;
} eif_embedding_t;

/**
 * @brief Initialize empty embedding model
 */
eif_status_t eif_word_embedding_init(eif_embedding_t* model, int vocab_capacity, int dim);

/**
 * @brief Add word vector
 */
eif_status_t eif_embedding_add(eif_embedding_t* model, const char* word, const float* vector);

/**
 * @brief Get vector for word
 */
const float* eif_embedding_get(const eif_embedding_t* model, const char* word);

/**
 * @brief Calculate Cosine Similarity between two words
 */
float eif_embedding_similarity(const eif_embedding_t* model, const char* word1, const char* word2);

/**
 * @brief Calculate Cosine Similarity between two vectors
 */
float eif_vector_cosine_similarity(const float* v1, const float* v2, int dim);

/**
 * @brief Free resources
 */
void eif_word_embedding_free(eif_embedding_t* model);

#ifdef __cplusplus
}
#endif

#endif // EIF_NLP_EMBEDDING_H
