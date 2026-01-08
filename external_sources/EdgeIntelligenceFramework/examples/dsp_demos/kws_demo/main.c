#include "../common/ascii_plot.h"
#include "../common/demo_cli.h"
#include "eif_audio.h"
#include "eif_memory.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 1MB Pool
static uint8_t pool_buffer[1024 * 1024];
static eif_memory_pool_t pool;

// ============================================================================
// Configuration
// ============================================================================

static bool json_mode = false;
static bool continuous_mode = false;
static int sample_count = 0;

static const char *keywords[] = {"KEYWORD", "BACKGROUND"};
#define NUM_KEYWORDS 2

// ============================================================================
// Helper Functions
// ============================================================================

static void softmax(float *x, int n) {
  float max_val = x[0];
  for (int i = 1; i < n; i++)
    if (x[i] > max_val)
      max_val = x[i];
  float sum = 0;
  for (int i = 0; i < n; i++) {
    x[i] = expf(x[i] - max_val);
    sum += x[i];
  }
  for (int i = 0; i < n; i++)
    x[i] /= sum;
}

// Mock classifier (simulates neural network)
static void mock_classify(const float32_t *mfcc_features, int num_frames,
                          int num_mfcc, float32_t *output) {
  float energy = 0;
  float high_freq = 0;

  for (int f = 0; f < num_frames; f++) {
    for (int c = 0; c < num_mfcc; c++) {
      energy +=
          mfcc_features[f * num_mfcc + c] * mfcc_features[f * num_mfcc + c];
      if (c > num_mfcc / 2)
        high_freq += fabsf(mfcc_features[f * num_mfcc + c]);
    }
  }

  // Mock scores
  output[0] = energy * 0.01f;         // Keyword
  output[1] = 1.0f - energy * 0.005f; // Background
  softmax(output, 2);
}

// ============================================================================
// JSON Output
// ============================================================================

static void output_json(int sample, float *probs, const float32_t *mfcc,
                        int frames, int coeffs, bool detected) {
  printf("{\"timestamp\": %d, \"type\": \"kws\"", sample);

  // Probabilities
  printf(", \"probs\": {");
  for (int i = 0; i < NUM_KEYWORDS; i++) {
    printf("\"%s\": %.4f%s", keywords[i], probs[i],
           i < NUM_KEYWORDS - 1 ? ", " : "");
  }
  printf("}");

  // Prediction
  int max_idx = 0;
  for (int i = 1; i < NUM_KEYWORDS; i++) {
    if (probs[i] > probs[max_idx])
      max_idx = i;
  }
  printf(", \"prediction\": \"%s\"", keywords[max_idx]);
  printf(", \"confidence\": %.4f", probs[max_idx]);
  printf(", \"detected\": %s", detected ? "true" : "false");

  // MFCC energy summary (first coefficients)
  printf(", \"mfcc_energy\": [");
  for (int c = 0; c < (coeffs < 5 ? coeffs : 5); c++) {
    float avg = 0;
    for (int f = 0; f < frames; f++) {
      avg += fabsf(mfcc[f * coeffs + c]);
    }
    avg /= frames;
    printf("%.3f%s", avg, c < 4 ? ", " : "");
  }
  printf("]");

  printf("}\n");
  fflush(stdout);
}

// ============================================================================
// Display Functions
// ============================================================================

static void display_mfcc(const float32_t *features, int frames, int coeffs) {
  printf("\n  %sMFCC Features [%d frames x %d coeffs]%s\n", ASCII_BOLD, frames,
         coeffs, ASCII_RESET);
  printf("  +");
  for (int c = 0; c < coeffs; c++)
    printf("-");
  printf("+\n");

  for (int f = 0; f < frames; f++) {
    printf("  |");
    for (int c = 0; c < coeffs; c++) {
      float val = fabsf(features[f * coeffs + c]);
      if (val > 2.0f)
        printf("#");
      else if (val > 1.5f)
        printf("*");
      else if (val > 1.0f)
        printf("+");
      else if (val > 0.5f)
        printf(".");
      else
        printf(" ");
    }
    printf("| t=%d\n", f);
  }

  printf("  +");
  for (int c = 0; c < coeffs; c++)
    printf("-");
  printf("+\n");
}

