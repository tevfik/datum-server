/**
 * @file main.c
 * @brief PID Controller Simulation
 *
 * Simulates a temperature control system (Heater) controlled by EIF's PID
 * algorithm.
 */

#include "eif_dsp.h"
#include <stdio.h>
#include <string.h>

// Simulation Parameters
#define DT 0.1f       // 100ms step
#define SIM_STEPS 500 // 50 seconds
#define AMBIENT_TEMP 20.0f
#define HEAT_CAPACITY 5.0f // J/K
#define LOSS_COEFF 0.5f    // W/K

// System Model: Simple thermal mass
float update_plant(float current_temp, float heater_power) {
  // Net Power = HeaterInput - Loss
  // Loss is proportional to Temp Diff
  float loss = (current_temp - AMBIENT_TEMP) * LOSS_COEFF;
  float net_power = heater_power - loss;

  // dT = Power / Capacity * dt
  float dt_temp = (net_power / HEAT_CAPACITY) * DT;
  return current_temp + dt_temp;
}

int main() {
  // 1. Initialize PID
  eif_dsp_pid_t pid;
  // Tuning: Kp=5.0 (Strong P), Ki=0.2 (Remove offset), Kd=0.1 (Damping)
  eif_dsp_pid_init(&pid, 5.0f, 0.2f, 0.1f, 0.0f, 100.0f); // Output 0-100 Watts

  // 2. Simulation Loop
  float current_temp = AMBIENT_TEMP;
  float setpoint = 20.0f;

// Arrays for logging
#define MAX_LOG SIM_STEPS
  float log_t[MAX_LOG];
  float log_setpoint[MAX_LOG];
  float log_temp[MAX_LOG];
  float log_power[MAX_LOG];

  for (int i = 0; i < SIM_STEPS; i++) {
    float t = i * DT;

    // Step change at t=5s
    if (t >= 5.0f)
      setpoint = 60.0f;
    // Disturbance at t=30s (Open window?)
    if (t >= 30.0f && t < 35.0f)
      current_temp -= 1.0f * DT;

    // Compute Control
    float error = setpoint - current_temp;
    float power = eif_dsp_pid_update(&pid, error, DT);

    // Update Plant
    current_temp = update_plant(current_temp, power);

    // Log
    log_t[i] = t;
    log_setpoint[i] = setpoint;
    log_temp[i] = current_temp;
    log_power[i] = power;
  }

  // 3. Output JSON
  printf("{\n");
  printf("  \"waveform\": {\n");

  printf("    \"setpoint\": [");
  for (int i = 0; i < SIM_STEPS; i++)
    printf("%.1f%s", log_setpoint[i], (i < SIM_STEPS - 1) ? ", " : "");
  printf("],\n");

  printf("    \"temperature\": [");
  for (int i = 0; i < SIM_STEPS; i++)
    printf("%.2f%s", log_temp[i], (i < SIM_STEPS - 1) ? ", " : "");
  printf("],\n");

  printf("    \"heater_power\": [");
  for (int i = 0; i < SIM_STEPS; i++)
    printf("%.2f%s", log_power[i], (i < SIM_STEPS - 1) ? ", " : "");
  printf("]\n");

  printf("  },\n");
  printf("  \"info\": \"PID Simulation: Heater temperature control with step "
         "response and disturbance\"\n");
  printf("}\n");

  return 0;
}
