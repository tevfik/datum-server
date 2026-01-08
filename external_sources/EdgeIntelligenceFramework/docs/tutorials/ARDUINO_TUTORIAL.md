# Arduino Tutorial: Complete Guide

Step-by-step projects for learning Edge Intelligence on Arduino.

---

## Prerequisites

### Hardware
- **Beginners**: Arduino Uno/Nano + sensors
- **Intermediate**: Arduino Nano 33 BLE or ESP32
- **Advanced**: Portenta H7 or custom boards

### Software
- Arduino IDE 2.x
- EIF Library installed (see ARDUINO_GUIDE.md)

---

## Project 1: Smart Sensor Smoother

**Difficulty**: ⭐ Beginner  
**Time**: 15 minutes  
**Hardware**: Arduino Uno + Potentiometer

### Goal
Learn to smooth noisy analog readings using EMA filter.

### Wiring
```
Potentiometer:
  - Left pin  → GND
  - Middle    → A0
  - Right pin → 5V
```

### Code
```cpp
/*
 * Sensor Smoother Tutorial
 * 
 * Watch the Serial Plotter to see raw vs smoothed values
 */

#include <EIF.h>

// EMA filter with alpha=0.1 (more smoothing) to 0.9 (less smoothing)
eif_ema_t smoother;

const int SENSOR_PIN = A0;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  // Try different alpha values: 0.1, 0.2, 0.5
  eif_ema_init(&smoother, 0.2f);
  
  Serial.println("Raw,Smoothed");
}

void loop() {
  // Read raw sensor
  int raw = analogRead(SENSOR_PIN);
  
  // Apply smoothing
  float smoothed = eif_ema_update(&smoother, (float)raw);
  
  // Output for Serial Plotter
  Serial.print(raw);
  Serial.print(",");
  Serial.println((int)smoothed);
  
  delay(20);  // 50 Hz
}
```

### Expected Output
Open **Tools → Serial Plotter** to see:
- Blue line (raw): Jumpy, noisy
- Red line (smoothed): Clean, follows trend

### Experiments
1. Change alpha to 0.1 - what happens?
2. Change alpha to 0.9 - what happens?
3. Add electrical noise (touch the wire) - how does smoothing help?

---

## Project 2: Perfect Button Debouncing

**Difficulty**: ⭐ Beginner  
**Time**: 20 minutes  
**Hardware**: Arduino + Push button

### Goal
Eliminate false button triggers using debouncer.

### The Problem
Mechanical buttons "bounce" - one press can register as 5-10 presses.

### Wiring
```
Button:
  - One leg  → Pin 2
  - Other leg → GND
  
(Using internal pullup, no external resistor needed)
```

### Code
```cpp
/*
 * Button Debounce Tutorial
 * 
 * Press button to toggle LED - no false triggers!
 */

#include <EIF.h>

eif_debounce_t debouncer;

const int BUTTON_PIN = 2;
const int LED_PIN = LED_BUILTIN;

bool ledState = false;
bool lastStable = false;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  // Require 5 consecutive same readings
  // At 100Hz sampling = 50ms debounce time
  eif_debounce_init(&debouncer, 5);
  
  Serial.println("Press button to toggle LED");
}

void loop() {
  // Read button (active low with pullup)
  bool raw = !digitalRead(BUTTON_PIN);
  
  // Apply debouncing
  bool stable = eif_debounce_update(&debouncer, raw);
  
  // Detect rising edge (button just pressed)
  if (stable && !lastStable) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    Serial.print("LED: ");
    Serial.println(ledState ? "ON" : "OFF");
  }
  
  lastStable = stable;
  delay(10);  // 100 Hz sampling
}
```

### How It Works
The debouncer requires 5 consecutive identical readings before changing state:
```
Raw:    1-0-1-1-1-1-1-1-0-0  (bouncing)
Stable: 0-0-0-0-0-1-1-1-1-1  (clean transition)
```

### Experiments
1. Remove debouncing - count false triggers
2. Change threshold to 3 or 10 - what's the tradeoff?
3. Add a second button

---

## Project 3: Temperature Anomaly Detector

**Difficulty**: ⭐⭐ Intermediate  
**Time**: 30 minutes  
**Hardware**: Arduino Nano 33 BLE or ESP32 + TMP36 sensor

### Goal
Detect unusual temperature changes automatically.

### Wiring (TMP36)
```
TMP36:
  - Left pin (flat side facing you) → 5V
  - Middle pin → A0
  - Right pin → GND
```

