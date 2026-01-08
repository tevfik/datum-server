/*
 * ButtonDebounce.ino
 *
 * Demonstrates debouncing a mechanical button using EIF.
 * Eliminates false triggers from contact bounce.
 *
 * Compatible with all Arduino boards.
 *
 * Wiring:
 *   - Connect button between pin 2 and GND (uses internal pullup)
 *   - LED on pin 13 (built-in on most boards)
 */

#include <EIF.h>

// Create debouncer - requires 5 consecutive same readings
eif_debounce_t debouncer;

const int BUTTON_PIN = 2;
const int LED_PIN = LED_BUILTIN;
const int SAMPLE_DELAY = 10; // 100 Hz sampling

bool ledState = false;
bool lastButtonState = false;

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  // Initialize debouncer (5 samples = 50ms at 100Hz)
  eif_debounce_init(&debouncer, 5);

  Serial.println("EIF Button Debounce Example");
  Serial.println("Press button to toggle LED");
}

void loop() {
  // Read button (active low due to pullup)
  bool rawState = !digitalRead(BUTTON_PIN);

  // Apply debouncing
  bool stableState = eif_debounce_update(&debouncer, rawState);

  // Detect rising edge (button press)
  if (stableState && !lastButtonState) {
    // Toggle LED
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);

    Serial.print("Button pressed! LED is now ");
    Serial.println(ledState ? "ON" : "OFF");
  }

  lastButtonState = stableState;

  delay(SAMPLE_DELAY);
}
