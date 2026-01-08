#include "../framework/eif_test_runner.h"
#include "eif_nlp.h"
#include "eif_nlp_phoneme.h"
#include "eif_transformer.h"
#include "eif_core.h"

static uint8_t pool_buffer[65536]; // Increased buffer size for transformer tests
static eif_memory_pool_t pool;

void setup_nlp() {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
}

// =============================================================================
// Tokenizer Tests
// =============================================================================

bool test_tokenizer_init() {
    setup_nlp();
    eif_tokenizer_t tok;
    eif_status_t status = eif_tokenizer_init(&tok, EIF_TOKENIZER_CHAR, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(EIF_TOKENIZER_CHAR, tok.type);
    return true;
}

bool test_tokenizer_char_encode() {
    setup_nlp();
    eif_tokenizer_t tok;
    eif_tokenizer_init(&tok, EIF_TOKENIZER_CHAR, &pool);
    
    const char* text = "Hello";
    uint32_t token_ids[10];
    uint32_t num_tokens;
    
    eif_status_t status = eif_tokenizer_encode(&tok, text, token_ids, &num_tokens, 10);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(5, num_tokens);
    TEST_ASSERT_EQUAL_INT('H', token_ids[0]);
    TEST_ASSERT_EQUAL_INT('e', token_ids[1]);
    TEST_ASSERT_EQUAL_INT('l', token_ids[2]);
    TEST_ASSERT_EQUAL_INT('l', token_ids[3]);
    TEST_ASSERT_EQUAL_INT('o', token_ids[4]);
    
    return true;
}

bool test_tokenizer_char_decode() {
    setup_nlp();
    eif_tokenizer_t tok;
    eif_tokenizer_init(&tok, EIF_TOKENIZER_CHAR, &pool);
    
    uint32_t token_ids[] = {'H', 'i'};
    char text[10];
    
    eif_status_t status = eif_tokenizer_decode(&tok, token_ids, 2, text, 10);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_STRING("Hi", text);
    
    return true;
}

// =============================================================================
// Phoneme Tests
// =============================================================================

bool test_phoneme_dict_init() {
    setup_nlp();
    eif_phoneme_dict_t dict;
    eif_status_t status = eif_phoneme_dict_init(&dict, 10, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(0, dict.size);
    TEST_ASSERT_EQUAL_INT(10, dict.capacity);
    return true;
}

bool test_phoneme_dict_add_lookup() {
    setup_nlp();
    eif_phoneme_dict_t dict;
    eif_phoneme_dict_init(&dict, 10, &pool);
    
    eif_phoneme_seq_t seq;
    seq.length = 2;
    seq.phonemes[0] = EIF_PH_HH;
    seq.phonemes[1] = EIF_PH_IY;
    
    eif_status_t status = eif_phoneme_dict_add(&dict, "HE", &seq);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    const eif_phoneme_seq_t* result = eif_phoneme_dict_lookup(&dict, "HE");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(2, result->length);
    TEST_ASSERT_EQUAL_INT(EIF_PH_HH, result->phonemes[0]);
    TEST_ASSERT_EQUAL_INT(EIF_PH_IY, result->phonemes[1]);
    
    const eif_phoneme_seq_t* not_found = eif_phoneme_dict_lookup(&dict, "SHE");
    TEST_ASSERT(not_found == NULL);
    
    return true;
}

bool test_phoneme_utils() {
    TEST_ASSERT_TRUE(eif_phoneme_is_vowel(EIF_PH_AA));
    TEST_ASSERT_TRUE(eif_phoneme_is_consonant(EIF_PH_B));
    TEST_ASSERT(!eif_phoneme_is_vowel(EIF_PH_B));
    
    const char* str = eif_phoneme_to_str(EIF_PH_AA);
    TEST_ASSERT_EQUAL_STRING("AA", str);
    
    eif_phoneme_t ph = eif_phoneme_from_str("AA");
    TEST_ASSERT_EQUAL_INT(EIF_PH_AA, ph);
    
    return true;
}

// =============================================================================
// Transformer Tests
// =============================================================================

bool test_transformer_init() {
    setup_nlp();
    eif_transformer_t model;
    // Small model for testing
    eif_status_t status = eif_transformer_init(&model, 
                                               2,   // layers
                                               16,  // embed_dim
                                               2,   // heads
                                               32,  // ff_dim
                                               100, // vocab_size
                                               10,  // max_seq_len
                                               &pool);
                                               
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(2, model.num_layers);
    TEST_ASSERT_EQUAL_INT(16, model.embed_dim);
    TEST_ASSERT_NOT_NULL(model.token_embed);
    TEST_ASSERT_NOT_NULL(model.layers);
    
    return true;
}

bool test_transformer_memory_check() {
    size_t size = eif_transformer_memory_required(2, 16, 2, 32, 100, 10);
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_TRUE(size < sizeof(pool_buffer));
    return true;
}

bool test_tokenizer_whitespace_encode() {
    setup_nlp();
    eif_tokenizer_t tok;
    eif_tokenizer_init(&tok, EIF_TOKENIZER_WHITESPACE, &pool);
    
    const char* text = "Hello World Test";
    uint32_t token_ids[10];
    uint32_t num_tokens;
    
    eif_status_t status = eif_tokenizer_encode(&tok, text, token_ids, &num_tokens, 10);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(3, num_tokens);
    // Current implementation assigns sequential IDs: 0, 1, 2
    TEST_ASSERT_EQUAL_INT(0, token_ids[0]);
    TEST_ASSERT_EQUAL_INT(1, token_ids[1]);
    TEST_ASSERT_EQUAL_INT(2, token_ids[2]);
    
    return true;
}

bool test_vocab_operations() {
    setup_nlp();
    eif_vocabulary_t vocab;
    eif_status_t status = eif_vocab_init(&vocab, 10, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Check special tokens were added
    TEST_ASSERT_EQUAL_INT(4, vocab.size); // PAD, UNK, BOS, EOS
    
    // Add new token
    status = eif_vocab_add(&vocab, "apple");
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(5, vocab.size);
    
    return true;
}

bool test_transformer_activations() {
    // Test Softmax
    float32_t data[] = {1.0f, 2.0f, 3.0f};
    eif_softmax(data, 3);
    
    // exp(1) = 2.718, exp(2) = 7.389, exp(3) = 20.085
    // sum = 30.192
    // p1 = 0.090, p2 = 0.244, p3 = 0.665
    
    float32_t sum = data[0] + data[1] + data[2];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, sum);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.090f, data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.244f, data[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.665f, data[2]);
    
    // Test GELU
    // GELU(0) = 0
    // GELU(1) approx 0.841
    // GELU(-1) approx -0.158
    float32_t gelu_data[] = {0.0f, 1.0f, -1.0f};
    eif_gelu(gelu_data, 3);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, gelu_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.841f, gelu_data[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.158f, gelu_data[2]);
    
    return true;
}

bool test_transformer_layer_norm() {
    float32_t input[] = {1.0f, 2.0f, 3.0f, 4.0f}; // Mean 2.5, Var 1.25, Std 1.118
    float32_t output[4];
    
    // Test without gamma/beta (standard normalization)
    eif_layer_norm(input, NULL, NULL, 1, 4, output);
    
    // (1-2.5)/1.118 = -1.341
    // (2-2.5)/1.118 = -0.447
    // (3-2.5)/1.118 = 0.447
    // (4-2.5)/1.118 = 1.341
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.341f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.447f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.447f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.341f, output[3]);
    
    // Test with gamma/beta
    float32_t gamma[] = {1.0f, 1.0f, 1.0f, 1.0f};
    float32_t beta[] = {0.5f, 0.5f, 0.5f, 0.5f};
    
    eif_layer_norm(input, gamma, beta, 1, 4, output);
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.841f, output[0]); // -1.341 + 0.5
    
    return true;
}

