# NLP Tutorial: Tiny Transformer for Text Processing

## Learning Objectives

- Tokenization and vocabulary management
- Word embeddings
- Attention mechanism basics
- Tiny transformer inference on MCU

**Level**: Intermediate to Advanced  
**Time**: 45 minutes

---

## 1. Text Processing Pipeline

```
"Hello world" → Tokenize → Embed → Transform → Output
                 [5, 12]   [vec]    [attn]     [logits]
```

---

## 2. Tokenization

### Word-Level Tokenizer

```c
eif_tokenizer_t tok;
eif_tokenizer_init(&tok, vocab_size, &pool);

// Build vocabulary
eif_tokenizer_add_token(&tok, "<pad>", 0);
eif_tokenizer_add_token(&tok, "<unk>", 1);
eif_tokenizer_add_token(&tok, "hello", 2);
eif_tokenizer_add_token(&tok, "world", 3);

// Tokenize text
int tokens[MAX_SEQ];
int n_tokens;
eif_tokenizer_encode(&tok, "hello world", tokens, &n_tokens);
// tokens = [2, 3], n_tokens = 2
```

### BPE (Byte-Pair Encoding)

More efficient for unknown words:
```
"playing" → ["play", "##ing"] → [45, 102]
```

---

## 3. Embeddings

```c
// Embedding layer: vocab_size × embed_dim
eif_embedding_t embed;
eif_embedding_init(&embed, vocab_size, embed_dim, &pool);

// Look up embeddings
float embedded[MAX_SEQ][EMBED_DIM];
for (int i = 0; i < n_tokens; i++) {
    eif_embedding_lookup(&embed, tokens[i], embedded[i]);
}
```

---

## 4. Attention Mechanism

### Self-Attention

```
Attention(Q, K, V) = softmax(QKᵀ/√d) × V

Q = Query (what am I looking for?)
K = Key (what do I contain?)
V = Value (what do I return?)
```

### EIF Implementation

```c
eif_attention_t attn;
eif_attention_init(&attn, embed_dim, n_heads, &pool);

// Self-attention on embedded sequence
float output[MAX_SEQ][EMBED_DIM];
eif_attention_forward(&attn, embedded, n_tokens, output);
```

---

## 5. Tiny Transformer

### Architecture

```
┌─────────────────┐
│ Token Embedding │ → [batch, seq, embed]
├─────────────────┤
│ Position Embed  │ → Add positional info
├─────────────────┤
│ Self-Attention  │ → 2 heads, 64 dim
├─────────────────┤
│ Feed-Forward    │ → 128 hidden
├─────────────────┤
│ Output Layer    │ → vocab_size logits
└─────────────────┘
```

### Inference

```c
eif_tiny_transformer_t model;
eif_tiny_transformer_init(&model, &config, &pool);

// Forward pass
float logits[VOCAB_SIZE];
eif_tiny_transformer_forward(&model, tokens, n_tokens, logits);

// Get predicted token
int next_token = argmax(logits, VOCAB_SIZE);
```

---

## 6. ESP32 Example: Command Parser

```c
void nlp_task(void* arg) {
    // Simple command classification
    // "turn on light" → CLASS_LIGHT_ON
    // "set temp 72"   → CLASS_SET_TEMP
    
    while (1) {
        char text[64];
        if (uart_read_line(text, sizeof(text))) {
            // Tokenize
            int tokens[10];
            int n;
            eif_tokenizer_encode(&tok, text, tokens, &n);
            
            // Classify
            float logits[NUM_CLASSES];
            eif_tiny_transformer_forward(&model, tokens, n, logits);
            
            int cmd = argmax(logits, NUM_CLASSES);
            execute_command(cmd);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
```

---

## 7. Summary

### Memory Requirements

| Component | ESP32 (520KB) |
|-----------|---------------|
| Embeddings (1000×64) | 256 KB |
| Attention (2 heads) | 32 KB |
| FF Layer | 64 KB |
| **Total** | ~350 KB |

### Key APIs
- `eif_tokenizer_*()` - Text to tokens
- `eif_embedding_*()` - Token to vector
- `eif_attention_*()` - Self-attention
- `eif_tiny_transformer_*()` - Full model
