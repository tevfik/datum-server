# Tiny Transformer Demo

Intent classification for voice assistants using a lightweight transformer.

## Features

- 2-layer transformer with multi-head attention
- Token + positional embeddings  
- Classification head for intent detection
- Memory usage analysis

## Usage

```bash
cd build && make transformer_demo && ./bin/transformer_demo
```

## Example Output

```
Model Configuration:
  Layers:     2
  Embed dim:  64
  Heads:      4
  FF dim:     128
  
Input                          Predicted       Logits
play music                     play_music      [0.12, -0.03, ...]
turn on the lights             smart_home      [-0.05, 0.18, ...]

Memory Usage:
Model memory:     45.2 KB
Pool used:        52.3 KB
```

## Intents Supported

| Intent | Example |
|--------|---------|
| play_music | "play music" |
| smart_home | "turn on the lights" |
| set_timer | "set timer for minutes" |
| get_weather | "what is weather today" |
| communication | "call mom" |

## Scaling

| Config | Memory |
|--------|--------|
| Tiny (demo) | 45 KB |
| Small | 512 KB |
| Medium | 2 MB |
