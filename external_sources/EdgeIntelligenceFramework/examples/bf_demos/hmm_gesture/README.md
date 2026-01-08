# HMM Gesture Recognition Demo

This demo illustrates using **Discrete Hidden Markov Models (HMM)** for simple series matching.

## Algorithm
- **Forward Algorithm**: Computes the probability of an observation sequence given a model (`P(O|Model)`).
- **Viterbi Algorithm**: Decodes the most likely sequence of hidden states.
- **Models**:
  - `SWIPE UP`: States prefer `UP` symbol in middle.
  - `SWIPE DOWN`: States prefer `DOWN` symbol in middle.

## Usage
```bash
./bin/hmm_gesture_demo          # ASCII output
./bin/hmm_gesture_demo --json   # JSON output with probs and path
```

## JSON Output
```json
{
  "type": "hmm_gesture",
  "prediction": "SWIPE UP",
  "log_probs": {"up": -2.3, "down": -10.5},
  "viterbi_path": [0, 1, 1, 0]
}
```
