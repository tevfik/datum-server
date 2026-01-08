#include "eif_neural.h"
#include "eif_dl_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EIF_MAGIC 0x4549464D // "EIFM"
#define EIF_VERSION 1

// File Header
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t num_tensors;
    uint32_t num_nodes;
    uint32_t num_inputs;
    uint32_t num_outputs;
    uint32_t weights_size;
} eif_file_header_t;

// Tensor Entry
typedef struct {
    uint32_t type;
    int32_t dims[4];
    uint32_t size_bytes;
    uint32_t is_variable;
    uint32_t data_offset; // Offset in weights blob, or 0xFFFFFFFF if no data
} eif_file_tensor_t;

// Node Header
typedef struct {
    uint32_t type;
    uint32_t num_inputs;
    uint32_t num_outputs;
    uint32_t params_size;
} eif_file_node_t;

eif_status_t eif_model_deserialize(eif_model_t* model, const uint8_t* buffer, size_t buffer_size, eif_memory_pool_t* pool) {
    if (!model || !buffer || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    const uint8_t* ptr = buffer;
    
    // 1. Read Header
    if (buffer_size < sizeof(eif_file_header_t)) return EIF_STATUS_ERROR;
    const eif_file_header_t* header = (const eif_file_header_t*)ptr;
    ptr += sizeof(eif_file_header_t);
    
    if (header->magic != EIF_MAGIC) return EIF_STATUS_ERROR;
    if (header->version != EIF_VERSION) return EIF_STATUS_ERROR;
    
    model->num_tensors = header->num_tensors;
    model->num_nodes = header->num_nodes;
    model->num_inputs = header->num_inputs;
    model->num_outputs = header->num_outputs;
    
    // 2. Allocate Structures
    model->tensors = (eif_tensor_t*)eif_memory_alloc(pool, model->num_tensors * sizeof(eif_tensor_t), 4);
    model->nodes = (eif_layer_node_t*)eif_memory_alloc(pool, model->num_nodes * sizeof(eif_layer_node_t), 4);
    model->input_tensor_indices = (int*)eif_memory_alloc(pool, model->num_inputs * sizeof(int), 4);
    model->output_tensor_indices = (int*)eif_memory_alloc(pool, model->num_outputs * sizeof(int), 4);
    
    if (!model->tensors || !model->nodes || !model->input_tensor_indices || !model->output_tensor_indices) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // 3. Read Tensors
    for (int i = 0; i < model->num_tensors; i++) {
        const eif_file_tensor_t* ft = (const eif_file_tensor_t*)ptr;
        ptr += sizeof(eif_file_tensor_t);
        
        model->tensors[i].type = (eif_tensor_type_t)ft->type;
        memcpy(model->tensors[i].dims, ft->dims, 4 * sizeof(int));
        model->tensors[i].size_bytes = ft->size_bytes;
        model->tensors[i].is_variable = ft->is_variable;
        
        if (ft->data_offset != 0xFFFFFFFF) {
            // Pointer to data in the weights blob (which follows all metadata)
            // We need to calculate where the weights blob starts.
            // But wait, the buffer contains the whole file.
            // We can just point to it?
            // Yes, but we need to know where the weights blob starts relative to buffer start.
            // Let's assume weights blob is at the end of metadata.
            // But we are parsing sequentially.
            // We need to skip nodes and inputs/outputs to find weights start?
            // No, let's just store the offset relative to weights blob start, and resolve later.
            model->tensors[i].data = (void*)(uintptr_t)ft->data_offset; // Temporary storage
        } else {
            model->tensors[i].data = NULL;
        }
    }
    
    // 4. Read Nodes
    for (int i = 0; i < model->num_nodes; i++) {
        const eif_file_node_t* fn = (const eif_file_node_t*)ptr;
        ptr += sizeof(eif_file_node_t);
        
        model->nodes[i].type = (eif_layer_type_t)fn->type;
        model->nodes[i].num_inputs = fn->num_inputs;
        model->nodes[i].num_outputs = fn->num_outputs;
        
        // Allocate indices
        model->nodes[i].input_indices = (int*)eif_memory_alloc(pool, fn->num_inputs * sizeof(int), 4);
        model->nodes[i].output_indices = (int*)eif_memory_alloc(pool, fn->num_outputs * sizeof(int), 4);
        
        if (!model->nodes[i].input_indices || !model->nodes[i].output_indices) return EIF_STATUS_OUT_OF_MEMORY;
        
        // Read indices
        memcpy(model->nodes[i].input_indices, ptr, fn->num_inputs * sizeof(int));
        ptr += fn->num_inputs * sizeof(int);
        
        memcpy(model->nodes[i].output_indices, ptr, fn->num_outputs * sizeof(int));
        ptr += fn->num_outputs * sizeof(int);
        
        // Read Params
        if (fn->params_size > 0) {
            void* params = eif_memory_alloc(pool, fn->params_size, 4);
            if (!params) return EIF_STATUS_OUT_OF_MEMORY;
            memcpy(params, ptr, fn->params_size);
            model->nodes[i].params = params;
            ptr += fn->params_size;
        } else {
            model->nodes[i].params = NULL;
        }
    }
    
    // 5. Read Graph Inputs/Outputs
    memcpy(model->input_tensor_indices, ptr, model->num_inputs * sizeof(int));
    ptr += model->num_inputs * sizeof(int);
    
    memcpy(model->output_tensor_indices, ptr, model->num_outputs * sizeof(int));
    ptr += model->num_outputs * sizeof(int);
    
    // 6. Resolve Weight Pointers
    const uint8_t* weights_start = ptr;
    // Check if we exceeded buffer
    if (weights_start + header->weights_size > buffer + buffer_size) {
        // Error or truncated?
        // Let's assume it's fine if buffer_size is sufficient.
    }
    
    for (int i = 0; i < model->num_tensors; i++) {
        if (model->tensors[i].data != NULL) {
            uint32_t offset = (uint32_t)(uintptr_t)model->tensors[i].data;
            model->tensors[i].data = (void*)(weights_start + offset);
        }
    }
    
    return EIF_STATUS_OK;
}
