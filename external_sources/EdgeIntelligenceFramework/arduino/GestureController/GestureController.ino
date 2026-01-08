/**
 * GestureController.ino - Arduino Gesture Recognition
 *
 * Turn your Arduino into a gesture controller!
 * Detects: Tap, Double-tap, Shake, Swipe (left/right/up/down)
 *
 * Hardware:
 * - Arduino Nano 33 BLE (built-in LSM9DS1 IMU)
 * - OR ESP32 + MPU6050
 * - OR Arduino Uno + ADXL345/MPU6050
 *
 * Wiring (for MPU6050):
 *   VCC -> 3.3V
 *   GND -> GND
 *   SCL -> SCL (A5 on Uno)
 *   SDA -> SDA (A4 on Uno)
 *
 * Upload, open Serial Monitor at 115200 baud.
 * Move the device to see detected gestures!
 */

#include <Wire.h>

// ============================================================================
// Board Selection - Uncomment one:
// ============================================================================
#define BOARD_NANO33_BLE
// #define BOARD_ESP32_MPU6050
// #define BOARD_MPU6050

#ifdef BOARD_NANO33_BLE
#include <Arduino_LSM9DS1.h>
#endif

// ============================================================================
// Configuration
// ============================================================================

#define SAMPLE_RATE_HZ 50
#define MOTION_THRESHOLD 12.0 // m/s², start detecting motion
#define IDLE_THRESHOLD 10.5   // m/s², motion ended
#define MAX_GESTURE_LEN 100

// ============================================================================
// Gesture Types
// ============================================================================

enum GestureType {
  GESTURE_NONE = 0,
  GESTURE_TAP,
  GESTURE_DOUBLE_TAP,
  GESTURE_SHAKE,
  GESTURE_SWIPE_LEFT,
  GESTURE_SWIPE_RIGHT,
  GESTURE_SWIPE_UP,
  GESTURE_SWIPE_DOWN,
  NUM_GESTURES
};

const char *gestureNames[] = {"None",     "Tap",        "Double Tap",
                              "Shake",    "Swipe Left", "Swipe Right",
                              "Swipe Up", "Swipe Down"};

const char *gestureEmojis[] = {"  ", "👆", "✌️", "🫨", "⬅️", "➡️", "⬆️", "⬇️"};

// ============================================================================
// Simple Gesture Detector (from EIF)
// ============================================================================

struct SimpleGesture {
  float ax_sum, ay_sum, az_sum;
  float ax_max, ay_max, az_max;
  int count;
  bool active;
  float threshold;
};

SimpleGesture detector = {0};

void initGestureDetector(float threshold) {
  detector.ax_sum = detector.ay_sum = detector.az_sum = 0;
  detector.ax_max = detector.ay_max = detector.az_max = 0;
  detector.count = 0;
  detector.active = false;
  detector.threshold = threshold;
}

GestureType updateGestureDetector(float ax, float ay, float az) {
  float mag = sqrt(ax * ax + ay * ay + az * az);

  // Detect motion start
  if (!detector.active && mag > detector.threshold) {
    detector.active = true;
    detector.ax_sum = detector.ay_sum = detector.az_sum = 0;
    detector.ax_max = detector.ay_max = detector.az_max = 0;
    detector.count = 0;
  }

  // Accumulate during motion
  if (detector.active) {
    detector.ax_sum += ax;
    detector.ay_sum += ay;
    detector.az_sum += az;

    if (fabs(ax) > fabs(detector.ax_max))
      detector.ax_max = ax;
    if (fabs(ay) > fabs(detector.ay_max))
      detector.ay_max = ay;
    if (fabs(az) > fabs(detector.az_max))
      detector.az_max = az;

    detector.count++;

    // Detect motion end
    if (mag < detector.threshold * 0.5f && detector.count > 5) {
      detector.active = false;

      // Classify based on dominant direction
      float abs_x = fabs(detector.ax_sum);
      float abs_y = fabs(detector.ay_sum);
      float abs_z = fabs(detector.az_sum);

      if (abs_x > abs_y && abs_x > abs_z) {
        return detector.ax_sum > 0 ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
      } else if (abs_y > abs_x && abs_y > abs_z) {
        return detector.ay_sum > 0 ? GESTURE_SWIPE_UP : GESTURE_SWIPE_DOWN;
      } else if (detector.count < 10 &&
                 fabs(detector.az_max) > detector.threshold * 2) {
        return GESTURE_TAP;
      } else if (detector.count > 30) {
        return GESTURE_SHAKE;
      }
    }
  }

  return GESTURE_NONE;
}

