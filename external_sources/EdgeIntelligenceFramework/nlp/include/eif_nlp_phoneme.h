/**
 * @file eif_nlp_phoneme.h
 * @brief Phoneme-based NLP processing for embedded systems
 * 
 * This module provides phoneme-level text processing capabilities:
 * - Grapheme-to-Phoneme (G2P) conversion
 * - Phoneme sequence encoding
 * - CMU Pronouncing Dictionary compatible format
 * - Phoneme-based text similarity
 * 
 * Designed for:
 * - Keyword spotting (KWS)
 * - Speech recognition preprocessing
 * - Pronunciation dictionaries
 * - Text-to-Speech (TTS) frontend
 */

#ifndef EIF_NLP_PHONEME_H
#define EIF_NLP_PHONEME_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constants
// =============================================================================

#define EIF_PHONEME_MAX_WORD_LEN    32
#define EIF_PHONEME_MAX_SEQ_LEN     16
#define EIF_PHONEME_VOCAB_SIZE      64

// CMU ARPABET Phoneme Set (39 phonemes + stress markers)
typedef enum {
    // Vowels
    EIF_PH_AA = 0,  // odd     AA D
    EIF_PH_AE,      // at      AE T
    EIF_PH_AH,      // hut     HH AH T
    EIF_PH_AO,      // ought   AO T
    EIF_PH_AW,      // cow     K AW
    EIF_PH_AY,      // hide    HH AY D
    EIF_PH_EH,      // Ed      EH D
    EIF_PH_ER,      // hurt    HH ER T
    EIF_PH_EY,      // ate     EY T
    EIF_PH_IH,      // it      IH T
    EIF_PH_IY,      // eat     IY T
    EIF_PH_OW,      // oat     OW T
    EIF_PH_OY,      // toy     T OY
    EIF_PH_UH,      // hood    HH UH D
    EIF_PH_UW,      // two     T UW
    
    // Consonants - Stops
    EIF_PH_B,       // be      B IY
    EIF_PH_D,       // dee     D IY
    EIF_PH_G,       // green   G R IY N
    EIF_PH_K,       // key     K IY
    EIF_PH_P,       // pee     P IY
    EIF_PH_T,       // tea     T IY
    
    // Consonants - Affricates  
    EIF_PH_CH,      // cheese  CH IY Z
    EIF_PH_JH,      // jee     JH IY
    
    // Consonants - Fricatives
    EIF_PH_DH,      // thee    DH IY
    EIF_PH_F,       // fee     F IY
    EIF_PH_S,       // sea     S IY
    EIF_PH_SH,      // she     SH IY
    EIF_PH_TH,      // theta   TH EY T AH
    EIF_PH_V,       // vee     V IY
    EIF_PH_Z,       // zee     Z IY
    EIF_PH_ZH,      // seizure S IY ZH ER
    
    // Consonants - Nasals
    EIF_PH_M,       // me      M IY
    EIF_PH_N,       // knee    N IY
    EIF_PH_NG,      // ping    P IH NG
    
    // Consonants - Liquids
    EIF_PH_L,       // lee     L IY
    EIF_PH_R,       // read    R IY D
    
    // Consonants - Semivowels
    EIF_PH_W,       // we      W IY
    EIF_PH_Y,       // yield   Y IY L D
    EIF_PH_HH,      // he      HH IY
    
    // Special
    EIF_PH_SIL,     // silence
    EIF_PH_UNK,     // unknown
    
    EIF_PH_COUNT    // Total count
} eif_phoneme_t;

// =============================================================================
// Types
// =============================================================================

/**
 * @brief Phoneme sequence for a word
 */
typedef struct {
    eif_phoneme_t phonemes[EIF_PHONEME_MAX_SEQ_LEN];
    uint8_t stress[EIF_PHONEME_MAX_SEQ_LEN];  // 0=no stress, 1=primary, 2=secondary
    uint8_t length;
} eif_phoneme_seq_t;

/**
 * @brief Dictionary entry mapping word to phonemes
 */
typedef struct {
    char word[EIF_PHONEME_MAX_WORD_LEN];
    eif_phoneme_seq_t phonemes;
} eif_phoneme_entry_t;

/**
 * @brief Phoneme dictionary (pronunciation lexicon)
 */
typedef struct {
    eif_phoneme_entry_t* entries;
    uint32_t size;
    uint32_t capacity;
    eif_memory_pool_t* pool;
} eif_phoneme_dict_t;

/**
 * @brief Simple G2P (Grapheme-to-Phoneme) rules
 */
typedef struct {
    char grapheme[4];           // Input grapheme pattern
    eif_phoneme_t phoneme;      // Output phoneme
    char context_before[4];     // Required context before (empty = any)
    char context_after[4];      // Required context after (empty = any)
} eif_g2p_rule_t;

/**
 * @brief G2P converter configuration
 */
typedef struct {
    eif_g2p_rule_t* rules;
    uint32_t num_rules;
    eif_phoneme_dict_t* dict;   // Optional: lookup dictionary first
    eif_memory_pool_t* pool;
} eif_g2p_t;

// =============================================================================
// Phoneme Dictionary API
// =============================================================================

/**
 * @brief Initialize phoneme dictionary
 */
eif_status_t eif_phoneme_dict_init(eif_phoneme_dict_t* dict,
                                    uint32_t capacity,
                                    eif_memory_pool_t* pool);

/**
 * @brief Add word-phoneme mapping to dictionary
 */
eif_status_t eif_phoneme_dict_add(eif_phoneme_dict_t* dict,
                                   const char* word,
                                   const eif_phoneme_seq_t* phonemes);

/**
 * @brief Look up phonemes for a word
 */
const eif_phoneme_seq_t* eif_phoneme_dict_lookup(const eif_phoneme_dict_t* dict,
                                                   const char* word);

/**
 * @brief Free dictionary resources
 */
void eif_phoneme_dict_free(eif_phoneme_dict_t* dict);

// =============================================================================
// G2P (Grapheme-to-Phoneme) API
// =============================================================================

/**
 * @brief Initialize G2P converter with default English rules
 */
eif_status_t eif_g2p_init_english(eif_g2p_t* g2p, eif_memory_pool_t* pool);

/**
 * @brief Convert text to phoneme sequence
 */
eif_status_t eif_g2p_convert(const eif_g2p_t* g2p,
                              const char* word,
                              eif_phoneme_seq_t* output);

/**
 * @brief Free G2P resources
 */
void eif_g2p_free(eif_g2p_t* g2p);

// =============================================================================
// Phoneme Utilities
// =============================================================================

/**
 * @brief Get string representation of phoneme (ARPABET)
 */
const char* eif_phoneme_to_str(eif_phoneme_t phoneme);

/**
 * @brief Parse phoneme from ARPABET string
 */
eif_phoneme_t eif_phoneme_from_str(const char* str);

/**
 * @brief Calculate phoneme edit distance (Levenshtein)
 */
int eif_phoneme_distance(const eif_phoneme_seq_t* a, 
                          const eif_phoneme_seq_t* b);

/**
 * @brief Check if phoneme is a vowel
 */
bool eif_phoneme_is_vowel(eif_phoneme_t phoneme);

/**
 * @brief Check if phoneme is a consonant
 */
bool eif_phoneme_is_consonant(eif_phoneme_t phoneme);

#ifdef __cplusplus
}
#endif

#endif // EIF_NLP_PHONEME_H
