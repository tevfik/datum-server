/**
 * @file interactive_demo.c
 * @brief Interactive CLI Demo Template
 * 
 * Demonstrates runtime parameter modification without recompiling.
 * 
 * Commands:
 *   START          - Begin processing
 *   STOP           - Stop processing
 *   SET <key> <val> - Set parameter
 *   GET <key>      - Get parameter value
 *   INJECT_NOISE   - Add noise to signal
 *   RESET          - Reset to defaults
 *   JSON <on|off>  - Toggle JSON output
 *   HELP           - Show commands
 *   QUIT           - Exit
 * 
 * Usage:
 *   ./interactive_demo
 *   echo "SET threshold 0.5\nSTART" | ./interactive_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

// ============================================================================
// Configuration Parameters (Runtime Modifiable)
// ============================================================================

typedef struct {
    float threshold;
    float learning_rate;
    int window_size;
    float noise_level;
    bool json_output;
    bool running;
} demo_config_t;

static demo_config_t config = {
    .threshold = 0.5f,
    .learning_rate = 0.01f,
    .window_size = 32,
    .noise_level = 0.0f,
    .json_output = false,
    .running = false
};

// ============================================================================
// Parameter Registry
// ============================================================================

typedef enum {
    PARAM_FLOAT,
    PARAM_INT,
    PARAM_BOOL
} param_type_t;

typedef struct {
    const char* name;
    param_type_t type;
    void* ptr;
    float min_val;
    float max_val;
} param_entry_t;

static param_entry_t params[] = {
    {"threshold",      PARAM_FLOAT, &config.threshold,      0.0f, 1.0f},
    {"learning_rate",  PARAM_FLOAT, &config.learning_rate,  0.0001f, 1.0f},
    {"window_size",    PARAM_INT,   &config.window_size,    1, 256},
    {"noise_level",    PARAM_FLOAT, &config.noise_level,    0.0f, 1.0f},
    {"json",           PARAM_BOOL,  &config.json_output,    0, 1},
    {NULL, 0, NULL, 0, 0}
};

// ============================================================================
// Command Handlers
// ============================================================================

static void print_help(void) {
    printf("\n");
    printf("+----------------------------------------------------------+\n");
    printf("|  Interactive Demo Commands                               |\n");
    printf("+----------------------------------------------------------+\n");
    printf("|  START              - Begin processing loop              |\n");
    printf("|  STOP               - Stop processing                    |\n");
    printf("|  SET <key> <value>  - Set parameter                      |\n");
    printf("|  GET <key>          - Get parameter value                |\n");
    printf("|  LIST               - List all parameters                |\n");
    printf("|  INJECT_NOISE       - Add random noise                   |\n");
    printf("|  RESET              - Reset to defaults                  |\n");
    printf("|  JSON on|off        - Toggle JSON output                 |\n");
    printf("|  HELP               - Show this help                     |\n");
    printf("|  QUIT               - Exit program                       |\n");
    printf("+----------------------------------------------------------+\n\n");
}

static void list_params(void) {
    printf("\nParameters:\n");
    for (int i = 0; params[i].name != NULL; i++) {
        printf("  %-16s = ", params[i].name);
        switch (params[i].type) {
            case PARAM_FLOAT:
                printf("%.4f", *(float*)params[i].ptr);
                printf("  (range: %.4f - %.4f)", params[i].min_val, params[i].max_val);
                break;
            case PARAM_INT:
                printf("%d", *(int*)params[i].ptr);
                printf("  (range: %.0f - %.0f)", params[i].min_val, params[i].max_val);
                break;
            case PARAM_BOOL:
                printf("%s", *(bool*)params[i].ptr ? "true" : "false");
                break;
        }
        printf("\n");
    }
    printf("\n");
}

static bool set_param(const char* name, const char* value) {
    for (int i = 0; params[i].name != NULL; i++) {
        if (strcmp(params[i].name, name) == 0) {
            switch (params[i].type) {
                case PARAM_FLOAT: {
                    float v = atof(value);
                    if (v < params[i].min_val || v > params[i].max_val) {
                        printf("Error: Value out of range [%.4f, %.4f]\n", 
                               params[i].min_val, params[i].max_val);
                        return false;
                    }
                    *(float*)params[i].ptr = v;
                    break;
                }
                case PARAM_INT: {
                    int v = atoi(value);
                    if (v < (int)params[i].min_val || v > (int)params[i].max_val) {
                        printf("Error: Value out of range [%.0f, %.0f]\n",
                               params[i].min_val, params[i].max_val);
                        return false;
                    }
                    *(int*)params[i].ptr = v;
                    break;
                }
                case PARAM_BOOL: {
                    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
                        strcmp(value, "on") == 0) {
                        *(bool*)params[i].ptr = true;
                    } else {
                        *(bool*)params[i].ptr = false;
                    }
                    break;
                }
            }
            printf("OK: %s = %s\n", name, value);
            return true;
        }
    }
    printf("Error: Unknown parameter '%s'\n", name);
    return false;
}

static void get_param(const char* name) {
    for (int i = 0; params[i].name != NULL; i++) {
        if (strcmp(params[i].name, name) == 0) {
            printf("%s = ", name);
            switch (params[i].type) {
                case PARAM_FLOAT: printf("%.4f\n", *(float*)params[i].ptr); break;
                case PARAM_INT:   printf("%d\n", *(int*)params[i].ptr); break;
                case PARAM_BOOL:  printf("%s\n", *(bool*)params[i].ptr ? "true" : "false"); break;
            }
            return;
        }
    }
    printf("Error: Unknown parameter '%s'\n", name);
}

static void reset_defaults(void) {
    config.threshold = 0.5f;
    config.learning_rate = 0.01f;
    config.window_size = 32;
    config.noise_level = 0.0f;
    config.json_output = false;
    printf("OK: Reset to defaults\n");
}

// ============================================================================
// Processing Loop (Demo Algorithm)
// ============================================================================

static float demo_signal[256];
static int demo_t = 0;

static void process_step(void) {
    // Generate synthetic signal
    float signal = sinf(2 * 3.14159f * demo_t / 50.0f);
    
    // Add noise if enabled
    if (config.noise_level > 0) {
        signal += config.noise_level * ((float)rand() / RAND_MAX - 0.5f) * 2;
    }
    
    // Simple threshold detection
    bool detected = fabsf(signal) > config.threshold;
    
    // Output
    if (config.json_output) {
        printf("{\"t\": %d, \"signal\": %.4f, \"threshold\": %.4f, \"detected\": %s}\n",
               demo_t, signal, config.threshold, detected ? "true" : "false");
    } else {
        char bar[21];
        int bar_pos = (int)((signal + 1.0f) * 10);
        if (bar_pos < 0) bar_pos = 0;
        if (bar_pos > 20) bar_pos = 20;
        memset(bar, '-', 20);
        bar[bar_pos] = '*';
        bar[20] = '\0';
        
        printf("[%4d] [%s] signal=%.2f %s\n", 
               demo_t, bar, signal, detected ? "DETECTED!" : "");
    }
    
    demo_t++;
    fflush(stdout);
}

// ============================================================================
// Command Parser
// ============================================================================

static bool parse_command(char* line) {
    // Trim newline
    char* nl = strchr(line, '\n');
    if (nl) *nl = '\0';
    
    // Skip empty lines
    if (strlen(line) == 0) return true;
    
    // Parse command
    char cmd[32], arg1[64], arg2[64];
    int n = sscanf(line, "%31s %63s %63s", cmd, arg1, arg2);
    
    if (n < 1) return true;
    
    // Convert command to uppercase
    for (char* p = cmd; *p; p++) *p = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;
    
    if (strcmp(cmd, "HELP") == 0 || strcmp(cmd, "?") == 0) {
        print_help();
    }
    else if (strcmp(cmd, "LIST") == 0) {
        list_params();
    }
    else if (strcmp(cmd, "SET") == 0 && n >= 3) {
        set_param(arg1, arg2);
    }
    else if (strcmp(cmd, "GET") == 0 && n >= 2) {
        get_param(arg1);
    }
    else if (strcmp(cmd, "START") == 0) {
        config.running = true;
        printf("OK: Started processing\n");
    }
    else if (strcmp(cmd, "STOP") == 0) {
        config.running = false;
        printf("OK: Stopped processing\n");
    }
    else if (strcmp(cmd, "RESET") == 0) {
        reset_defaults();
    }
    else if (strcmp(cmd, "INJECT_NOISE") == 0) {
        config.noise_level = 0.3f;
        printf("OK: Noise injected (level=0.3)\n");
    }
    else if (strcmp(cmd, "JSON") == 0 && n >= 2) {
        set_param("json", arg1);
    }
    else if (strcmp(cmd, "QUIT") == 0 || strcmp(cmd, "EXIT") == 0) {
        return false;
    }
    else {
        printf("Unknown command: %s (type HELP for commands)\n", cmd);
    }
    
    return true;
}

// ============================================================================
// Main Loop
// ============================================================================

int main(int argc, char** argv) {
    printf("\n");
    printf("+----------------------------------------------------------+\n");
    printf("|       EIF Interactive Demo Template                      |\n");
    printf("+----------------------------------------------------------+\n");
    printf("| Type HELP for commands, QUIT to exit                     |\n");
    printf("+----------------------------------------------------------+\n\n");
    
    // Check for --json flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            config.json_output = true;
        }
        if (strcmp(argv[i], "--start") == 0) {
            config.running = true;
        }
    }
    
    char line[256];
    int idle_count = 0;
    
    while (1) {
        // Run processing if active
        if (config.running) {
            process_step();
            
            // Non-blocking check for input (simple version)
            // In real impl, use select() or poll()
            idle_count++;
            if (idle_count > 20) {
                config.running = false;  // Auto-stop after 20 steps for demo
                printf("(Auto-stopped after 20 steps. Type START to continue)\n");
            }
        }
        
        // Prompt
        if (!config.running) {
            printf("> ");
            fflush(stdout);
            
            if (fgets(line, sizeof(line), stdin) == NULL) {
                break;  // EOF
            }
            
            if (!parse_command(line)) {
                break;  // QUIT
            }
            
            idle_count = 0;
        }
    }
    
    printf("\nGoodbye!\n");
    return 0;
}
