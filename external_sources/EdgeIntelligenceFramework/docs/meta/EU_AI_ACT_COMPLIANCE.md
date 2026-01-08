# EU AI Act Compliance Strategy for EIF

This document outlines how the Edge Intelligence Framework (EIF) supports development of AI systems compliant with the European Union Artificial Intelligence Act (EU AI Act).

---

## 1. Transparency and Documentation (Article 11 & 13)

EIF provides tools and templates to ensure users can understand the system's capabilities and limitations.

*   **Model Cards:** Use `docs/templates/MODEL_CARD_TEMPLATE.md` to document model purpose, performance, and training data.
*   **Version Control:** EIF encourages Git-based versioning for model weights and configuration.

## 2. Record Keeping (Article 12)

Traceability is critical for high-risk systems. EIF provides a standardized logging interface.

### `eif_logging.h`
Use `EIF_LOG_INFO`, `EIF_LOG_WARN`, `EIF_LOG_ERROR` to record system events.

```c
eif_log_init(my_flash_log_callback, EIF_LOG_INFO);
EIF_LOG_INFO("Model inference started");
if (score < threshold) {
    EIF_LOG_WARN("Low confidence prediction: %f", score);
}
```

## 3. Accuracy, Robustness, and Cybersecurity (Article 15)

Safety systems must be resilient to errors.

### Runtime Safety
*   **Assertions `eif_assert.h`:** Validate inputs and internal states.
*   **Memory Guard `eif_memory_guard.h`:** Detect buffer overflows proactively.
*   **Error Handling:** Check `eif_status_t` returns for all operations.

## 4. Human Oversight (Article 14)

Systems must allow human override or monitoring.

*   **Async API `eif_async.h`:** Allows non-blocking execution so main loop can check for user override commands.
*   **Stop Hooks:** Implement callbacks in your application loop to abort inference if a sensor (e.g. button) is triggered.

```c
// Human override check logic
if (check_emergency_stop_button()) {
    eif_inference_cancel(handle);
    EIF_LOG_WARN("Inference cancelled by human operator");
}
```
