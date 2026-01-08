/**
 * @file main.c
 * @brief Wake Word Detection Demo
 *
 * A fun demo showing how wake word detection works with simulated audio
 * and MFCC feature extraction visualization.
 *
 * Build: cd build && make wake_word_demo
 * Run:   ./bin/wake_word_demo
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// =============================================================================
// Configuration
// =============================================================================

#define SAMPLE_RATE 16000
#define FRAME_SIZE 512 // 32ms frame
#define HOP_SIZE 160   // 10ms hop
#define NUM_MEL_FILTERS 26
#define NUM_MFCC 13
#define NUM_FRAMES 20 // ~200ms of audio for detection

// Wake word templates
#define MAX_TEMPLATES 5
#define TEMPLATE_FRAMES 15

// =============================================================================
// MFCC Feature Extraction (Simplified for Demo)
// =============================================================================

typedef struct {
  float mfcc[NUM_MFCC];
  float energy;
} mfcc_frame_t;

// Pre-computed mel filter bank centers (simplified)
static const float MEL_CENTERS[] = {200,  293,  400,  523,  666,
                                    833,  1030, 1264, 1542, 1875,
                                    2277, 2765, 3363, 4099, 5011};

// Hamming window coefficients (precomputed for efficiency)
static void apply_hamming(float *frame, int len) {
  for (int i = 0; i < len; i++) {
    float w = 0.54f - 0.46f * cosf(2.0f * 3.14159f * i / (len - 1));
    frame[i] *= w;
  }
}

// Simple DFT magnitude (not FFT for clarity - use FFT in production)
static void compute_power_spectrum(float *frame, int len, float *power) {
  int half = len / 2;
  for (int k = 0; k < half; k++) {
    float real = 0, imag = 0;
    for (int n = 0; n < len; n++) {
      float angle = 2.0f * 3.14159f * k * n / len;
      real += frame[n] * cosf(angle);
      imag -= frame[n] * sinf(angle);
    }
    power[k] = (real * real + imag * imag) / len;
  }
}

// Apply mel filterbank
static void apply_mel_filterbank(float *power, int len, float *mel_out) {
  // Simplified triangular filterbank
  for (int f = 0; f < NUM_MEL_FILTERS; f++) {
    mel_out[f] = 0;
    float center = 200 + f * 300; // Simple spacing
    float width = 300;

    for (int k = 0; k < len; k++) {
      float freq = (float)k * SAMPLE_RATE / (2 * len);
      float weight = 1.0f - fabsf(freq - center) / width;
      if (weight > 0) {
        mel_out[f] += power[k] * weight;
      }
    }
    // Log compression
    mel_out[f] = logf(mel_out[f] + 1e-10f);
  }
}

// DCT to get cepstral coefficients
static void compute_dct(float *mel_log, float *mfcc) {
  for (int i = 0; i < NUM_MFCC; i++) {
    mfcc[i] = 0;
    for (int j = 0; j < NUM_MEL_FILTERS; j++) {
      mfcc[i] += mel_log[j] * cosf(3.14159f * i * (j + 0.5f) / NUM_MEL_FILTERS);
    }
    mfcc[i] *= sqrtf(2.0f / NUM_MEL_FILTERS);
  }
}

// Extract MFCCs from audio frame
static void extract_mfcc(float *audio, int len, mfcc_frame_t *out) {
  float frame[FRAME_SIZE];
  float power[FRAME_SIZE / 2];
  float mel[NUM_MEL_FILTERS];

  // Copy and window
  for (int i = 0; i < len && i < FRAME_SIZE; i++) {
    frame[i] = audio[i];
  }
  apply_hamming(frame, FRAME_SIZE);

  // Compute energy
  out->energy = 0;
  for (int i = 0; i < FRAME_SIZE; i++) {
    out->energy += frame[i] * frame[i];
  }
  out->energy = logf(out->energy + 1e-10f);

  // Power spectrum
  compute_power_spectrum(frame, FRAME_SIZE, power);

  // Mel filterbank
  apply_mel_filterbank(power, FRAME_SIZE / 2, mel);

  // DCT
  compute_dct(mel, out->mfcc);
}

// =============================================================================
// Wake Word Templates
// =============================================================================

typedef struct {
  char name[32];
  mfcc_frame_t frames[TEMPLATE_FRAMES];
  int num_frames;
  float threshold;
} wake_word_template_t;

static wake_word_template_t templates[MAX_TEMPLATES];
static int num_templates = 0;

// =============================================================================
// DTW Distance for MFCC sequences
// =============================================================================

static float mfcc_distance(mfcc_frame_t *a, mfcc_frame_t *b) {
  float dist = 0;
  for (int i = 0; i < NUM_MFCC; i++) {
    float d = a->mfcc[i] - b->mfcc[i];
    dist += d * d;
  }
  return sqrtf(dist);
}

static float dtw_mfcc(mfcc_frame_t *query, int q_len, mfcc_frame_t *template,
                      int t_len) {
  // Simplified DTW using two rows
  float prev[50], curr[50];

  for (int j = 0; j <= t_len; j++)
    prev[j] = 1e30f;
  prev[0] = 0;

  for (int i = 1; i <= q_len; i++) {
    curr[0] = 1e30f;
    for (int j = 1; j <= t_len; j++) {
      float cost = mfcc_distance(&query[i - 1], &template[j - 1]);
      float m = prev[j - 1];
      if (prev[j] < m)
        m = prev[j];
      if (curr[j - 1] < m)
        m = curr[j - 1];
      curr[j] = cost + m;
    }
    for (int j = 0; j <= t_len; j++)
      prev[j] = curr[j];
  }

  return prev[t_len] / (q_len + t_len); // Normalize
}

// =============================================================================
// Audio Simulation
// =============================================================================

typedef enum {
  AUDIO_SILENCE,
  AUDIO_NOISE,
  AUDIO_SPEECH_HEY,
  AUDIO_SPEECH_DEVICE,
  AUDIO_SPEECH_HEY_DEVICE,
  AUDIO_SPEECH_OTHER
} audio_type_t;

static const char *audio_names[] = {"Silence",          "Background Noise",
                                    "\"Hey\"",          "\"Device\"",
                                    "\"Hey TinyEdge\"", "Other Speech"};

// Generate simulated audio with known characteristics
static void generate_audio(audio_type_t type, float *buffer, int len) {
  float t;

  for (int i = 0; i < len; i++) {
    t = (float)i / SAMPLE_RATE;

    switch (type) {
    case AUDIO_SILENCE:
      buffer[i] = ((float)(rand() % 100) / 10000.0f - 0.005f);
      break;

    case AUDIO_NOISE:
      buffer[i] = ((float)(rand() % 1000) / 500.0f - 1.0f) * 0.1f;
      break;

    case AUDIO_SPEECH_HEY:
      // "Hey" - short burst with vowel
      if (i < len * 3 / 4) {
        float env = sinf(3.14159f * i / (len * 3 / 4));
        buffer[i] = env * (0.3f * sinf(2 * 3.14159f * 180 * t) +
                           0.2f * sinf(2 * 3.14159f * 520 * t) +
                           0.1f * sinf(2 * 3.14159f * 1200 * t));
      } else {
        buffer[i] = 0;
      }
      break;

    case AUDIO_SPEECH_DEVICE:
      // "Device" - voiced with multiple syllables
      if (i > len / 5) {
        float env = sinf(3.14159f * (i - len / 5) / (len * 4 / 5));
        buffer[i] = env * (0.25f * sinf(2 * 3.14159f * 150 * t) +
                           0.15f * sinf(2 * 3.14159f * 400 * t) +
                           0.1f * sinf(2 * 3.14159f * 800 * t));
      } else {
        buffer[i] = 0;
      }
      break;

    case AUDIO_SPEECH_HEY_DEVICE:
      // Combined "Hey TinyEdge"
      if (i < len * 2 / 5) {
        float env = sinf(3.14159f * i / (len * 2 / 5));
        buffer[i] = env * (0.3f * sinf(2 * 3.14159f * 180 * t) +
                           0.2f * sinf(2 * 3.14159f * 520 * t));
      } else if (i > len / 2) {
        float env = sinf(3.14159f * (i - len / 2) / (len / 2));
        buffer[i] = env * (0.25f * sinf(2 * 3.14159f * 150 * t) +
                           0.15f * sinf(2 * 3.14159f * 400 * t));
      } else {
        buffer[i] = 0;
      }
      break;

    case AUDIO_SPEECH_OTHER:
      // Random speech-like sounds
      {
        float f0 = 100 + (rand() % 100);
        float env = 0.5f + 0.5f * sinf(3.14159f * 3 * t);
        buffer[i] = env * (0.2f * sinf(2 * 3.14159f * f0 * t) +
                           0.1f * sinf(2 * 3.14159f * 2 * f0 * t));
      }
      break;
    }

    // Add small noise
    buffer[i] += ((float)(rand() % 100) / 5000.0f - 0.01f);
  }
}

// =============================================================================
// Visualization
// =============================================================================

static void print_header(void) {
  printf("\033[2J\033[H");
  printf(
      "╔══════════════════════════════════════════════════════════════════╗\n");
  printf(
      "║           🎤 EIF Wake Word Detection Demo 🎤                     ║\n");
  printf(
      "╠══════════════════════════════════════════════════════════════════╣\n");
  printf(
      "║  MFCC feature extraction • DTW template matching • Voice detect  ║\n");
  printf("╚══════════════════════════════════════════════════════════════════╝"
         "\n\n");
}

static void print_mfcc_heatmap(mfcc_frame_t *frames, int n) {
  printf("MFCC Spectrogram (%d frames):\n", n);

  const char *intensity[] = {"░", "▒", "▓", "█"};

  for (int c = NUM_MFCC - 1; c >= 0; c--) {
    printf("  C%2d │", c);
    for (int f = 0; f < n && f < 40; f++) {
      float val = frames[f].mfcc[c];
      int idx = (int)((val + 5) / 3); // Normalize
      if (idx < 0)
        idx = 0;
      if (idx > 3)
        idx = 3;
      printf("%s", intensity[idx]);
    }
    printf("\n");
  }
  printf("      └");
  for (int i = 0; i < 40; i++)
    printf("─");
  printf("\n");
}

static void print_waveform(float *audio, int len) {
  printf("Waveform:\n  ");
  const char *blocks[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

  int step = len / 50;
  if (step < 1)
    step = 1;

  for (int i = 0; i < len; i += step) {
    float val = fabsf(audio[i]);
    int idx = (int)(val * 20);
    if (idx > 7)
      idx = 7;
    printf("%s", blocks[idx]);
  }
  printf("\n");
}

// =============================================================================
// Demo Runner
// =============================================================================

static void create_template(audio_type_t type, const char *name) {
  // Generate sample audio
  float audio[SAMPLE_RATE]; // 1 second
  generate_audio(type, audio, SAMPLE_RATE);

  // Extract MFCCs
  wake_word_template_t *t = &templates[num_templates];
  strncpy(t->name, name, 31);
  t->num_frames = 0;
  t->threshold = 2.5f;

  for (int i = 0;
       i < TEMPLATE_FRAMES && i * HOP_SIZE < SAMPLE_RATE - FRAME_SIZE; i++) {
    extract_mfcc(&audio[i * HOP_SIZE], FRAME_SIZE, &t->frames[i]);
    t->num_frames++;
  }

  num_templates++;
}

static void run_detection_test(audio_type_t audio_type, bool batch_mode) {
  // Generate test audio
  float audio[SAMPLE_RATE];
  generate_audio(audio_type, audio, SAMPLE_RATE);

  // Extract MFCCs
  mfcc_frame_t test_frames[NUM_FRAMES];
  for (int i = 0; i < NUM_FRAMES && i * HOP_SIZE < SAMPLE_RATE - FRAME_SIZE;
       i++) {
    extract_mfcc(&audio[i * HOP_SIZE], FRAME_SIZE, &test_frames[i]);
  }

  if (!batch_mode) {
    print_header();
    printf("Testing: %s\n", audio_names[audio_type]);
    printf("═══════════════════════════════════════════════════\n\n");
    print_waveform(audio, SAMPLE_RATE / 4);
    printf("\n");
    print_mfcc_heatmap(test_frames, NUM_FRAMES);
    printf("\n");
  } else {
    printf("  %-20s ", audio_names[audio_type]);
  }

  // Compare against templates
  bool detected = false;
  float best_score = 1e30f;
  const char *best_match = "None";

  for (int t = 0; t < num_templates; t++) {
    float dist = dtw_mfcc(test_frames, NUM_FRAMES, templates[t].frames,
                          templates[t].num_frames);

    if (dist < templates[t].threshold && dist < best_score) {
      best_score = dist;
      best_match = templates[t].name;
      detected = true;
    }
  }

  if (!batch_mode) {
    printf("╭───────────────────────────────────────╮\n");
    printf("│  Wake Word: %s                    │\n",
           detected ? "DETECTED! 🔔" : "Not detected   ");
    if (detected) {
      printf("│  Matched: %-20s      │\n", best_match);
      printf("│  Score: %.3f                         │\n", best_score);
    }
    printf("╰───────────────────────────────────────╯\n");
  } else {
    if (audio_type == AUDIO_SPEECH_HEY_DEVICE && detected) {
      printf("→ ✅ Wake word detected: %s\n", best_match);
    } else if (audio_type == AUDIO_SPEECH_HEY_DEVICE && !detected) {
      printf("→ ❌ Missed wake word\n");
    } else if (detected) {
      printf("→ ⚠️  False positive: %s\n", best_match);
    } else {
      printf("→ ✓  Correctly ignored\n");
    }
  }
}

static void run_all_tests(bool batch_mode) {
  audio_type_t tests[] = {AUDIO_SILENCE,       AUDIO_NOISE,
                          AUDIO_SPEECH_OTHER,  AUDIO_SPEECH_HEY,
                          AUDIO_SPEECH_DEVICE, AUDIO_SPEECH_HEY_DEVICE};
  int n = sizeof(tests) / sizeof(tests[0]);

  // Create template for wake word
  num_templates = 0;
  create_template(AUDIO_SPEECH_HEY_DEVICE, "Hey TinyEdge");

  if (batch_mode) {
    printf("\n╔════════════════════════════════════════════════════════════════"
           "══╗\n");
    printf("║            🎤 Wake Word Detection Test Suite 🎤                  "
           " ║\n");
    printf("╠══════════════════════════════════════════════════════════════════"
           "╣\n");
    printf("║  Wake word: \"Hey TinyEdge\" • Template matching with DTW        "
           "  ║\n");
    printf("╚══════════════════════════════════════════════════════════════════"
           "╝\n\n");
  }

  for (int i = 0; i < n; i++) {
    run_detection_test(tests[i], batch_mode);
    if (!batch_mode) {
      printf("\nPress Enter for next test...");
      getchar();
    }
  }

  if (batch_mode) {
    printf("\n═════════════════════════════════════════════════════════════════"
           "═\n");
    printf("Demo complete! Tested %d audio scenarios.\n", n);
  }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char *argv[]) {
  srand((unsigned int)time(NULL));

  bool batch_mode = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--batch") == 0 || strcmp(argv[i], "-b") == 0) {
      batch_mode = true;
    } else if (strcmp(argv[i], "--help") == 0) {
      printf("Usage: wake_word_demo [--batch]\n");
      return 0;
    }
  }

  run_all_tests(batch_mode);

  return 0;
}
