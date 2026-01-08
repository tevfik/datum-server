/**
 * @file eif_el_fewshot.c
 * @brief Few-Shot Learning Implementation (Prototypical Networks)
 * 
 * Quick adaptation with minimal examples using:
 * - Prototype-based classification
 * - Metric learning (Euclidean/Cosine)
 * - Running mean prototype updates
 */

#include "eif_el.h"
#include <string.h>
#include <math.h>

// Forward declarations for utility functions
float32_t eif_euclidean_distance(const float32_t* a, const float32_t* b, int dim);
float32_t eif_cosine_similarity(const float32_t* a, const float32_t* b, int dim);

eif_status_t eif_fewshot_init(eif_fewshot_t* fs,
                               int max_classes,
                               int embed_dim,
                               eif_memory_pool_t* pool) {
    if (!fs || !pool || max_classes <= 0 || embed_dim <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    fs->max_classes = max_classes;
    fs->num_classes = 0;
    fs->embed_dim = embed_dim;
    fs->distance_type = EIF_DISTANCE_EUCLIDEAN;
    fs->pool = pool;
    
    fs->prototypes = (eif_prototype_t*)eif_memory_alloc(pool,
        max_classes * sizeof(eif_prototype_t), sizeof(void*));
    fs->embed_buffer = (float32_t*)eif_memory_alloc(pool,
        embed_dim * sizeof(float32_t), sizeof(float32_t));
    
    if (!fs->prototypes || !fs->embed_buffer) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    for (int i = 0; i < max_classes; i++) {
        fs->prototypes[i].embedding = (float32_t*)eif_memory_alloc(pool,
            embed_dim * sizeof(float32_t), sizeof(float32_t));
        if (!fs->prototypes[i].embedding) {
            return EIF_STATUS_OUT_OF_MEMORY;
        }
        memset(fs->prototypes[i].embedding, 0, embed_dim * sizeof(float32_t));
        fs->prototypes[i].embed_dim = embed_dim;
        fs->prototypes[i].num_samples = 0;
        fs->prototypes[i].class_id = -1;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_fewshot_add_example(eif_fewshot_t* fs,
                                      const float32_t* embedding,
                                      int class_id) {
    if (!fs || !embedding || class_id < 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    eif_prototype_t* proto = NULL;
    
    for (int i = 0; i < fs->num_classes; i++) {
        if (fs->prototypes[i].class_id == class_id) {
            proto = &fs->prototypes[i];
            break;
        }
    }
    
    if (!proto) {
        if (fs->num_classes >= fs->max_classes) {
            return EIF_STATUS_OUT_OF_MEMORY;
        }
        proto = &fs->prototypes[fs->num_classes++];
        proto->class_id = class_id;
        memset(proto->embedding, 0, fs->embed_dim * sizeof(float32_t));
        proto->num_samples = 0;
    }
    
    // Running mean update
    int n = proto->num_samples;
    for (int d = 0; d < fs->embed_dim; d++) {
        proto->embedding[d] = (proto->embedding[d] * n + embedding[d]) / (n + 1);
    }
    proto->num_samples++;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_fewshot_update_prototype(eif_fewshot_t* fs,
                                           const float32_t* embedding,
                                           int class_id) {
    return eif_fewshot_add_example(fs, embedding, class_id);
}

int eif_fewshot_classify(const eif_fewshot_t* fs,
                          const float32_t* embedding,
                          float32_t* distance) {
    if (!fs || !embedding || fs->num_classes == 0) {
        return -1;
    }
    
    int best_class = -1;
    float32_t best_dist = 1e30f;
    
    for (int i = 0; i < fs->num_classes; i++) {
        float32_t dist;
        
        if (fs->distance_type == EIF_DISTANCE_COSINE) {
            dist = 1.0f - eif_cosine_similarity(embedding, 
                fs->prototypes[i].embedding, fs->embed_dim);
        } else {
            dist = eif_euclidean_distance(embedding,
                fs->prototypes[i].embedding, fs->embed_dim);
        }
        
        if (dist < best_dist) {
            best_dist = dist;
            best_class = fs->prototypes[i].class_id;
        }
    }
    
    if (distance) *distance = best_dist;
    return best_class;
}

eif_status_t eif_fewshot_predict_proba(const eif_fewshot_t* fs,
                                        const float32_t* embedding,
                                        float32_t* probabilities) {
    if (!fs || !embedding || !probabilities || fs->num_classes == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    float32_t neg_dists[64];
    float32_t max_neg_dist = -1e30f;
    
    for (int i = 0; i < fs->num_classes; i++) {
        float32_t dist = eif_euclidean_distance(embedding,
            fs->prototypes[i].embedding, fs->embed_dim);
        neg_dists[i] = -dist;
        if (neg_dists[i] > max_neg_dist) {
            max_neg_dist = neg_dists[i];
        }
    }
    
    float32_t sum = 0.0f;
    for (int i = 0; i < fs->num_classes; i++) {
        probabilities[i] = expf(neg_dists[i] - max_neg_dist);
        sum += probabilities[i];
    }
    
    for (int i = 0; i < fs->num_classes; i++) {
        probabilities[i] /= sum;
    }
    
    return EIF_STATUS_OK;
}

void eif_fewshot_reset(eif_fewshot_t* fs) {
    if (!fs) return;
    
    for (int i = 0; i < fs->num_classes; i++) {
        memset(fs->prototypes[i].embedding, 0, fs->embed_dim * sizeof(float32_t));
        fs->prototypes[i].num_samples = 0;
        fs->prototypes[i].class_id = -1;
    }
    fs->num_classes = 0;
}

int eif_fewshot_num_classes(const eif_fewshot_t* fs) {
    return fs ? fs->num_classes : 0;
}
