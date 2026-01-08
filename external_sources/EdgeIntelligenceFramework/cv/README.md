# Computer Vision Module (`cv/`)

Image processing and feature detection for embedded computer vision.

## Capabilities

### Image Operations
- RGB to Grayscale conversion
- Resize (bilinear interpolation)
- Crop, Flip, Rotate
- Clone, Create

### Filters
- Box Blur, Gaussian Blur
- Sobel (edge detection)
- Canny Edge Detector
- Median Filter

### Thresholding
- Binary threshold
- Otsu's automatic threshold
- Adaptive threshold

### Feature Detection
- FAST corners
- Harris corners
- Template matching

### Morphology
- Erosion, Dilation
- Opening, Closing
- Connected components
- Contour detection

### Object Detection
- Non-Maximum Suppression (NMS)
- Integral image
- Histogram

### Tracking
- Kalman tracker
- Background subtraction (MOG-like)
- Object tracking

## Usage

```c
#include "eif_cv.h"

// Load image
eif_image_t img;
eif_image_create(&img, 640, 480, EIF_IMAGE_GRAY);

// Apply Gaussian blur
eif_cv_gaussian_blur(&img, &blurred, 5);

// Canny edge detection
eif_cv_canny(&blurred, &edges, 50, 150);

// FAST corners
eif_cv_fast_corners(&img, corners, &n_corners, 20);

// Template matching
float score;
int x, y;
eif_cv_template_match(&img, &template, &x, &y, &score);
```

## Demos

```bash
./bin/edge_detection_demo     # Sobel, Canny
./bin/feature_matching_demo   # FAST, Template
./bin/object_tracking_demo    # Kalman tracking
```

## Output Formats
- PPM (RGB images)
- PGM (Grayscale images)
- Python viewer: `tools/ppm_viewer.py`

## Files
- `eif_cv_image.h` - Image data type
- `eif_cv_filter.h` - Filters
- `eif_cv_feature.h` - Feature detection
- `eif_cv_morph.h` - Morphology
- `eif_cv_detect.h` - Detection
- `eif_cv_track.h` - Tracking
