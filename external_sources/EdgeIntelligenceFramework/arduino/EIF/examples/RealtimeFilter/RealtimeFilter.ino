/*
 * RealtimeFilter.ino
 *
 * Demonstrates real-time audio filtering using a biquad lowpass filter.
 * Useful for removing high-frequency noise from sensor signals.
 *
 * Compatible with all Arduino boards.
 *
 * Wiring:
 *   - Audio/sensor input on A0
 *   - Filtered output on PWM pin 9
 *
 * For AVR boards, use the fixed-point version for better performance.
 */

#include <EIF.h>

#if defined(__AVR__)
// Use fixed-point on AVR for speed
eif_biquad_q15_t filter;
#define USE_FIXED_POINT
#else
// Use floating point on ARM/ESP32
eif_biquad_t filter;
#endif

const int INPUT_PIN = A0;
const int OUTPUT_PIN = 9;

// Sample rate in Hz (adjust based on your needs)
const float SAMPLE_RATE = 4000.0f;

// Cutoff frequency in Hz
const float CUTOFF_FREQ = 500.0f;

void setup() {
  Serial.begin(115200);

  pinMode(OUTPUT_PIN, OUTPUT);

// Initialize filter
#ifdef USE_FIXED_POINT
  // Use pre-computed Butterworth lowpass (fc/fs = 0.1)
  eif_biquad_q15_butter_lp_01(&filter);
  Serial.println("Using Q15 fixed-point filter (AVR optimized)");
#else
  // Design lowpass filter: fs=4000Hz, fc=500Hz, Q=0.707
  eif_biquad_lowpass(&filter, SAMPLE_RATE, CUTOFF_FREQ, 0.707f);
  Serial.println("Using floating-point filter");
#endif

  Serial.println("EIF Realtime Filter Example");
  Serial.print("Sample rate: ");
  Serial.print(SAMPLE_RATE);
  Serial.print(" Hz, Cutoff: ");
  Serial.print(CUTOFF_FREQ);
  Serial.println(" Hz");

// Speed up ADC on AVR
#if defined(__AVR__)
  ADCSRA = (ADCSRA & 0xF8) | 0x04; // Prescaler 16 (~77kHz ADC)
#endif
}

void loop() {
  // Read input
  int rawInput = analogRead(INPUT_PIN);

#ifdef USE_FIXED_POINT
  // Convert to Q15 (center around 0)
  int16_t input = (rawInput - 512) << 5;

  // Apply filter
  int16_t output = eif_biquad_q15_process(&filter, input);

  // Convert back to 8-bit for PWM output
  int pwmOutput = (output >> 7) + 128;
#else
  // Convert to float (-1.0 to 1.0)
  float input = (rawInput - 512) / 512.0f;

  // Apply filter
  float output = eif_biquad_process(&filter, input);

  // Convert to 8-bit for PWM output
  int pwmOutput = (int)(output * 127.0f + 128.0f);
#endif

  // Clamp and output
  pwmOutput = constrain(pwmOutput, 0, 255);
  analogWrite(OUTPUT_PIN, pwmOutput);

  // Maintain sample rate (approximate)
  // For precise timing, use timer interrupts
  delayMicroseconds(1000000 / (int)SAMPLE_RATE);
}
