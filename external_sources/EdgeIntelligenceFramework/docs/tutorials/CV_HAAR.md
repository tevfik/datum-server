# Computer Vision: Haar Cascades & Integral Images

## Overview
This module provides the foundation for object detection (e.g., Face Detection) using Haar-like features. The core optimization techniques include:
- **Integral Image**: Allows calculating the sum of any rectangular region in O(1) time.
- **Haar Features**: Simple rectangular filters sensitive to edges and lines.

## API Usage

### 1. Integral Image Computation
This is the first step for many CV algorithms.
```c
#include "eif_cv_haar.h"

// Initialize image structure
eif_cv_image_t img;
img.data = ...; 

eif_integral_image_t ii = {0};
eif_cv_compute_integral(&img, &ii);
```

### 2. Feature Calculation
Compute sum of pixels in a generic rectangle instantly:
```c
uint32_t sum = eif_cv_integral_sum(&ii, x, y, width, height);
```

## Haar Cascade Classifier (Planned/Advanced)
A full cascade classifier loads a trained XML/Model file (e.g., from OpenCV) and evaluates stages of trees.
The current implementation provides the `eif_haar_cascade_t` structure and loading hooks.

## Performance
- **Integral Image**: O(N) where N is number of pixels. Pass once over the image.
- **Sum Query**: O(1). Constant time regardless of rectangle size.

## Example
See `examples/cv_demos/haar_demo/main.c` for an example of constructing an image and checking feature sums.
