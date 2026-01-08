/**
 * @file eif_nlp_tokenizer.c
 * @brief NLP Tokenizer implementation
 */

#include "eif_nlp.h"
#include "eif_status.h"
#include <string.h>
#include <ctype.h>

// =============================================================================
// Tokenizer Implementation
// =============================================================================

eif_status_t eif_tokenizer_init(eif_tokenizer_t* tok, 
                                 eif_tokenizer_type_t type,
                                 eif_memory_pool_t* pool) {
    if (!tok || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    tok->type = type;
    tok->pool = pool;
    tok->vocab_size = 0;
    tok->max_token_len = EIF_NLP_MAX_TOKEN_LEN;
    tok->vocab = NULL;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_tokenizer_encode(const eif_tokenizer_t* tok,
                                   const char* text,
                                   uint32_t* token_ids,
                                   uint32_t* num_tokens,
                                   uint32_t max_tokens) {
    if (!tok || !text || !token_ids || !num_tokens) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    *num_tokens = 0;
    
    switch (tok->type) {
        case EIF_TOKENIZER_WHITESPACE: {
            // Simple whitespace tokenizer (stub implementation)
            const char* p = text;
            while (*p && *num_tokens < max_tokens) {
                // Skip whitespace
                while (*p && isspace((unsigned char)*p)) p++;
                if (!*p) break;
                
                // Count token length
                const char* start = p;
                (void)start;  // Suppress unused warning
                while (*p && !isspace((unsigned char)*p)) p++;
                
                // For now, just count tokens (vocabulary lookup would happen here)
                token_ids[*num_tokens] = (*num_tokens);  // Placeholder
                (*num_tokens)++;
            }
            break;
        }
        
        case EIF_TOKENIZER_CHAR: {
            // Character-level tokenizer
            for (size_t i = 0; text[i] && *num_tokens < max_tokens; i++) {
                token_ids[*num_tokens] = (uint32_t)text[i];
                (*num_tokens)++;
            }
            break;
        }
        
        case EIF_TOKENIZER_BPE:
            // BPE requires vocabulary - not implemented yet
            return EIF_STATUS_NOT_SUPPORTED;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_tokenizer_decode(const eif_tokenizer_t* tok,
                                   const uint32_t* token_ids,
                                   uint32_t num_tokens,
                                   char* text,
                                   uint32_t max_len) {
    if (!tok || !token_ids || !text) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    if (tok->type == EIF_TOKENIZER_CHAR) {
        uint32_t i;
        for (i = 0; i < num_tokens && i < max_len - 1; i++) {
            text[i] = (char)token_ids[i];
        }
        text[i] = '\0';
        return EIF_STATUS_OK;
    }
    
    // For other types, need vocabulary
    text[0] = '\0';
    return EIF_STATUS_NOT_SUPPORTED;
}

void eif_tokenizer_free(eif_tokenizer_t* tok) {
    if (tok) {
        tok->vocab = NULL;
        tok->vocab_size = 0;
    }
}

// =============================================================================
// Vocabulary Implementation
// =============================================================================

eif_status_t eif_vocab_init(eif_vocabulary_t* vocab,
                            uint32_t capacity,
                            eif_memory_pool_t* pool) {
    if (!vocab || !pool || capacity == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    vocab->entries = (eif_vocab_entry_t*)eif_memory_alloc(pool, 
        capacity * sizeof(eif_vocab_entry_t), sizeof(void*));
    if (!vocab->entries) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    vocab->size = 0;
    vocab->capacity = capacity;
    vocab->pool = pool;
    
    // Add special tokens
    vocab->pad_id = 0;
    vocab->unk_id = 1;
    vocab->bos_id = 2;
    vocab->eos_id = 3;
    
    // Initialize special tokens
    eif_vocab_add(vocab, "[PAD]");
    eif_vocab_add(vocab, "[UNK]");
    eif_vocab_add(vocab, "[BOS]");
    eif_vocab_add(vocab, "[EOS]");
    
    return EIF_STATUS_OK;
}

eif_status_t eif_vocab_add(eif_vocabulary_t* vocab, const char* token) {
    if (!vocab || !token) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    if (vocab->size >= vocab->capacity) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    eif_vocab_entry_t* entry = &vocab->entries[vocab->size];
    entry->id = vocab->size;
    strncpy(entry->token, token, EIF_NLP_MAX_TOKEN_LEN - 1);
    entry->token[EIF_NLP_MAX_TOKEN_LEN - 1] = '\0';
    entry->count = 0;
    
    vocab->size++;
    return EIF_STATUS_OK;
}

uint32_t eif_vocab_get_id(const eif_vocabulary_t* vocab, const char* token) {
    if (!vocab || !token) {
        return vocab ? vocab->unk_id : 0;
    }
    
    for (uint32_t i = 0; i < vocab->size; i++) {
        if (strcmp(vocab->entries[i].token, token) == 0) {
            return i;
        }
    }
    
    return vocab->unk_id;
}

const char* eif_vocab_get_token(const eif_vocabulary_t* vocab, uint32_t id) {
    if (!vocab || id >= vocab->size) {
        return NULL;
    }
    return vocab->entries[id].token;
}

void eif_vocab_free(eif_vocabulary_t* vocab) {
    if (vocab) {
        vocab->entries = NULL;
        vocab->size = 0;
        vocab->capacity = 0;
    }
}

// =============================================================================
// Embedding Implementation
// =============================================================================

eif_status_t eif_embedding_init(eif_embedding_t* emb,
                                 uint32_t vocab_size,
                                 uint32_t embed_dim,
                                 eif_memory_pool_t* pool) {
    if (!emb || !pool || vocab_size == 0 || embed_dim == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    size_t weight_size = vocab_size * embed_dim * sizeof(float32_t);
    emb->weights = (float32_t*)eif_memory_alloc(pool, weight_size, sizeof(float32_t));
    if (!emb->weights) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    emb->vocab_size = vocab_size;
    emb->embed_dim = embed_dim;
    emb->pool = pool;
    
    // Initialize with zeros (user should load pretrained weights)
    memset(emb->weights, 0, weight_size);
    
    return EIF_STATUS_OK;
}

eif_status_t eif_embedding_lookup(const eif_embedding_t* emb,
                                   const uint32_t* token_ids,
                                   uint32_t num_tokens,
                                   float32_t* output) {
    if (!emb || !token_ids || !output) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    for (uint32_t i = 0; i < num_tokens; i++) {
        uint32_t id = token_ids[i];
        if (id >= emb->vocab_size) {
            id = 1;  // Use UNK embedding
        }
        
        memcpy(&output[i * emb->embed_dim],
               &emb->weights[id * emb->embed_dim],
               emb->embed_dim * sizeof(float32_t));
    }
    
    return EIF_STATUS_OK;
}

void eif_embedding_free(eif_embedding_t* emb) {
    if (emb) {
        emb->weights = NULL;
        emb->vocab_size = 0;
        emb->embed_dim = 0;
    }
}
