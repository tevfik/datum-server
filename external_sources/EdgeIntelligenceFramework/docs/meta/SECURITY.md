# Security Policy

- Supported Versions: latest `main`
- Reporting: Please open a private issue or email security contact (see repository contact) with:
  - Reproducer code (minimal)
  - Platform and build configuration
  - Impact assessment

## Practices
- No dynamic allocation in core paths; use `eif_memory_pool_t`.
- Validate inputs with `EIF_VALIDATE_*` in debug builds; critical checks always on.
- Use safe string APIs (`snprintf`, `strncpy`) and bounds checks.
- Optional memory guards via `eif_memory_guard.h` for canary protection.

## CI
- Static analysis via `cppcheck`
- Insecure patterns scan via `flawfinder`
- Sanitizers (ASan/UBSan) test job
