# Tokenizer Tutorial: Text Processing for NLP

## Learning Objectives

- Character and word-level tokenization
- Byte-Pair Encoding (BPE) algorithm
- Vocabulary building
- Text encoding/decoding

**Level**: Beginner to Intermediate  
**Time**: 30 minutes

---

## 1. What is Tokenization?

Converting text to numbers for neural networks:

```
"Hello world!" → [15496, 995, 0]  (word tokens)
"Hello world!" → "Hello", "world", "!"  (words)
"Hello world!" → ['H','e','l','l','o',' ','w',...] (chars)
```

---

## 2. Tokenization Methods

### 2.1 Character-Level

```c
char vocab[] = "abcdefghijklmnopqrstuvwxyz ";

int char_to_id(char c) {
    for (int i = 0; i < strlen(vocab); i++) {
        if (vocab[i] == c) return i;
    }
    return -1;  // Unknown
}

// "hello" → [7, 4, 11, 11, 14]
```

### 2.2 Word-Level

```c
typedef struct {
    char* words[10000];
    int count;
} word_vocab_t;

int word_to_id(word_vocab_t* v, const char* word) {
    for (int i = 0; i < v->count; i++) {
        if (strcmp(v->words[i], word) == 0) return i;
    }
    return 0;  // <UNK> token
}
```

### 2.3 Byte-Pair Encoding (BPE)

Most powerful for NLP:

```
Training:
1. Start with character vocabulary
2. Find most frequent pair (e.g., "t" + "h")
3. Merge to new token "th"
4. Repeat N times

Result: Subword vocabulary
"unhappiness" → ["un", "happ", "iness"]
```

---

## 3. EIF Implementation

```c
eif_tokenizer_t tok;
eif_tokenizer_init(&tok, vocab_size, &pool);

// Add special tokens
eif_tokenizer_add_token(&tok, "<pad>", 0);
eif_tokenizer_add_token(&tok, "<unk>", 1);
eif_tokenizer_add_token(&tok, "<sos>", 2);
eif_tokenizer_add_token(&tok, "<eos>", 3);

// Build vocabulary from text
eif_tokenizer_fit(&tok, corpus, corpus_len);

// Encode text
int tokens[100];
int n_tokens;
eif_tokenizer_encode(&tok, "hello world", tokens, &n_tokens);

// Decode tokens
char text[256];
eif_tokenizer_decode(&tok, tokens, n_tokens, text);
```

---

## 4. Memory Considerations

| Method | Vocab Size | Memory |
|--------|------------|--------|
| Character | 128 | 0.5 KB |
| Word (10K) | 10,000 | 100 KB |
| BPE (8K) | 8,000 | 80 KB |

For ESP32: Use character or small BPE vocabulary.

---

## 5. ESP32 Example

```c
void nlp_task(void* arg) {
    eif_tokenizer_t tok;
    eif_tokenizer_init(&tok, 256, &pool);  // Char-level
    
    // Pre-load fixed vocabulary
    for (int i = 0; i < 128; i++) {
        char c[2] = {(char)i, '\0'};
        eif_tokenizer_add_token(&tok, c, i);
    }
    
    while (1) {
        char input[64];
        uart_read_line(input, 64);
        
        int tokens[64];
        int n;
        eif_tokenizer_encode(&tok, input, tokens, &n);
        
        // Process tokens with model...
    }
}
```

---

## Summary

### Key APIs
- `eif_tokenizer_init()` - Initialize
- `eif_tokenizer_fit()` - Build vocabulary
- `eif_tokenizer_encode()` - Text → tokens
- `eif_tokenizer_decode()` - Tokens → text
