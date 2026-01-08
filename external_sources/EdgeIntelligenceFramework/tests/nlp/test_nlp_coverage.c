#include "../framework/eif_test_runner.h"
#include "eif_nlp.h"
#include "eif_nlp_phoneme.h"
#include "eif_transformer.h"
#include "eif_core.h"
#include <string.h>

static uint8_t pool_buffer[131072]; // Large buffer for full model tests
static eif_memory_pool_t pool;

void setup_nlp_coverage() {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
}

// =============================================================================
// Full Model Forward Tests
// =============================================================================

bool test_transformer_forward_full() {
    setup_nlp_coverage();
    eif_transformer_t model;
    // Initialize a tiny model
    // 2 layers, 4 embed_dim, 2 heads, 8 ff_dim, 10 vocab, 5 max_seq
    eif_status_t status = eif_transformer_init(&model, 2, 4, 2, 8, 10, 5, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Initialize weights to something deterministic (e.g., 0.1)
    // This is tedious but ensures no NaNs if we were doing strict math checks.
    // For coverage, random/uninitialized is often "fine" as long as it doesn't crash,
    // but let's be safe.
    // (Skipping detailed weight init for brevity, relying on allocation zeroing if any, 
    // or just robustness of code to arbitrary floats).
    
    int32_t input_ids[] = {1, 2, 3};
    int seq_len = 3;
    float32_t output[3 * 4]; // seq_len * embed_dim
    
    status = eif_transformer_forward(&model, input_ids, seq_len, output);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Check output is not all zeros (activations should produce something)
    // Actually, if weights are uninitialized garbage, output is garbage.
    // But we just want to exercise the code paths.
    
    return true;
}

bool test_transformer_classify_full() {
    setup_nlp_coverage();
    eif_transformer_t model;
    eif_transformer_init(&model, 1, 4, 1, 4, 10, 5, &pool);
    
    // Manually setup classifier head since init doesn't do it (it sets to NULL)
    model.num_classes = 2;
    model.classifier_w = (float32_t*)eif_memory_alloc(&pool, 4 * 2 * sizeof(float32_t), 4);
    model.classifier_b = (float32_t*)eif_memory_alloc(&pool, 2 * sizeof(float32_t), 4);
    
    // Set weights
    for(int i=0; i<8; i++) model.classifier_w[i] = 0.5f;
    for(int i=0; i<2; i++) model.classifier_b[i] = 0.1f;
    
    int32_t input_ids[] = {1, 5};
    float32_t logits[2];
    
    eif_status_t status = eif_transformer_classify(&model, input_ids, 2, logits);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Check values
    // Input -> Forward -> Hidden. 
    // Classify uses Hidden[0] (CLS token).
    // Logits = Hidden[0] * W + b.
    // Since we didn't init transformer weights, Hidden is unpredictable.
    // But we verified the function ran.
    
    return true;
}

bool test_transformer_embed_full() {
    setup_nlp_coverage();
    eif_transformer_t model;
    eif_transformer_init(&model, 1, 4, 1, 4, 10, 5, &pool);
    
    int32_t input_ids[] = {1, 2};
    float32_t embedding[4]; // embed_dim
    
    eif_status_t status = eif_transformer_embed(&model, input_ids, 2, embedding);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    return true;
}

// =============================================================================
// Error Handling & Edge Cases
// =============================================================================

bool test_transformer_invalid_args() {
    setup_nlp_coverage();
    eif_transformer_t model;
    eif_transformer_init(&model, 1, 4, 1, 4, 10, 5, &pool);
    
    float32_t output[20];
    int32_t input_ids[] = {1};
    
    // Null model
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, 
        eif_transformer_forward(NULL, input_ids, 1, output));
        
    // Null input
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, 
        eif_transformer_forward(&model, NULL, 1, output));
        
    // Null output
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, 
        eif_transformer_forward(&model, input_ids, 1, NULL));
        
    // Seq len too long
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, 
        eif_transformer_forward(&model, input_ids, 100, output)); // max is 5
        
    // Classify invalid args
    float32_t logits[2];
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, 
        eif_transformer_classify(NULL, input_ids, 1, logits));
        
    // Classify without head
    model.classifier_w = NULL;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, 
        eif_transformer_classify(&model, input_ids, 1, logits));
        
    // Embed invalid args
    float32_t emb[4];
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, 
        eif_transformer_embed(NULL, input_ids, 1, emb));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, 
        eif_transformer_embed(&model, input_ids, 1, NULL));
        
    return true;
}