### Code
```cpp
/*
 * Temperature Anomaly Detector
 * 
 * Learns normal range, alerts on anomalies
 */

#include <EIF.h>

eif_z_threshold_t detector;
eif_ema_t smoother;

const int TEMP_PIN = A0;
const int LED_PIN = LED_BUILTIN;
const int BUZZER_PIN = 8;  // Optional buzzer

// Calibration
bool calibrated = false;
int calibrationCount = 0;
float baselineTemp = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize detector
  // alpha=0.05 (slow adaptation), threshold=3.0 (3 sigma)
  eif_z_threshold_init(&detector, 0.05f, 3.0f);
  eif_ema_init(&smoother, 0.3f);
  
  Serial.println("Temperature Anomaly Detector");
  Serial.println("Calibrating for 30 seconds...");
}

float readTemperature() {
  int raw = analogRead(TEMP_PIN);
  float voltage = raw * (5.0f / 1023.0f);
  return (voltage - 0.5f) * 100.0f;  // TMP36 formula
}

void loop() {
  float temp = readTemperature();
  temp = eif_ema_update(&smoother, temp);  // Smooth
  
  // Calibration phase (first 30 seconds)
  if (!calibrated) {
    calibrationCount++;
    baselineTemp += temp;
    
    if (calibrationCount >= 300) {  // At 100ms delay
      baselineTemp /= calibrationCount;
      calibrated = true;
      Serial.print("Calibration complete. Baseline: ");
      Serial.print(baselineTemp, 1);
      Serial.println(" °C");
      Serial.println("Now monitoring for anomalies...");
    }
    
    // Blink during calibration
    digitalWrite(LED_PIN, (millis() / 500) % 2);
    delay(100);
    return;
  }
  
  // Detection phase
  bool isAnomaly = eif_z_threshold_check(&detector, temp);
  
  // Get current bounds
  float lower, upper;
  eif_z_threshold_get_bounds(&detector, &lower, &upper);
  
  // Display
  Serial.print("Temp: ");
  Serial.print(temp, 1);
  Serial.print(" °C  [");
  Serial.print(lower, 1);
  Serial.print(" - ");
  Serial.print(upper, 1);
  Serial.print("]  ");
  
  if (isAnomaly) {
    Serial.println("*** ANOMALY! ***");
    digitalWrite(LED_PIN, HIGH);
    tone(BUZZER_PIN, 1000, 100);  // Beep
  } else {
    Serial.println("Normal");
    digitalWrite(LED_PIN, LOW);
  }
  
  delay(100);  // 10 Hz
}
```

### Testing
1. Let device calibrate (LED blinks)
2. Touch sensor with finger - anomaly!
3. Put near AC vent - anomaly!
4. Move away - returns to normal

### How Threshold Adaptation Works
```
Time 0:  Mean=25°C, Std=0.5°C → Threshold: [23.5, 26.5]
Time 60: Mean=25°C, Std=0.3°C → Threshold: [24.1, 25.9]
         (Tighter as it learns the stable range)
```

---

## Project 4: Gesture Recognition Wand

**Difficulty**: ⭐⭐⭐ Advanced  
**Time**: 45 minutes  
**Hardware**: Arduino Nano 33 BLE (has IMU) or ESP32 + MPU6050

### Goal
Recognize hand gestures using accelerometer.

### Wiring (ESP32 + MPU6050)
```
MPU6050:
  - VCC → 3.3V
  - GND → GND
  - SDA → GPIO 21
  - SCL → GPIO 22
```

### Code
```cpp
/*
 * Gesture Recognition Wand
 * 
 * Detects: Swipe Left/Right/Up/Down, Shake
 */

#include <Wire.h>
#include <EIF.h>

// For Nano 33 BLE, use:
// #include <Arduino_LSM9DS1.h>
// #define USE_NANO33

eif_simple_gesture_t gesture;
eif_ema_t smooth_x, smooth_y, smooth_z;

const int LED_PIN = LED_BUILTIN;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  pinMode(LED_PIN, OUTPUT);
  
  #ifdef USE_NANO33
    if (!IMU.begin()) {
      Serial.println("IMU init failed!");
      while(1);
    }
  #else
    // MPU6050 init
    Wire.begin();
    Wire.beginTransmission(0x68);
    Wire.write(0x6B);  // PWR_MGMT_1
    Wire.write(0);     // Wake up
    Wire.endTransmission(true);
  #endif
  
  // Initialize gesture detector
  // Motion threshold: 1.5 g
  eif_simple_gesture_init(&gesture, 1.5f);
  
  // Smoothers for accelerometer
  eif_ema_init(&smooth_x, 0.3f);
  eif_ema_init(&smooth_y, 0.3f);
  eif_ema_init(&smooth_z, 0.3f);
  
  Serial.println("Gesture Wand Ready!");
  Serial.println("Try: Swipe left, right, up, down, or shake");
}

void readAccel(float *ax, float *ay, float *az) {
  #ifdef USE_NANO33
    float x, y, z;
    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(x, y, z);
      *ax = x * 9.81f;
      *ay = y * 9.81f;
      *az = z * 9.81f;
    }
  #else
    // MPU6050
    Wire.beginTransmission(0x68);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(0x68, 6, true);
    
    int16_t raw_x = Wire.read() << 8 | Wire.read();
    int16_t raw_y = Wire.read() << 8 | Wire.read();
    int16_t raw_z = Wire.read() << 8 | Wire.read();
    
    // Convert to m/s² (±2g range)
    *ax = raw_x / 16384.0f * 9.81f;
    *ay = raw_y / 16384.0f * 9.81f;
    *az = raw_z / 16384.0f * 9.81f;
  #endif
}

const char* gestureNames[] = {
  "None", "Swipe Left", "Swipe Right", "Swipe Up", "Swipe Down",
  "Circle CW", "Circle CCW", "Tap", "Double Tap", "Shake"
};

void loop() {
  float ax, ay, az;
  readAccel(&ax, &ay, &az);
  
  // Smooth readings
  ax = eif_ema_update(&smooth_x, ax);
  ay = eif_ema_update(&smooth_y, ay);
  az = eif_ema_update(&smooth_z, az);
  
  // Detect gesture
  eif_gesture_type_t detected = eif_simple_gesture_update(&gesture, ax, ay, az);
  
  if (detected != EIF_GESTURE_NONE) {
    Serial.print("GESTURE: ");
    Serial.println(gestureNames[detected]);
    
    // Visual feedback
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
  }
  
  delay(20);  // 50 Hz
}
```

