/**
 * @file eif_demo_cli.c
 * @brief CLI Parser Implementation
 */

#include "eif_demo_cli.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

void eif_cli_init(eif_cli_t *cli) {
  if (!cli)
    return;
  memset(cli, 0, sizeof(eif_cli_t));
  cli->running = true;
  cli->json_mode = false;

  // Configure stdin to be non-blocking
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void eif_cli_register_float(eif_cli_t *cli, const char *name, float *ptr,
                            bool readonly) {
  if (!cli || cli->param_count >= EIF_CLI_MAX_PARAMS)
    return;
  eif_cli_param_t *p = &cli->params[cli->param_count++];
  strncpy(p->name, name, EIF_CLI_NAME_KEN - 1);
  p->ptr = ptr;
  p->type = EIF_CLI_PARAM_FLOAT;
  p->readonly = readonly;
}

void eif_cli_register_int(eif_cli_t *cli, const char *name, int *ptr,
                          bool readonly) {
  if (!cli || cli->param_count >= EIF_CLI_MAX_PARAMS)
    return;
  eif_cli_param_t *p = &cli->params[cli->param_count++];
  strncpy(p->name, name, EIF_CLI_NAME_KEN - 1);
  p->ptr = ptr;
  p->type = EIF_CLI_PARAM_INT;
  p->readonly = readonly;
}

static void eif_cli_handle_command(eif_cli_t *cli) {
  char *cmd = strtok(cli->buffer, " ");
  if (!cmd)
    return;

  // Convert to upper for command match
  for (int i = 0; cmd[i]; i++)
    cmd[i] = toupper(cmd[i]);

  if (strcmp(cmd, "HELP") == 0) {
    if (cli->json_mode)
      return;
    printf("Available Commands:\n");
    printf("  SET <param> <value>\n");
    printf("  GET <param>\n");
    printf("  START\n");
    printf("  STOP\n");
    printf("  JSON <0|1> (Toggle machine readable output)\n");
    printf("Parameters:\n");
    for (int i = 0; i < cli->param_count; i++) {
      printf("  %s%s\n", cli->params[i].name,
             cli->params[i].readonly ? " (RO)" : "");
    }

  } else if (strcmp(cmd, "START") == 0) {
    cli->running = true;
    if (!cli->json_mode)
      printf("Simulation Started\n");

  } else if (strcmp(cmd, "STOP") == 0) {
    cli->running = false;
    if (!cli->json_mode)
      printf("Simulation Stopped\n");

  } else if (strcmp(cmd, "JSON") == 0) {
    char *val_str = strtok(NULL, " ");
    if (val_str) {
      cli->json_mode = (atoi(val_str) > 0);
      if (!cli->json_mode)
        printf("JSON Mode Disabled\n");
    } else {
      // Toggle if no arg
      cli->json_mode = !cli->json_mode;
    }

  } else if (strcmp(cmd, "GET") == 0) {
    char *param_name = strtok(NULL, " ");
    if (!param_name)
      return;

    for (int i = 0; i < cli->param_count; i++) {
      if (strcasecmp(cli->params[i].name, param_name) == 0) {
        if (cli->json_mode) {
          printf("{\"type\": \"param\", \"name\": \"%s\", \"value\": ",
                 cli->params[i].name);
          if (cli->params[i].type == EIF_CLI_PARAM_FLOAT)
            printf("%f}\n", *(float *)cli->params[i].ptr);
          else
            printf("%d}\n", *(int *)cli->params[i].ptr);
        } else {
          printf("%s = ", cli->params[i].name);
          if (cli->params[i].type == EIF_CLI_PARAM_FLOAT)
            printf("%f\n", *(float *)cli->params[i].ptr);
          else
            printf("%d\n", *(int *)cli->params[i].ptr);
        }
        return;
      }
    }
    if (!cli->json_mode)
      printf("Unknown parameter: %s\n", param_name);

  } else if (strcmp(cmd, "SET") == 0) {
    char *param_name = strtok(NULL, " ");
    char *value_str = strtok(NULL, " ");
    if (!param_name || !value_str)
      return;

    for (int i = 0; i < cli->param_count; i++) {
      if (strcasecmp(cli->params[i].name, param_name) == 0) {
        if (cli->params[i].readonly) {
          if (!cli->json_mode)
            printf("Parameter is Read-Only\n");
          return;
        }

        if (cli->params[i].type == EIF_CLI_PARAM_FLOAT) {
          *(float *)cli->params[i].ptr = strtof(value_str, NULL);
        } else {
          *(int *)cli->params[i].ptr = atoi(value_str);
        }

        if (!cli->json_mode)
          printf("Updated %s\n", cli->params[i].name);
        return;
      }
    }
  }
}

bool eif_cli_process_input(eif_cli_t *cli, char c) {
  if (!cli)
    return false;

  if (c == '\n' || c == '\r') {
    if (cli->buffer_pos > 0) {
      cli->buffer[cli->buffer_pos] = '\0';
      eif_cli_handle_command(cli);
      cli->buffer_pos = 0;
      return true;
    }
  } else {
    if (cli->buffer_pos < EIF_CLI_BUFFER_SIZE - 1) {
      cli->buffer[cli->buffer_pos++] = c;
    }
  }
  return false;
}

char eif_cli_get_char() {
  char c = 0;
  if (read(STDIN_FILENO, &c, 1) > 0) {
    return c;
  }
  return 0;
}

void eif_cli_print_status(eif_cli_t *cli, const char *message) {
  if (cli && !cli->json_mode && message) {
    printf("%s\n", message);
  }
}
