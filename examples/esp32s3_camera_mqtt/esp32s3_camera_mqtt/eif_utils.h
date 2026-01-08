#define EIF_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// MCU Optimization: Memory Abstraction
// Users can define EIF_NO_STDLIB to provide their own implementations
// or map to specific hardware accelerators.

#ifndef EIF_NO_STDLIB
#include <string.h>
#define EIF_MEMCPY(dest, src, n) memcpy(dest, src, n)
#define EIF_MEMSET(dest, val, n) memset(dest, val, n)
#else
// Fallback or User-Defined
// For now, we assume user will provide these if EIF_NO_STDLIB is set.
// Or we can provide a simple loop implementation here if needed.
extern void *eif_memcpy(void *dest, const void *src, size_t n);
extern void *eif_memset(void *s, int c, size_t n);
#define EIF_MEMCPY(dest, src, n) eif_memcpy(dest, src, n)
#define EIF_MEMSET(dest, val, n) eif_memset(dest, val, n)
#endif

#ifdef __cplusplus
}
#endif

#endif // EIF_UTILS_H
