/**
 * ActivityMonitor.ino - Arduino Activity Recognition
 *
 * Complete end-to-end activity recognition for Arduino.
 * Supports:
 *   - Arduino Nano 33 BLE (built-in IMU)
 *   - ESP32 with MPU6050
 *   - Any board with accelerometer
 *
 * Upload, open Serial Monitor at 115200 baud.
 */

#include <Wire.h>

// Uncomment ONE of these based on your board:
#define BOARD_NANO33_BLE
// #define BOARD_ESP32_MPU6050
// #define BOARD_GENERIC

#ifdef BOARD_NANO33_BLE
#include <Arduino_LSM9DS1.h>
#endif

// =============================================================================
// EIF Includes (copy from EIF library)
// =============================================================================

// Simplified versions for Arduino
#define EIF_ACTIVITY_WINDOW_SIZE 128

typedef struct {
  float x, y, z;
} eif_accel_sample_t;

typedef struct {
  float mean_x, mean_y, mean_z;
  float std_x, std_y, std_z;
  float magnitude_mean, magnitude_std;
  float sma, energy;
  int zero_crossings;
  float peak_frequency;
} eif_activity_features_t;

// Activity types
enum ActivityType {
  ACTIVITY_STATIONARY = 0,
  ACTIVITY_WALKING,
  ACTIVITY_RUNNING,
  ACTIVITY_STAIRS,
  NUM_ACTIVITIES
};

const char *activityNames[] = {"Stationary", "Walking", "Running", "Stairs"};

// =============================================================================
// Configuration
// =============================================================================

#define SAMPLE_RATE_HZ 50
#define SAMPLE_INTERVAL_MS (1000 / SAMPLE_RATE_HZ)

// =============================================================================
// Globals
// =============================================================================

eif_accel_sample_t sampleBuffer[EIF_ACTIVITY_WINDOW_SIZE];
int bufferIndex = 0;
unsigned long lastSampleTime = 0;
unsigned long lastDisplayTime = 0;

// Simple EMA smoother
float smoothX = 0, smoothY = 0, smoothZ = 0;
const float emaAlpha = 0.3;

// =============================================================================
// Accelerometer Reading
// =============================================================================

bool readAccelerometer(float *ax, float *ay, float *az) {
#ifdef BOARD_NANO33_BLE
  if (IMU.accelerationAvailable()) {
    float x, y, z;
    IMU.readAcceleration(x, y, z);
    *ax = x * 9.81; // Convert to m/s²
    *ay = y * 9.81;
    *az = z * 9.81;
    return true;
  }
  return false;

#elif defined(BOARD_ESP32_MPU6050)
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6, true);

  int16_t raw_x = Wire.read() << 8 | Wire.read();
  int16_t raw_y = Wire.read() << 8 | Wire.read();
  int16_t raw_z = Wire.read() << 8 | Wire.read();

  *ax = raw_x / 16384.0 * 9.81;
  *ay = raw_y / 16384.0 * 9.81;
  *az = raw_z / 16384.0 * 9.81;
  return true;

#else
  // Generic: Use analog pins or replace with your sensor
  *ax = (analogRead(A0) - 512) * 0.02;
  *ay = (analogRead(A1) - 512) * 0.02;
  *az = (analogRead(A2) - 512) * 0.02 + 9.81;
  return true;
#endif
}

// =============================================================================
// Feature Extraction
// =============================================================================

void extractFeatures(eif_accel_sample_t *samples, int n,
                     eif_activity_features_t *features) {
  // Calculate means
  float sumX = 0, sumY = 0, sumZ = 0;
  float sumMag = 0;

  for (int i = 0; i < n; i++) {
    sumX += samples[i].x;
    sumY += samples[i].y;
    sumZ += samples[i].z;
    float mag = sqrt(samples[i].x * samples[i].x + samples[i].y * samples[i].y +
                     samples[i].z * samples[i].z);
    sumMag += mag;
  }

  features->mean_x = sumX / n;
  features->mean_y = sumY / n;
  features->mean_z = sumZ / n;
  features->magnitude_mean = sumMag / n;

  // Calculate standard deviations
  float varX = 0, varY = 0, varZ = 0, varMag = 0;
  float sumAbsTotal = 0;
  float sumEnergy = 0;
  int zeroCrossings = 0;
  float lastMag = 0;

  for (int i = 0; i < n; i++) {
    float dx = samples[i].x - features->mean_x;
    float dy = samples[i].y - features->mean_y;
    float dz = samples[i].z - features->mean_z;

    varX += dx * dx;
    varY += dy * dy;
    varZ += dz * dz;

    float mag = sqrt(samples[i].x * samples[i].x + samples[i].y * samples[i].y +
                     samples[i].z * samples[i].z);
    float dm = mag - features->magnitude_mean;
    varMag += dm * dm;

    sumAbsTotal += fabs(samples[i].x) + fabs(samples[i].y) + fabs(samples[i].z);
    sumEnergy += mag * mag;

    if (i > 0 && ((lastMag - features->magnitude_mean) *
                      (mag - features->magnitude_mean) <
                  0)) {
      zeroCrossings++;
    }
    lastMag = mag;
  }

  features->std_x = sqrt(varX / n);
  features->std_y = sqrt(varY / n);
  features->std_z = sqrt(varZ / n);
  features->magnitude_std = sqrt(varMag / n);
  features->sma = sumAbsTotal / n;
  features->energy = sumEnergy / n;
  features->zero_crossings = zeroCrossings;

  // Simplified peak frequency estimation
  features->peak_frequency =
      (float)zeroCrossings / (n / (float)SAMPLE_RATE_HZ) / 2.0;
}

