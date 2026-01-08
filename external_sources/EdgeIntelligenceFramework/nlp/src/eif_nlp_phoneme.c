/**
 * @file eif_nlp_phoneme.c
 * @brief Phoneme-based NLP implementation
 */

#include "eif_nlp_phoneme.h"
#include <string.h>
#include <ctype.h>

// =============================================================================
// Phoneme String Table
// =============================================================================

static const char* phoneme_strs[] = {
    "AA", "AE", "AH", "AO", "AW", "AY", "EH", "ER", "EY", "IH",
    "IY", "OW", "OY", "UH", "UW", "B", "D", "G", "K", "P",
    "T", "CH", "JH", "DH", "F", "S", "SH", "TH", "V", "Z",
    "ZH", "M", "N", "NG", "L", "R", "W", "Y", "HH", "SIL", "UNK"
};

// =============================================================================
// Phoneme Dictionary Implementation
// =============================================================================

eif_status_t eif_phoneme_dict_init(eif_phoneme_dict_t* dict,
                                    uint32_t capacity,
                                    eif_memory_pool_t* pool) {
    if (!dict || !pool || capacity == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    dict->entries = (eif_phoneme_entry_t*)eif_memory_alloc(pool,
        capacity * sizeof(eif_phoneme_entry_t), sizeof(void*));
    if (!dict->entries) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    dict->size = 0;
    dict->capacity = capacity;
    dict->pool = pool;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_phoneme_dict_add(eif_phoneme_dict_t* dict,
                                   const char* word,
                                   const eif_phoneme_seq_t* phonemes) {
    if (!dict || !word || !phonemes) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    if (dict->size >= dict->capacity) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    eif_phoneme_entry_t* entry = &dict->entries[dict->size];
    strncpy(entry->word, word, EIF_PHONEME_MAX_WORD_LEN - 1);
    entry->word[EIF_PHONEME_MAX_WORD_LEN - 1] = '\0';
    
    // Convert word to uppercase for matching
    for (int i = 0; entry->word[i]; i++) {
        entry->word[i] = (char)toupper((unsigned char)entry->word[i]);
    }
    
    memcpy(&entry->phonemes, phonemes, sizeof(eif_phoneme_seq_t));
    dict->size++;
    
    return EIF_STATUS_OK;
}

const eif_phoneme_seq_t* eif_phoneme_dict_lookup(const eif_phoneme_dict_t* dict,
                                                   const char* word) {
    if (!dict || !word) return NULL;
    
    // Convert to uppercase for matching
    char upper[EIF_PHONEME_MAX_WORD_LEN];
    int i;
    for (i = 0; word[i] && i < EIF_PHONEME_MAX_WORD_LEN - 1; i++) {
        upper[i] = (char)toupper((unsigned char)word[i]);
    }
    upper[i] = '\0';
    
    // Linear search (could be binary search if sorted)
    for (uint32_t j = 0; j < dict->size; j++) {
        if (strcmp(dict->entries[j].word, upper) == 0) {
            return &dict->entries[j].phonemes;
        }
    }
    
    return NULL;
}

void eif_phoneme_dict_free(eif_phoneme_dict_t* dict) {
    if (dict) {
        dict->entries = NULL;
        dict->size = 0;
        dict->capacity = 0;
    }
}

// =============================================================================
// Simple G2P Rules (English)
// =============================================================================

// Common English G2P patterns
static const struct {
    const char* pattern;
    eif_phoneme_t phoneme;
} simple_rules[] = {
    // Vowels
    {"a", EIF_PH_AE},
    {"e", EIF_PH_EH},
    {"i", EIF_PH_IH},
    {"o", EIF_PH_AA},
    {"u", EIF_PH_AH},
    
    // Consonants
    {"b", EIF_PH_B},
    {"c", EIF_PH_K},
    {"d", EIF_PH_D},
    {"f", EIF_PH_F},
    {"g", EIF_PH_G},
    {"h", EIF_PH_HH},
    {"j", EIF_PH_JH},
    {"k", EIF_PH_K},
    {"l", EIF_PH_L},
    {"m", EIF_PH_M},
    {"n", EIF_PH_N},
    {"p", EIF_PH_P},
    {"q", EIF_PH_K},
    {"r", EIF_PH_R},
    {"s", EIF_PH_S},
    {"t", EIF_PH_T},
    {"v", EIF_PH_V},
    {"w", EIF_PH_W},
    {"x", EIF_PH_K},
    {"y", EIF_PH_Y},
    {"z", EIF_PH_Z},
    
    // Digraphs
    {"ch", EIF_PH_CH},
    {"sh", EIF_PH_SH},
    {"th", EIF_PH_TH},
    {"ng", EIF_PH_NG},
    {"ph", EIF_PH_F},
    {"wh", EIF_PH_W},
};

#define NUM_SIMPLE_RULES (sizeof(simple_rules) / sizeof(simple_rules[0]))

eif_status_t eif_g2p_init_english(eif_g2p_t* g2p, eif_memory_pool_t* pool) {
    if (!g2p || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    g2p->pool = pool;
    g2p->dict = NULL;
    g2p->rules = NULL;
    g2p->num_rules = 0;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_g2p_convert(const eif_g2p_t* g2p,
                              const char* word,
                              eif_phoneme_seq_t* output) {
    if (!g2p || !word || !output) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Check dictionary first
    if (g2p->dict) {
        const eif_phoneme_seq_t* seq = eif_phoneme_dict_lookup(g2p->dict, word);
        if (seq) {
            memcpy(output, seq, sizeof(eif_phoneme_seq_t));
            return EIF_STATUS_OK;
        }
    }
    
    // Fall back to simple rule-based conversion
    memset(output, 0, sizeof(eif_phoneme_seq_t));
    
    size_t len = strlen(word);
    size_t i = 0;
    
    while (i < len && output->length < EIF_PHONEME_MAX_SEQ_LEN) {
        char c = (char)tolower((unsigned char)word[i]);
        char next = (i + 1 < len) ? (char)tolower((unsigned char)word[i + 1]) : '\0';
        
        bool matched = false;
        
        // Try digraphs first
        if (next) {
            char digraph[3] = {c, next, '\0'};
            for (size_t r = 0; r < NUM_SIMPLE_RULES; r++) {
                if (strcmp(simple_rules[r].pattern, digraph) == 0) {
                    output->phonemes[output->length++] = simple_rules[r].phoneme;
                    i += 2;
                    matched = true;
                    break;
                }
            }
        }
        
        // Try single character
        if (!matched) {
            char single[2] = {c, '\0'};
            for (size_t r = 0; r < NUM_SIMPLE_RULES; r++) {
                if (strcmp(simple_rules[r].pattern, single) == 0) {
                    output->phonemes[output->length++] = simple_rules[r].phoneme;
                    matched = true;
                    break;
                }
            }
            i++;
        }
        
        if (!matched) {
            i++;  // Skip unknown character
        }
    }
    
    return EIF_STATUS_OK;
}

void eif_g2p_free(eif_g2p_t* g2p) {
    if (g2p) {
        g2p->rules = NULL;
        g2p->num_rules = 0;
    }
}

// =============================================================================
// Phoneme Utilities
// =============================================================================

const char* eif_phoneme_to_str(eif_phoneme_t phoneme) {
    if (phoneme >= 0 && phoneme < EIF_PH_COUNT) {
        return phoneme_strs[phoneme];
    }
    return "UNK";
}

eif_phoneme_t eif_phoneme_from_str(const char* str) {
    if (!str) return EIF_PH_UNK;
    
    for (int i = 0; i < EIF_PH_COUNT; i++) {
        if (strcmp(phoneme_strs[i], str) == 0) {
            return (eif_phoneme_t)i;
        }
    }
    return EIF_PH_UNK;
}

int eif_phoneme_distance(const eif_phoneme_seq_t* a, 
                          const eif_phoneme_seq_t* b) {
    if (!a || !b) return -1;
    
    // Simple Levenshtein distance for phoneme sequences
    int m = a->length;
    int n = b->length;
    
    // Use simple O(min(m,n)) space approach
    if (m == 0) return n;
    if (n == 0) return m;
    
    // Simple distance computation (not full Levenshtein for memory efficiency)
    int diff = 0;
    int max_len = (m > n) ? m : n;
    int min_len = (m < n) ? m : n;
    
    for (int i = 0; i < min_len; i++) {
        if (a->phonemes[i] != b->phonemes[i]) {
            diff++;
        }
    }
    
    return diff + (max_len - min_len);
}

bool eif_phoneme_is_vowel(eif_phoneme_t phoneme) {
    return phoneme >= EIF_PH_AA && phoneme <= EIF_PH_UW;
}

bool eif_phoneme_is_consonant(eif_phoneme_t phoneme) {
    return phoneme >= EIF_PH_B && phoneme <= EIF_PH_HH;
}
