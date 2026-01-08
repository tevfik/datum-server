# NLP: Word Embeddings (Lightweight)

## Overview
Store and query word vectors (embeddings) on edge devices. This enables semantic search, analogy resolution, and simple intent classification by comparing vector directions (Cosine Similarity).

## API Usage

### 1. Initialization
```c
#include "eif_nlp_embedding.h"

eif_embedding_t emb;
// Capacity 100 words, 50 dimensions
eif_word_embedding_init(&emb, 100, 50);
```

### 2. Loading Vectors
Vectors are typically pre-trained (e.g., GloVe, Word2Vec) and quantized/reduced before loading.
```c
float vec[] = {0.1, -0.2, ...};
eif_embedding_add(&emb, "king", vec);
```

### 3. Similarity Check
```c
float score = eif_embedding_similarity(&emb, "king", "queen");
// Returns value between -1.0 (Opposite) and 1.0 (Identical)
```

## Use Cases
- **Command Alias**: "Turn on light" vs "Enable lamp". If "light" and "lamp" are close, they can map to same action.
- **Context Awareness**: Detecting topic of user speech.

## Example
See `examples/nlp_demos/word_embedding/main.c` for an implementation of the classic "King - Man + Woman = Queen" analogy check.
