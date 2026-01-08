# Object Tracking Demo

Demonstrates background subtraction and multi-object tracking.

## Algorithms

### Background Subtraction
Running average model with adaptive learning:
- Maintains mean and variance per pixel
- Foreground = pixels differing significantly from background
- Learning rate controls adaptation speed

### Multi-Object Tracker
Simple IoU-based tracker with state management:
- **Tentative**: New track, not yet confirmed
- **Confirmed**: Track with sufficient consecutive detections
- **Deleted**: Track lost for too long

## API Usage

```c
#include "eif_cv.h"

// Background subtraction
eif_cv_bg_model_t model;
eif_cv_bg_init(&model, width, height, alpha, &pool);
eif_cv_bg_update(&model, &frame, &fg_mask, threshold);

// Multi-object tracker
eif_cv_tracker_t tracker;
eif_cv_tracker_init(&tracker, max_tracks, max_age, min_hits, iou_thresh, &pool);
eif_cv_tracker_update(&tracker, detections, num_detections);
int n = eif_cv_tracker_get_tracks(&tracker, tracks, max_out);
```

## Parameters

| Parameter | Description |
|-----------|-------------|
| `alpha` | Background learning rate (0.01-0.3) |
| `max_age` | Max frames without detection before deletion |
| `min_hits` | Min consecutive hits to confirm track |
| `iou_thresh` | IoU threshold for detection-track matching |

## Build & Run

```bash
cd build && make object_tracking_demo && ./bin/object_tracking_demo
```