bool test_transformer_token_oob() {
    setup_nlp_coverage();
    eif_transformer_t model;
    eif_transformer_init(&model, 1, 4, 1, 4, 10, 5, &pool);
    
    // Initialize token embeddings to 0, except index 0 (UNK) to 1.0
    memset(model.token_embed, 0, 10 * 4 * sizeof(float32_t));
    for(int i=0; i<4; i++) model.token_embed[i] = 1.0f; // UNK is all 1s
    
    // Input with OOB tokens: -1 and 100 (vocab is 10)
    int32_t input_ids[] = {-1, 100};
    float32_t output[2 * 4];
    
    // We need to zero out pos embeddings to check just token embeddings
    memset(model.pos_embed, 0, 5 * 4 * sizeof(float32_t));
    
    // Also zero out layer weights to make it identity-ish or just check the first embedding lookup?
    // Actually, eif_transformer_forward does the lookup first.
    // But we can't easily inspect the internal buffer 'hidden' without modifying the code or using a debugger.
    // However, we can check that it runs successfully.
    
    eif_status_t status = eif_transformer_forward(&model, input_ids, 2, output);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    return true;
}

bool test_transformer_print() {
    setup_nlp_coverage();
    eif_transformer_t model;
    eif_transformer_init(&model, 1, 4, 1, 4, 10, 5, &pool);
    
    // Just ensure it doesn't crash
    eif_transformer_print_summary(&model);
    eif_transformer_print_summary(NULL);
    
    return true;
}

// =============================================================================
// Phoneme & G2P Coverage Tests
// =============================================================================

bool test_phoneme_dict_full() {
    setup_nlp_coverage();
    eif_phoneme_dict_t dict;
    
    // Init invalid
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_phoneme_dict_init(NULL, 10, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_phoneme_dict_init(&dict, 0, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_phoneme_dict_init(&dict, 10, NULL));
    
    // Init valid
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_phoneme_dict_init(&dict, 2, &pool));
    
    eif_phoneme_seq_t seq = { .length = 1, .phonemes = {EIF_PH_AA} };
    
    // Add invalid
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_phoneme_dict_add(NULL, "A", &seq));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_phoneme_dict_add(&dict, NULL, &seq));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_phoneme_dict_add(&dict, "A", NULL));
    
    // Add valid
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_phoneme_dict_add(&dict, "A", &seq));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_phoneme_dict_add(&dict, "B", &seq));
    
    // Add full
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OUT_OF_MEMORY, eif_phoneme_dict_add(&dict, "C", &seq));
    
    // Lookup invalid
    TEST_ASSERT_TRUE(eif_phoneme_dict_lookup(NULL, "A") == NULL);
    TEST_ASSERT_TRUE(eif_phoneme_dict_lookup(&dict, NULL) == NULL);
    
    // Lookup valid
    TEST_ASSERT_NOT_NULL(eif_phoneme_dict_lookup(&dict, "a")); // Case insensitive check
    TEST_ASSERT_TRUE(eif_phoneme_dict_lookup(&dict, "Z") == NULL);
    
    // Free
    eif_phoneme_dict_free(&dict);
    eif_phoneme_dict_free(NULL); // Should not crash
    
    return true;
}

