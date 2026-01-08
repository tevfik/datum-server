/**
 * @file main.c
 * @brief Interactive PID Demo
 *
 * Simulated Heater Control with Runtime Tuning.
 * Commands: SET kp 5.0, STOP, START, etc.
 */

#include "eif_demo_cli.h"
#include "eif_dsp.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Simulation Constants
#define DT 0.1f // 100ms
#define AMBIENT 20.0f
#define HEAT_CAP 5.0f
#define LOSS_COEFF 0.5f

// Tunable Globals (Exposed to CLI)
float g_kp = 5.0f;
float g_ki = 0.2f;
float g_kd = 0.1f;
float g_setpoint = 40.0f;
float g_temp = AMBIENT;
float g_power = 0.0f;
int g_delay_ms = 100000; // 100ms loop delay

float update_plant(float current_temp, float power) {
  float loss = (current_temp - AMBIENT) * LOSS_COEFF;
  float net_power = power - loss;
  float dt_temp = (net_power / HEAT_CAP) * DT;
  return current_temp + dt_temp;
}

int main() {
  // 1. Setup CLI
  eif_cli_t cli;
  eif_cli_init(&cli);

  // 2. Register Parameters
  eif_cli_register_float(&cli, "kp", &g_kp, false);
  eif_cli_register_float(&cli, "ki", &g_ki, false);
  eif_cli_register_float(&cli, "kd", &g_kd, false);
  eif_cli_register_float(&cli, "setpoint", &g_setpoint, false);
  eif_cli_register_float(&cli, "temp", &g_temp, true);   // Read Only
  eif_cli_register_float(&cli, "power", &g_power, true); // Read Only
  eif_cli_register_int(&cli, "delay", &g_delay_ms, false);

  // 3. Initialize PID
  eif_dsp_pid_t pid;
  eif_dsp_pid_init(&pid, g_kp, g_ki, g_kd, 0.0f, 100.0f);

  printf("Interactive PID Demo Started.\n");
  printf("Type 'HELP' for commands.\n");
  printf("Default Setpoint: %.1f C\n", g_setpoint);

  float t = 0;

  while (1) {
    // --- CLI Processing ---
    char c = eif_cli_get_char();
    if (c)
      eif_cli_process_input(&cli, c);

    if (cli.running) {
      // Update PID Gains (in case they changed via CLI)
      // Note: In a real system, we might use setters,
      // but here we just re-init or update struct directly if supported.
      // eif_dsp_pid_init resets state, so we manually update coeffs:
      pid.Kp = g_kp;
      pid.Ki = g_ki;
      pid.Kd = g_kd;

      // Compute Control
      float error = g_setpoint - g_temp;
      g_power = eif_dsp_pid_update(&pid, error, DT);

      // Update Plant
      g_temp = update_plant(g_temp, g_power);

      // Output Status
      if (cli.json_mode) {
        printf("{\"t\": %.1f, \"setpoint\": %.1f, \"temp\": %.2f, \"power\": "
               "%.2f}\n",
               t, g_setpoint, g_temp, g_power);
      } else {
        // Print roughly every second
        static int count = 0;
        if (count++ % 10 == 0) {
          printf("[%5.1fs] Temp: %.2fC (Target: %.1f) Power: %.1fW\r", t,
                 g_temp, g_setpoint, g_power);
          fflush(stdout);
        }
      }

      t += DT;
    }

    usleep(g_delay_ms);
  }

  return 0;
}
