/**
 * @file eif_nlp_tokenizer.h
 * @brief Simple Text Tokenizer
 *
 * Lightweight tokenization for embedded NLP:
 * - Word-level tokenization
 * - Simple vocabulary lookup
 * - Basic text normalization
 *
 * Designed for keyword spotting and simple commands.
 */

#ifndef EIF_NLP_TOKENIZER_H
#define EIF_NLP_TOKENIZER_H

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_TOK_MAX_VOCAB 256
#define EIF_TOK_MAX_WORD_LEN 32
#define EIF_TOK_MAX_TOKENS 32

/**
 * @brief Vocabulary entry
 */
typedef struct {
  char word[EIF_TOK_MAX_WORD_LEN];
  int id;
} eif_vocab_entry_t;

/**
 * @brief Simple tokenizer
 */
typedef struct {
  eif_vocab_entry_t vocab[EIF_TOK_MAX_VOCAB];
  int vocab_size;
  int unk_id; ///< Unknown token ID
  int pad_id; ///< Padding token ID
} eif_tokenizer_t;

/**
 * @brief Initialize tokenizer
 */
static inline void eif_tokenizer_init(eif_tokenizer_t *tok) {
  tok->vocab_size = 0;
  tok->unk_id = 0;
  tok->pad_id = 1;

  // Add special tokens
  strncpy(tok->vocab[0].word, "<UNK>", EIF_TOK_MAX_WORD_LEN - 1);
  tok->vocab[0].word[EIF_TOK_MAX_WORD_LEN - 1] = '\0';
  tok->vocab[0].id = 0;
  strncpy(tok->vocab[1].word, "<PAD>", EIF_TOK_MAX_WORD_LEN - 1);
  tok->vocab[1].word[EIF_TOK_MAX_WORD_LEN - 1] = '\0';
  tok->vocab[1].id = 1;
  tok->vocab_size = 2;
}

/**
 * @brief Add word to vocabulary
 * @return Token ID
 */
static inline int eif_tokenizer_add_word(eif_tokenizer_t *tok,
                                         const char *word) {
  if (tok->vocab_size >= EIF_TOK_MAX_VOCAB) {
    return tok->unk_id;
  }

  // Check if already exists
  for (int i = 0; i < tok->vocab_size; i++) {
    if (strcmp(tok->vocab[i].word, word) == 0) {
      return i;
    }
  }

  // Add new word
  int id = tok->vocab_size;
  strncpy(tok->vocab[id].word, word, EIF_TOK_MAX_WORD_LEN - 1);
  tok->vocab[id].word[EIF_TOK_MAX_WORD_LEN - 1] = '\0';
  tok->vocab[id].id = id;
  tok->vocab_size++;

  return id;
}

/**
 * @brief Lookup word in vocabulary
 * @return Token ID (unk_id if not found)
 */
static inline int eif_tokenizer_lookup(eif_tokenizer_t *tok, const char *word) {
  for (int i = 0; i < tok->vocab_size; i++) {
    if (strcmp(tok->vocab[i].word, word) == 0) {
      return i;
    }
  }
  return tok->unk_id;
}

/**
 * @brief Convert to lowercase (in-place)
 */
static inline void to_lowercase(char *str) {
  for (int i = 0; str[i]; i++) {
    str[i] = tolower((unsigned char)str[i]);
  }
}

/**
 * @brief Tokenize text into IDs
 * @param text Input text
 * @param tokens Output token IDs
 * @param max_tokens Maximum tokens to extract
 * @return Number of tokens extracted
 */
static inline int eif_tokenizer_encode(eif_tokenizer_t *tok, const char *text,
                                       int *tokens, int max_tokens) {
  char buffer[256];
  strncpy(buffer, text, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  to_lowercase(buffer);

  int count = 0;
  char *word = strtok(buffer, " \t\n\r.,!?;:");

  while (word != NULL && count < max_tokens) {
    tokens[count++] = eif_tokenizer_lookup(tok, word);
    word = strtok(NULL, " \t\n\r.,!?;:");
  }

  return count;
}

/**
 * @brief Get word from ID
 */
static inline const char *eif_tokenizer_decode(eif_tokenizer_t *tok, int id) {
  if (id >= 0 && id < tok->vocab_size) {
    return tok->vocab[id].word;
  }
  return "<UNK>";
}

#ifdef __cplusplus
}
#endif

#endif // EIF_NLP_TOKENIZER_H