// ============================================================================
// IMU Reading
// ============================================================================

bool readAccelerometer(float &ax, float &ay, float &az) {
#ifdef BOARD_NANO33_BLE
  if (IMU.accelerationAvailable()) {
    float x, y, z;
    IMU.readAcceleration(x, y, z);
    ax = x * 9.81; // Convert to m/s²
    ay = y * 9.81;
    az = z * 9.81;
    return true;
  }
  return false;

#elif defined(BOARD_ESP32_MPU6050) || defined(BOARD_MPU6050)
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6, true);

  int16_t raw_x = Wire.read() << 8 | Wire.read();
  int16_t raw_y = Wire.read() << 8 | Wire.read();
  int16_t raw_z = Wire.read() << 8 | Wire.read();

  ax = raw_x / 16384.0 * 9.81;
  ay = raw_y / 16384.0 * 9.81;
  az = raw_z / 16384.0 * 9.81;
  return true;
#endif

  return false;
}

// ============================================================================
// Statistics
// ============================================================================

int gestureCounts[NUM_GESTURES] = {0};
unsigned long lastGestureTime = 0;

void printStats() {
  Serial.println("\n--- Gesture Statistics ---");
  for (int i = 1; i < NUM_GESTURES; i++) {
    Serial.print(gestureNames[i]);
    Serial.print(": ");
    Serial.println(gestureCounts[i]);
  }
  Serial.println("--------------------------\n");
}

// ============================================================================
// Setup & Loop
// ============================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║    EIF Gesture Controller Demo        ║");
  Serial.println("╚═══════════════════════════════════════╝\n");

#ifdef BOARD_NANO33_BLE
  if (!IMU.begin()) {
    Serial.println("ERROR: IMU initialization failed!");
    while (1)
      ;
  }
  Serial.println("Board: Arduino Nano 33 BLE");
#elif defined(BOARD_ESP32_MPU6050) || defined(BOARD_MPU6050)
  Wire.begin();
  Wire.beginTransmission(0x68);
  Wire.write(0x6B); // Power management
  Wire.write(0);    // Wake up
  Wire.endTransmission(true);
  Serial.println("Board: MPU6050");
#endif

  Serial.print("Sample rate: ");
  Serial.print(SAMPLE_RATE_HZ);
  Serial.println(" Hz");

  Serial.println("\nMove the device to detect gestures!");
  Serial.println("Supported: Tap, Shake, Swipe (left/right/up/down)");
  Serial.println("Press 's' for statistics\n");

  initGestureDetector(MOTION_THRESHOLD);
  lastGestureTime = millis();
}

void loop() {
  static unsigned long lastSample = 0;
  const unsigned long sampleInterval = 1000 / SAMPLE_RATE_HZ;

  // Check for stats request
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's' || c == 'S') {
      printStats();
    }
  }

  // Sample at fixed rate
  if (millis() - lastSample >= sampleInterval) {
    lastSample = millis();

    float ax, ay, az;
    if (readAccelerometer(ax, ay, az)) {
      GestureType gesture = updateGestureDetector(ax, ay, az);

      if (gesture != GESTURE_NONE) {
        // Avoid rapid duplicate detections
        if (millis() - lastGestureTime > 500) {
          gestureCounts[gesture]++;
          lastGestureTime = millis();

          // Print detected gesture
          Serial.print("🎯 Detected: ");
          Serial.print(gestureEmojis[gesture]);
          Serial.print(" ");
          Serial.println(gestureNames[gesture]);
        }
      }
    }
  }

  // Heartbeat LED for visual feedback
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    lastBlink = millis();
    // Toggle built-in LED
    static bool ledState = false;
    ledState = !ledState;
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, ledState);
#endif
  }
}
