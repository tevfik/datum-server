# RNN Sequence Classifier Demo

Demonstrates using LSTM for gesture sequence classification.

## Quick Start

```bash
cd build && make sequence_classifier
./bin/sequence_classifier --batch
```

## Output

```
╔═══════════════════════════════════════════════════════════════╗
║          🧠 RNN Sequence Classifier Demo                       ║
╚═══════════════════════════════════════════════════════════════╝

Testing 4 gesture types with LSTM classifier:

  Wave     X:▃▆█▆▃▁▃▆█▆▃▁▃▆█▆▃▁▃▆ Y:▄▅▆▆▅▄▃▂▂▃▄▅▆▆▅▄▃▂▂▃ → ✅ Wave
  Circle   X:█▇▆▄▂▁▁▂▄▆▇█▇▆▄▂▁▁▂▄ Y:▄▆▇█▇▆▄▂▁▁▂▄▆▇█▇▆▄▂▁ → ✅ Circle
  Swipe    X:▄▄▄▄▅▆▇█▇▆▅▄▄▄▄▄▄▄▄▄ Y:▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄ → ✅ Swipe
  Tap      X:▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄ Y:▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄ → ✅ Tap
```

## Architecture

```
Input (20 timesteps x 3 axes)
         ↓
    LSTM (16 hidden units)
         ↓
    Dense (4 classes)
         ↓
    Softmax → Prediction
```

## LSTM Features

- **Q15 Fixed-Point**: All computations in 16-bit
- **LUT Activations**: Fast sigmoid/tanh via lookup tables
- **Stateful Option**: Persist state between calls
- **Memory Efficient**: ~1KB for hidden state

## Files

| File | Description |
|------|-------------|
| `main.c` | Demo application |
| `eif_rnn.h` | RNN/LSTM/GRU implementation |
