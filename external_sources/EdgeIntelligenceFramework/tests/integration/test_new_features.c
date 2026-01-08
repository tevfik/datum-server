#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../framework/eif_test_runner.h"

#include "../../ml/include/eif_ml_kmeans.h"
#include "../../nlp/include/eif_nlp_embedding.h"
#include "../../cv/include/eif_cv_haar.h"

// =============================================================================
// ML: K-Means Tests
// =============================================================================
bool test_kmeans_basic(void) {
    // 2D Data, 2 Clusters
    // Cluster 1: Near (0,0)
    // Cluster 2: Near (10,10)
    float data[] = {
        0.1f, 0.1f,
        0.2f, 0.0f,
        0.0f, 0.2f,
        10.1f, 10.1f,
        10.0f, 10.2f,
        9.9f, 10.0f
    };
    
    eif_kmeans_t km;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_kmeans_init(&km, 2, 2));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_kmeans_fit(&km, data, 6));
    
    // Check centroids
    // One should be approx (0.1, 0.1), other (10.0, 10.1)
    // Order is random, so check distance to theoretical centers
    
    float c1[2] = {0.1f, 0.1f};
    float c2[2] = {10.0f, 10.0f};
    
    int c1_match = -1;
    int c2_match = -1;
    
    for(int i=0; i<2; i++) {
        float d1 = (km.centroids[i*2] - c1[0])*(km.centroids[i*2] - c1[0]) + 
                   (km.centroids[i*2+1] - c1[1])*(km.centroids[i*2+1] - c1[1]);
        if (d1 < 1.0f) c1_match = i;
        
        float d2 = (km.centroids[i*2] - c2[0])*(km.centroids[i*2] - c2[0]) + 
                   (km.centroids[i*2+1] - c2[1])*(km.centroids[i*2+1] - c2[1]);
        if (d2 < 1.0f) c2_match = i;
    }
    
    TEST_ASSERT_TRUE(c1_match != -1);
    TEST_ASSERT_TRUE(c2_match != -1);
    TEST_ASSERT_TRUE(c1_match != c2_match);
    
    // Test Prediction
    float sample1[] = {0.5f, 0.5f};
    int p1 = eif_kmeans_predict(&km, sample1);
    TEST_ASSERT_EQUAL_INT(c1_match, p1);

    eif_kmeans_free(&km);
    return true;
}

// =============================================================================
// NLP: Embedding Tests
// =============================================================================
bool test_embedding_cosine(void) {
    eif_embedding_t emb;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_word_embedding_init(&emb, 10, 3));
    
    float v_king[] = {1.0f, 0.0f, 0.0f};
    float v_queen[] = {1.0f, 0.1f, 0.0f}; // Close to King
    float v_apple[] = {0.0f, 1.0f, 1.0f}; // Far
    
    eif_embedding_add(&emb, "king", v_king);
    eif_embedding_add(&emb, "queen", v_queen);
    eif_embedding_add(&emb, "apple", v_apple);
    
    float sim1 = eif_embedding_similarity(&emb, "king", "queen");
    float sim2 = eif_embedding_similarity(&emb, "king", "apple");
    
    TEST_ASSERT_TRUE(sim1 > 0.9f);  // High similarity
    TEST_ASSERT_TRUE(sim2 < 0.5f);  // Low similarity
    
    eif_word_embedding_free(&emb);
    return true;
}

// =============================================================================
// CV: Haar Tests
// =============================================================================
bool test_haar_integral(void) {
    // 4x4 Image, all 1s
    eif_cv_image_t img;
    img.width = 4; img.height = 4;
    img.stride = 4;
    img.data = (uint8_t*)malloc(16);
    memset(img.data, 1, 16);
    
    eif_integral_image_t ii = {0};
    eif_cv_compute_integral(&img, &ii);
    
    // Check Sums
    // (0,0) should be 1
    TEST_ASSERT_EQUAL_INT(1, ii.data[0]);
    // (3,3) should be 16 (Sum of 4x4 ones)
    TEST_ASSERT_EQUAL_INT(16, ii.data[15]);
    
    // Check eif_cv_integral_sum for rect (1,1,2,2) -> middle 2x2 square
    // Should be 4
    uint32_t s = eif_cv_integral_sum(&ii, 1, 1, 2, 2);
    TEST_ASSERT_EQUAL_INT(4, s);
    
    free(img.data);
    free(ii.data);
    return true;
}

int run_new_features_tests(void) {
    int failed = 0;
    if (!test_kmeans_basic()) { printf("test_kmeans_basic FAILED\n"); failed++; }
    if (!test_embedding_cosine()) { printf("test_embedding_cosine FAILED\n"); failed++; }
    if (!test_haar_integral()) { printf("test_haar_integral FAILED\n"); failed++; }
    return failed;
}