bool test_transformer_attention() {
    // Simple attention test
    // 1 head, dim 2, seq 2
    int seq_len = 2;
    int embed_dim = 2;
    int num_heads = 1;
    
    float32_t input[] = {1.0f, 0.0f,  // Token 1
                         0.0f, 1.0f}; // Token 2
                         
    // Identity weights for Q, K, V
    float32_t w_identity[] = {1.0f, 0.0f, 0.0f, 1.0f};
    
    eif_attention_weights_t weights;
    weights.wq = w_identity;
    weights.wk = w_identity;
    weights.wv = w_identity;
    weights.wo = w_identity;
    
    float32_t output[4];
    float32_t scratch[100]; // Sufficient scratch
    
    eif_status_t status = eif_attention_forward(&weights, input, seq_len, embed_dim, num_heads, output, scratch);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // With identity weights:
    // Q = input, K = input, V = input
    // Scores:
    // Q1=[1,0] . K1=[1,0] = 1
    // Q1=[1,0] . K2=[0,1] = 0
    // Q2=[0,1] . K1=[1,0] = 0
    // Q2=[0,1] . K2=[0,1] = 1
    // Scaled by 1/sqrt(2) = 0.707
    // Softmax([0.707, 0]) -> [0.67, 0.33] approx
    // Softmax([0, 0.707]) -> [0.33, 0.67] approx
    
    // Output 1 = 0.67*V1 + 0.33*V2 = [0.67, 0.33]
    // Output 2 = 0.33*V1 + 0.67*V2 = [0.33, 0.67]
    
    // Check that output is not zero and has expected structure
    TEST_ASSERT_TRUE(output[0] > 0.5f);
    TEST_ASSERT_TRUE(output[1] < 0.5f);
    TEST_ASSERT_TRUE(output[2] < 0.5f);
    TEST_ASSERT_TRUE(output[3] > 0.5f);
    
    return true;
}

