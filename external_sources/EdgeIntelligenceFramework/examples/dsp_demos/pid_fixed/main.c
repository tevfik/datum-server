/**
 * @file main.c
 * @brief Fixed-Point PID Demo (Q16.16)
 *
 * Demonstrates the use of the external 'fix16pid' library.
 * Wraps floating-point CLI inputs to fixed-point for the controller.
 */

#include "eif_demo_cli.h"
#include "fix16pid.h"
#include <math.h>
#include <stdio.h>
#include <unistd.h>

// Simulation Constants (Float for Plant Model)
#define DT 0.1f
#define AMBIENT 20.0f
#define HEAT_CAP 5.0f
#define LOSS_COEFF 0.5f

// Tunable Globals (Float for CLI interaction)
float g_kp = 5.0f;
float g_ki = 0.2f;
float g_kd = 0.1f;
float g_setpoint = 40.0f;
float g_temp = AMBIENT;
float g_power = 0.0f;
int g_delay_ms = 100000;

// Fixed PID Instance
PIDController pid_fixed;

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

  eif_cli_register_float(&cli, "kp", &g_kp, false);
  eif_cli_register_float(&cli, "ki", &g_ki, false);
  eif_cli_register_float(&cli, "kd", &g_kd, false);
  eif_cli_register_float(&cli, "setpoint", &g_setpoint, false);
  eif_cli_register_float(&cli, "temp", &g_temp, true);
  eif_cli_register_float(&cli, "power", &g_power, true);
  eif_cli_register_int(&cli, "delay", &g_delay_ms, false);

  // 2. Initialize Fixed PID
  PIDController_Init(&pid_fixed);
  // Initial Params
  pid_fixed.Kp = fix16_from_float(g_kp);
  pid_fixed.Ki = fix16_from_float(g_ki);
  pid_fixed.Kd = fix16_from_float(g_kd);
  pid_fixed.tau = fix16_from_float(0.02f);
  pid_fixed.limMin = fix16_from_float(0.0f);
  pid_fixed.limMax = fix16_from_float(100.0f); // Max 100W
  pid_fixed.limMinInt = fix16_from_float(-50.0f);
  pid_fixed.limMaxInt = fix16_from_float(50.0f);
  pid_fixed.interval = fix16_from_float(DT);

  printf("Fixed-Point (Q16.16) PID Demo Started.\n");
  float t = 0;

  while (1) {
    char c = eif_cli_get_char();
    if (c)
      eif_cli_process_input(&cli, c);

    if (cli.running) {
      // Update PID Gains from CLI Globals
      pid_fixed.Kp = fix16_from_float(g_kp);
      pid_fixed.Ki = fix16_from_float(g_ki);
      pid_fixed.Kd = fix16_from_float(g_kd);

      // Run PID (Convert Inputs -> Fix16 -> Float Output)
      fix16_t sp = fix16_from_float(g_setpoint);
      fix16_t meas = fix16_from_float(g_temp);
      fix16_t out =
          PIDController_Update(&pid_fixed, sp, meas, pid_fixed.interval);

      g_power = fix16_to_float(out);

      // Update Plant
      g_temp = update_plant(g_temp, g_power);

      if (cli.json_mode) {
        printf("{\"t\": %.1f, \"setpoint\": %.1f, \"temp\": %.2f, \"power\": "
               "%.2f}\n",
               t, g_setpoint, g_temp, g_power);
      } else {
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
