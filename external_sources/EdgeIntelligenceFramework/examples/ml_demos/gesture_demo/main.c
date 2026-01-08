/**
 * @file main.c
 * @brief Interactive Gesture Recognition Demo - Improved Version
 *
 * A fun, educational demo showing real-time gesture recognition
 * with ASCII visualization of accelerometer patterns.
 *
 * Now with working detection, more gesture types, and better visualization!
 *
 * Build: cd build && make gesture_demo
 * Run:   ./bin/gesture_demo --batch
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// For sleep
#ifdef _WIN32
#include <windows.h>
#define sleep_ms(x) Sleep(x)
#else
#include <unistd.h>
#define sleep_ms(x) usleep((x) * 1000)
#endif

// =============================================================================
// Gesture Types
// =============================================================================

typedef enum {
  GESTURE_NONE = 0,
  GESTURE_TAP,
  GESTURE_DOUBLE_TAP,
  GESTURE_SHAKE,
  GESTURE_SWIPE_LEFT,
  GESTURE_SWIPE_RIGHT,
  GESTURE_SWIPE_UP,
  GESTURE_SWIPE_DOWN,
  GESTURE_CIRCLE,
  GESTURE_PICK_UP,
  GESTURE_PUT_DOWN,
  GESTURE_FLIP,
  NUM_GESTURES
} gesture_type_t;

static const char *gesture_names[] = {"None",     "Tap",        "Double Tap",
                                      "Shake",    "Swipe Left", "Swipe Right",
                                      "Swipe Up", "Swipe Down", "Circle",
                                      "Pick Up",  "Put Down",   "Flip"};

static const char *gesture_emojis[] = {"  ", "👆", "✌️ ", "🫨", "⬅️ ", "➡️ ",
                                       "⬆️ ", "⬇️ ", "🔄", "🫴", "🫳", "🔃"};

// =============================================================================
// Configuration
// =============================================================================

#define SAMPLE_RATE 50.0f
#define BUFFER_SIZE 100

// =============================================================================
// Custom Gesture Detector (tuned for simulation)
// =============================================================================

typedef struct {
  float buffer_x[BUFFER_SIZE];
  float buffer_y[BUFFER_SIZE];
  float buffer_z[BUFFER_SIZE];
  float buffer_mag[BUFFER_SIZE];
  int count;
  bool active;
  int idle_count;
} gesture_detector_t;

static void detector_init(gesture_detector_t *d) {
  memset(d, 0, sizeof(gesture_detector_t));
}

static void detector_reset(gesture_detector_t *d) {
  d->count = 0;
  d->active = false;
  d->idle_count = 0;
}

// Analyze captured gesture and classify
static gesture_type_t detector_classify(gesture_detector_t *d) {
  if (d->count < 5)
    return GESTURE_NONE;

  int n = d->count;
  float *x = d->buffer_x;
  float *y = d->buffer_y;
  float *z = d->buffer_z;
  float *mag = d->buffer_mag;

  // Calculate features
  float sum_x = 0, sum_y = 0, sum_z = 0;
  float max_x = -100, max_y = -100, max_z = -100;
  float min_x = 100, min_y = 100, min_z = 100;
  float max_mag = 0;
  int peak_count = 0;
  int zero_cross = 0;

  for (int i = 0; i < n; i++) {
    sum_x += x[i];
    sum_y += y[i];
    sum_z += z[i] - 9.81f; // Remove gravity

    if (x[i] > max_x)
      max_x = x[i];
    if (y[i] > max_y)
      max_y = y[i];
    if (z[i] > max_z)
      max_z = z[i];
    if (x[i] < min_x)
      min_x = x[i];
    if (y[i] < min_y)
      min_y = y[i];
    if (z[i] < min_z)
      min_z = z[i];
    if (mag[i] > max_mag)
      max_mag = mag[i];

    // Count peaks (local maxima)
    if (i > 0 && i < n - 1) {
      if (mag[i] > mag[i - 1] && mag[i] > mag[i + 1] && mag[i] > 12) {
        peak_count++;
      }
    }

    // Zero crossings in X
    if (i > 0 && x[i] * x[i - 1] < 0) {
      zero_cross++;
    }
  }

  float range_x = max_x - min_x;
  float range_y = max_y - min_y;
  float range_z = max_z - min_z;
  float duration_ms = n * 1000.0f / SAMPLE_RATE;

  // Classification rules

  // SHAKE: many zero crossings, oscillating
  if (zero_cross >= 6 && range_x > 10) {
    return GESTURE_SHAKE;
  }

  // FLIP: big Z change, moderate duration
  if (range_z > 15 && duration_ms > 200) {
    return GESTURE_FLIP;
  }

  // CIRCLE: both X and Y have significant range
  if (range_x > 8 && range_y > 8 && duration_ms > 300) {
    return GESTURE_CIRCLE;
  }

  // PICK UP: Z increases significantly, stays up
  if (z[n - 1] > z[0] + 3 && duration_ms > 200) {
    return GESTURE_PICK_UP;
  }

  // PUT DOWN: Z decreases significantly
  if (z[0] > z[n - 1] + 3 && duration_ms > 200) {
    return GESTURE_PUT_DOWN;
  }

  // DOUBLE TAP: exactly 2 peaks
  if (peak_count == 2 && duration_ms < 600) {
    return GESTURE_DOUBLE_TAP;
  }

  // SINGLE TAP: 1 peak, short
  if (peak_count == 1 && duration_ms < 300) {
    return GESTURE_TAP;
  }

  // SWIPES: dominant direction
  float abs_sum_x = fabsf(sum_x);
  float abs_sum_y = fabsf(sum_y);

  if (abs_sum_x > abs_sum_y && abs_sum_x > 20) {
    return (sum_x > 0) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
  }
  if (abs_sum_y > abs_sum_x && abs_sum_y > 20) {
    return (sum_y > 0) ? GESTURE_SWIPE_UP : GESTURE_SWIPE_DOWN;
  }

  return GESTURE_NONE;
}

static gesture_type_t detector_update(gesture_detector_t *d, float ax, float ay,
                                      float az) {
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float delta_mag = fabsf(mag - 9.81f);

  // Start detection on motion
  if (!d->active && delta_mag > 2.0f) {
    d->active = true;
    d->count = 0;
    d->idle_count = 0;
  }

  if (d->active) {
    // Store sample
    if (d->count < BUFFER_SIZE) {
      d->buffer_x[d->count] = ax;
      d->buffer_y[d->count] = ay;
      d->buffer_z[d->count] = az;
      d->buffer_mag[d->count] = mag;
      d->count++;
    }

    // Check for motion end
    if (delta_mag < 1.5f) {
      d->idle_count++;
      if (d->idle_count > 8) {
        // Motion ended - classify
        gesture_type_t result = detector_classify(d);
        detector_reset(d);
        return result;
      }
    } else {
      d->idle_count = 0;
    }

    // Buffer full - force classify
    if (d->count >= BUFFER_SIZE) {
      gesture_type_t result = detector_classify(d);
      detector_reset(d);
      return result;
    }
  }

  return GESTURE_NONE;
}

// =============================================================================
// Gesture Simulation
// =============================================================================

typedef struct {
  float ax, ay, az;
} accel_t;

static accel_t simulate_gesture(gesture_type_t type, int sample, int total) {
  accel_t a = {0, 0, 9.81f};
  float t = (float)sample / SAMPLE_RATE;
  float progress = (float)sample / total;
  float noise = ((float)(rand() % 100) / 1000.0f - 0.05f);

  switch (type) {
  case GESTURE_TAP:
    // Strong Z spike
    if (sample >= 5 && sample <= 15) {
      float spike = 1.0f - fabsf((float)(sample - 10)) / 5.0f;
      a.az += 15.0f * spike;
    }
    break;

  case GESTURE_DOUBLE_TAP:
    // Two Z spikes
    if (sample >= 5 && sample <= 12) {
      float spike = 1.0f - fabsf((float)(sample - 8)) / 4.0f;
      a.az += 12.0f * spike;
    }
    if (sample >= 22 && sample <= 30) {
      float spike = 1.0f - fabsf((float)(sample - 26)) / 4.0f;
      a.az += 12.0f * spike;
    }
    break;

  case GESTURE_SHAKE:
    // Strong oscillating X
    if (sample >= 5 && sample <= 70) {
      a.ax = 12.0f * sinf(2.0f * 3.14159f * 5.0f * t);
      a.ay = 4.0f * sinf(2.0f * 3.14159f * 3.0f * t);
    }
    break;

  case GESTURE_SWIPE_LEFT:
    // Negative X impulse
    if (sample >= 10 && sample <= 35) {
      float env = sinf(3.14159f * (float)(sample - 10) / 25.0f);
      a.ax = -18.0f * env;
    }
    break;

  case GESTURE_SWIPE_RIGHT:
    // Positive X impulse
    if (sample >= 10 && sample <= 35) {
      float env = sinf(3.14159f * (float)(sample - 10) / 25.0f);
      a.ax = 18.0f * env;
    }
    break;

  case GESTURE_SWIPE_UP:
    // Positive Y impulse
    if (sample >= 10 && sample <= 35) {
      float env = sinf(3.14159f * (float)(sample - 10) / 25.0f);
      a.ay = 18.0f * env;
    }
    break;

  case GESTURE_SWIPE_DOWN:
    // Negative Y impulse
    if (sample >= 10 && sample <= 35) {
      float env = sinf(3.14159f * (float)(sample - 10) / 25.0f);
      a.ay = -18.0f * env;
    }
    break;

  case GESTURE_CIRCLE:
    // Circular motion in X-Y plane
    if (sample >= 5 && sample <= 55) {
      float phase = 2.0f * 3.14159f * (float)(sample - 5) / 50.0f;
      a.ax = 10.0f * cosf(phase);
      a.ay = 10.0f * sinf(phase);
    }
    break;

  case GESTURE_PICK_UP:
    // Z ramps up
    if (sample >= 10 && sample <= 40) {
      float ramp = (float)(sample - 10) / 30.0f;
      a.az = 9.81f + 6.0f * ramp;
    } else if (sample > 40) {
      a.az = 9.81f + 6.0f;
    }
    break;

  case GESTURE_PUT_DOWN:
    // Z ramps down
    if (sample < 10) {
      a.az = 9.81f + 5.0f;
    } else if (sample >= 10 && sample <= 40) {
      float ramp = 1.0f - (float)(sample - 10) / 30.0f;
      a.az = 9.81f + 5.0f * ramp;
    }
    break;

  case GESTURE_FLIP:
    // Big Z swing
    if (sample >= 10 && sample <= 40) {
      float env = sinf(3.14159f * (float)(sample - 10) / 30.0f);
      a.az = 9.81f + 20.0f * env;
    }
    break;

  default:
    break;
  }

  a.ax += noise;
  a.ay += noise;
  a.az += noise * 0.5f;

  return a;
}

// =============================================================================
// ASCII Visualization
// =============================================================================

static void print_header(void) {
  printf("\033[2J\033[H");
  printf("╔═══════════════════════════════════════════════════════════════╗\n");
  printf("║        🎮 EIF Gesture Recognition Demo v2.0 🎮                ║\n");
  printf("╠═══════════════════════════════════════════════════════════════╣\n");
  printf("║  Now with 11 gesture types and improved detection!            ║\n");
  printf(
      "╚═══════════════════════════════════════════════════════════════╝\n\n");
}

static void print_waveform_mini(float *data, int len, float min_v,
                                float max_v) {
  const char *blocks[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
  float range = max_v - min_v;
  if (range < 0.1f)
    range = 0.1f;

  int step = (len > 40) ? len / 40 : 1;
  printf("  ");
  for (int i = 0; i < len && i / step < 40; i += step) {
    float norm = (data[i] - min_v) / range;
    if (norm < 0)
      norm = 0;
    if (norm > 1)
      norm = 1;
    int idx = (int)(norm * 7.99f);
    printf("%s", blocks[idx]);
  }
  printf("\n");
}

// =============================================================================
// Demo Runner
// =============================================================================

static void run_gesture_demo(gesture_type_t gesture_type, bool batch_mode) {
  gesture_detector_t detector;
  detector_init(&detector);

  float history_mag[100];
  float history_x[100];
  int hist_idx = 0;

  gesture_type_t detected = GESTURE_NONE;
  int total_samples = 80;

  if (!batch_mode) {
    print_header();
    printf("Simulating: %s %s\n", gesture_emojis[gesture_type],
           gesture_names[gesture_type]);
    printf("════════════════════════════════════════\n\n");
  }

  // Generate and process samples
  for (int i = 0; i < total_samples && hist_idx < 100; i++) {
    accel_t a = simulate_gesture(gesture_type, i, total_samples);

    float mag = sqrtf(a.ax * a.ax + a.ay * a.ay + a.az * a.az);
    history_mag[hist_idx] = mag;
    history_x[hist_idx] = a.ax;
    hist_idx++;

    gesture_type_t result = detector_update(&detector, a.ax, a.ay, a.az);
    if (result != GESTURE_NONE) {
      detected = result;
    }
  }

  // Force final classification if still active
  if (detector.active && detector.count > 5) {
    detected = detector_classify(&detector);
  }

  if (batch_mode) {
    printf("  %s %-12s ", gesture_emojis[gesture_type],
           gesture_names[gesture_type]);
    print_waveform_mini(history_mag, hist_idx, 8, 25);

    if (detected == gesture_type) {
      printf("                          → ✅ Correct: %s\n",
             gesture_names[detected]);
    } else if (detected != GESTURE_NONE) {
      printf("                          → ⚠️  Got: %s\n",
             gesture_names[detected]);
    } else {
      printf("                          → ❌ Not detected\n");
    }
  } else {
    printf("Waveform (Magnitude):\n");
    print_waveform_mini(history_mag, hist_idx, 8, 25);
    printf("\nWaveform (X-axis):\n");
    print_waveform_mini(history_x, hist_idx, -20, 20);

    printf("\n╭─────────────────────────────────╮\n");
    printf("│  Expected: %-15s     │\n", gesture_names[gesture_type]);
    printf("│  Detected: %-15s     │\n",
           detected ? gesture_names[detected] : "None");
    printf("│  Result:   %s                     │\n",
           (detected == gesture_type)   ? "✅"
           : (detected != GESTURE_NONE) ? "⚠️ "
                                        : "❌");
    printf("╰─────────────────────────────────╯\n");
  }
}

static void run_all_gestures(bool batch_mode) {
  gesture_type_t gestures[] = {
      GESTURE_TAP,        GESTURE_DOUBLE_TAP,  GESTURE_SHAKE,
      GESTURE_SWIPE_LEFT, GESTURE_SWIPE_RIGHT, GESTURE_SWIPE_UP,
      GESTURE_SWIPE_DOWN, GESTURE_CIRCLE,      GESTURE_PICK_UP,
      GESTURE_PUT_DOWN,   GESTURE_FLIP};
  int n = sizeof(gestures) / sizeof(gestures[0]);

  if (batch_mode) {
    printf("\n╔════════════════════════════════════════════════════════════════"
           "══╗\n");
    printf("║          🎮 EIF Gesture Recognition Test Suite 🎮                "
           "║\n");
    printf("╠══════════════════════════════════════════════════════════════════"
           "╣\n");
    printf("║  11 gesture types • Real-time waveforms • Feature classification "
           "║\n");
    printf("╚══════════════════════════════════════════════════════════════════"
           "╝\n\n");
  }

  int correct = 0;
  for (int i = 0; i < n; i++) {
    run_gesture_demo(gestures[i], batch_mode);
    if (!batch_mode) {
      printf("\nPress Enter for next...");
      getchar();
    }
  }

  if (batch_mode) {
    printf("\n═════════════════════════════════════════════════════════════════"
           "═\n");
    printf("Demo complete! Tested %d gesture types.\n", n);
  }
}

static void run_interactive(void) {
  print_header();

  printf("Choose a gesture to simulate:\n\n");
  printf("  [1] Tap          [5] Swipe Right    [9]  Pick Up\n");
  printf("  [2] Double Tap   [6] Swipe Up       [10] Put Down\n");
  printf("  [3] Shake        [7] Swipe Down     [11] Flip\n");
  printf("  [4] Swipe Left   [8] Circle         [q]  Quit\n\n");

  char buf[10];
  while (1) {
    printf("Enter choice: ");
    if (fgets(buf, sizeof(buf), stdin) == NULL)
      break;

    if (buf[0] == 'q' || buf[0] == 'Q')
      return;

    int choice = atoi(buf);
    if (choice >= 1 && choice < NUM_GESTURES) {
      run_gesture_demo((gesture_type_t)choice, false);
    } else {
      printf("Invalid choice.\n");
    }
  }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char *argv[]) {
  srand((unsigned int)time(NULL));

  bool batch_mode = false;
  bool interactive = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--batch") == 0 || strcmp(argv[i], "-b") == 0) {
      batch_mode = true;
    } else if (strcmp(argv[i], "--interactive") == 0 ||
               strcmp(argv[i], "-i") == 0) {
      interactive = true;
    } else if (strcmp(argv[i], "--help") == 0) {
      printf("Usage: gesture_demo [--batch | --interactive]\n");
      return 0;
    }
  }

  if (interactive) {
    run_interactive();
  } else if (batch_mode) {
    run_all_gestures(true);
  } else {
    run_all_gestures(false);
  }

  return 0;
}
