# Edge Detection Tutorial: Image Processing Fundamentals

## Learning Objectives

By the end of this tutorial, you will understand:
- Digital image representation
- Convolution and kernel operations
- Sobel and Canny edge detection
- Real-time image processing on ESP32-CAM

**Level**: Beginner  
**Prerequisites**: Basic understanding of 2D arrays  
**Time**: 30-45 minutes

---

## 1. Digital Images

### Representation

```
Image = 2D array of pixel values

Grayscale: 0 (black) to 255 (white)
RGB: 3 channels × 8 bits each
```

```c
// EIF image structure
eif_image_t image;
eif_image_create(&image, 320, 240, EIF_IMAGE_GRAY);

// Access pixel at (x, y)
uint8_t pixel = image.data[y * image.width + x];
```

### Memory Calculation

| Resolution | Grayscale | RGB |
|------------|-----------|-----|
| 160×120 (QQVGA) | 19 KB | 57 KB |
| 320×240 (QVGA) | 75 KB | 225 KB |
| 640×480 (VGA) | 300 KB | 900 KB |

For ESP32-CAM: Use QVGA grayscale for real-time.

---

## 2. Convolution

### The Concept

Slide a **kernel** over the image, computing weighted sums:

```
Original:           Kernel (blur):     Result:
┌───┬───┬───┐       ┌───┬───┬───┐
│ 10│ 20│ 30│       │1/9│1/9│1/9│
├───┼───┼───┤   ×   ├───┼───┼───┤   = 40
│ 40│ 50│ 60│       │1/9│1/9│1/9│
├───┼───┼───┤       ├───┼───┼───┤
│ 70│ 80│ 90│       │1/9│1/9│1/9│
└───┴───┴───┘       └───┴───┴───┘
```

### Common Kernels

**Box Blur (3×3):**
```
1/9 × │ 1  1  1 │
      │ 1  1  1 │
      │ 1  1  1 │
```

**Gaussian Blur:**
```
1/16 × │ 1  2  1 │
       │ 2  4  2 │
       │ 1  2  1 │
```

**Sharpen:**
```
│  0 -1  0 │
│ -1  5 -1 │
│  0 -1  0 │
```

---

## 3. Sobel Edge Detection

### Concept

Detects edges by computing **gradients**:

```
Gx kernel:          Gy kernel:
│ -1  0  1 │        │ -1 -2 -1 │
│ -2  0  2 │        │  0  0  0 │
│ -1  0  1 │        │  1  2  1 │

Gradient magnitude: G = √(Gx² + Gy²)
```

### EIF Implementation

```c
eif_image_t input, output;

// Load/create input image
eif_image_create(&input, 320, 240, EIF_IMAGE_GRAY);

// Apply Sobel
eif_cv_sobel(&input, &output, EIF_SOBEL_BOTH);

// output now contains edge magnitudes
```

### Output Interpretation

```
           Original                    Sobel Output
    ┌─────────────────┐          ┌─────────────────┐
    │    ┌───────┐    │          │    ╔═══════╗    │
    │    │ Box   │    │    →     │    ║       ║    │
    │    │       │    │          │    ║       ║    │
    │    └───────┘    │          │    ╚═══════╝    │
    └─────────────────┘          └─────────────────┘
```

---

## 4. Canny Edge Detection

### The Algorithm

1. **Gaussian Blur**: Reduce noise
2. **Sobel Gradients**: Find edges
3. **Non-Maximum Suppression**: Thin edges
4. **Double Threshold**: Strong/weak edges
5. **Hysteresis**: Connect weak edges to strong

### Parameters

| Parameter | Low | Default | High |
|-----------|-----|---------|------|
| Low Threshold | 20 | 50 | 100 |
| High Threshold | 50 | 150 | 200 |

**Rule**: High ≈ 2-3× Low

### EIF Implementation

```c
eif_image_t input, edges;

// Create images
eif_image_create(&input, 320, 240, EIF_IMAGE_GRAY);
eif_image_create(&edges, 320, 240, EIF_IMAGE_GRAY);

// Apply Canny
int low_threshold = 50;
int high_threshold = 150;
eif_cv_canny(&input, &edges, low_threshold, high_threshold);
```

---

## 5. Code Walkthrough

### Complete Example

```c
#include "eif_cv.h"
#include "eif_memory.h"

int main(void) {
    // Memory pool
    static uint8_t pool_buf[128 * 1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buf, sizeof(pool_buf));
    
    // Create images
    eif_image_t input, blurred, edges;
    int width = 320, height = 240;
    
    eif_image_create(&input, width, height, EIF_IMAGE_GRAY);
    eif_image_create(&blurred, width, height, EIF_IMAGE_GRAY);
    eif_image_create(&edges, width, height, EIF_IMAGE_GRAY);
    
    // Load test pattern (checkerboard)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int block = ((x / 40) + (y / 40)) % 2;
            input.data[y * width + x] = block ? 255 : 0;
        }
    }
    
    // Step 1: Gaussian blur (noise reduction)
    eif_cv_gaussian_blur(&input, &blurred, 5);
    
    // Step 2: Canny edge detection
    eif_cv_canny(&blurred, &edges, 50, 150);
    
    // Count edge pixels
    int edge_count = 0;
    for (int i = 0; i < width * height; i++) {
        if (edges.data[i] > 0) edge_count++;
    }
    
    printf("Edge pixels: %d\n", edge_count);
    
    return 0;
}
```

---

## 6. ESP32-CAM Integration

### Camera Setup

```c
#include "esp_camera.h"
#include "eif_cv.h"

// Camera pins for ESP32-CAM AI-Thinker
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
// ... (full pin configuration)

void init_camera(void) {
    camera_config_t config = {
        .pixel_format = PIXFORMAT_GRAYSCALE,
        .frame_size = FRAMESIZE_QVGA,  // 320×240
        .fb_count = 1,
    };
    esp_camera_init(&config);
}
```

### Real-Time Edge Detection

```c
void edge_detection_task(void* arg) {
    eif_image_t edges;
    eif_image_create(&edges, 320, 240, EIF_IMAGE_GRAY);
    
    while (1) {
        // Capture frame
        camera_fb_t* fb = esp_camera_fb_get();
        
        // Wrap in EIF image
        eif_image_t frame = {
            .data = fb->buf,
            .width = fb->width,
            .height = fb->height,
            .format = EIF_IMAGE_GRAY
        };
        
        // Detect edges
        eif_cv_canny(&frame, &edges, 50, 150);
        
        // Stream or display edges
        stream_jpeg(&edges);
        
        esp_camera_fb_return(fb);
        vTaskDelay(50 / portTICK_PERIOD_MS);  // ~20 FPS
    }
}
```

---

## 7. Experiments

### Experiment 1: Threshold Tuning
Try different Canny thresholds (30/80, 50/150, 80/200).

### Experiment 2: Blur Kernel Size
Compare blur sizes (3, 5, 7) on noisy images.

### Experiment 3: Real-World Scenes
Test on faces, text, outdoor scenes.

---

## 8. Summary

### Key Concepts
1. **Convolution**: Kernel sliding over image
2. **Sobel**: Gradient-based edge detection
3. **Canny**: Multi-stage edge detector
4. **Thresholds**: Trade-off sensitivity vs noise

### EIF APIs
- `eif_cv_sobel()` - Gradient detection
- `eif_cv_canny()` - Full edge detection
- `eif_cv_gaussian_blur()` - Noise reduction
- `eif_image_create()` - Image allocation

### Next Steps
- Try `feature_matching` for FAST corners
- Implement on ESP32-CAM
- Add object tracking with detected edges
