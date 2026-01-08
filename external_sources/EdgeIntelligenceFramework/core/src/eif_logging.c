/**
 * @file eif_logging.c
 * @brief Implementation of EIF Logging System
 */

#include "eif_logging.h"
#include <stdarg.h>
#include <stdio.h>

// Static state (avoid dynamic allocation)
static eif_log_callback_t g_log_callback = NULL;
static eif_log_level_t g_max_level = EIF_LOG_INFO;
static char g_log_buffer[256]; // Fixed buffer for formatting

void eif_log_init(eif_log_callback_t callback, eif_log_level_t max_level) {
  g_log_callback = callback;
  g_max_level = max_level;
}

void eif_log_set_level(eif_log_level_t level) { g_max_level = level; }

void eif_log(eif_log_level_t level, const char *fmt, ...) {
  // Check level filter
  if (level > g_max_level)
    return;

  // Check callback
  if (!g_log_callback)
    return;

  // Format message
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_log_buffer, sizeof(g_log_buffer), fmt, args);
  va_end(args);

  // Ensure null termination
  g_log_buffer[sizeof(g_log_buffer) - 1] = '\0';

  // Dispatch
  g_log_callback(level, g_log_buffer);
}
