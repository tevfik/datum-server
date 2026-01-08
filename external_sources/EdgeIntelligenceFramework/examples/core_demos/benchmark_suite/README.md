# Benchmark Suite

Performance benchmarks for all EIF components.

## Features

- **DSP Benchmarks** - EMA, Median, FIR, Biquad filters
- **ML Benchmarks** - Threshold, Kalman, Activity recognition
- **Memory Analysis** - Struct sizes for memory planning

## Running

```bash
./build/bin/benchmark_suite --batch
```

## Sample Output

```
╔══════════════════╗
║  DSP Benchmarks  ║
╚══════════════════╝

| Benchmark                      |        Time |    Throughput |   Memory |
|--------------------------------|-------------|---------------|----------|
| EMA filter                     |      0.0 µs | 28285714 ops/s |     12 B |
| Median filter (7)              |      0.2 µs |  4626168 ops/s |     40 B |
| FIR filter (16 taps, float)    |      0.1 µs |  6780822 ops/s |    520 B |
| FIR filter (16 taps, Q15)      |      0.1 µs | 10760870 ops/s |    264 B |
| Biquad filter (float)          |      0.0 µs | 24146341 ops/s |     28 B |

╔═════════════════╗
║  ML Benchmarks  ║
╚═════════════════╝

| Benchmark                      |        Time |    Throughput |   Memory |
|--------------------------------|-------------|---------------|----------|
| Adaptive Z-threshold           |      0.1 µs | 14347826 ops/s |     48 B |
| Complementary filter           |      0.0 µs | 26052632 ops/s |     12 B |
| 1D Kalman filter               |      0.1 µs | 19800000 ops/s |     16 B |
| Activity features (128 samples) |      5.2 µs |   191532 ops/s |   1596 B |
| Activity classification        |      0.0 µs | 26756757 ops/s |     60 B |
```

## Interpreting Results

- **Time**: Average microseconds per operation
- **Throughput**: Operations per second (higher = better)
- **Memory**: Stack/heap bytes required

## Platform Scaling

| Platform | Approximate Factor |
|----------|-------------------|
| x86_64 (benchmark host) | 1x |
| ARM Cortex-M4 @ 80MHz | 50-100x slower |
| AVR @ 16MHz (no FPU) | 200-500x slower |
| ESP32 @ 240MHz | 10-20x slower |

To estimate embedded time: `embedded_time = host_time × factor`
