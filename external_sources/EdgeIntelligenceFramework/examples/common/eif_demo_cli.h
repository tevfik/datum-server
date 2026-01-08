/**
 * @file eif_demo_cli.h
 * @brief Reusable CLI Parser for EIF Demos
 *
 * Provides a simple command handling interface for interactive demos.
 * Supports: SET, GET, START, STOP, STATUS, HELP, JSON
 */

#ifndef EIF_DEMO_CLI_H
#define EIF_DEMO_CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_CLI_MAX_PARAMS 16
#define EIF_CLI_NAME_KEN 16
#define EIF_CLI_BUFFER_SIZE 64

typedef enum { EIF_CLI_PARAM_FLOAT, EIF_CLI_PARAM_INT } eif_cli_param_type_t;

typedef struct {
  char name[EIF_CLI_NAME_KEN];
  void *ptr;
  eif_cli_param_type_t type;
  bool readonly;
} eif_cli_param_t;

typedef struct {
  eif_cli_param_t params[EIF_CLI_MAX_PARAMS];
  int param_count;

  char buffer[EIF_CLI_BUFFER_SIZE];
  int buffer_pos;

  bool json_mode;
  bool running;
} eif_cli_t;

/**
 * @brief Initialize the CLI context
 */
void eif_cli_init(eif_cli_t *cli);

/**
 * @brief Register a floating point parameter
 */
void eif_cli_register_float(eif_cli_t *cli, const char *name, float *ptr,
                            bool readonly);

/**
 * @brief Register an integer parameter
 */
void eif_cli_register_int(eif_cli_t *cli, const char *name, int *ptr,
                          bool readonly);

/**
 * @brief Process a single character input (non-blocking)
 * @return true if a command was executed
 */
bool eif_cli_process_input(eif_cli_t *cli, char c);

/**
 * @brief Check if input is available (Standard implementation reading stdin)
 * @return Character or 0 if none
 */
char eif_cli_get_char();

/**
 * @brief Print status if not in JSON mode
 */
void eif_cli_print_status(eif_cli_t *cli, const char *message);

#ifdef __cplusplus
}
#endif

#endif // EIF_DEMO_CLI_H