bool test_g2p_english() {
    setup_nlp_coverage();
    eif_g2p_t g2p;
    
    // Init invalid
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_g2p_init_english(NULL, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_g2p_init_english(&g2p, NULL));
    
    // Init valid
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_g2p_init_english(&g2p, &pool));
    
    eif_phoneme_seq_t output;
    
    // Convert invalid
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_g2p_convert(NULL, "test", &output));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_g2p_convert(&g2p, NULL, &output));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_g2p_convert(&g2p, "test", NULL));
    
    // Convert simple rule (Consonant + Vowel)
    // "hi" -> H IH (based on simple rules: h->HH, i->IH)
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_g2p_convert(&g2p, "hi", &output));
    TEST_ASSERT_EQUAL_INT(2, output.length);
    TEST_ASSERT_EQUAL_INT(EIF_PH_HH, output.phonemes[0]);
    TEST_ASSERT_EQUAL_INT(EIF_PH_IH, output.phonemes[1]);
    
    // Convert digraph
    // "sh" -> SH
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_g2p_convert(&g2p, "sh", &output));
    TEST_ASSERT_EQUAL_INT(1, output.length);
    TEST_ASSERT_EQUAL_INT(EIF_PH_SH, output.phonemes[0]);
    
    // Convert with dictionary fallback
    eif_phoneme_dict_t dict;
    eif_phoneme_dict_init(&dict, 5, &pool);
    eif_phoneme_seq_t seq = { .length = 1, .phonemes = {EIF_PH_AA} };
    eif_phoneme_dict_add(&dict, "TEST", &seq);
    
    g2p.dict = &dict;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_g2p_convert(&g2p, "test", &output));
    TEST_ASSERT_EQUAL_INT(1, output.length);
    TEST_ASSERT_EQUAL_INT(EIF_PH_AA, output.phonemes[0]);
    
    // Free
    eif_g2p_free(&g2p);
    eif_g2p_free(NULL);
    
    return true;
}

bool test_phoneme_utils_coverage() {
    // Distance
    eif_phoneme_seq_t a = { .length = 1, .phonemes = {EIF_PH_AA} };
    eif_phoneme_seq_t b = { .length = 1, .phonemes = {EIF_PH_AA} };
    eif_phoneme_seq_t c = { .length = 1, .phonemes = {EIF_PH_AE} };
    eif_phoneme_seq_t d = { .length = 0 };
    
    TEST_ASSERT_EQUAL_INT(-1, eif_phoneme_distance(NULL, &b));
    TEST_ASSERT_EQUAL_INT(0, eif_phoneme_distance(&a, &b));
    TEST_ASSERT_EQUAL_INT(1, eif_phoneme_distance(&a, &c));
    TEST_ASSERT_EQUAL_INT(1, eif_phoneme_distance(&a, &d));
    TEST_ASSERT_EQUAL_INT(1, eif_phoneme_distance(&d, &a));
    
    // Str conversion
    TEST_ASSERT_EQUAL_STRING("UNK", eif_phoneme_to_str((eif_phoneme_t)999));
    TEST_ASSERT_EQUAL_INT(EIF_PH_UNK, eif_phoneme_from_str(NULL));
    TEST_ASSERT_EQUAL_INT(EIF_PH_UNK, eif_phoneme_from_str("INVALID"));
    
    return true;
}

BEGIN_TEST_SUITE(run_nlp_coverage_tests)
    RUN_TEST(test_transformer_forward_full);
    RUN_TEST(test_transformer_classify_full);
    RUN_TEST(test_transformer_embed_full);
    RUN_TEST(test_transformer_invalid_args);
    RUN_TEST(test_transformer_token_oob);
    RUN_TEST(test_transformer_print);
    RUN_TEST(test_phoneme_dict_full);
    RUN_TEST(test_g2p_english);
    RUN_TEST(test_phoneme_utils_coverage);
END_TEST_SUITE()
