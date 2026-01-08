/**
 * @file eif_logging.h
 * @brief EIF Logging System for EU AI Act Compliance (Article 12 - Record
 * Keeping)
 *
 * Provides standardized event logging with levels and timestamps.
 * Critical for tracing system behavior and post-incident analysis.
 */

#ifndef EIF_LOGGING_H
#define EIF_LOGGING_H

#include "eif_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Log Levels
// =============================================================================

typedef enum {
  EIF_LOG_ERROR = 0, /**< Critical errors, system might stop */
  EIF_LOG_WARN = 1,  /**< Non-critical issues, recovery possible */
  EIF_LOG_INFO = 2, /**< General operational events (start, stop, completion) */
  EIF_LOG_DEBUG = 3, /**< Diagnostic information */
  EIF_LOG_TRACE = 4  /**< Fine-grained execution trace */
} eif_log_level_t;

// =============================================================================
// Logging Backend Interface
// =============================================================================

/**
 * @brief Function pointer for writing log messages
 * @param level Log level of the message
 * @param message Null-terminated message string
 */
typedef void (*eif_log_callback_t)(eif_log_level_t level, const char *message);

// =============================================================================
// API
// =============================================================================

/**
 * @brief Initialize logging system
 * @param callback Function to handle log output (UART, Flash, etc.)
 * @param max_level Maximum level to log (messages above this are ignored)
 */
void eif_log_init(eif_log_callback_t callback, eif_log_level_t max_level);

/**
 * @brief Set current log level filter
 */
void eif_log_set_level(eif_log_level_t level);

/**
 * @brief Log a message
 * @param level Message severity
 * @param fmt Format string (printf style)
 * @param ... Arguments
 */
void eif_log(eif_log_level_t level, const char *fmt, ...);

// =============================================================================
// Convenience Macros
// =============================================================================

#define EIF_LOG_ERROR(fmt, ...)                                                \
  eif_log(EIF_LOG_ERROR, "[ERR]  " fmt, ##__VA_ARGS__)
#define EIF_LOG_WARN(fmt, ...)                                                 \
  eif_log(EIF_LOG_WARN, "[WARN] " fmt, ##__VA_ARGS__)
#define EIF_LOG_INFO(fmt, ...)                                                 \
  eif_log(EIF_LOG_INFO, "[INFO] " fmt, ##__VA_ARGS__)

#ifdef EIF_ENABLE_DEBUG_LOGGING
#define EIF_LOG_DEBUG(fmt, ...)                                                \
  eif_log(EIF_LOG_DEBUG, "[DBG]  " fmt, ##__VA_ARGS__)
#define EIF_LOG_TRACE(fmt, ...)                                                \
  eif_log(EIF_LOG_TRACE, "[TRC]  " fmt, ##__VA_ARGS__)
#else
#define EIF_LOG_DEBUG(fmt, ...) ((void)0)
#define EIF_LOG_TRACE(fmt, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif // EIF_LOGGING_H