bool test_transformer_ffn() {
    int seq_len = 1;
    int embed_dim = 2;
    int ff_dim = 2;
    
    float32_t input[] = {1.0f, -1.0f};
    
    // Weights
    float32_t w1[] = {1.0f, 0.0f, 0.0f, 1.0f}; // Identity
    float32_t w2[] = {1.0f, 0.0f, 0.0f, 1.0f}; // Identity
    float32_t b1[] = {0.0f, 0.0f};
    float32_t b2[] = {0.0f, 0.0f};
    
    eif_ffn_weights_t weights;
    weights.w1 = w1;
    weights.w2 = w2;
    weights.b1 = b1;
    weights.b2 = b2;
    weights.ff_dim = ff_dim;
    
    float32_t output[2];
    
    eif_status_t status = eif_ffn_forward(&weights, input, seq_len, embed_dim, output);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Hidden = input * w1 + b1 = [1, -1]
    // GELU([1, -1]) = [0.841, -0.158]
    // Output = Hidden * w2 + b2 = [0.841, -0.158]
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.841f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.158f, output[1]);
    
    return true;
}

BEGIN_TEST_SUITE(run_nlp_tests)
    RUN_TEST(test_tokenizer_init);
    RUN_TEST(test_tokenizer_char_encode);
    RUN_TEST(test_tokenizer_char_decode);
    RUN_TEST(test_tokenizer_whitespace_encode);
    RUN_TEST(test_vocab_operations);
    RUN_TEST(test_phoneme_dict_init);
    RUN_TEST(test_phoneme_dict_add_lookup);
    RUN_TEST(test_phoneme_utils);
    RUN_TEST(test_transformer_init);
    RUN_TEST(test_transformer_memory_check);
    RUN_TEST(test_transformer_activations);
    RUN_TEST(test_transformer_layer_norm);
    RUN_TEST(test_transformer_attention);
    RUN_TEST(test_transformer_ffn);
END_TEST_SUITE()
