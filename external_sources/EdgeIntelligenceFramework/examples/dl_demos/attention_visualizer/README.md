# Attention Visualization Demo

Visualize self-attention weights with ASCII graphics.

## Quick Start

```bash
cd build && make attention_visualizer
./bin/attention_visualizer
```

## What It Shows

1. **Attention Heatmap**: Matrix showing attention weights between all token pairs
2. **Bar Charts**: Detailed view of attention from specific query tokens
3. **Statistics**: Max attention targets and entropy per position

## Output Example

```
📝 Input sequence:
   "The cat sat on the mat . [END]"

Attention Heatmap:
─────────────────────────────────────────────────
        The   cat   sat   on    the   mat   .     [END]
The     █0.85 ░0.02 ░0.03 ░0.02 ░0.04 ░0.02 ░0.01 ░0.01
cat     ░0.05 █0.42 ▓0.28 ░0.05 ░0.03 ▒0.15 ░0.01 ░0.01
sat     ░0.03 ▓0.25 █0.45 ░0.08 ░0.04 ▒0.12 ░0.02 ░0.01
...

Attention from 'cat' to all tokens:
─────────────────────────────────────────────────
  The    [███░░░░░░░░░░░░░░░░░░░░░░░░░░░] 0.05
  cat    [████████████░░░░░░░░░░░░░░░░░░] 0.42
  sat    [████████░░░░░░░░░░░░░░░░░░░░░░] 0.28
  mat    [████░░░░░░░░░░░░░░░░░░░░░░░░░░] 0.15
  ...
```

## Understanding Attention

- **High weight (█)**: Model considers these tokens strongly related
- **Low weight (░)**: Tokens have weak relationship
- **Entropy**: Low = focused attention, High = distributed attention

## API Used

```c
#include "eif_attention.h"

// Simple self-attention
eif_self_attention_simple(embeddings, W_qkv, output, seq_len, dim);

// Softmax for visualization
eif_attn_softmax_row(scores, seq_len);
```
