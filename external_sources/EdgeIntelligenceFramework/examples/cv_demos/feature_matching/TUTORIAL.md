# Feature Matching Tutorial: FAST Corners & Template Matching

## Learning Objectives

- FAST corner detection algorithm
- Template matching for object recognition
- Feature point matching between images

**Level**: Beginner to Intermediate  
**Time**: 30 minutes

---

## 1. FAST Corner Detection

### The Algorithm

FAST (Features from Accelerated Segment Test) detects corners by examining a circle of 16 pixels around each candidate:

```
       1  2  3
    16  ●  ●  ● 4
  15 ●        ● 5
  14 ●   P    ● 6
  13 ●        ● 7
    12 ●  ●  ● 8
       11 10 9
```

**Corner criteria**: N contiguous pixels brighter/darker than center by threshold.

### EIF Implementation

```c
eif_cv_point_t corners[500];
int n_corners;

eif_cv_fast(&image, corners, &n_corners, 500,
    20,      // Threshold
    EIF_FAST_9_16  // 9 of 16 contiguous
);

printf("Found %d corners\n", n_corners);
```

---

## 2. Harris Corner Detection

More robust but slower:

```c
eif_cv_point_t corners[500];
int n_corners;

eif_cv_harris(&image, corners, &n_corners, 500,
    0.04f,   // k parameter
    100.0f   // Response threshold
);
```

---

## 3. Template Matching

Find an object by sliding a template across the image:

```c
eif_image_t image, template;
int match_x, match_y;
float score;

eif_cv_template_match(&image, &template,
    &match_x, &match_y, &score,
    EIF_TM_SQDIFF_NORMED
);

printf("Best match at (%d, %d) with score %.2f\n",
       match_x, match_y, score);
```

### Matching Methods

| Method | Description |
|--------|-------------|
| `SQDIFF` | Sum of squared differences (lower=better) |
| `CCORR` | Cross-correlation (higher=better) |
| `CCOEFF` | Correlation coefficient (higher=better) |

---

## 4. ESP32-CAM Example

```c
void feature_task(void* arg) {
    eif_cv_point_t corners[100];
    
    while (1) {
        camera_fb_t* fb = esp_camera_fb_get();
        
        eif_image_t img = {
            .data = fb->buf,
            .width = fb->width,
            .height = fb->height
        };
        
        int n;
        eif_cv_fast(&img, corners, &n, 100, 25, EIF_FAST_9_16);
        
        // Draw corners on image
        for (int i = 0; i < n; i++) {
            draw_circle(fb, corners[i].x, corners[i].y, 3);
        }
        
        esp_camera_fb_return(fb);
    }
}
```

---

## Summary

### Key APIs
- `eif_cv_fast()` - FAST corner detection
- `eif_cv_harris()` - Harris corners
- `eif_cv_template_match()` - Find template

### Performance
- FAST: ~5ms for QVGA on ESP32
- Harris: ~50ms for QVGA
- Template: Depends on template size
