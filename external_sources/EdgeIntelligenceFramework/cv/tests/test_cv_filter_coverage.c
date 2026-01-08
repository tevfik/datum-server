/**
 * @file test_cv_filter_coverage.c
 * @brief Comprehensive Coverage Tests for CV Filter Module
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "eif_cv.h"
#include "eif_cv_filter.h"

// Mock test framework
static int tests_run = 0;
static int tests_passed = 0;
static uint8_t pool_buffer[4 * 1024 * 1024];
static eif_memory_pool_t pool;

#define TEST(name) do { \
    printf("Running %s... ", #name); \
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer)); \
    if (name()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
    tests_run++; \
} while(0)

// Helper to create a test image
static void create_test_image(eif_cv_image_t* img, int w, int h, uint8_t val) {
    eif_cv_image_create(img, w, h, EIF_CV_GRAY8, &pool);
    memset(img->data, val, img->stride * h);
}

// Helper to create a gradient image
static void create_gradient_image(eif_cv_image_t* img, int w, int h) {
    eif_cv_image_create(img, w, h, EIF_CV_GRAY8, &pool);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            img->data[y * img->stride + x] = (uint8_t)((x + y) % 256);
        }
    }
}

// ============================================================================
// Generic Filter Tests
// ============================================================================

static bool test_filter2d_identity(void) {
    eif_cv_image_t src, dst;
    create_gradient_image(&src, 10, 10);
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    // Identity kernel 3x3
    float32_t kernel[9] = {0, 0, 0, 0, 1, 0, 0, 0, 0};
    
    eif_status_t status = eif_cv_filter2d(&src, &dst, kernel, 3);
    if (status != EIF_STATUS_OK) return false;
    
    // Check center pixels (border handling might differ)
    for (int y = 1; y < 9; y++) {
        for (int x = 1; x < 9; x++) {
            if (src.data[y * src.stride + x] != dst.data[y * dst.stride + x]) return false;
        }
    }
    return true;
}

static bool test_filter2d_invalid(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 0);
    create_test_image(&dst, 10, 10, 0);
    float32_t kernel[9] = {0};
    
    if (eif_cv_filter2d(NULL, &dst, kernel, 3) == EIF_STATUS_OK) return false;
    if (eif_cv_filter2d(&src, NULL, kernel, 3) == EIF_STATUS_OK) return false;
    if (eif_cv_filter2d(&src, &dst, NULL, 3) == EIF_STATUS_OK) return false;
    if (eif_cv_filter2d(&src, &dst, kernel, 2) == EIF_STATUS_OK) return false; // Even ksize
    
    return true;
}

static bool test_sep_filter2d(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 100);
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    // Box blur 3x3 separable: [1/3, 1/3, 1/3] x [1/3, 1/3, 1/3]
    float32_t kx[3] = {0.33333f, 0.33333f, 0.33333f};
    float32_t ky[3] = {0.33333f, 0.33333f, 0.33333f};
    
    eif_status_t status = eif_cv_sep_filter2d(&src, &dst, kx, 3, ky, 3, &pool);
    if (status != EIF_STATUS_OK) return false;
    
    // Center pixel should be ~100
    int val = dst.data[5 * dst.stride + 5];
    return (val >= 99 && val <= 101);
}

// ============================================================================
// Blur Tests
// ============================================================================

static bool test_blur_gaussian(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 0);
    src.data[5 * src.stride + 5] = 255; // Impulse
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    eif_status_t status = eif_cv_blur_gaussian(&src, &dst, 3, 1.0f, &pool);
    if (status != EIF_STATUS_OK) return false;
    
    // Test default sigma (<= 0)
    status = eif_cv_blur_gaussian(&src, &dst, 3, 0.0f, &pool);
    if (status != EIF_STATUS_OK) return false;
    
    // Center should be highest, neighbors lower
    uint8_t center = dst.data[5 * dst.stride + 5];
    uint8_t neighbor = dst.data[5 * dst.stride + 6];
    
    return center > neighbor && neighbor > 0;
}

static bool test_blur_median(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 100);
    // Add salt and pepper noise
    src.data[5 * src.stride + 5] = 255;
    src.data[5 * src.stride + 6] = 0;
    
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    eif_status_t status = eif_cv_blur_median(&src, &dst, 3);
    if (status != EIF_STATUS_OK) return false;
    
    // Noise should be removed (replaced by median ~100)
    uint8_t val1 = dst.data[5 * dst.stride + 5];
    uint8_t val2 = dst.data[5 * dst.stride + 6];
    
    return (val1 >= 90 && val1 <= 110) && (val2 >= 90 && val2 <= 110);
}

static bool test_blur_bilateral(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 100);
    // Edge: left 100, right 200
    for (int y = 0; y < 10; y++) {
        for (int x = 5; x < 10; x++) {
            src.data[y * src.stride + x] = 200;
        }
    }
    
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    // Bilateral should preserve edge
    eif_status_t status = eif_cv_blur_bilateral(&src, &dst, 5, 10.0f, 10.0f);
    if (status != EIF_STATUS_OK) return false;
    
    uint8_t left = dst.data[5 * dst.stride + 4];
    uint8_t right = dst.data[5 * dst.stride + 5];
    
    // Edge should still be sharp-ish
    return (right - left) > 50;
}

// ============================================================================
// Edge Detection Tests
// ============================================================================

static bool test_sobel(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 0);
    // Vertical edge
    for (int y = 0; y < 10; y++) {
        for (int x = 5; x < 10; x++) {
            src.data[y * src.stride + x] = 255;
        }
    }
    
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    // Sobel X
    eif_status_t status = eif_cv_sobel(&src, &dst, 1, 0, 3);
    if (status != EIF_STATUS_OK) return false;
    
    // Edge at x=5 should be detected
    // Sobel output is normalized to [0, 255] with 128 as zero?
    // Code says: sum = sum / 4.0f + 128.0f;
    // Left of edge (x=4): 0, 0, 255 -> gradient positive?
    // Right of edge (x=5): 0, 255, 255 -> gradient?
    
    // Let's just check that it's not 128 (flat) at the edge
    uint8_t edge_val = dst.data[5 * dst.stride + 5];
    return edge_val != 128;
}

static bool test_scharr(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 0);
    // Vertical edge
    for (int y = 0; y < 10; y++) {
        for (int x = 5; x < 10; x++) {
            src.data[y * src.stride + x] = 255;
        }
    }
    
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    eif_status_t status = eif_cv_scharr(&src, &dst, 1, 0);
    if (status != EIF_STATUS_OK) return false;
    
    // Scharr is just a filter2d call, so output is not normalized to 128 center?
    // Wait, eif_cv_scharr calls eif_cv_filter2d.
    // eif_cv_filter2d clamps to [0, 255].
    // Scharr kernel has negative values.
    // If result is negative, it clamps to 0.
    // If result is positive, it clamps to 255.
    
    // At the edge, we should see some response.
    // Depending on direction, it might be 0 or 255.
    
    // Check a range around the edge
    bool found_edge = false;
    for (int x = 3; x < 7; x++) {
        if (dst.data[5 * dst.stride + x] > 0) found_edge = true;
    }
    return found_edge;
}

static bool test_laplacian(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 0);
    src.data[5 * src.stride + 5] = 255; // Point
    
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    eif_status_t status = eif_cv_laplacian(&src, &dst, 3);
    if (status != EIF_STATUS_OK) return false;
    
    // Laplacian of a point should be strong at center
    // Kernel center is -4. 255 * -4 = -1020 -> clamped to 0.
    // Neighbors are 1. 255 * 1 = 255.
    // So center should be 0, neighbors 255?
    // Wait, if src has 255 at center, and 0 elsewhere.
    // At center: sum = 255 * (-4) + 0 = -1020 -> 0.
    // At neighbor: sum = 255 * 1 + 0 = 255 -> 255.
    
    return dst.data[5 * dst.stride + 5] == 0 && dst.data[5 * dst.stride + 6] == 255;
}

static bool test_gradient_magnitude(void) {
    eif_cv_image_t gx, gy, mag;
    create_test_image(&gx, 10, 10, 128 + 10); // dx = 10
    create_test_image(&gy, 10, 10, 128 + 0);  // dy = 0
    eif_cv_image_create(&mag, 10, 10, EIF_CV_GRAY8, &pool);
    
    eif_status_t status = eif_cv_gradient_magnitude(&gx, &gy, &mag);
    if (status != EIF_STATUS_OK) return false;
    
    // Magnitude should be 10
    return mag.data[0] == 10;
}

// ============================================================================
// Other Tests
// ============================================================================

static bool test_adaptive_threshold(void) {
    eif_cv_image_t src, dst;
    create_gradient_image(&src, 20, 20); // Gradient
    eif_cv_image_create(&dst, 20, 20, EIF_CV_GRAY8, &pool);
    
    eif_status_t status = eif_cv_adaptive_threshold(&src, &dst, 255, 5, 0, &pool);
    if (status != EIF_STATUS_OK) return false;
    
    // Adaptive threshold on a gradient should produce some pattern
    // Just check it runs and produces binary output
    bool has_0 = false;
    bool has_255 = false;
    for (int i = 0; i < 400; i++) {
        if (dst.data[i] == 0) has_0 = true;
        if (dst.data[i] == 255) has_255 = true;
    }
    return has_0 && has_255;
}

static bool test_sharpen(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 100);
    src.data[5 * src.stride + 5] = 200; // Detail
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    eif_status_t status = eif_cv_sharpen(&src, &dst, 1.0f, &pool);
    if (status != EIF_STATUS_OK) return false;
    
    // Sharpening should increase contrast of the detail
    // Original: 200. Blurred will be < 200.
    // Result = 200 + 1.0 * (200 - blurred) > 200.
    
    return dst.data[5 * dst.stride + 5] > 200;
}

static bool test_threshold_types(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 100);
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    // BINARY: 100 > 50 -> 255
    eif_cv_threshold(&src, &dst, 50, 255, EIF_CV_THRESH_BINARY);
    if (dst.data[0] != 255) return false;
    
    // BINARY_INV: 100 > 50 -> 0
    eif_cv_threshold(&src, &dst, 50, 255, EIF_CV_THRESH_BINARY_INV);
    if (dst.data[0] != 0) return false;
    
    // TRUNC: 100 > 50 -> 50
    eif_cv_threshold(&src, &dst, 50, 255, EIF_CV_THRESH_TRUNC);
    if (dst.data[0] != 50) return false;
    
    // TOZERO: 100 > 50 -> 100
    eif_cv_threshold(&src, &dst, 50, 255, EIF_CV_THRESH_TOZERO);
    if (dst.data[0] != 100) return false;
    
    // TOZERO_INV: 100 > 50 -> 0
    eif_cv_threshold(&src, &dst, 50, 255, EIF_CV_THRESH_TOZERO_INV);
    if (dst.data[0] != 0) return false;
    
    return true;
}

static bool test_otsu_coverage(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 10, 10, 0);
    // Bimodal distribution: half 50, half 200
    for (int i = 0; i < 50; i++) src.data[i] = 50;
    for (int i = 50; i < 100; i++) src.data[i] = 200;
    
    eif_cv_image_create(&dst, 10, 10, EIF_CV_GRAY8, &pool);
    
    uint8_t thresh = eif_cv_threshold_otsu(&src, &dst, 255);
    
    // Threshold should be between 50 and 200 (inclusive of 50 because of implementation details)
    return thresh >= 50 && thresh < 200;
}

static bool test_large_kernels(void) {
    eif_cv_image_t src, dst;
    create_test_image(&src, 32, 32, 0);
    eif_cv_image_create(&dst, 32, 32, EIF_CV_GRAY8, &pool);
    
    // Box blur max ksize is 15
    if (eif_cv_blur_box(&src, &dst, 17) == EIF_STATUS_OK) return false;
    
    // Median blur max ksize is 15
    if (eif_cv_blur_median(&src, &dst, 17) == EIF_STATUS_OK) return false;
    
    return true;
}

int main(void) {
    printf("=== Running Test Suite: CV Filter Coverage ===\n");
    
    TEST(test_filter2d_identity);
    TEST(test_filter2d_invalid);
    TEST(test_sep_filter2d);
    TEST(test_blur_gaussian);
    TEST(test_blur_median);
    TEST(test_blur_bilateral);
    TEST(test_sobel);
    TEST(test_scharr);
    TEST(test_laplacian);
    TEST(test_gradient_magnitude);
    TEST(test_adaptive_threshold);
    TEST(test_sharpen);
    TEST(test_threshold_types);
    TEST(test_otsu_coverage);
    TEST(test_large_kernels);
    
    printf("Results: %d Run, %d Passed, %d Failed\n", tests_run, tests_passed, tests_run - tests_passed);
    return (tests_run == tests_passed) ? 0 : -1;
}
