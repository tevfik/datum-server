/**
 * @file eif_nlp.h
 * @brief Edge Intelligence Framework - Natural Language Processing
 * 
 * Lightweight NLP components for embedded systems:
 * - Tokenization (whitespace, character, BPE)
 * - Vocabulary management
 * - Text embeddings
 * - Phoneme processing (see eif_nlp_phoneme.h)
 */

#ifndef EIF_NLP_H
#define EIF_NLP_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constants
// =============================================================================

#define EIF_NLP_MAX_TOKEN_LEN     64
#define EIF_NLP_MAX_VOCAB_SIZE    4096
#define EIF_NLP_MAX_SEQ_LEN       128

// =============================================================================
// Tokenizer Types
// =============================================================================

typedef enum {
    EIF_TOKENIZER_WHITESPACE,   // Split on whitespace
    EIF_TOKENIZER_CHAR,         // Character-level
    EIF_TOKENIZER_BPE           // Byte-Pair Encoding
} eif_tokenizer_type_t;

typedef struct {
    eif_tokenizer_type_t type;
    uint32_t vocab_size;
    uint32_t max_token_len;
    char** vocab;               // Vocabulary array
    eif_memory_pool_t* pool;
} eif_tokenizer_t;

// =============================================================================
// Vocabulary Types
// =============================================================================

typedef struct {
    uint32_t id;
    char token[EIF_NLP_MAX_TOKEN_LEN];
    uint32_t count;
} eif_vocab_entry_t;

typedef struct {
    eif_vocab_entry_t* entries;
    uint32_t size;
    uint32_t capacity;
    uint32_t unk_id;            // Unknown token ID
    uint32_t pad_id;            // Padding token ID
    uint32_t bos_id;            // Beginning of sequence ID
    uint32_t eos_id;            // End of sequence ID
    eif_memory_pool_t* pool;
} eif_vocabulary_t;

// =============================================================================
// Embedding Types
// =============================================================================

typedef struct {
    float32_t* weights;         // [vocab_size x embed_dim]
    uint32_t vocab_size;
    uint32_t embed_dim;
    eif_memory_pool_t* pool;
} eif_embedding_t;

// =============================================================================
// Tokenizer API
// =============================================================================

/**
 * @brief Initialize a tokenizer
 */
eif_status_t eif_tokenizer_init(eif_tokenizer_t* tok, 
                                 eif_tokenizer_type_t type,
                                 eif_memory_pool_t* pool);

/**
 * @brief Tokenize text into token IDs
 */
eif_status_t eif_tokenizer_encode(const eif_tokenizer_t* tok,
                                   const char* text,
                                   uint32_t* token_ids,
                                   uint32_t* num_tokens,
                                   uint32_t max_tokens);

/**
 * @brief Decode token IDs back to text
 */
eif_status_t eif_tokenizer_decode(const eif_tokenizer_t* tok,
                                   const uint32_t* token_ids,
                                   uint32_t num_tokens,
                                   char* text,
                                   uint32_t max_len);

/**
 * @brief Free tokenizer resources
 */
void eif_tokenizer_free(eif_tokenizer_t* tok);

// =============================================================================
// Vocabulary API
// =============================================================================

/**
 * @brief Initialize vocabulary
 */
eif_status_t eif_vocab_init(eif_vocabulary_t* vocab,
                            uint32_t capacity,
                            eif_memory_pool_t* pool);

/**
 * @brief Add token to vocabulary
 */
eif_status_t eif_vocab_add(eif_vocabulary_t* vocab, const char* token);

/**
 * @brief Get token ID
 */
uint32_t eif_vocab_get_id(const eif_vocabulary_t* vocab, const char* token);

/**
 * @brief Get token by ID
 */
const char* eif_vocab_get_token(const eif_vocabulary_t* vocab, uint32_t id);

/**
 * @brief Free vocabulary resources
 */
void eif_vocab_free(eif_vocabulary_t* vocab);

// =============================================================================
// Embedding API
// =============================================================================

/**
 * @brief Initialize embedding layer
 */
eif_status_t eif_embedding_init(eif_embedding_t* emb,
                                 uint32_t vocab_size,
                                 uint32_t embed_dim,
                                 eif_memory_pool_t* pool);

/**
 * @brief Lookup embeddings for token IDs
 */
eif_status_t eif_embedding_lookup(const eif_embedding_t* emb,
                                   const uint32_t* token_ids,
                                   uint32_t num_tokens,
                                   float32_t* output);

/**
 * @brief Free embedding resources
 */
void eif_embedding_free(eif_embedding_t* emb);

#ifdef __cplusplus
}
#endif

#endif // EIF_NLP_H
