# Performance Benchmarks

Benchmark results for Edge Intelligence Framework components.

> Results measured on x86_64 host CPU. Embedded performance will vary based on processor speed,
> availability of FPU, and compiler optimizations.

## DSP Components

| Component | Time | Throughput | Memory |
|-----------|------|------------|--------|
| EMA filter | <0.1 µs | 28M ops/s | 12 B |
| Median filter (7) | 0.2 µs | 4.6M ops/s | 40 B |
| FIR (16 taps, float) | 0.1 µs | 6.8M ops/s | 520 B |
| FIR (16 taps, Q15) | 0.1 µs | 10.8M ops/s | 264 B |
| Biquad (float) | <0.1 µs | 24M ops/s | 28 B |

**Key Observations:**
- Q15 fixed-point FIR is ~1.6x faster than float
- All filters suitable for real-time audio (>44.1kHz)
- Minimal memory footprint for embedded use

## ML Components

| Component | Time | Throughput | Memory |
|-----------|------|------------|--------|
| Adaptive Z-threshold | 0.1 µs | 14M ops/s | 48 B |
| Complementary filter | <0.1 µs | 26M ops/s | 12 B |
| 1D Kalman filter | 0.1 µs | 20M ops/s | 16 B |
| Activity features (128) | 5.2 µs | 192K ops/s | 1.6 KB |
| Activity classifier | <0.1 µs | 27M ops/s | 60 B |

**Key Observations:**
- All classifiers under 1 µs per inference
- Feature extraction dominates activity recognition time
- Sensor fusion fast enough for >1kHz sample rates

## Memory Summary

| Category | Small | Medium | Large |
|----------|-------|--------|-------|
| **Filters** | 12-28 B | 40-264 B | 520 B |
| **ML** | 12-60 B | 1.6 KB | 4+ KB |
| **Total typical** | <1 KB | 2-4 KB | 8-16 KB |

## Platform Estimates

### ARM Cortex-M4 @ 80MHz

| Operation | Estimated Time |
|-----------|---------------|
| EMA | <1 µs |
| FIR (16, Q15) | 2-4 µs |
| Biquad (float) | 1-2 µs |
| Activity classify | 1-2 µs |
| Activity features | 50-100 µs |

### AVR @ 16MHz (no FPU)

| Operation | Estimated Time |
|-----------|---------------|
| EMA (float) | 5-10 µs |
| FIR (16, Q15) | 10-20 µs |
| Median (7) | 15-25 µs |

### ESP32 @ 240MHz

| Operation | Estimated Time |
|-----------|---------------|
| All filters | <1 µs |
| Activity pipeline | 10-20 µs |
| Neural inference (tiny) | 100-500 µs |

## Running Benchmarks

```bash
# Build and run
cd build && cmake .. && make benchmark_suite
./bin/benchmark_suite --batch

# For detailed output
./bin/benchmark_suite
```

## Optimizing Performance

1. **Use Q15 fixed-point** on MCUs without FPU
2. **Reduce filter orders** when possible
3. **Use block processing** for audio (process multiple samples at once)
4. **Profile on target** - host benchmarks are estimates only
5. **Enable compiler optimizations** (-O2 or -O3)
