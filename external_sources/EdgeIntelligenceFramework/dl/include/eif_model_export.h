/**
 * @file eif_model_export.h
 * @brief Model Export Utilities for Embedded Deployment
 *
 * Tools for exporting trained models to C arrays:
 * - Weight array generation
 * - Quantized weight export
 * - Header file generation
 * - Memory layout helpers
 *
 * Enables seamless deployment of trained models to MCUs.
 */

#ifndef EIF_MODEL_EXPORT_H
#define EIF_MODEL_EXPORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Export Configuration
// =============================================================================

typedef struct {
  const char *model_name;  ///< Name for the model (used in variable names)
  const char *output_path; ///< Output file path
  bool include_guard;      ///< Add include guards
  bool const_qualifier;    ///< Add const qualifier
  bool flash_section;      ///< Add section attribute for Flash
  int elements_per_line;   ///< Elements per line in output
} eif_export_config_t;

/**
 * @brief Default export configuration
 */
static inline void eif_export_config_default(eif_export_config_t *cfg) {
  cfg->model_name = "model";
  cfg->output_path = "model_weights.h";
  cfg->include_guard = true;
  cfg->const_qualifier = true;
  cfg->flash_section = true;
  cfg->elements_per_line = 8;
}

// =============================================================================
// Float Array Export
// =============================================================================

/**
 * @brief Export float array to C header format (to buffer)
 */
static inline int eif_export_float_array(const float *data, int size,
                                         const char *array_name, char *buffer,
                                         int buffer_size,
                                         int elements_per_line) {
  int written = 0;

  // Array declaration
  written += snprintf(buffer + written, buffer_size - written,
                      "const float %s[%d] = {\n    ", array_name, size);

  // Array contents
  for (int i = 0; i < size; i++) {
    int chars =
        snprintf(buffer + written, buffer_size - written, "%.8ef", data[i]);
    written += chars;

    if (i < size - 1) {
      written += snprintf(buffer + written, buffer_size - written, ", ");
    }

    if ((i + 1) % elements_per_line == 0 && i < size - 1) {
      written += snprintf(buffer + written, buffer_size - written, "\n    ");
    }
  }

  written += snprintf(buffer + written, buffer_size - written, "\n};\n");

  return written;
}

/**
 * @brief Export int8 array (quantized weights)
 */
static inline int eif_export_int8_array(const int8_t *data, int size,
                                        const char *array_name, char *buffer,
                                        int buffer_size,
                                        int elements_per_line) {
  int written = 0;

  written += snprintf(buffer + written, buffer_size - written,
                      "const int8_t %s[%d] = {\n    ", array_name, size);

  for (int i = 0; i < size; i++) {
    written +=
        snprintf(buffer + written, buffer_size - written, "%4d", data[i]);

    if (i < size - 1) {
      written += snprintf(buffer + written, buffer_size - written, ", ");
    }

    if ((i + 1) % elements_per_line == 0 && i < size - 1) {
      written += snprintf(buffer + written, buffer_size - written, "\n    ");
    }
  }

  written += snprintf(buffer + written, buffer_size - written, "\n};\n");

  return written;
}

/**
 * @brief Export Q15 array (fixed-point)
 */
static inline int eif_export_q15_array(const int16_t *data, int size,
                                       const char *array_name, char *buffer,
                                       int buffer_size, int elements_per_line) {
  int written = 0;

  written += snprintf(buffer + written, buffer_size - written,
                      "const int16_t %s[%d] = {\n    ", array_name, size);

  for (int i = 0; i < size; i++) {
    written +=
        snprintf(buffer + written, buffer_size - written, "%6d", data[i]);

    if (i < size - 1) {
      written += snprintf(buffer + written, buffer_size - written, ", ");
    }

    if ((i + 1) % elements_per_line == 0 && i < size - 1) {
      written += snprintf(buffer + written, buffer_size - written, "\n    ");
    }
  }

  written += snprintf(buffer + written, buffer_size - written, "\n};\n");

  return written;
}

// =============================================================================
// Header Generation
// =============================================================================

/**
 * @brief Generate include guard start
 */
static inline int eif_export_guard_start(const char *model_name, char *buffer,
                                         int buffer_size) {
  char upper_name[64];
  int i;
  for (i = 0; model_name[i] && i < 63; i++) {
    char c = model_name[i];
    upper_name[i] = (c >= 'a' && c <= 'z') ? c - 32 : c;
  }
  upper_name[i] = '\0';

  return snprintf(buffer, buffer_size,
                  "#ifndef %s_WEIGHTS_H\n#define %s_WEIGHTS_H\n\n"
                  "#include <stdint.h>\n\n",
                  upper_name, upper_name);
}

