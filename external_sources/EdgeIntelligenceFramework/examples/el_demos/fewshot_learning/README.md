# Few-Shot Learning Demo - Prototypical Networks

Learn to recognize new gestures from just 3 examples each.

## Scenario

A wearable device learns to recognize 5 custom gestures:
- Wave, Swipe, Tap, Circle, Pinch
- Only 3 training examples per gesture
- Classify new gestures by distance to learned prototypes

## Features

- **5-way 3-shot**: Recognize 5 classes from 3 examples each
- **Prototype visualization**: ASCII embedding representation
- **Probability bars**: Per-class confidence scores
- **Accuracy analysis**: Per-class and overall metrics

## Build & Run

```bash
cmake -B build && cmake --build build --target fewshot_learning_demo
./build/bin/fewshot_learning_demo
```