// =============================================================================
// Classification (Rule-based)
// =============================================================================

int classifyActivity(eif_activity_features_t *features) {
  float magStd = features->magnitude_std;

  // Simple rules based on magnitude standard deviation
  if (magStd < 0.5) {
    return ACTIVITY_STATIONARY;
  } else if (magStd > 3.0) {
    return ACTIVITY_RUNNING;
  } else if (features->peak_frequency > 2.5 && magStd > 1.5) {
    return ACTIVITY_STAIRS;
  } else {
    return ACTIVITY_WALKING;
  }
}

float getConfidence(eif_activity_features_t *features) {
  float magStd = features->magnitude_std;
  float conf = 1.0 - (magStd > 5.0 ? 0.5 : magStd / 10.0);
  return conf < 0.5 ? 0.5 : conf;
}

// =============================================================================
// Setup & Loop
// =============================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("=====================================");
  Serial.println("  EIF Smart Activity Monitor");
  Serial.println("=====================================");

#ifdef BOARD_NANO33_BLE
  if (!IMU.begin()) {
    Serial.println("ERROR: IMU init failed!");
    while (1)
      ;
  }
  Serial.println("Board: Arduino Nano 33 BLE");
#elif defined(BOARD_ESP32_MPU6050)
  Wire.begin();
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  Serial.println("Board: ESP32 + MPU6050");
#else
  Serial.println("Board: Generic (analog input)");
#endif

  Serial.print("Sample rate: ");
  Serial.print(SAMPLE_RATE_HZ);
  Serial.println(" Hz");

  Serial.print("Window size: ");
  Serial.print(EIF_ACTIVITY_WINDOW_SIZE);
  Serial.print(" samples (");
  Serial.print((float)EIF_ACTIVITY_WINDOW_SIZE / SAMPLE_RATE_HZ, 1);
  Serial.println(" seconds)");

  Serial.println("\nCollecting data...\n");

  lastSampleTime = millis();
  lastDisplayTime = millis();
}

void loop() {
  unsigned long now = millis();

  // Sample at fixed rate
  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;

    float ax, ay, az;
    if (readAccelerometer(&ax, &ay, &az)) {
      // Apply smoothing
      smoothX = emaAlpha * ax + (1 - emaAlpha) * smoothX;
      smoothY = emaAlpha * ay + (1 - emaAlpha) * smoothY;
      smoothZ = emaAlpha * az + (1 - emaAlpha) * smoothZ;

      // Add to buffer
      sampleBuffer[bufferIndex].x = smoothX;
      sampleBuffer[bufferIndex].y = smoothY;
      sampleBuffer[bufferIndex].z = smoothZ;
      bufferIndex++;

      // Buffer full - classify!
      if (bufferIndex >= EIF_ACTIVITY_WINDOW_SIZE) {
        eif_activity_features_t features;
        extractFeatures(sampleBuffer, EIF_ACTIVITY_WINDOW_SIZE, &features);

        int activity = classifyActivity(&features);
        float confidence = getConfidence(&features);

        // Display result
        Serial.print("Activity: ");
        Serial.print(activityNames[activity]);
        Serial.print(" (");
        Serial.print(confidence * 100, 0);
        Serial.print("%)");
        Serial.print("  mag_std=");
        Serial.print(features.magnitude_std, 2);
        Serial.print("  sma=");
        Serial.println(features.sma, 2);

        // Reset buffer (with overlap for continuous detection)
        int hop = EIF_ACTIVITY_WINDOW_SIZE / 4;
        memmove(sampleBuffer, &sampleBuffer[hop],
                (EIF_ACTIVITY_WINDOW_SIZE - hop) * sizeof(eif_accel_sample_t));
        bufferIndex = EIF_ACTIVITY_WINDOW_SIZE - hop;
      }
    }
  }
}
