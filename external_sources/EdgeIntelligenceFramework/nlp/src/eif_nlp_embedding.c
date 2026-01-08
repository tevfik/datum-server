#include "eif_nlp_embedding.h"
#include "eif_hal_simd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

eif_status_t eif_word_embedding_init(eif_embedding_t* model, int vocab_capacity, int dim) {
    if (!model || vocab_capacity <= 0 || dim <= 0) return EIF_STATUS_ERROR;
    
    model->vocab = (char**)malloc(vocab_capacity * sizeof(char*));
    model->vectors = (float*)malloc(vocab_capacity * dim * sizeof(float));
    model->vocab_size = 0;
    model->dim = dim;
    
    if (!model->vocab || !model->vectors) {
        eif_word_embedding_free(model);
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_embedding_add(eif_embedding_t* model, const char* word, const float* vector) {
    if (!model || !word || !vector) return EIF_STATUS_ERROR;
    
    // Check dupe? (Skipped for performance/simplicity)
    
    model->vocab[model->vocab_size] = strdup(word);
    memcpy(&model->vectors[model->vocab_size * model->dim], vector, model->dim * sizeof(float));
    model->vocab_size++;
    
    return EIF_STATUS_OK;
}

const float* eif_embedding_get(const eif_embedding_t* model, const char* word) {
    if (!model || !word) return NULL;
    
    for (int i = 0; i < model->vocab_size; i++) {
        if (strcmp(model->vocab[i], word) == 0) {
            return &model->vectors[i * model->dim];
        }
    }
    return NULL;
}

float eif_vector_cosine_similarity(const float* v1, const float* v2, int dim) {
    if (!v1 || !v2) return 0.0f;
    
    float dot = eif_simd_dot_f32(v1, v2, dim);
    float mag1 = eif_simd_dot_f32(v1, v1, dim);
    float mag2 = eif_simd_dot_f32(v2, v2, dim);
    
    if (mag1 == 0 || mag2 == 0) return 0.0f;
    
    return dot / (sqrtf(mag1) * sqrtf(mag2));
}

float eif_embedding_similarity(const eif_embedding_t* model, const char* word1, const char* word2) {
    const float* v1 = eif_embedding_get(model, word1);
    const float* v2 = eif_embedding_get(model, word2);
    
    if (!v1 || !v2) return -2.0f; // Error code (cosine sim is [-1, 1])
    
    return eif_vector_cosine_similarity(v1, v2, model->dim);
}

void eif_word_embedding_free(eif_embedding_t* model) {
    if (model) {
        if (model->vocab) {
            for (int i = 0; i < model->vocab_size; i++) {
                free(model->vocab[i]);
            }
            free(model->vocab);
        }
        if (model->vectors) free(model->vectors);
        model->vocab = NULL;
        model->vectors = NULL;
        model->vocab_size = 0;
    }
}
