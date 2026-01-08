# PID Controller Simulation

This demo simulates a closed-loop control system (Temperature Control) using EIF's PID algorithm.

## Scenario
- **System**: A heater attempting to reach a target temperature.
- **Physics**: First-order system (Heat Capacity + Heat Loss).
- **Control**: PID controller adjusts heater power (0-100W) based on error.
- **Events**:
  - `t=0`: Simulation starts. Ambient=20C. Setpoint=20C.
  - `t=5s`: Setpoint jumps to 60C (Step Input).
  - `t=30s`: Temperature drops suddenly (Simulated Disturbance, e.g., open window).

## Usage
Run the demo and pipe output:
```bash
./bin/pid_sim_demo > pid_output.json
```

## Visualization
Use the plotter:
```bash
python3 tools/eif_plotter.py --file pid_output.json
```
Observe the "Step Response" (how fast it reaches 60C) and "Disturbance Rejection" (how it recovers at 30s).
