/*
 * SensorSmoothing.ino
 *
 * Demonstrates EMA (Exponential Moving Average) smoothing
 * for noisy analog sensor readings.
 *
 * Compatible with all Arduino boards.
 *
 * Wiring:
 *   - Connect any analog sensor (potentiometer, light sensor, etc.) to A0
 *
 * Open Serial Plotter to see raw vs smoothed values.
 */

#include <EIF.h>

// Create EMA filter with alpha = 0.2 (lower = more smoothing)
eif_ema_t smoother;

const int SENSOR_PIN = A0;
const int SAMPLE_DELAY = 20; // 50 Hz

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10); // Wait for serial on USB boards

  // Initialize EMA filter
  eif_ema_init(&smoother, 0.2f);

  Serial.println("EIF Sensor Smoothing Example");
  Serial.println("Format: Raw,Smoothed");
}

void loop() {
  // Read raw sensor value
  int raw = analogRead(SENSOR_PIN);

  // Apply EMA smoothing
  float smoothed = eif_ema_update(&smoother, (float)raw);

  // Output for Serial Plotter
  Serial.print(raw);
  Serial.print(",");
  Serial.println((int)smoothed);

  delay(SAMPLE_DELAY);
}
