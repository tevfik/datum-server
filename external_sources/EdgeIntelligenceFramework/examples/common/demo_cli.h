/**
 * @file demo_cli.h
 * @brief Shared CLI parsing utilities for EIF demos
 *
 * Provides consistent argument handling across all demo applications.
 * Supports --help, --batch, and --version flags.
 */

#ifndef EIF_DEMO_CLI_H
#define EIF_DEMO_CLI_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

// Demo CLI result codes
typedef enum {
  DEMO_RUN = 0,  // Run demo normally
  DEMO_EXIT = 1, // Exit after help/version printed
  DEMO_BATCH = 2 // Run in batch mode (skip prompts)
} demo_cli_result_t;

// Global batch mode flag
static bool _demo_batch_mode = false;

/**
 * @brief Parse command line arguments for demos
 *
 * @param argc Argument count from main()
 * @param argv Argument values from main()
 * @param demo_name Short name of the demo (e.g., "rl_gridworld")
 * @param description One-line description of what the demo does
 * @return DEMO_RUN, DEMO_EXIT, or DEMO_BATCH
 */
static inline demo_cli_result_t demo_parse_args(int argc, char **argv,
                                                const char *demo_name,
                                                const char *description) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      printf("\n%s - %s\n\n", demo_name, description);
      printf("Usage: %s [OPTIONS]\n\n", argv[0]);
      printf("Options:\n");
      printf("  -h, --help     Show this help message and exit\n");
      printf("  -b, --batch    Run in batch mode (no interactive prompts)\n");
      printf("  -v, --version  Show version information\n");
      printf("\nPart of the Edge Intelligence Framework (EIF)\n\n");
      return DEMO_EXIT;
    }

    if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
      printf("%s (EIF Demo)\n", demo_name);
      printf("Edge Intelligence Framework v1.0\n");
      return DEMO_EXIT;
    }

    if (strcmp(argv[i], "--batch") == 0 || strcmp(argv[i], "-b") == 0) {
      _demo_batch_mode = true;
    }
  }

  return _demo_batch_mode ? DEMO_BATCH : DEMO_RUN;
}

/**
 * @brief Check if running in batch mode
 * @return true if --batch flag was passed
 */
static inline bool demo_is_batch_mode(void) { return _demo_batch_mode; }

/**
 * @brief Wait for user input (skipped in batch mode)
 * @param prompt Message to display (can be NULL)
 */
static inline void demo_wait(const char *prompt) {
  if (_demo_batch_mode)
    return;

  if (prompt) {
    printf("%s", prompt);
  }
  // Consume characters until newline
  int c;
  while ((c = getchar()) != '\n' && c != EOF)
    ;
}

/**
 * @brief Sleep for specified milliseconds (works in batch mode too)
 * @param ms Milliseconds to sleep
 */
static inline void demo_sleep_ms(unsigned int ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  usleep(ms * 1000);
#endif
}

#endif // EIF_DEMO_CLI_H
