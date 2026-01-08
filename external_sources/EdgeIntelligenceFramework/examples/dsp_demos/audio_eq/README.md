# Audio EQ Demo

This demo simulates a digital audio equalizer using the Edge Intelligence Framework's DSP module.

## Algorithm
It uses **Bi-Quad IIR Filters** (Infinite Impulse Response) to filter a synthetic audio signal.
- **Signal**: Mix of 100Hz (Bass) and 10kHz (Treble) sine waves.
- **Low Pass Filter**: Cutoff at 500Hz (Should keep only Bass).
- **High Pass Filter**: Cutoff at 5kHz (Should keep only Treble).

## Usage
Run the demo and pipe output to a file:
```bash
./bin/audio_eq_demo > eq_output.json
```

## Visualization
Use the provided plotter tool to view the original and filtered signals:
```bash
python3 tools/eif_plotter.py --file eq_output.json
```
You will see three lines:
1. `input`: The mixed signal.
2. `low_pass_500hz`: Smooth sine wave (100Hz).
3. `high_pass_5khz`: Rapid sine wave (10kHz).
