/**
 * @file main.c
 * @brief NLP Tokenizer and Phoneme Demo
 * 
 * This demo showcases the EIF NLP module capabilities:
 * 
 * 1. TOKENIZATION
 *    - Character-level tokenization for byte-level processing
 *    - Whitespace tokenization for word-level processing
 *    - Vocabulary management with special tokens
 * 
 * 2. PHONEME PROCESSING
 *    - Grapheme-to-Phoneme (G2P) conversion
 *    - ARPABET phoneme representation
 *    - Phoneme sequence comparison (edit distance)
 *    - Phoneme dictionary lookup
 * 
 * Applications:
 *    - Keyword spotting (KWS) on microcontrollers
 *    - TTS (Text-to-Speech) preprocessing
 *    - Speech recognition frontend
 *    - Pronunciation similarity matching
 */

#include <stdio.h>
#include <string.h>
#include "eif_nlp.h"
#include "eif_nlp_phoneme.h"
#include "eif_memory.h"

// Memory pool for NLP operations
static uint8_t pool_buffer[32768];  // 32KB for all demos
static eif_memory_pool_t pool;

/**
 * @brief Demo 1: Character-level tokenization
 * 
 * Character tokenization is useful for:
 * - Byte-level language models
 * - Character RNNs
 * - Handling unknown words gracefully
 */
void demo_char_tokenization(void) {
    printf("\n=== Demo 1: Character Tokenization ===\n");
    
    eif_tokenizer_t tok;
    eif_tokenizer_init(&tok, EIF_TOKENIZER_CHAR, &pool);
    
    const char* text = "Hello";
    uint32_t token_ids[32];
    uint32_t num_tokens;
    
    eif_tokenizer_encode(&tok, text, token_ids, &num_tokens, 32);
    
    printf("Input: \"%s\"\n", text);
    printf("Tokens (%u): ", num_tokens);
    for (uint32_t i = 0; i < num_tokens; i++) {
        printf("%c(%u) ", (char)token_ids[i], token_ids[i]);
    }
    printf("\n");
    
    // Decode back
    char decoded[32];
    eif_tokenizer_decode(&tok, token_ids, num_tokens, decoded, 32);
    printf("Decoded: \"%s\"\n", decoded);
    
    eif_tokenizer_free(&tok);
}

/**
 * @brief Demo 2: Vocabulary management
 * 
 * Vocabulary features:
 * - Special tokens: [PAD], [UNK], [BOS], [EOS]
 * - Dynamic token addition
 * - Token ID lookup
 */
void demo_vocabulary(void) {
    printf("\n=== Demo 2: Vocabulary Management ===\n");
    
    eif_vocabulary_t vocab;
    eif_vocab_init(&vocab, 100, &pool);
    
    // Add custom tokens
    eif_vocab_add(&vocab, "hello");
    eif_vocab_add(&vocab, "world");
    eif_vocab_add(&vocab, "edge");
    eif_vocab_add(&vocab, "ai");
    
    printf("Vocabulary size: %u\n", vocab.size);
    printf("Special tokens:\n");
    printf("  [PAD] -> ID %u\n", vocab.pad_id);
    printf("  [UNK] -> ID %u\n", vocab.unk_id);
    printf("  [BOS] -> ID %u\n", vocab.bos_id);
    printf("  [EOS] -> ID %u\n", vocab.eos_id);
    
    // Lookup tokens
    printf("\nToken lookups:\n");
    printf("  'hello' -> ID %u\n", eif_vocab_get_id(&vocab, "hello"));
    printf("  'world' -> ID %u\n", eif_vocab_get_id(&vocab, "world"));
    printf("  'unknown' -> ID %u (UNK)\n", eif_vocab_get_id(&vocab, "unknown"));
    
    // Reverse lookup
    printf("\nID lookups:\n");
    printf("  ID 4 -> '%s'\n", eif_vocab_get_token(&vocab, 4));
    printf("  ID 5 -> '%s'\n", eif_vocab_get_token(&vocab, 5));
    
    eif_vocab_free(&vocab);
}

/**
 * @brief Demo 3: Grapheme-to-Phoneme (G2P) conversion
 * 
 * G2P converts text to phoneme sequences using:
 * - Rule-based conversion for simple patterns
 * - Dictionary lookup for irregular words
 * - ARPABET phoneme representation
 */
void demo_g2p(void) {
    printf("\n=== Demo 3: Grapheme-to-Phoneme (G2P) ===\n");
    
    eif_g2p_t g2p;
    eif_g2p_init_english(&g2p, &pool);
    
    const char* words[] = {"hello", "world", "stop", "speech", "think"};
    int num_words = sizeof(words) / sizeof(words[0]);
    
    printf("Word -> Phoneme conversion (ARPABET):\n\n");
    
    for (int i = 0; i < num_words; i++) {
        eif_phoneme_seq_t seq;
        eif_g2p_convert(&g2p, words[i], &seq);
        
        printf("  %-10s -> ", words[i]);
        for (int j = 0; j < seq.length; j++) {
            printf("%s ", eif_phoneme_to_str(seq.phonemes[j]));
        }
        printf("\n");
    }
    
    eif_g2p_free(&g2p);
}

