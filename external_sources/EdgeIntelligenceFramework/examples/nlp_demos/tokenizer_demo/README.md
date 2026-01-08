# NLP Tokenizer and Phoneme Demo

This demo showcases the EIF Natural Language Processing module capabilities for embedded systems.

## Features Demonstrated

### 1. Tokenization
- **Character-level**: Processes text as individual characters (useful for byte-level models)
- **Whitespace**: Splits text on whitespace for word-level processing

### 2. Vocabulary Management
- Special tokens: `[PAD]`, `[UNK]`, `[BOS]`, `[EOS]`
- Dynamic token addition
- Bidirectional token/ID lookup

### 3. Phoneme Processing
- **ARPABET**: Standard 39-phoneme set used in CMU Pronouncing Dictionary
- **G2P (Grapheme-to-Phoneme)**: Converts text to phoneme sequences
- **Phoneme Distance**: Edit distance for similarity matching
- **Dictionary Lookup**: Fast phoneme lookup for known words

## Building

```bash
mkdir build && cd build
cmake ..
make nlp_demo
```

## Running

```bash
./bin/nlp_demo
```

## Expected Output

```
========================================
EIF NLP Module Demo
========================================

=== Demo 1: Character Tokenization ===
Input: "Hello"
Tokens (5): H(72) e(101) l(108) l(108) o(111) 
Decoded: "Hello"

=== Demo 2: Vocabulary Management ===
Vocabulary size: 8
Special tokens:
  [PAD] -> ID 0
  [UNK] -> ID 1
  [BOS] -> ID 2
  [EOS] -> ID 3

=== Demo 3: Grapheme-to-Phoneme (G2P) ===
Word -> Phoneme conversion (ARPABET):
  hello      -> HH EH L L OW 
  world      -> W OW R L D 
  ...
```

## Applications

| Use Case | Description |
|----------|-------------|
| **Keyword Spotting (KWS)** | Match spoken words using phoneme sequences |
| **TTS Frontend** | Convert text to phonemes for speech synthesis |
| **Fuzzy Matching** | Find similar-sounding words using phoneme distance |
| **Pronunciation** | Build pronunciation dictionaries for custom vocabulary |

## Memory Usage

- Typical memory pool: 8KB for demo
- Per-word phoneme sequence: ~20 bytes
- Dictionary entry: ~50 bytes

## ARPABET Phoneme Set

The module uses the standard ARPABET phoneme set:

**Vowels (15):** AA, AE, AH, AO, AW, AY, EH, ER, EY, IH, IY, OW, OY, UH, UW

**Consonants (24):** B, CH, D, DH, F, G, HH, JH, K, L, M, N, NG, P, R, S, SH, T, TH, V, W, Y, Z, ZH

## API Reference

See `eif_nlp.h` and `eif_nlp_phoneme.h` for complete API documentation.
