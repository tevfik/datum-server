/*
 * AnomalyDetection.ino
 *
 * Demonstrates adaptive anomaly detection using Z-score thresholds.
 * The threshold automatically adapts to the normal sensor range.
 *
 * Compatible with ARM-based Arduinos (Due, Zero, Nano 33, etc.)
 * and ESP32.
 *
 * Wiring:
 *   - Connect temperature/light/any sensor to A0
 *   - LED on pin 13 indicates anomaly
 */

#include <EIF.h>

#if !defined(__arm__) && !defined(ESP32) && !defined(ESP8266)
#error "This example requires ARM or ESP32 board for adequate RAM"
#endif

// Adaptive Z-score threshold detector
// alpha = 0.05 (slow adaptation)
// threshold = 3.0 (3 standard deviations)
eif_z_threshold_t anomalyDetector;

const int SENSOR_PIN = A0;
const int LED_PIN = LED_BUILTIN;
const int SAMPLE_DELAY = 100; // 10 Hz

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  pinMode(LED_PIN, OUTPUT);

  // Initialize adaptive threshold
  // alpha = 0.05 means slow adaptation to changes
  // threshold = 3.0 means trigger at 3 standard deviations
  eif_z_threshold_init(&anomalyDetector, 0.05f, 3.0f);

  Serial.println("EIF Anomaly Detection Example");
  Serial.println("Wait for calibration (about 20 readings)...");
}

void loop() {
  // Read sensor
  float value = analogRead(SENSOR_PIN) / 1023.0f;

  // Check for anomaly
  bool isAnomaly = eif_z_threshold_check(&anomalyDetector, value);

  // Get current threshold bounds
  float lower, upper;
  eif_z_threshold_get_bounds(&anomalyDetector, &lower, &upper);

  // Output
  Serial.print("Value: ");
  Serial.print(value, 3);
  Serial.print(" | Bounds: [");
  Serial.print(lower, 3);
  Serial.print(", ");
  Serial.print(upper, 3);
  Serial.print("] | ");

  if (isAnomaly) {
    Serial.println("*** ANOMALY! ***");
    digitalWrite(LED_PIN, HIGH);
  } else {
    Serial.println("Normal");
    digitalWrite(LED_PIN, LOW);
  }

  delay(SAMPLE_DELAY);
}