### Gestures Supported
| Gesture | Motion |
|---------|--------|
| Swipe Left | Move wand left quickly |
| Swipe Right | Move wand right quickly |
| Swipe Up | Move wand up quickly |
| Swipe Down | Move wand down quickly |
| Shake | Shake wand rapidly |

### Tips
- Hold device firmly
- Make deliberate, quick movements
- Wait for motion to stop before next gesture

---

## Project 5: Wearable Activity Tracker

**Difficulty**: ⭐⭐⭐ Advanced  
**Time**: 60 minutes  
**Hardware**: Arduino Nano 33 BLE

### Goal
Track activities: Stationary, Walking, Running

### Code
```cpp
/*
 * Wearable Activity Tracker
 * 
 * Strap to wrist or ankle for best results
 */

#include <Arduino_LSM9DS1.h>
#include <EIF.h>

eif_activity_window_t window;
eif_accel_sample_t samples[EIF_ACTIVITY_WINDOW_SIZE];

// Step counter
int stepCount = 0;
float lastMagnitude = 0;
bool stepHigh = false;

// Activity counters
unsigned long activityStartTime = 0;
eif_activity_t currentActivity = EIF_ACTIVITY_STATIONARY;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  if (!IMU.begin()) {
    Serial.println("IMU init failed!");
    while(1);
  }
  
  // Window hop = 32 samples (~0.6s at 50Hz)
  eif_activity_window_init(&window, 32);
  
  Serial.println("Activity Tracker Ready!");
  Serial.println("Strap device to wrist and move around");
  Serial.println("");
  Serial.println("Activity       | Steps | Duration");
  Serial.println("---------------|-------|----------");
  
  activityStartTime = millis();
}

void loop() {
  float x, y, z;
  
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    
    // Convert to m/s²
    x *= 9.81f;
    y *= 9.81f;
    z *= 9.81f;
    
    // Simple step detection
    float magnitude = sqrt(x*x + y*y + z*z);
    if (magnitude > 11.0f && !stepHigh && lastMagnitude < 11.0f) {
      stepCount++;
      stepHigh = true;
    }
    if (magnitude < 10.5f) {
      stepHigh = false;
    }
    lastMagnitude = magnitude;
    
    // Add to activity window
    if (eif_activity_window_add(&window, x, y, z)) {
      // Window full - classify
      eif_activity_window_get_samples(&window, samples);
      
      eif_activity_features_t features;
      eif_activity_extract_features(samples, EIF_ACTIVITY_WINDOW_SIZE, &features);
      
      eif_activity_t activity = eif_activity_classify_rules(&features);
      
      // Activity changed?
      if (activity != currentActivity) {
        unsigned long duration = (millis() - activityStartTime) / 1000;
        
        // Print previous activity
        Serial.print(eif_activity_names[currentActivity]);
        for (int i = strlen(eif_activity_names[currentActivity]); i < 15; i++) {
          Serial.print(" ");
        }
        Serial.print("| ");
        Serial.print(stepCount);
        Serial.print("    | ");
        Serial.print(duration);
        Serial.println("s");
        
        // Reset
        currentActivity = activity;
        activityStartTime = millis();
        stepCount = 0;
      }
    }
  }
  
  delay(20);  // 50 Hz
}
```

### Sample Output
```
Activity Tracker Ready!
Strap device to wrist and move around

Activity       | Steps | Duration
---------------|-------|----------
Stationary     | 0     | 45s
Walking        | 127   | 180s
Running        | 89    | 60s
Stationary     | 0     | 30s
```

### Tips for Accuracy
- Secure attachment (wrist strap works best)
- Consistent orientation
- Calibrate step threshold for your gait

---

## Troubleshooting

### Library Not Found
```
fatal error: EIF.h: No such file or directory
```
**Solution**: Copy EIF library to `~/Arduino/libraries/EIF/`

### Out of Memory (AVR)
```
Error: region 'ram' overflowed
```
**Solution**: Use fixed-point filters only, reduce buffer sizes

### Erratic Readings
- Check wiring connections
- Add decoupling capacitor (0.1µF) near sensor VCC
- Increase smoothing alpha

---

## Next Steps

1. Combine multiple projects
2. Add BLE to send data to phone
3. Log data to SD card
4. Try ML classification (see TINYML_DEPLOYMENT.md)
