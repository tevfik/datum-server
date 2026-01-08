#include "eif_nlp_embedding_fixed.h"
#include "eif_hal_simd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

eif_status_t eif_word_embedding_init_fixed(eif_embedding_fixed_t* model, int vocab_capacity, int dim) {
    if (!model || vocab_capacity <= 0 || dim <= 0) return EIF_STATUS_ERROR;
    
    model->vocab = (char**)malloc(vocab_capacity * sizeof(char*));
    model->vectors = (q7_t*)malloc(vocab_capacity * dim * sizeof(q7_t));
    model->vocab_size = 0;
    model->dim = dim;
    
    if (!model->vocab || !model->vectors) {
        eif_word_embedding_free_fixed(model);
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    return EIF_STATUS_OK;
}

void eif_embedding_quantize_vec(const float* src, q7_t* dst, int dim) {
    for (int i = 0; i < dim; i++) {
        // Clamp to -1.0 .. 1.0 then scale
        float val = src[i];
        if (val > 1.0f) val = 1.0f;
        if (val < -1.0f) val = -1.0f;
        dst[i] = (q7_t)(val * 127.0f);
    }
}

eif_status_t eif_embedding_add_fixed(eif_embedding_fixed_t* model, const char* word, const float* vector) {
    if (!model || !word || !vector) return EIF_STATUS_ERROR;
    
    q7_t* temp_vec = (q7_t*)malloc(model->dim * sizeof(q7_t));
    if (!temp_vec) return EIF_STATUS_OUT_OF_MEMORY;
    
    eif_embedding_quantize_vec(vector, temp_vec, model->dim);
    
    eif_status_t status = eif_embedding_add_q7(model, word, temp_vec);
    free(temp_vec);
    return status;
}

eif_status_t eif_embedding_add_q7(eif_embedding_fixed_t* model, const char* word, const q7_t* vector) {
    if (!model || !word || !vector) return EIF_STATUS_ERROR;
    
    model->vocab[model->vocab_size] = strdup(word);
    memcpy(&model->vectors[model->vocab_size * model->dim], vector, model->dim * sizeof(q7_t));
    model->vocab_size++;
    
    return EIF_STATUS_OK;
}

const q7_t* eif_embedding_get_fixed(const eif_embedding_fixed_t* model, const char* word) {
    if (!model || !word) return NULL;
    
    for (int i = 0; i < model->vocab_size; i++) {
        if (strcmp(model->vocab[i], word) == 0) {
            return &model->vectors[i * model->dim];
        }
    }
    return NULL;
}

float eif_embedding_similarity_fixed(const eif_embedding_fixed_t* model, const char* word1, const char* word2) {
    const q7_t* v1 = eif_embedding_get_fixed(model, word1);
    const q7_t* v2 = eif_embedding_get_fixed(model, word2);
    
    if (!v1 || !v2) return -2.0f; 

    // Q7 dot product returns int32
    int32_t dot = eif_simd_dot_q7(v1, v2, model->dim);
    int32_t mag1 = eif_simd_dot_q7(v1, v1, model->dim);
    int32_t mag2 = eif_simd_dot_q7(v2, v2, model->dim);
    
    if (mag1 == 0 || mag2 == 0) return 0.0f;
    
    // Scale back to float for probability interpretation
    // Magnitudes are sum(q7*q7), so order of magnitude is dim * 127*127
    // Dot is sum(q7*q7)
    // Division naturally normalizes the scale factors
    
    return (float)dot / (sqrtf((float)mag1) * sqrtf((float)mag2));
}

void eif_word_embedding_free_fixed(eif_embedding_fixed_t* model) {
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