/**
 * @brief Generate include guard end
 */
static inline int eif_export_guard_end(const char *model_name, char *buffer,
                                       int buffer_size) {
  char upper_name[64];
  int i;
  for (i = 0; model_name[i] && i < 63; i++) {
    char c = model_name[i];
    upper_name[i] = (c >= 'a' && c <= 'z') ? c - 32 : c;
  }
  upper_name[i] = '\0';

  return snprintf(buffer, buffer_size, "\n#endif // %s_WEIGHTS_H\n",
                  upper_name);
}

// =============================================================================
// Model Metadata Export
// =============================================================================

typedef struct {
  const char *name;
  int num_layers;
  int total_params;
  int input_size;
  int output_size;
  const char *quant_type; // "float", "int8", "q15"
} eif_model_metadata_t;

/**
 * @brief Export model metadata as defines
 */
static inline int eif_export_metadata(const eif_model_metadata_t *meta,
                                      char *buffer, int buffer_size) {
  char upper_name[64];
  int i;
  for (i = 0; meta->name[i] && i < 63; i++) {
    char c = meta->name[i];
    upper_name[i] = (c >= 'a' && c <= 'z') ? c - 32 : c;
  }
  upper_name[i] = '\0';

  return snprintf(buffer, buffer_size,
                  "// Model: %s\n"
                  "#define %s_NUM_LAYERS %d\n"
                  "#define %s_TOTAL_PARAMS %d\n"
                  "#define %s_INPUT_SIZE %d\n"
                  "#define %s_OUTPUT_SIZE %d\n"
                  "#define %s_QUANT_TYPE \"%s\"\n\n",
                  meta->name, upper_name, meta->num_layers, upper_name,
                  meta->total_params, upper_name, meta->input_size, upper_name,
                  meta->output_size, upper_name, meta->quant_type);
}

// =============================================================================
// Layer Export Helpers
// =============================================================================

/**
 * @brief Export dense layer weights
 */
static inline int eif_export_dense_layer(const float *weights, int in_size,
                                         int out_size, const float *bias,
                                         const char *layer_name, char *buffer,
                                         int buffer_size) {
  int written = 0;

  // Comment
  written +=
      snprintf(buffer + written, buffer_size - written,
               "// Dense layer %s: %d -> %d\n", layer_name, in_size, out_size);

  // Weights
  char weight_name[128];
  snprintf(weight_name, sizeof(weight_name), "%s_weights", layer_name);
  written += eif_export_float_array(weights, in_size * out_size, weight_name,
                                    buffer + written, buffer_size - written, 8);

  // Bias
  written += snprintf(buffer + written, buffer_size - written, "\n");
  char bias_name[128];
  snprintf(bias_name, sizeof(bias_name), "%s_bias", layer_name);
  written += eif_export_float_array(bias, out_size, bias_name, buffer + written,
                                    buffer_size - written, 8);

  return written;
}

// =============================================================================
// Python Script Generator
// =============================================================================

/**
 * @brief Generate Python script for model conversion
 */
static inline void eif_print_conversion_script(void) {
  printf("# Python conversion script for Keras/TensorFlow models\n");
  printf("# Save this as convert_model.py\n\n");
  printf("import numpy as np\n");
  printf("import tensorflow as tf\n\n");
  printf("def export_to_c(model, output_file='model_weights.h'):\n");
  printf("    with open(output_file, 'w') as f:\n");
  printf("        f.write('#ifndef MODEL_WEIGHTS_H\\n')\n");
  printf("        f.write('#define MODEL_WEIGHTS_H\\n\\n')\n");
  printf("        \n");
  printf("        for i, layer in enumerate(model.layers):\n");
  printf("            weights = layer.get_weights()\n");
  printf("            if len(weights) > 0:\n");
  printf("                # Export weights\n");
  printf("                w = weights[0].flatten()\n");
  printf("                f.write(f'const float layer{i}_weights[{len(w)}] = "
         "{{\\n')\n");
  printf("                for j, val in enumerate(w):\n");
  printf("                    f.write(f'{val:.8e}f')\n");
  printf("                    if j < len(w)-1: f.write(', ')\n");
  printf("                    if (j+1) %% 8 == 0: f.write('\\n')\n");
  printf("                f.write('};\\n\\n')\n");
  printf("        \n");
  printf("        f.write('#endif\\n')\n");
  printf("    print(f'Exported to {output_file}')\n\n");
  printf("# Usage:\n");
  printf("# model = tf.keras.models.load_model('my_model.h5')\n");
  printf("# export_to_c(model)\n");
}

#ifdef __cplusplus
}
#endif

#endif // EIF_MODEL_EXPORT_H
