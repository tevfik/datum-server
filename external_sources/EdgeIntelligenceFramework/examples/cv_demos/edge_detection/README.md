# Edge Detection Demo

Demonstrates edge detection algorithms from the EIF Computer Vision library.

## Algorithms

### Sobel Operator
Computes image gradients using 3x3 convolution kernels:
- **Gx**: Horizontal gradient (detects vertical edges)
- **Gy**: Vertical gradient (detects horizontal edges)
- **Magnitude**: √(Gx² + Gy²)

### Canny Edge Detector
Multi-stage algorithm:
1. Gaussian blur (noise reduction)
2. Gradient computation
3. Non-maximum suppression (edge thinning)
4. Hysteresis thresholding (edge linking)

## API Usage

```c
#include "eif_cv.h"

// Sobel gradient
eif_cv_sobel(&src, &dst, 1, 0, 3);  // dx=1, dy=0, ksize=3

// Canny edges
eif_cv_canny(&src, &edges, low_thresh, high_thresh, &pool);
```

## Parameters

| Function | Parameter | Description |
|----------|-----------|-------------|
| `eif_cv_sobel` | `dx, dy` | Derivative order (1 for first derivative) |
| `eif_cv_sobel` | `ksize` | Kernel size (3, 5, or 7) |
| `eif_cv_canny` | `low_thresh` | Lower hysteresis threshold |
| `eif_cv_canny` | `high_thresh` | Upper hysteresis threshold |

## Build & Run

```bash
cd build
make edge_detection_demo
./bin/edge_detection_demo
```
