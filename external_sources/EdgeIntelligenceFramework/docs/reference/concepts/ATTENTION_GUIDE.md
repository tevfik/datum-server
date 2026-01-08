# Attention Mechanisms Guide

Understanding and using attention for embedded NLP and sequence modeling.

---

## What is Attention?

Attention allows a model to focus on relevant parts of an input sequence when producing outputs. Instead of compressing all information into a fixed-size vector, attention maintains access to all input positions.

```
Query: "What does 'cat' relate to?"

Input:   The   cat   sat   on   the   mat
Weights: 0.05  0.42  0.28  0.05  0.05  0.15
                ▲     ▲               ▲
           "cat" attends strongly to "sat" and "mat"
```

---

## Scaled Dot-Product Attention

The core attention operation:

```
Attention(Q, K, V) = softmax(Q × K^T / √d_k) × V
```

**Implementation:**

```c
#include "eif_attention.h"

eif_scaled_dot_product_attention(
    query, key, value,    // Input tensors
    output, scores,       // Output + optional scores
    seq_len, head_dim,
    scale,                // 1/sqrt(head_dim) in Q15
    causal                // Mask future positions
);
```

---

## Multi-Head Attention

Split attention into multiple "heads" to learn different relationship types:

```c
eif_attention_t attn;
eif_attention_init(&attn, seq_len, embed_dim, num_heads);

// Set weights (from trained model)
attn.W_q = query_weights;
attn.W_k = key_weights;
attn.W_v = value_weights;
attn.W_o = output_weights;

// Run attention
eif_multi_head_attention(&attn, input, output, workspace, scores);
```

---

## Memory-Efficient Self-Attention

For very constrained devices, use the streaming variant:

```c
// Processes one position at a time
// Requires only O(d) memory instead of O(n²)
eif_self_attention_simple(
    embeddings,      // [seq_len × dim]
    W_qkv,          // Combined QKV weights
    output,
    seq_len, dim
);
```

---

## Visualization

The attention visualizer demo shows attention patterns:

```bash
make attention_visualizer
./bin/attention_visualizer
```

Output:
```
Attention Heatmap:
─────────────────────────────────────────────────
        The   cat   sat   on    the   mat
The     █0.85 ░0.02 ░0.03 ░0.02 ░0.04 ░0.02
cat     ░0.05 █0.42 ▓0.28 ░0.05 ░0.03 ▒0.15
sat     ░0.03 ▓0.25 █0.45 ░0.08 ░0.04 ▒0.12
```

---

## Memory Requirements

| Configuration | Memory (bytes) |
|---------------|----------------|
| seq=8, dim=16 | ~512 |
| seq=32, dim=64 | ~8KB |
| seq=64, dim=128 | ~32KB |

**Formula:** `3 × seq_len × embed_dim × 2` bytes (Q, K, V tensors)

---

## Use Cases on MCU

1. **Keyword Spotting**: Attend to relevant audio frames
2. **Gesture Recognition**: Focus on key motion phases
3. **Sensor Fusion**: Weight sensor contributions dynamically
4. **Text Classification**: (with small vocabulary)

---

## API Reference

### Initialization

```c
eif_attention_t attn;
eif_attention_init(&attn, seq_len, embed_dim, num_heads);
attn.causal = true;  // For autoregressive models
```

### Operations

| Function | Description |
|----------|-------------|
| `eif_scaled_dot_product_attention()` | Core attention |
| `eif_multi_head_attention()` | Multi-head with projections |
| `eif_self_attention_simple()` | Memory-efficient variant |
| `eif_attn_softmax_row()` | Row-wise softmax |

---

## See Also

- [Attention Demo](../../examples/dl_demos/attention_visualizer/)
- [RNN Fundamentals](RNN_FUNDAMENTALS.md)
