# Object Tracking Tutorial: Kalman-Based Visual Tracking

## Learning Objectives

- Motion prediction with Kalman filter
- Multi-object tracking
- Track management (creation, deletion)
- Real-time tracking on ESP32-CAM

**Level**: Intermediate  
**Time**: 45 minutes

---

## 1. Tracking vs Detection

```
Detection: "There's an object at (x,y)"     → Per-frame
Tracking:  "Object #1 moved from A to B"   → Across frames
```

### Why Track?
- Maintain identity across frames
- Predict position during occlusion
- Smooth noisy detections
- Enable behavior analysis

---

## 2. Kalman Filter for Tracking

### State Vector

```
x = [px, py, vx, vy]ᵀ

px, py = Position
vx, vy = Velocity
```

### Motion Model

```c
// Constant velocity model
F = │ 1  0  dt  0 │
    │ 0  1  0  dt │
    │ 0  0  1   0 │
    │ 0  0  0   1 │
```

### EIF Implementation

```c
eif_kalman_tracker_t tracker;
eif_kalman_tracker_init(&tracker, &pool);

// Update with new detection
float detection[2] = {x, y};
eif_kalman_tracker_update(&tracker, detection);

// Get predicted position
float predicted[2];
eif_kalman_tracker_predict(&tracker, predicted);
```

---

## 3. Multi-Object Tracking

### Hungarian Algorithm (Data Association)

Match detections to tracks optimally:

```c
eif_mot_t mot;
eif_mot_init(&mot, max_tracks, &pool);

// Each frame:
eif_detection_t detections[10];
int n_detections = get_detections(detections);

eif_mot_update(&mot, detections, n_detections);

// Get active tracks
for (int i = 0; i < mot.n_tracks; i++) {
    printf("Track %d at (%.1f, %.1f)\n",
           mot.tracks[i].id,
           mot.tracks[i].x,
           mot.tracks[i].y);
}
```

---

## 4. ESP32-CAM Example

```c
void tracking_task(void* arg) {
    eif_mot_t mot;
    eif_mot_init(&mot, 10, &pool);
    
    while (1) {
        // Capture frame
        camera_fb_t* fb = esp_camera_fb_get();
        
        // Simple blob detection (color threshold)
        eif_detection_t detections[10];
        int n = detect_blobs(fb, detections, 10);
        
        // Update tracker
        eif_mot_update(&mot, detections, n);
        
        // Draw tracks on frame
        for (int i = 0; i < mot.n_tracks; i++) {
            draw_box(fb, mot.tracks[i].x, mot.tracks[i].y, 20, 20);
        }
        
        esp_camera_fb_return(fb);
    }
}
```

---

## 5. Summary

### Key APIs
- `eif_kalman_tracker_*()` - Single object tracking
- `eif_mot_*()` - Multi-object tracking
- `eif_hungarian()` - Optimal assignment
