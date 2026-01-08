# Changelog

## 1.1.0 - 2026-01-06
- **Optimization**: Significant speedup for Int8 Depthwise Conv2D using pointer arithmetic and channel-first loop reordering.
- **New Layers**: Added ArgMax, TopK, Minimum, Maximum, Gather, Split, Pad, MatMul, ReduceMean, ReduceSum.
- **Enhanced Support**: Full integration of Embedding and Math operators (Exp, Log, Sqrt).
- **Core**: Removed duplicate layer definitions and standardized dispatch logic.

## 1.0.0 - 2025-12-24
- Centralized version: generated `core/include/eif_version.h` from CMake
- Replaced dynamic allocations with pool in KNN, FFT Q15, RFFT, matrix inverse, matrix profile
- Replaced unsafe `strcpy/sprintf` with `strncpy/snprintf`
- CI: added cppcheck, flawfinder, ASan/UBSan, coverage
- Added quality scripts and Makefile targets (`quality`, `asan`)
- Added SECURITY.md