/**
 * @brief Demo 4: Phoneme similarity comparison
 * 
 * Phoneme distance is useful for:
 * - Finding similar-sounding words
 * - Fuzzy keyword matching in speech recognition
 * - Pronunciation error detection
 */
void demo_phoneme_similarity(void) {
    printf("\n=== Demo 4: Phoneme Similarity ===\n");
    
    eif_g2p_t g2p;
    eif_g2p_init_english(&g2p, &pool);
    
    // Compare word pairs
    const char* pairs[][2] = {
        {"cat", "bat"},     // Similar (1 phoneme diff)
        {"hello", "hallo"}, // Similar (1 vowel diff)
        {"stop", "step"},   // Similar 
        {"hello", "world"}, // Different
    };
    
    printf("Phoneme edit distance between words:\n\n");
    
    for (int i = 0; i < 4; i++) {
        eif_phoneme_seq_t seq1, seq2;
        eif_g2p_convert(&g2p, pairs[i][0], &seq1);
        eif_g2p_convert(&g2p, pairs[i][1], &seq2);
        
        int dist = eif_phoneme_distance(&seq1, &seq2);
        
        printf("  %-8s vs %-8s = distance %d\n", 
               pairs[i][0], pairs[i][1], dist);
    }
    
    eif_g2p_free(&g2p);
}

/**
 * @brief Demo 5: Phoneme dictionary
 * 
 * Dictionaries provide accurate phoneme mappings for:
 * - Irregular pronunciations
 * - Names and proper nouns
 * - Domain-specific vocabulary
 */
void demo_phoneme_dictionary(void) {
    printf("\n=== Demo 5: Phoneme Dictionary ===\n");
    
    eif_phoneme_dict_t dict;
    eif_phoneme_dict_init(&dict, 100, &pool);
    
    // Add some dictionary entries
    eif_phoneme_seq_t hello_seq = {
        .phonemes = {EIF_PH_HH, EIF_PH_AH, EIF_PH_L, EIF_PH_OW},
        .length = 4
    };
    eif_phoneme_dict_add(&dict, "hello", &hello_seq);
    
    eif_phoneme_seq_t world_seq = {
        .phonemes = {EIF_PH_W, EIF_PH_ER, EIF_PH_L, EIF_PH_D},
        .length = 4
    };
    eif_phoneme_dict_add(&dict, "world", &world_seq);
    
    printf("Dictionary entries: %u\n\n", dict.size);
    
    // Lookup
    const char* lookup_words[] = {"hello", "world", "unknown"};
    for (int i = 0; i < 3; i++) {
        const eif_phoneme_seq_t* seq = eif_phoneme_dict_lookup(&dict, lookup_words[i]);
        printf("  %-10s -> ", lookup_words[i]);
        if (seq) {
            for (int j = 0; j < seq->length; j++) {
                printf("%s ", eif_phoneme_to_str(seq->phonemes[j]));
            }
        } else {
            printf("(not found)");
        }
        printf("\n");
    }
    
    eif_phoneme_dict_free(&dict);
}

/**
 * @brief Demo 6: Phoneme properties
 */
void demo_phoneme_properties(void) {
    printf("\n=== Demo 6: Phoneme Properties ===\n");
    
    printf("\nVowels: ");
    for (int i = 0; i < EIF_PH_COUNT; i++) {
        if (eif_phoneme_is_vowel((eif_phoneme_t)i)) {
            printf("%s ", eif_phoneme_to_str((eif_phoneme_t)i));
        }
    }
    
    printf("\n\nConsonants: ");
    for (int i = 0; i < EIF_PH_COUNT; i++) {
        if (eif_phoneme_is_consonant((eif_phoneme_t)i)) {
            printf("%s ", eif_phoneme_to_str((eif_phoneme_t)i));
        }
    }
    printf("\n");
}

int main(void) {
    printf("========================================\n");
    printf("EIF NLP Module Demo\n");
    printf("========================================\n");
    printf("\nThis demo showcases NLP capabilities for\n");
    printf("embedded systems and edge AI applications.\n");
    
    // Initialize memory pool
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Run demos
    demo_char_tokenization();
    demo_vocabulary();
    demo_g2p();
    demo_phoneme_similarity();
    demo_phoneme_dictionary();
    demo_phoneme_properties();
    
    printf("\n========================================\n");
    printf("Demo complete!\n");
    printf("========================================\n");
    
    return 0;
}
