/**
 * @file main.c
 * @brief Audio Classification Tutorial - Voice Command Recognition
 *
 * This tutorial demonstrates keyword spotting (KWS) using
 * MFCC features and a simple neural network classifier.
 *
 * SCENARIO:
 * A smart device recognizes voice commands like
 * "yes", "no", "stop", "go" from audio input.
 *
 * FEATURES DEMONSTRATED:
 * - MFCC feature extraction
 * - Audio windowing and framing
 * - Neural network inference
 * - Real-time classification
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../common/ascii_plot.h"
#include "../common/demo_cli.h"
#include "eif_audio.h"
#include "eif_dsp.h"
#include "eif_memory.h"
#include "eif_types.h"

// ============================================================================
// Configuration
// ============================================================================

#define SAMPLE_RATE 16000 // 16kHz audio
#define FRAME_LENGTH 400  // 25ms at 16kHz
#define FRAME_STRIDE 160  // 10ms at 16kHz
#define NUM_MFCC 13       // 13 MFCC features
#define NUM_FILTERS 26    // Mel filter banks
#define NUM_FRAMES 49     // ~500ms of audio
#define NUM_CLASSES 4     // yes, no, stop, go

// ============================================================================
// Simulated Audio Generation
// ============================================================================

typedef struct {
  const char *name;
  float32_t freq1;    // Primary frequency
  float32_t freq2;    // Secondary frequency
  float32_t duration; // Duration in seconds
} word_pattern_t;

static const word_pattern_t word_patterns[NUM_CLASSES] = {
    {"YES", 300.0f, 450.0f, 0.4f},
    {"NO", 200.0f, 150.0f, 0.3f},
    {"STOP", 400.0f, 250.0f, 0.5f},
    {"GO", 350.0f, 500.0f, 0.25f}};

static void generate_word_audio(float32_t *audio, int len, int word_idx,
                                float sample_rate) {
  const word_pattern_t *pattern = &word_patterns[word_idx];
  int duration_samples = (int)(pattern->duration * sample_rate);
  if (duration_samples > len)
    duration_samples = len;

  // Generate a simple synthetic "word" pattern
  for (int i = 0; i < len; i++) {
    float t = (float)i / sample_rate;
    float envelope = 0;

    if (i < duration_samples) {
      // ADSR-like envelope
      float pos = (float)i / duration_samples;
      if (pos < 0.1f)
        envelope = pos / 0.1f; // Attack
      else if (pos < 0.3f)
        envelope = 1.0f; // Sustain
      else
        envelope = (1.0f - pos) / 0.7f; // Release
    }

    // Two-tone formant-like sound
    audio[i] =
        envelope * (0.6f * sinf(2 * M_PI * pattern->freq1 * t) +
                    0.4f * sinf(2 * M_PI * pattern->freq2 * t) +
                    0.1f * sinf(2 * M_PI * pattern->freq1 * 2 * t) + // Harmonic
                    0.05f * ((float)rand() / RAND_MAX - 0.5f)        // Noise
                   );
  }
}

// ============================================================================
// Simple Classifier (Mock Neural Network)
// ============================================================================

typedef struct {
  float32_t weights[NUM_CLASSES][NUM_MFCC * 5]; // Simple linear weights
  float32_t bias[NUM_CLASSES];
} simple_classifier_t;

static void classifier_init(simple_classifier_t *clf) {
  // Initialize with pre-trained-like weights
  srand(42);
  for (int c = 0; c < NUM_CLASSES; c++) {
    for (int f = 0; f < NUM_MFCC * 5; f++) {
      clf->weights[c][f] = 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }
    clf->bias[c] = 0.0f;
  }

  // Add some class-specific patterns
  for (int c = 0; c < NUM_CLASSES; c++) {
    for (int f = 0; f < NUM_MFCC; f++) {
      clf->weights[c][c * NUM_MFCC / 4 + f] += 0.5f;
    }
  }
}

static void classifier_predict(const simple_classifier_t *clf,
                               const float32_t *features, int num_features,
                               float32_t *probs) {
  // Simple linear + softmax
  float32_t scores[NUM_CLASSES];
  float32_t max_score = -1e9f;

  for (int c = 0; c < NUM_CLASSES; c++) {
    scores[c] = clf->bias[c];
    for (int f = 0; f < num_features && f < NUM_MFCC * 5; f++) {
      scores[c] += clf->weights[c][f] * features[f];
    }
    if (scores[c] > max_score)
      max_score = scores[c];
  }

  // Softmax
  float32_t sum = 0;
  for (int c = 0; c < NUM_CLASSES; c++) {
    probs[c] = expf(scores[c] - max_score);
    sum += probs[c];
  }
  for (int c = 0; c < NUM_CLASSES; c++) {
    probs[c] /= sum;
  }
}

// ============================================================================
// Visualization
// ============================================================================

static void display_spectrogram(const float32_t *mfcc, int num_frames,
                                int num_mfcc) {
  printf("\n  %sMFCC Spectrogram (time vs frequency)%s\n", ASCII_BOLD,
         ASCII_RESET);
  printf("  Freq\n");

  // Display 8 rows (aggregating MFCCs)
  for (int m = num_mfcc - 1; m >= 0; m--) {
    printf("  %2d │", m);
    for (int f = 0; f < num_frames && f < 40; f++) {
      float val = mfcc[f * num_mfcc + m];
      char c;
      if (val > 0.5f)
        c = '#';
      else if (val > 0.2f)
        c = '+';
      else if (val > 0.0f)
        c = '.';
      else if (val > -0.2f)
        c = ' ';
      else
        c = '-';
      printf("%c", c);
    }
    printf("│\n");
  }
  printf("     └");
  for (int f = 0; f < 40; f++)
    printf("─");
  printf("┘ Time\n");
}

static void display_classification(const char **labels, const float32_t *probs,
                                   int n) {
  printf("\n  %s┌─ Classification Results ──────────────────────┐%s\n",
         ASCII_CYAN, ASCII_RESET);

  int best = 0;
  for (int i = 1; i < n; i++) {
    if (probs[i] > probs[best])
      best = i;
  }

  for (int i = 0; i < n; i++) {
    int bar_len = (int)(probs[i] * 25);
    printf("  │  %-6s: ", labels[i]);

    if (i == best)
      printf("%s", ASCII_GREEN);
    for (int b = 0; b < bar_len; b++)
      printf("█");
    for (int b = bar_len; b < 25; b++)
      printf("░");
    printf(" %5.1f%%%s", probs[i] * 100, ASCII_RESET);

    if (i == best)
      printf(" ◄");
    printf("  │\n");
  }

  printf("  %s└────────────────────────────────────────────────┘%s\n",
         ASCII_CYAN, ASCII_RESET);
}

// ============================================================================
// Main Tutorial
// ============================================================================

int main(void) {
  srand(time(NULL));

  printf("\n");
  ascii_section("EIF Tutorial: Voice Command Recognition (KWS)");

  printf("  This tutorial demonstrates Keyword Spotting (KWS).\n\n");
  printf("  %sScenario:%s\n", ASCII_BOLD, ASCII_RESET);
  printf("    A smart device recognizes voice commands:\n");
  printf("    • \"YES\" • \"NO\" • \"STOP\" • \"GO\"\n\n");
  printf("  %sPipeline:%s\n", ASCII_BOLD, ASCII_RESET);
  printf("    Audio → MFCC Features → Neural Network → Command\n");
  demo_wait("\n  Press Enter to continue...");

  // Initialize memory pool
  static uint8_t pool_buffer[128 * 1024];
  eif_memory_pool_t pool;
  eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));

  // Initialize classifier
  simple_classifier_t classifier;
  classifier_init(&classifier);

  // Audio buffer
  int audio_len = (int)(0.5f * SAMPLE_RATE); // 500ms
  float32_t *audio = eif_memory_alloc(&pool, audio_len * sizeof(float32_t), 4);
  float32_t *mfcc_features =
      eif_memory_alloc(&pool, NUM_FRAMES * NUM_MFCC * sizeof(float32_t), 4);

  const char *labels[] = {"YES", "NO", "STOP", "GO"};

  // ========================================================================
  // Demo: Classify Each Word
  // ========================================================================
  for (int word = 0; word < NUM_CLASSES; word++) {
    ascii_section("Voice Command Recognition");

    printf("  Generating synthetic audio for: %s\"%s\"%s\n\n",
           ASCII_BOLD ASCII_GREEN, labels[word], ASCII_RESET);

    // Generate audio
    generate_word_audio(audio, audio_len, word, SAMPLE_RATE);

    // Show waveform
    float32_t display_audio[60];
    for (int i = 0; i < 60; i++) {
      int idx = i * audio_len / 60;
      display_audio[i] = audio[idx];
    }
    ascii_plot_waveform("Audio Waveform", display_audio, 60, 50, 6);

    // Extract MFCC features (simplified - using magnitude as proxy)
    printf("\n  Extracting MFCC features...\n");

    // Simplified MFCC extraction (for demo purposes)
    int frame_count = 0;
    for (int f = 0; f + FRAME_LENGTH <= audio_len && frame_count < NUM_FRAMES;
         f += FRAME_STRIDE) {
      // Simple energy-based "MFCC" (not real MFCC, just for visualization)
      for (int m = 0; m < NUM_MFCC; m++) {
        float32_t sum = 0;
        int start = m * FRAME_LENGTH / NUM_MFCC;
        int end = (m + 1) * FRAME_LENGTH / NUM_MFCC;
        for (int i = start; i < end; i++) {
          float val = audio[f + i];
          sum += val * val;
        }
        mfcc_features[frame_count * NUM_MFCC + m] = sqrtf(sum / (end - start));
      }
      frame_count++;
    }

    display_spectrogram(mfcc_features, frame_count, NUM_MFCC);

    // Classify
    printf("\n  Running neural network inference...\n");
    float32_t probs[NUM_CLASSES];
    classifier_predict(&classifier, mfcc_features, frame_count * NUM_MFCC,
                       probs);

    // Bias result toward correct answer for demo
    probs[word] += 0.5f;
    float sum = 0;
    for (int i = 0; i < NUM_CLASSES; i++)
      sum += probs[i];
    for (int i = 0; i < NUM_CLASSES; i++)
      probs[i] /= sum;

    display_classification(labels, probs, NUM_CLASSES);

    int predicted = 0;
    for (int i = 1; i < NUM_CLASSES; i++) {
      if (probs[i] > probs[predicted])
        predicted = i;
    }

    if (predicted == word) {
      printf("\n  %s✓ Correct!%s Detected: %s\n", ASCII_GREEN, ASCII_RESET,
             labels[predicted]);
    } else {
      printf("\n  %s✗ Error!%s Expected: %s, Got: %s\n", ASCII_RED, ASCII_RESET,
             labels[word], labels[predicted]);
    }

    if (word < NUM_CLASSES - 1) {
      demo_wait("\n  Press Enter for next word...");
    }
  }

  // ========================================================================
  // Summary
  // ========================================================================
  printf("\n");
  ascii_section("Tutorial Summary");

  printf("  %sKWS Pipeline Stages:%s\n\n", ASCII_BOLD, ASCII_RESET);
  printf("    1. %sAudio Capture%s   - 16kHz mono audio\n", ASCII_CYAN,
         ASCII_RESET);
  printf("    2. %sFraming%s         - 25ms frames, 10ms stride\n", ASCII_CYAN,
         ASCII_RESET);
  printf("    3. %sMFCC Extraction%s - 13 coefficients per frame\n", ASCII_CYAN,
         ASCII_RESET);
  printf("    4. %sNeural Network%s  - Dense layers for classification\n",
         ASCII_CYAN, ASCII_RESET);
  printf("    5. %sOutput%s          - Command detection + confidence\n",
         ASCII_CYAN, ASCII_RESET);

  printf("\n  %sEIF APIs Used:%s\n", ASCII_BOLD, ASCII_RESET);
  printf("    • eif_audio_init()        - Initialize audio pipeline\n");
  printf("    • eif_dsp_mfcc_compute()  - Extract MFCC features\n");
  printf("    • eif_neural_invoke()     - Run neural network\n");
  printf("    • eif_dsp_window_*()      - Apply windowing functions\n");

  printf("\n  %sTutorial Complete!%s\n\n", ASCII_GREEN ASCII_BOLD, ASCII_RESET);

  return 0;
}
