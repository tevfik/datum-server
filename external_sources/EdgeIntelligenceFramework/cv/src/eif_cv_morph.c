/**
 * @file eif_cv_morph.c
 * @brief Morphological Operations
 */

#include "eif_cv_morph.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// ============================================================================
// Structuring Elements
// ============================================================================

eif_status_t eif_cv_morph_element(uint8_t* elem, eif_cv_morph_shape_t shape, int ksize) {
    if (!elem || ksize < 1) return EIF_STATUS_INVALID_ARGUMENT;
    
    int radius = ksize / 2;
    
    for (int y = 0; y < ksize; y++) {
        for (int x = 0; x < ksize; x++) {
            switch (shape) {
                case EIF_CV_MORPH_RECT:
                    elem[y * ksize + x] = 1;
                    break;
                    
                case EIF_CV_MORPH_CROSS:
                    elem[y * ksize + x] = (x == radius || y == radius) ? 1 : 0;
                    break;
                    
                case EIF_CV_MORPH_ELLIPSE: {
                    float dx = (float)(x - radius) / radius;
                    float dy = (float)(y - radius) / radius;
                    elem[y * ksize + x] = (dx * dx + dy * dy <= 1.0f) ? 1 : 0;
                    break;
                }
            }
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Basic Morphological Operations
// ============================================================================

eif_status_t eif_cv_erode(const eif_cv_image_t* src, eif_cv_image_t* dst,
                           int ksize, int iterations) {
    if (!src || !dst || ksize < 1 || (ksize % 2) == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int radius = ksize / 2;
    
    // Work with src directly, copy to dst for each iteration
    const eif_cv_image_t* input = src;
    
    for (int iter = 0; iter < iterations; iter++) {
        for (int y = 0; y < src->height; y++) {
            for (int x = 0; x < src->width; x++) {
                uint8_t min_val = 255;
                
                for (int ky = -radius; ky <= radius; ky++) {
                    for (int kx = -radius; kx <= radius; kx++) {
                        int sy = y + ky;
                        int sx = x + kx;
                        
                        if (sy < 0) sy = 0;
                        if (sy >= src->height) sy = src->height - 1;
                        if (sx < 0) sx = 0;
                        if (sx >= src->width) sx = src->width - 1;
                        
                        uint8_t val = input->data[sy * input->stride + sx];
                        if (val < min_val) min_val = val;
                    }
                }
                
                dst->data[y * dst->stride + x] = min_val;
            }
        }
        
        // For subsequent iterations, use dst as input
        if (iter == 0 && iterations > 1) {
            input = dst;
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_dilate(const eif_cv_image_t* src, eif_cv_image_t* dst,
                            int ksize, int iterations) {
    if (!src || !dst || ksize < 1 || (ksize % 2) == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int radius = ksize / 2;
    const eif_cv_image_t* input = src;
    
    for (int iter = 0; iter < iterations; iter++) {
        for (int y = 0; y < src->height; y++) {
            for (int x = 0; x < src->width; x++) {
                uint8_t max_val = 0;
                
                for (int ky = -radius; ky <= radius; ky++) {
                    for (int kx = -radius; kx <= radius; kx++) {
                        int sy = y + ky;
                        int sx = x + kx;
                        
                        if (sy < 0) sy = 0;
                        if (sy >= src->height) sy = src->height - 1;
                        if (sx < 0) sx = 0;
                        if (sx >= src->width) sx = src->width - 1;
                        
                        uint8_t val = input->data[sy * input->stride + sx];
                        if (val > max_val) max_val = val;
                    }
                }
                
                dst->data[y * dst->stride + x] = max_val;
            }
        }
        
        if (iter == 0 && iterations > 1) {
            input = dst;
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_morph_open(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                int ksize, eif_memory_pool_t* pool) {
    if (!src || !dst || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    eif_cv_image_t temp;
    eif_cv_image_create(&temp, src->width, src->height, src->format, pool);
    
    // Open = erode then dilate
    eif_cv_erode(src, &temp, ksize, 1);
    eif_cv_dilate(&temp, dst, ksize, 1);
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_morph_close(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                 int ksize, eif_memory_pool_t* pool) {
    if (!src || !dst || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    eif_cv_image_t temp;
    eif_cv_image_create(&temp, src->width, src->height, src->format, pool);
    
    // Close = dilate then erode
    eif_cv_dilate(src, &temp, ksize, 1);
    eif_cv_erode(&temp, dst, ksize, 1);
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Advanced Morphological Operations
// ============================================================================

eif_status_t eif_cv_morph_gradient(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                    int ksize, eif_memory_pool_t* pool) {
    if (!src || !dst || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    eif_cv_image_t dilated, eroded;
    eif_cv_image_create(&dilated, src->width, src->height, src->format, pool);
    eif_cv_image_create(&eroded, src->width, src->height, src->format, pool);
    
    eif_cv_dilate(src, &dilated, ksize, 1);
    eif_cv_erode(src, &eroded, ksize, 1);
    
    // Gradient = dilated - eroded
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            int diff = dilated.data[y * dilated.stride + x] - 
                       eroded.data[y * eroded.stride + x];
            dst->data[y * dst->stride + x] = (uint8_t)(diff > 0 ? diff : 0);
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_morph_tophat(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                  int ksize, eif_memory_pool_t* pool) {
    if (!src || !dst || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    eif_cv_image_t opened;
    eif_cv_image_create(&opened, src->width, src->height, src->format, pool);
    
    eif_cv_morph_open(src, &opened, ksize, pool);
    
    // Top hat = src - opened
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            int diff = src->data[y * src->stride + x] - 
                       opened.data[y * opened.stride + x];
            dst->data[y * dst->stride + x] = (uint8_t)(diff > 0 ? diff : 0);
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_morph_blackhat(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                    int ksize, eif_memory_pool_t* pool) {
    if (!src || !dst || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    eif_cv_image_t closed;
    eif_cv_image_create(&closed, src->width, src->height, src->format, pool);
    
    eif_cv_morph_close(src, &closed, ksize, pool);
    
    // Black hat = closed - src
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            int diff = closed.data[y * closed.stride + x] - 
                       src->data[y * src->stride + x];
            dst->data[y * dst->stride + x] = (uint8_t)(diff > 0 ? diff : 0);
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Connected Components
// ============================================================================

static int find_root(int* parent, int x) {
    if (parent[x] != x) {
        parent[x] = find_root(parent, parent[x]);
    }
    return parent[x];
}

static void union_labels(int* parent, int a, int b) {
    int root_a = find_root(parent, a);
    int root_b = find_root(parent, b);
    if (root_a != root_b) {
        parent[root_b] = root_a;
    }
}

int eif_cv_connected_components(const eif_cv_image_t* binary,
                                 int32_t* labels, int connectivity,
                                 eif_memory_pool_t* pool) {
    if (!binary || !labels || !pool) return 0;
    if (connectivity != 4 && connectivity != 8) connectivity = 8;
    
    int w = binary->width;
    int h = binary->height;
    
    // Union-find parent array
    int max_labels = w * h / 4 + 1;
    int* parent = eif_memory_alloc(pool, max_labels * sizeof(int), 4);
    if (!parent) return 0;
    
    for (int i = 0; i < max_labels; i++) {
        parent[i] = i;
    }
    
    int next_label = 1;
    
    // First pass: assign temporary labels
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (binary->data[y * binary->stride + x] == 0) {
                labels[y * w + x] = 0;
                continue;
            }
            
            // Check neighbors
            int neighbors[4];
            int num_neighbors = 0;
            
            // Left
            if (x > 0 && labels[y * w + x - 1] > 0) {
                neighbors[num_neighbors++] = labels[y * w + x - 1];
            }
            // Top
            if (y > 0 && labels[(y - 1) * w + x] > 0) {
                neighbors[num_neighbors++] = labels[(y - 1) * w + x];
            }
            
            if (connectivity == 8) {
                // Top-left
                if (x > 0 && y > 0 && labels[(y - 1) * w + x - 1] > 0) {
                    neighbors[num_neighbors++] = labels[(y - 1) * w + x - 1];
                }
                // Top-right
                if (x < w - 1 && y > 0 && labels[(y - 1) * w + x + 1] > 0) {
                    neighbors[num_neighbors++] = labels[(y - 1) * w + x + 1];
                }
            }
            
            if (num_neighbors == 0) {
                // New label
                labels[y * w + x] = next_label++;
                if (next_label >= max_labels) next_label = max_labels - 1;
            } else {
                // Find minimum label
                int min_label = neighbors[0];
                for (int i = 1; i < num_neighbors; i++) {
                    int root = find_root(parent, neighbors[i]);
                    if (root < min_label) min_label = root;
                }
                labels[y * w + x] = min_label;
                
                // Union all neighbors
                for (int i = 0; i < num_neighbors; i++) {
                    union_labels(parent, min_label, neighbors[i]);
                }
            }
        }
    }
    
    // Second pass: resolve labels
    int* label_map = eif_memory_alloc(pool, next_label * sizeof(int), 4);
    if (!label_map) return 0;
    memset(label_map, 0, next_label * sizeof(int));
    
    int num_components = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int label = labels[y * w + x];
            if (label > 0) {
                int root = find_root(parent, label);
                if (label_map[root] == 0) {
                    label_map[root] = ++num_components;
                }
                labels[y * w + x] = label_map[root];
            }
        }
    }
    
    return num_components;
}

eif_status_t eif_cv_component_stats(const int32_t* labels, int width, int height,
                                     int num_components,
                                     eif_cv_component_stats_t* stats) {
    if (!labels || !stats || num_components <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Initialize stats
    for (int i = 0; i < num_components; i++) {
        stats[i].label = i + 1;
        stats[i].area = 0;
        stats[i].bbox.x = width;
        stats[i].bbox.y = height;
        stats[i].bbox.width = 0;
        stats[i].bbox.height = 0;
        stats[i].centroid_x = 0;
        stats[i].centroid_y = 0;
    }
    
    // Compute stats
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int label = labels[y * width + x];
            if (label > 0 && label <= num_components) {
                eif_cv_component_stats_t* s = &stats[label - 1];
                s->area++;
                s->centroid_x += x;
                s->centroid_y += y;
                
                if (x < s->bbox.x) s->bbox.x = x;
                if (y < s->bbox.y) s->bbox.y = y;
                if (x > s->bbox.x + s->bbox.width) s->bbox.width = x - s->bbox.x;
                if (y > s->bbox.y + s->bbox.height) s->bbox.height = y - s->bbox.y;
            }
        }
    }
    
    // Finalize centroids
    for (int i = 0; i < num_components; i++) {
        if (stats[i].area > 0) {
            stats[i].centroid_x /= stats[i].area;
            stats[i].centroid_y /= stats[i].area;
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Contour Detection (simplified)
// ============================================================================

int eif_cv_find_contours(const eif_cv_image_t* binary,
                          eif_cv_contour_t* contours, int max_contours,
                          eif_memory_pool_t* pool) {
    if (!binary || !contours || !pool) return 0;
    
    // Use connected components as base
    int32_t* labels = eif_memory_alloc(pool, binary->width * binary->height * sizeof(int32_t), 4);
    if (!labels) return 0;
    
    int num_comp = eif_cv_connected_components(binary, labels, 8, pool);
    if (num_comp > max_contours) num_comp = max_contours;
    
    // For each component, trace boundary
    for (int c = 0; c < num_comp; c++) {
        int target_label = c + 1;
        contours[c].num_points = 0;
        contours[c].capacity = 256;
        contours[c].points = eif_memory_alloc(pool, contours[c].capacity * sizeof(eif_cv_point_t), 4);
        
        if (!contours[c].points) continue;
        
        // Find boundary pixels
        for (int y = 0; y < binary->height; y++) {
            for (int x = 0; x < binary->width; x++) {
                if (labels[y * binary->width + x] != target_label) continue;
                
                // Check if boundary (has non-target neighbor)
                bool is_boundary = false;
                for (int dy = -1; dy <= 1 && !is_boundary; dy++) {
                    for (int dx = -1; dx <= 1 && !is_boundary; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx < 0 || nx >= binary->width || ny < 0 || ny >= binary->height) {
                            is_boundary = true;
                        } else if (labels[ny * binary->width + nx] != target_label) {
                            is_boundary = true;
                        }
                    }
                }
                
                if (is_boundary && contours[c].num_points < contours[c].capacity) {
                    contours[c].points[contours[c].num_points].x = x;
                    contours[c].points[contours[c].num_points].y = y;
                    contours[c].num_points++;
                }
            }
        }
    }
    
    return num_comp;
}

float32_t eif_cv_contour_area(const eif_cv_contour_t* contour) {
    if (!contour || contour->num_points < 3) return 0;
    
    // Shoelace formula
    float32_t area = 0;
    for (int i = 0; i < contour->num_points; i++) {
        int j = (i + 1) % contour->num_points;
        area += contour->points[i].x * contour->points[j].y;
        area -= contour->points[j].x * contour->points[i].y;
    }
    
    return fabsf(area) / 2.0f;
}

float32_t eif_cv_contour_perimeter(const eif_cv_contour_t* contour) {
    if (!contour || contour->num_points < 2) return 0;
    
    float32_t perimeter = 0;
    for (int i = 0; i < contour->num_points; i++) {
        int j = (i + 1) % contour->num_points;
        float32_t dx = (float32_t)(contour->points[j].x - contour->points[i].x);
        float32_t dy = (float32_t)(contour->points[j].y - contour->points[i].y);
        perimeter += sqrtf(dx * dx + dy * dy);
    }
    
    return perimeter;
}

eif_cv_rect_t eif_cv_contour_bounding_rect(const eif_cv_contour_t* contour) {
    eif_cv_rect_t rect = {0, 0, 0, 0};
    if (!contour || contour->num_points == 0) return rect;
    
    int min_x = contour->points[0].x, max_x = min_x;
    int min_y = contour->points[0].y, max_y = min_y;
    
    for (int i = 1; i < contour->num_points; i++) {
        if (contour->points[i].x < min_x) min_x = contour->points[i].x;
        if (contour->points[i].x > max_x) max_x = contour->points[i].x;
        if (contour->points[i].y < min_y) min_y = contour->points[i].y;
        if (contour->points[i].y > max_y) max_y = contour->points[i].y;
    }
    
    rect.x = min_x;
    rect.y = min_y;
    rect.width = max_x - min_x + 1;
    rect.height = max_y - min_y + 1;
    
    return rect;
}
