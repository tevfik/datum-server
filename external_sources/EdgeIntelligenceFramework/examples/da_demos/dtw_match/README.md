# DTW Pattern Matching Demo

This demo illustrates standard **Dynamic Time Warping (DTW)** for sequence matching.

## Algorithm
DTW calculates the optimal alignment between two time series by finding a path through the cost matrix that minimizes total distance. This implementation supports:
- **Sakoe-Chiba constraints**: Windowing to limit path deviation from diagonal (speeds up computation).
- **O(N) memory**: Uses only 2 rows for cost calculation (if path not needed).

## Usage
```bash
./bin/dtw_match_demo          # ASCII output
./bin/dtw_match_demo --json   # JSON output with signals
```

## JSON Output
```json
{
  "type": "dtw_match",
  "best_match": "Sine",
  "distances": {"Sine": 2.1, "Square": 15.4, ...},
  "input": [...]
}
```
