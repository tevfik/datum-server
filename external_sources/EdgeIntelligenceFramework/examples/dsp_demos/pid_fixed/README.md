# Fixed-Point PID Demo

Demonstrates the integration of the **fix16pid** library (Q16.16 Fixed-Point Math) within the framework.

## Description
This demo performs the same heater simulation as the floating-point interactive demo, but the core PID controller logic runs using 32-bit fixed-point arithmetic (`fix16_t`).

**Key Features:**
- **Zero FPU Usage**: Ideal for low-power microcontrollers (Cortex-M0, ESP32-C3).
- **User Integration**: Showcases how external libraries (`fix16pid`) are integrated into the EIF build system.
- **Transparent CLI**: Tuning inputs (Floats) are automatically converted to Fixed-Point by the CLI wrapper.

## Usage
Run the binary:
```bash
./bin/pid_fixed_demo
```

### Commands
- `SET kp <val>`: Set P gain (Auto-converts to Q16.16).
- `SET ki <val>`: Set I gain.
- `GET temp`: Read temperature (Converted back to Float for display).
- `JSON 1`: Enable JSON output for plotting.
