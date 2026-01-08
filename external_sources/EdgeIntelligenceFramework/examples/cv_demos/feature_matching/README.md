# Feature Detection & Matching Demo

Demonstrates corner detection and template matching from the EIF CV library.

## Algorithms

### FAST Corner Detection
Features from Accelerated Segment Test (FAST-9):
- High-speed corner detection
- Uses 16-pixel Bresenham circle
- Non-maximum suppression for clean results

### Harris Corner Detection
Classic corner detector based on gradient structure tensor:
- Computes eigenvalues of gradient covariance
- Parameters: k (sensitivity), threshold (response cutoff)

### Template Matching
Sliding window search for pattern matching:
- Methods: SQDIFF, CCORR, CCOEFF (+ normalized variants)
- Returns location and match score

## API Usage

```c
#include "eif_cv.h"

// FAST corners
eif_cv_keypoint_t corners[100];
int n = eif_cv_detect_fast(&img, threshold, use_nms, corners, 100);

// Harris corners
n = eif_cv_detect_harris(&img, block_size, ksize, k, thresh, corners, 100, &pool);

// Template matching
eif_cv_match_template(&src, &templ, result, EIF_CV_TM_SQDIFF, &pool);
```

## Build & Run

```bash
cd build && make feature_matching_demo && ./bin/feature_matching_demo
```
