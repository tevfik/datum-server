/*
 * ActivityRecognition.ino
 *
 * Demonstrates Human Activity Recognition using accelerometer data.
 * Works with the onboard IMU on Nano 33 BLE/IoT or external MPU6050.
 *
 * Compatible with ARM-based Arduinos with adequate RAM.
 *
 * Activities detected:
 *   - Stationary
 *   - Walking
 *   - Running
 *   - Stairs up/down
 */

#include <EIF.h>

#if !defined(__arm__) && !defined(ESP32)
#error "This example requires ARM or ESP32 board for adequate RAM"
#endif

// Check for onboard IMU
#if defined(ARDUINO_NANO33BLE) || defined(ARDUINO_ARDUINO_NANO33BLE)
#include <Arduino_LSM9DS1.h>
#define HAS_ONBOARD_IMU
#endif

// Activity recognition state
eif_activity_window_t window;
eif_accel_sample_t samples[EIF_ACTIVITY_WINDOW_SIZE];

const int LED_PIN = LED_BUILTIN;
const int SAMPLE_DELAY = 20; // 50 Hz

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  pinMode(LED_PIN, OUTPUT);

#ifdef HAS_ONBOARD_IMU
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1)
      ;
  }
  Serial.println("Using onboard LSM9DS1 IMU");
#else
  Serial.println("No onboard IMU - using simulated data");
  Serial.println("Connect MPU6050 for real data");
#endif

  // Initialize activity window
  // Hop size = 64 samples (about 1.3 seconds at 50Hz)
  eif_activity_window_init(&window, 64);

  Serial.println("EIF Activity Recognition Example");
  Serial.println("Move the device to detect activities");
}

void loop() {
  float x, y, z;

#ifdef HAS_ONBOARD_IMU
  // Read from onboard IMU
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    // Convert to m/s² (IMU gives values in g)
    x *= 9.81f;
    y *= 9.81f;
    z *= 9.81f;
  } else {
    return; // No data ready
  }
#else
  // Simulated data for demo
  static float t = 0;
  t += 0.02f;
  x = 0.5f * sin(t * 2.0f);
  y = 0.3f * sin(t * 2.0f + 1.0f);
  z = 9.8f + 0.5f * sin(t * 2.0f);
#endif

  // Add sample to window
  if (eif_activity_window_add(&window, x, y, z)) {
    // Window is full - classify activity
    eif_activity_window_get_samples(&window, samples);

    // Extract features
    eif_activity_features_t features;
    eif_activity_extract_features(samples, EIF_ACTIVITY_WINDOW_SIZE, &features);

    // Classify
    eif_activity_t activity = eif_activity_classify_rules(&features);

    // Output result
    Serial.print("Activity: ");
    Serial.print(eif_activity_names[activity]);
    Serial.print(" | Mag: ");
    Serial.print(features.magnitude_mean, 2);
    Serial.print(" | Std: ");
    Serial.println(features.magnitude_std, 2);

    // Visual feedback
    if (activity == EIF_ACTIVITY_RUNNING) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  }

  delay(SAMPLE_DELAY);
}
