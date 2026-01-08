# EIF Design Notes

Architecture decisions, tradeoffs, and rationale behind the Edge Intelligence Framework.

> **Why this document?** Understanding *why* things are built a certain way
> helps you use the framework effectively and extend it correctly.

---

## Core Philosophy

### 1. Header-Only Implementation

**Decision**: Most EIF modules are header-only (inline functions).

**Why**:
- Zero linking complexity for embedded users
- Compiler can see full implementation → better optimization
- Easy to include specific modules without full library
- No ABI compatibility issues across toolchains

**Tradeoff**:
- Longer compile times (acceptable for MCU projects)
- Code duplication if linked into multiple .o files

**When we use .c files**:
- Large lookup tables (e.g., sin tables, mel filterbanks)
- Complex algorithms that benefit from separate compilation

---

### 2. No Dynamic Memory Allocation

**Decision**: All EIF algorithms use static allocation.

**Why**:
- Predictable memory usage (critical for embedded)
- No heap fragmentation
- No malloc failures at runtime
- Easier to analyze memory budget

**Tradeoff**:
- Fixed-size buffers (user must size appropriately)
- Some memory waste if buffer oversized

**How we handle dynamic needs**:
```c
// User provides buffer
float buffer[256];
eif_fft(buffer, 256);  // Uses user's buffer

// Or we use MAX defines
#define EIF_ACTIVITY_WINDOW_SIZE 128  // User can override
```

---

### 3. Explicit State Structures

**Decision**: All algorithms take explicit state struct as first argument.

**Why**:
- No global variables → thread-safe
- Multiple instances of same algorithm
- Clear memory ownership
- Easy to reset (just re-init struct)

**Pattern**:
```c
// State struct
eif_biquad_t filter1;
eif_biquad_t filter2;  // Can have multiple

// All functions take pointer to state
eif_biquad_lowpass(&filter1, fs, fc, q);
float out = eif_biquad_process(&filter1, in);
```

---

### 4. Float as Default, Fixed-Point as Option

**Decision**: Float32 is the default data type.

**Why**:
- Easier to understand and debug
- Direct correspondence to algorithms in papers
- Most modern MCUs have FPU (Cortex-M4F, M7, ESP32)
- Rapid prototyping

**When we provide fixed-point**:
- DSP filters (FIR, IIR) - most performance-critical
- AVR compatibility explicitly needed
- Neural network inference (int8 for quantized models)

**Naming convention**:
```c
eif_fir_t       // Float version
eif_fir_q15_t   // Q15 fixed-point version
```

---

## Module Organization

### Why This Structure?

```
eif/
├── core/    # Foundation (no deps)
├── dsp/     # Depends on core only
├── ml/      # Depends on core, optionally dsp
├── dl/      # Depends on core, optionally dsp
├── bf/      # Depends on core
├── cv/      # Depends on core, dsp
├── ...
```

**Principle**: Minimal dependencies, maximum reusability.

**You can include just `dsp/` without pulling in `dl/`.**

---

### Include Structure

Each module follows this pattern:

```
module/
├── include/
│   ├── eif_module.h        # Main header (includes all)
│   ├── eif_module_sub1.h   # Specific feature
│   └── eif_module_sub2.h   # Another feature
└── src/
    └── eif_module_tables.c  # (optional) Large data
```

**User can include**:
```c
#include "eif_dsp.h"           // Everything
#include "eif_dsp_biquad.h"    // Just biquad
```

---

## Algorithm Selection Rationale

### Why EMA for Smoothing (Not Just Moving Average)?

**EMA** (Exponential Moving Average) is our recommended default:
- O(1) memory (just stores previous output)
- O(1) computation per sample
- Tunable smoothing via alpha
- Good frequency response

**Moving Average** requires:
- O(N) memory for N-sample window
- O(1) average computation with ring buffer

**When to use MA**: When you need "exact N-sample average" (e.g., for specification compliance).

---

### Why Biquad Cascade (Not Direct High-Order IIR)?

High-order IIR filters can be implemented as:
1. Single high-order transfer function (prone to numerical issues)
2. Cascade of 2nd-order sections (stable, what we do)

**We chose cascade because**:
- Each 2nd-order section is independently stable
- Less sensitive to coefficient quantization
- Industry standard approach (CMSIS-DSP, etc.)

---

### Why Rule-Based Activity Classifier (Not Just NN)?

**Rule-based classifier in `eif_activity.h`**:
```c
if (magnitude_std < 0.5) return STATIONARY;
if (magnitude_std > 3.0) return RUNNING;
return WALKING;
```

**Why include this simple approach?**
- Works on AVR (no NN, no float issues)
- No training data needed
- Transparent and modifiable
- Baseline for comparison
- Sometimes "good enough" is good enough

**Neural network** is available in `dl/` for users who need higher accuracy.

---

## Error Handling Philosophy

### No Exceptions, No Error Codes (Mostly)

**Decision**: Functions generally don't return error codes.

**Why**:
- In embedded, invalid input is programming error
- Error checking adds overhead
- Caller should validate before calling

**What we do instead**:
```c
// Preconditions documented
/**
 * @brief Process sample through filter.
 * @pre filter must be initialized via eif_biquad_lowpass() etc.
 */
float eif_biquad_process(eif_biquad_t *bq, float in);
```

**Exceptions** (where we do check):
- Memory pool allocation (can fail)
- File I/O in tools (can fail)
- Initialization functions (may validate params)

---

## Performance Decisions

### Why Inline Static Functions?

```c
static inline float eif_ema_update(eif_ema_t *e, float x) {
    e->value = e->alpha * x + e->one_minus_alpha * e->value;
    return e->value;
}
```

**Why `static inline`?**
- `static`: Internal linkage, avoids ODR violations
- `inline`: Hint to inline, reduced call overhead
- Compiler can optimize when seeing full implementation

**Measured impact**: 2-5x faster on tight loops.

---

### ARM CMSIS-DSP Compatibility

**Decision**: EIF works standalone but can leverage CMSIS-DSP.

**How**:
```c
#ifdef EIF_USE_CMSIS_DSP
  arm_biquad_cascade_df1_f32(&S, pSrc, pDst, blockSize);
#else
  for (int i = 0; i < blockSize; i++) {
      pDst[i] = eif_biquad_process(&filter, pSrc[i]);
  }
#endif
```

**Why both?**
- CMSIS-DSP: Optimized for ARM, using SIMD when available
- Native EIF: Works on any platform, no dependencies

---

## Future Considerations

### What We Might Add

1. **SIMD support**: For ESP32-S3 (has vector extensions)
2. **Async/DMA support**: For high-throughput audio
3. **Model encryption**: For protecting IP
4. **Power profiling**: Track energy per inference

### What We Won't Add

1. **C++ templates**: Keeps it pure C99
2. **RTOS integration**: User's choice
3. **Cloud connectivity**: Out of scope (edge focused)
