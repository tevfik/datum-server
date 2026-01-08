#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eif_cv_haar.h"

int main(void) {
    printf("=== EIF CV Haar Cascade Demo ===\n");
    
    // 1. Create a 20x20 Image
    int w = 20;
    int h = 20;
    eif_cv_image_t img;
    img.width = w;
    img.height = h;
    img.stride = w;
    img.data = (uint8_t*)malloc(w * h);
    
    // Fill with background (light gray)
    memset(img.data, 200, w * h);
    
    // Draw a "face" rect at (5, 5) size 10x10
    // Eyes: dark rectangles
    // (6, 6) 3x3 and (11, 6) 3x3
    for(int y=0; y<h; y++) {
        for(int x=0; x<w; x++) {
            // Left Eye
            if (x >= 6 && x < 9 && y >= 6 && y < 9) img.data[y*w+x] = 50;
            // Right Eye
            if (x >= 11 && x < 14 && y >= 6 && y < 9) img.data[y*w+x] = 50;
            // Nose (bridge)
            if (x >= 9 && x < 11 && y >= 6 && y < 12) img.data[y*w+x] = 100;
        }
    }
    
    printf("Synthesized 20x20 image with face pattern at (5,5).\n");
    
    // 2. Load Cascade (Dummy loader in our implementation for now)
    eif_haar_cascade_t cascade = {0};
    // Initialize dummy cascade
    // In a real scenario, this would load XML/Data from flash
    // For this demo, we rely on the implementation specifics or just show API calls
    
    // Since we don't have a real trained model file parser yet, 
    // we will manually construct a simple feature if needed, 
    // or just run the detection which might return nothing or random on empty model.
    // 
    // However, the current eif_cv_haar.c implementation has basic structure but no default model.
    // We will simulate detection by manually using Integral Image to show we can compute features.
    
    printf("Computing Integral Image...\n");
    eif_integral_image_t ii = {0};
    eif_cv_compute_integral(&img, &ii);
    
    printf("Integral Image Computed.\n");
    printf("Sum of whole image: %u\n", ii.data[w*h - 1]);
    
    // Check sum of the "Left Eye" region (6,6, 3,3)
    uint32_t eye_sum = eif_cv_integral_sum(&ii, 6, 6, 3, 3);
    printf("Sum of Left Eye region (3x3 pixels, val~50): %u (Expected ~450)\n", eye_sum);
    
    // Check sum of Background region (0,0, 3,3)
    uint32_t bg_sum = eif_cv_integral_sum(&ii, 0, 0, 3, 3);
    printf("Sum of Background region (3x3 pixels, val~200): %u (Expected ~1800)\n", bg_sum);
    
    printf("\nHaar Features are differences of sums like these.\n");
    printf("A Haar classifier checks many such features to detect objects.\n");

    free(img.data);
    free(ii.data);
    // eif_cv_haar_free(&cascade); // If we had init it
    
    return 0;
}