static void display_classification(float *output, bool detected) {
  printf("\n  %s+-- Classification --------------------------+%s\n", ASCII_CYAN,
         ASCII_RESET);

  for (int i = 0; i < NUM_KEYWORDS; i++) {
    int bar_len = (int)(output[i] * 30);
    printf("  |  %-10s: ", keywords[i]);
    for (int b = 0; b < bar_len; b++)
      printf("#");
    for (int b = bar_len; b < 30; b++)
      printf(".");
    printf(" %5.1f%% |\n", output[i] * 100);
  }

  printf("  %s+--------------------------------------------+%s\n", ASCII_CYAN,
         ASCII_RESET);

  if (detected) {
    printf("\n  %s>>> KEYWORD DETECTED! <<<%s\n", ASCII_GREEN ASCII_BOLD,
           ASCII_RESET);
  }
}

static void print_usage(const char *prog) {
  printf("Usage: %s [OPTIONS]\n\n", prog);
  printf("Options:\n");
  printf("  --json        Output JSON for real-time plotting\n");
  printf("  --continuous  Run continuously without pauses\n");
  printf("  --chunks N    Number of audio chunks to process (default: 50)\n");
  printf("  --help        Show this help\n");
  printf("\nExamples:\n");
  printf("  %s --json | python3 tools/eif_plotter.py --stdin\n", prog);
  printf("  %s --continuous --chunks 200\n", prog);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
  int num_chunks = 50;

  // Parse arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0) {
      json_mode = true;
    } else if (strcmp(argv[i], "--continuous") == 0) {
      continuous_mode = true;
    } else if (strcmp(argv[i], "--chunks") == 0 && i + 1 < argc) {
      num_chunks = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    }
  }

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF Tutorial: Keyword Spotting (KWS)");

    printf("  This tutorial demonstrates a complete KWS pipeline:\n");
    printf("    1. Audio streaming\n");
    printf("    2. MFCC feature extraction\n");
    printf("    3. Neural network classification\n\n");
    printf("  Keywords: [KEYWORD] vs [BACKGROUND]\n");

    if (!continuous_mode) {
      demo_wait("\n  Press Enter to continue...");
    }
  }

  eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));

  // Audio configuration
  eif_audio_config_t audio_cfg = {.sample_rate = 16000,
                                  .frame_length = 512,
                                  .frame_stride = 256,
                                  .num_mfcc = 13,
                                  .num_filters = 26,
                                  .lower_freq = 20.0f,
                                  .upper_freq = 4000.0f,
                                  .output_frames = 10};

  if (!json_mode) {
    printf("\n  %s+-- Audio Configuration ---------------------+%s\n",
           ASCII_CYAN, ASCII_RESET);
    printf("  |  Sample Rate:     %5d Hz                  |\n",
           audio_cfg.sample_rate);
    printf("  |  Frame Length:    %5d samples (32 ms)     |\n",
           audio_cfg.frame_length);
    printf("  |  Frame Stride:    %5d samples (16 ms)     |\n",
           audio_cfg.frame_stride);
    printf("  |  MFCC Coeffs:     %5d                     |\n",
           audio_cfg.num_mfcc);
    printf("  |  Mel Filters:     %5d                     |\n",
           audio_cfg.num_filters);
    printf("  |  Freq Range:      %5.0f - %.0f Hz          |\n",
           audio_cfg.lower_freq, audio_cfg.upper_freq);
    printf("  %s+--------------------------------------------+%s\n", ASCII_CYAN,
           ASCII_RESET);
  }

  eif_audio_preprocessor_t audio_ctx;
  if (eif_audio_init(&audio_ctx, &audio_cfg, &pool) != EIF_STATUS_OK) {
    if (!json_mode) {
      printf("  %s[X] Failed to init audio preprocessor%s\n", ASCII_RED,
             ASCII_RESET);
    }
    return 1;
  }

  if (!json_mode) {
    printf("  %s[OK] Audio preprocessor initialized%s\n\n", ASCII_GREEN,
           ASCII_RESET);

    if (!continuous_mode) {
      demo_wait("  Press Enter to start streaming audio...");
    }

    ascii_section("Audio Streaming & Classification");
    printf("  Streaming simulated audio (%d chunks)...\n\n", num_chunks);
  }

  // Simulate Audio Stream
  float32_t chunk[256];
  int detection_count = 0;
  int num_inferences = 0;

  for (int i = 0; i < num_chunks; i++) {
    // Generate test audio (simulated speech)
    for (int j = 0; j < 256; j++) {
      float t = (float)(i * 256 + j) / 16000.0f;
      // Mix of frequencies (simulate speech)
      chunk[j] = 0.3f * sinf(2.0f * M_PI * 200.0f * t) +
                 0.2f * sinf(2.0f * M_PI * 500.0f * t) +
                 0.1f * sinf(2.0f * M_PI * 1000.0f * t) +
                 0.05f * ((float)rand() / RAND_MAX - 0.5f);
    }

    // Push to preprocessor
    eif_audio_push(&audio_ctx, chunk, 256);

    // Check if ready for inference
    if (eif_audio_is_ready(&audio_ctx)) {
      const float32_t *features = eif_audio_get_features(&audio_ctx);

      if (features) {
        // Run mock classification
        float32_t output[NUM_KEYWORDS];
        mock_classify(features, audio_cfg.output_frames, audio_cfg.num_mfcc,
                      output);

        bool detected = output[0] > 0.5f;
        if (detected)
          detection_count++;

        if (json_mode) {
          output_json(sample_count++, output, features, audio_cfg.output_frames,
                      audio_cfg.num_mfcc, detected);
        } else if (num_inferences < 3 || continuous_mode) {
          printf("  Chunk %d: Features ready\n", i);
          display_mfcc(features, audio_cfg.output_frames, audio_cfg.num_mfcc);
          display_classification(output, detected);

          num_inferences++;
          if (!continuous_mode && num_inferences < 3) {
            printf("\n  Press Enter for next inference...");
            getchar();
          }
        }
      }
    }

    // Progress indicator (non-JSON mode)
    if (!json_mode && i % 10 == 9) {
      printf("  [");
      for (int p = 0; p <= i / 10; p++)
        printf("#");
      for (int p = i / 10 + 1; p < num_chunks / 10; p++)
        printf(".");
      printf("] %d%% complete\n", (i + 1) * 100 / num_chunks);
    }
  }

  // Summary
  if (!json_mode) {
    printf("\n");
    ascii_section("Tutorial Summary");

    printf("  %sKWS Pipeline:%s\n\n", ASCII_BOLD, ASCII_RESET);
    printf("    +--------+    +--------+    +--------+    +--------+\n");
    printf("    | Audio  |--->| Frame  |--->|  MFCC  |--->|   NN   |\n");
    printf("    | Stream |    |  +Win  |    |Extract |    |Classify|\n");
    printf("    +--------+    +--------+    +--------+    +--------+\n");

    printf("\n  %sKey EIF APIs:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    - eif_audio_init()          Initialize preprocessor\n");
    printf("    - eif_audio_push()          Stream audio samples\n");
    printf("    - eif_audio_is_ready()      Check feature availability\n");
    printf("    - eif_audio_get_features()  Get MFCC features\n");
    printf("    - eif_neural_invoke()       Run classifier (with model)\n");

    printf("\n  %sDetections: %d keyword(s) detected%s\n",
           detection_count > 0 ? ASCII_GREEN : "", detection_count,
           ASCII_RESET);

    printf("\n  %sTutorial Complete!%s\n\n", ASCII_GREEN ASCII_BOLD,
           ASCII_RESET);
  }

  return 0;
}
