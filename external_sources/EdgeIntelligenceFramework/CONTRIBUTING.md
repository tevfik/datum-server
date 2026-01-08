# Contributing to EIF

Thank you for your interest in contributing to the Edge Intelligence Framework!

## 🚀 Quick Start

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes
4. Run tests: `make test`
5. Commit: `git commit -m "Add my feature"`
6. Push: `git push origin feature/my-feature`
7. Open a Pull Request

## 📋 Guidelines

### Code Style

- **C99 standard** - No C++ features
- **4-space indentation**
- **snake_case** for functions and variables
- **UPPER_CASE** for macros and constants
- **eif_** prefix for all public symbols

```c
// Good
eif_status_t eif_matrix_multiply(const eif_matrix_t* a, 
                                  const eif_matrix_t* b,
                                  eif_matrix_t* result);

// Bad
int MatMul(Matrix* A, Matrix* B, Matrix* C);
```

### Memory Rules

- **No malloc/free** - Use `eif_memory_pool_t`
- **No global state** - Pass context explicitly
- **Static allocation** - Declare buffers at compile time

### Documentation

- Add Doxygen-style comments to public functions
- Update `docs/` if adding new features
- Include usage examples in headers

```c
/**
 * @brief Compute matrix multiplication C = A * B
 * @param a Input matrix A (m x n)
 * @param b Input matrix B (n x p)
 * @param result Output matrix C (m x p)
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_matrix_multiply(...);
```

### Testing

- Add tests for new features in `tests/`
- Follow existing test patterns
- All tests must pass before merge

```c
bool test_my_feature(void) {
    // Setup
    // Action
    // Assert
    TEST_ASSERT_TRUE(result == expected);
    return true;
}
```

## 🏗️ Project Structure

```
core/       # Core utilities (memory, matrix, types)
dsp/        # Signal processing
ml/         # Machine learning algorithms
dl/         # Deep learning layers
cv/         # Computer vision
bf/         # Bayesian filters
hal/        # Hardware abstraction
tests/      # Unit tests
examples/   # Demo applications
docs/       # Documentation
```

## 🐛 Reporting Issues

Please include:
- EIF version
- Target platform (ESP32, STM32, etc.)
- Minimal code to reproduce
- Expected vs actual behavior

## 📧 Questions?

Open an issue with the `question` label.

---

Thank you for contributing! 🎉
