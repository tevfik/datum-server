# Interactive PID Demo

This demo allows real-time tuning of a PID controller simulation using the CLI.

## Description
Simulates a heater system where you can adjust the Proportional (`kp`), Integral (`ki`), and Derivative (`kd`) gains on the fly to observe improved response or instability.

## Usage
Run the binary:
```bash
./bin/pid_interactive_demo
```

### Commands
- `SET kp 10.0`: Change Proportional gain.
- `SET ki 0.5`: Change Integral gain.
- `SET kd 0.1`: Change Derivative gain.
- `SET setpoint 60.0`: Change target temperature.
- `GET temp`: Read current temperature.
- `JSON 1`: Enable machine-readable output.
- `STOP` / `START`: Control simulation.
