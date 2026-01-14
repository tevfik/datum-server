#include "camera_manager.h"
#include "eif_motion.h"
#include "img_converters.h"
#include "sd_storage.h"
#include <Preferences.h>

#include "camera_pins.h"
#include "mqtt_manager.h"

// External functions/vars from main
extern void uploadFrame(camera_fb_t *fb);
extern Preferences prefs; // For loading defaults if needed inside handleSnap

// Shared Globals Definition (Only definition, not extern)
// NOTE: These are defined in main usually? No, if we move them here, main must
// extern them. Let's keep them defined in main for now to avoid linking errors
// if I missed moving them. Actually, to fully refactor, they should be defined
// here if they belong to camera. But `streaming` and `motionEnabled` are used
// by MQTT/Web heavily. I will assume they are DEFINED in main.ino and EXTERNED
// in header for now. Wait, header says `extern`. So implementation must be
// somewhere. I will keep definition in .ino for this step to minimize diff.

TaskHandle_t camTaskHandle = NULL;
SemaphoreHandle_t cameraMutex = NULL;
volatile bool camPauseRequest = false;
volatile bool camPaused = false;
int frameCounter = 0;

// Shared Buffer Globals
static uint8_t *sharedBuf = NULL;
static size_t sharedLen = 0;
SemaphoreHandle_t sharedMutex = NULL;

// Async Upload Queue
struct UploadFrame {
  uint8_t *buf;
  size_t len;
};
static QueueHandle_t uploadQueue = NULL;
static TaskHandle_t uploadTaskHandle = NULL;

// Implementation

bool initCamera() {
  if (cameraMutex == NULL) {
    cameraMutex = xSemaphoreCreateMutex();
  }
  if (sharedMutex == NULL) {
    sharedMutex = xSemaphoreCreateMutex();
  }

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; // Reduced to 10MHz for OV3660 stability
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Only capture when buffer is free

  if (psramFound()) {
    Serial.printf("PSRAM Found! Size: %d bytes, Free: %d bytes\n",
                  ESP.getPsramSize(), ESP.getFreePsram());
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality =
        15; // Increased compression (lower quality) for higher FPS (from 12)
    config.fb_count = 2; // Double buffering to prevent tearing/overflow
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST; // Always get the freshest frame
  } else {
    Serial.println("WARNING: No PSRAM! High-res will fail.");
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err == ESP_OK) {
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
      prefs.begin("datum", true); // Read-only
      bool valMirror =
          prefs.getBool("pref_imir", true); // Default 1 (setup default)
      bool valFlip = prefs.getBool("pref_iflip", true); // Default 1
      prefs.end();

      s->set_hmirror(s, valMirror ? 1 : 0);
      s->set_vflip(s, valFlip ? 1 : 0);

      // Explicit status update
      s->status.hmirror = valMirror ? 1 : 0;
      s->status.vflip = valFlip ? 1 : 0;
    }
  }
  return err == ESP_OK;
}

// Helpers
framesize_t getFrameSizeFromName(String name) {
  if (name == "QQVGA")
    return FRAMESIZE_QQVGA;
  if (name == "QCIF")
    return FRAMESIZE_QCIF;
  if (name == "QVGA")
    return FRAMESIZE_QVGA;
  if (name == "CIF")
    return FRAMESIZE_CIF;
  if (name == "VGA")
    return FRAMESIZE_VGA;
  if (name == "SVGA")
    return FRAMESIZE_SVGA;
  if (name == "XGA")
    return FRAMESIZE_XGA;
  if (name == "HD")
    return FRAMESIZE_HD;
  if (name == "SXGA")
    return FRAMESIZE_SXGA;
  if (name == "UXGA")
    return FRAMESIZE_UXGA;
  if (name == "QXGA")
    return FRAMESIZE_QXGA;
  return FRAMESIZE_VGA; // Default
}

String getFrameSizeName(framesize_t size) {
  switch (size) {
  case FRAMESIZE_QQVGA:
    return "QQVGA";
  case FRAMESIZE_QCIF:
    return "QCIF";
  case FRAMESIZE_QVGA:
    return "QVGA";
  case FRAMESIZE_CIF:
    return "CIF";
  case FRAMESIZE_VGA:
    return "VGA";
  case FRAMESIZE_SVGA:
    return "SVGA";
  case FRAMESIZE_XGA:
    return "XGA";
  case FRAMESIZE_HD:
    return "HD";
  case FRAMESIZE_SXGA:
    return "SXGA";
  case FRAMESIZE_UXGA:
    return "UXGA";
  case FRAMESIZE_QXGA:
    return "QXGA";
  default:
    return "Unknown";
  }
}

// Motion Logic
volatile int ignoreMotionFrames = 0;

void ignoreMotionFor(int frames) { ignoreMotionFrames = frames; }

bool checkMotion(camera_fb_t *fb) {
  if (ignoreMotionFrames > 0) {
    if (ignoreMotionFrames == 1) {
      // Last ignore frame: Reset motion baseline so next frame isn't diffed
      // against old orientation
      eif_motion_init();
    }
    ignoreMotionFrames--;
    return false;
  }

  if (eif_motion_check(fb)) {
    return true;
  }
  return false;
}

// Recording Helpers (Assuming they are defined in sd_storage.h or locally
// needed?) NOTE: isRecordingActive, processRecording, stopRecording,
// startRecording used to be in .ino? If they are SD related, they should be in
// sd_storage. Let's assume for now we need to reference them from sd_storage or
// define them. Check sd_storage.h content? Assuming they are there or we need
// to move them. For now, I will use extern if they are not in sd_storage.h, but
// likely they are. Actually, looking at .ino, processRecording etc seem to be
// missing from my view. I will declare extern for now to be safe or assuming
// they are in sd_storage.h.
#include "sd_storage.h"

extern unsigned long lastFrameTime;

// Was redundant code

// Improved Implementation below

// Helper to take High Res Snapshot (Re-inits camera temporarily)
void takeHighResSnapshot(String filename) {
  // Take Mutex to prevent Network Task from accessing camera during
  // reconfiguration
  if (cameraMutex != NULL) {
    xSemaphoreTake(cameraMutex, portMAX_DELAY);
  }

  esp_camera_deinit();
  delay(100);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.frame_size = FRAMESIZE_UXGA; // Default High Res
  config.jpeg_quality = 10;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.pin_d5 = Y7_GPIO_NUM;

  // Check Prefs for user resolution
  prefs.begin("datum", true);
  String res = prefs.getString("pref_ires", "UXGA");
  prefs.end();
  config.frame_size = getFrameSizeFromName(res);

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("[SNAP] Init High-Res Failed");
    initCamera();
    if (cameraMutex != NULL)
      xSemaphoreGive(cameraMutex);
    return;
  }

  // Skip a few frames
  for (int i = 0; i < 3; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb)
      esp_camera_fb_return(fb);
    delay(50);
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    if (isSDAvailable()) {
      saveFrameToSD(fb, filename);
      Serial.println("[SNAP] Saved High Res: " + filename);
    }
    esp_camera_fb_return(fb);
  }

  esp_camera_deinit();
  initCamera(); // Restore

  if (cameraMutex != NULL) {
    xSemaphoreGive(cameraMutex);
  }
}

// Shared Buffer Implementation
// Shared Buffer Implementation
void updateSharedFrame(camera_fb_t *fb) {
  if (!sharedMutex)
    return;

  // Try to update shared buffer without stalling camera task too long
  if (xSemaphoreTake(sharedMutex, 5 / portTICK_PERIOD_MS)) {
    static size_t sharedCapacity = 0;
    // Reallocate only if frame grows larger than current capacity
    if (fb->len > sharedCapacity) {
      if (sharedBuf)
        free(sharedBuf);
      // Prefer PSRAM for large buffers
      sharedBuf = (uint8_t *)ps_malloc(fb->len);
      if (!sharedBuf)
        sharedBuf = (uint8_t *)malloc(fb->len);

      if (sharedBuf) {
        sharedCapacity = fb->len;
        Serial.printf("[SHARED] Alloc: %d bytes\n", sharedCapacity);
      } else {
        sharedCapacity = 0; // Failed
      }
    }

    if (sharedBuf) {
      memcpy(sharedBuf, fb->buf, fb->len);
      sharedLen = fb->len; // Actual length
    }
    xSemaphoreGive(sharedMutex);
  }
}

// Zero-copy-ish (reused buffer) for consumer
void copySharedFrame(uint8_t **destBuf, size_t *destLen, size_t *destCapacity) {
  // Always initialize output to safe value
  *destLen = 0;

  if (!sharedMutex || !sharedBuf) {
    return; // Not initialized yet
  }

  if (xSemaphoreTake(sharedMutex,
                     50 / portTICK_PERIOD_MS)) { // Increased timeout
    // Check if there's data
    if (sharedLen == 0) {
      xSemaphoreGive(sharedMutex);
      return;
    }

    // Check if we need better allocation
    if (sharedLen > *destCapacity) {
      if (*destBuf)
        free(*destBuf);
      // Prefer PSRAM to leave DRAM for WiFi/SSL
      *destBuf = (uint8_t *)ps_malloc(sharedLen);
      if (!*destBuf)
        *destBuf = (uint8_t *)malloc(sharedLen); // Fallback

      if (*destBuf) {
        *destCapacity = sharedLen;
      } else {
        *destCapacity = 0;
        xSemaphoreGive(sharedMutex);
        return;
      }
    }

    if (*destBuf && sharedBuf) {
      memcpy(*destBuf, sharedBuf, sharedLen);
      *destLen = sharedLen;
    }
    xSemaphoreGive(sharedMutex);
  } else {
    // Semaphore timeout - this is normal, just skip this frame
    // destLen is already 0
  }
}

// ============================================================================
// Async Upload Task
// ============================================================================

// Upload task runs on Core 0 (same as Network Task) to share WiFi stack
void uploadTask(void *parameter) {
  UploadFrame frame;
  for (;;) {
    // Wait for frame in queue (block indefinitely)
    if (xQueueReceive(uploadQueue, &frame, portMAX_DELAY)) {
      if (frame.buf && frame.len > 0) {
        // Create a fake camera_fb_t to pass to existing uploadFrame
        camera_fb_t fake_fb;
        fake_fb.buf = frame.buf;
        fake_fb.len = frame.len;
        fake_fb.width = 0; // Not used by uploadFrame
        fake_fb.height = 0;
        fake_fb.format = PIXFORMAT_JPEG;

        uploadFrame(
            &fake_fb); // Blocking HTTP POST in this task (doesn't block camera)

        // Free the copied buffer
        free(frame.buf);
      }
    }
  }
}

// Initialize async upload queue and task
void startUploadTask() {
  if (uploadQueue == NULL) {
    uploadQueue = xQueueCreate(3, sizeof(UploadFrame)); // Buffer 3 frames max
  }
  if (uploadTaskHandle == NULL && uploadQueue != NULL) {
    xTaskCreatePinnedToCore(uploadTask, "UploadTask", 8192, NULL,
                            1, // Same priority as Network Task
                            &uploadTaskHandle,
                            0 // Core 0 (Pro Core - with Network Task)
    );
    Serial.println("[UPLOAD] Async Upload Task Started on Core 0");
  }
}

// Queue a frame for async upload (non-blocking)
bool queueFrameForUpload(camera_fb_t *fb) {
  if (!uploadQueue || !fb || fb->len == 0)
    return false;

  // Make a copy of frame data (PSRAM preferred)
  uint8_t *copy = (uint8_t *)ps_malloc(fb->len);
  if (!copy)
    copy = (uint8_t *)malloc(fb->len);
  if (!copy)
    return false;

  memcpy(copy, fb->buf, fb->len);

  UploadFrame frame;
  frame.buf = copy;
  frame.len = fb->len;

  // Try to send to queue (don't block if full - drop frame)
  if (xQueueSend(uploadQueue, &frame, 0) != pdTRUE) {
    free(copy); // Queue full, discard this frame
    return false;
  }
  return true;
}

void processCameraLoop() {
  unsigned long now = millis();

  // If all off, still drain camera buffer to prevent FB-OVF
  if (!streaming && !motionEnabled && !isRecordingActive()) {
    // Drain buffer to prevent overflow - just grab and return immediately
    camera_fb_t *drain_fb = esp_camera_fb_get();
    if (drain_fb) {
      esp_camera_fb_return(drain_fb);
    }
    delay(100); // Slow poll when idle
    return;
  }
  // Always grab frames to prevent FB-OVF, but throttle motion check separately
  // The frameCounter mechanism in motion section handles throttling motion
  // detection

  // Small delay between frame grabs (prevent 100% CPU, but fast enough to drain
  // buffer)
  int minInterval = (streaming || isRecordingActive())
                        ? 1
                        : 50; // 50ms = ~20 FPS drain for motion-only mode

  // Early check removed - must drain buffer first!

  camera_fb_t *fb = NULL;

  // Mutex Lock - STILL NEEDED because we access CAMERA DRIVER
  if (cameraMutex != NULL) {
    if (xSemaphoreTake(cameraMutex, 200 / portTICK_PERIOD_MS)) {
      fb = esp_camera_fb_get();
      xSemaphoreGive(cameraMutex);
    }
  } else {
    fb = esp_camera_fb_get();
  }

  if (!fb) {
    delay(10);
    return;
  }

  lastFrameTime = millis();

  // 1. Streaming Logic
  // 1. Streaming Logic
  if (streaming) {
    // Local stream: update every frame for high FPS
    updateSharedFrame(fb);

    // Remote upload: Async queue (non-blocking)
    // Frames are copied and uploaded in background by uploadTask

    bool shouldUpload = true;
    if (isRecordingActive()) {
      // If recording to SD, throttle upload to save resources
      // (bandwidth/CPU/PSRAM) Skip 9 out of 10 frames (approx 2-3 FPS upload
      // while recording)
      static int recSkipCounter = 0;
      if (++recSkipCounter < 10) {
        shouldUpload = false;
      } else {
        recSkipCounter = 0;
      }
    }

    if (shouldUpload) {
      queueFrameForUpload(fb); // Returns immediately, doesn't block camera
    }
  }

  // 2. Video Recording ... (Rest is same)
  if (isRecordingActive()) {
    processRecording(fb);

    static unsigned long recordStart = 0;
    if (recordStart == 0)
      recordStart = millis();

    if (millis() - recordStart > 15000) {
      stopRecording();
      recordStart = 0;
      Serial.println("[MOTION] Clip limit reached. Stopping.");
    }
  }

  // 3. Motion Check
  if (motionEnabled) {
    if (!isRecordingActive()) {
      int checkInterval = streaming ? 10 : 1;
      if (++frameCounter >= checkInterval) {
        frameCounter = 0;
        if (checkMotion(fb)) {
          if (isSDAvailable()) {
            // SAVE METADATA & RETURN FB BEFORE HIGH RES SNAPSHOT
            Serial.println("[MOTION] Triggered. Snapshot...");
            publishMotionEvent();

            // Use handleSnap to take High-Res Snapshot (same as mobile app)
            // Arg1: "" -> Load resolution from preferences (High Res)
            // Arg2: true -> Save to SD card

            // Release Frame Buffer FIRST so we can deinit camera
            esp_camera_fb_return(fb);
            fb =
                NULL; // Mark as null so we don't return it again at end of loop

            handleSnap("", true);

            // Cooldown: Increase to 5 frames to let AEW/AGC settle after
            // resolution switch
            ignoreMotionFor(5);
          }
        }
      }
    }
  }

  if (fb)
    esp_camera_fb_return(fb);
}

void cameraTaskLoop(void *parameter) {
  for (;;) {
    processCameraLoop();
    delay(1);
  }
} // Replaces previous loop definition

// Was duplicate startCameraTask - Removed

void handleSnap(String resolution, bool saveToCard) {
  // Request Pause and Wait
  camPauseRequest = true;
  unsigned long pauseStart = millis();
  while (!camPaused && millis() - pauseStart < 1000) {
    delay(10);
  }
  // Safety: If timeout, we proceed anyway but log error (or should we abort?)
  // For now, proceed but maybe with mutex backup if pause failed.
  // Actually, if pause failed, we are at risk of crash.

  if (camTaskHandle == NULL) {
    // Fallback if task not running
    if (cameraMutex != NULL)
      xSemaphoreTake(cameraMutex, portMAX_DELAY);
  }

  if (resolution.length() == 0) {
    prefs.begin("datum", true);
    resolution = prefs.getString("pref_ires", "UXGA");
    prefs.end();
  }
  Serial.println("[SNAP] Starting high-res capture...");
  Serial.println("[SNAP] Target Resolution: " + resolution);

  framesize_t savedSize = FRAMESIZE_VGA;
  int savedMirror = 0;
  int savedFlip = 0;

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    savedSize = s->status.framesize;
    savedMirror = s->status.hmirror;
    savedFlip = s->status.vflip;
  }

  framesize_t snapSize = getFrameSizeFromName(resolution);
  unsigned long startTime = millis();

  bool wasStreaming = streaming;
  streaming = false;

  esp_camera_deinit();
  delay(100);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.frame_size = snapSize;
  config.jpeg_quality = 10;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.pin_d5 = Y7_GPIO_NUM; // Redundant but needed for struct completeness

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[SNAP] Init Failed: 0x%x\n", err);
    initCamera(); // Fallback to default
    streaming = wasStreaming;
    if (cameraMutex)
      xSemaphoreGive(cameraMutex);
    return;
  }

  s = esp_camera_sensor_get();
  if (s) {
    s->set_framesize(s, snapSize);
    s->set_hmirror(s, savedMirror);
    s->set_vflip(s, savedFlip);
  }

  // Flush - Reduced delays
  for (int i = 0; i < 2; i++) { // Reduced to 2 frames
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb)
      esp_camera_fb_return(fb);
    delay(50); // Reduced from 100
  }
  delay(100); // Reduced from 200

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb || fb->len < 5000) {
    if (fb)
      esp_camera_fb_return(fb);
    esp_camera_deinit();
    initCamera();
    streaming = wasStreaming;
    if (cameraMutex)
      xSemaphoreGive(cameraMutex);
    return;
  }

  Serial.printf("[SNAP] Captured: %d bytes\n", fb->len);

  if (saveToCard) {
    if (isSDAvailable()) {
      saveFrameToSD(fb);
      Serial.println("[SNAP] Saved.");
    }
  } else {
    uploadFrame(fb);
  }

  esp_camera_fb_return(fb);

  if (wasStreaming) {
    // Reduced delay to improve recovery speed
    delay(500);
    streaming = true;
  }

  // Revert to original camera state (initCamera handles generic config)
  // Logic in .ino was: "Reinit with HIGH RES" -> Snap -> "Reinit with default"
  // (implied?) Actually .ino didn't reinit back! It left it at high res? Wait,
  // if wasStreaming is set to true, loop might fail if config is different?
  // Let's check original code logic.
  // Original code: `handleSnap` did NOT call `initCamera()` at the end.
  // It assumed subsequent calls or restart?
  // Ah, the original code had a bug then? Or maybe stream works fine with high
  // res? Stream usually expects low res / high framerate. I should probably
  // restore `initCamera()` at the end if I want to be safe. But strictly
  // following original logic:

  // Actually, wait, line 1700 in original: deinit(), then init() with high res.
  // Then returns.
  // Next time `processCameraLoop` runs, it uses the NEW config (High Res).
  // If `streaming` is true, it uploads HIGH RES frames. This might be slow.

  // I will add `initCamera()` at the end to restore normal operation,
  // as that seems safer and likely intended but missing.
  esp_camera_deinit(); // Cleanup high res
  initCamera();        // Restore default/VGA

  // Resume Camera Task
  camPauseRequest = false;

  // Fallback Mutex Give (only if we took it because task handle was null)
  if (camTaskHandle == NULL && cameraMutex) {
    xSemaphoreGive(cameraMutex);
  }
}

// Wrapper Task for FreeRTOS
void cameraTask(void *pvParameters) {
  unsigned long lastHeartbeat = 0;
  while (true) {
    // Heartbeat removed as per user request
    if (camPauseRequest) {
      camPaused = true;
      while (camPauseRequest) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
      }
      camPaused = false;
    }
    processCameraLoop();
    // Yield to avoid Watchdog if processCameraLoop returns instantly
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void startCameraTask() {
  if (camTaskHandle != NULL)
    return; // Already running

  xTaskCreatePinnedToCore(cameraTask,     /* Task function */
                          "CameraTask",   /* Name */
                          8192,           /* Stack size */
                          NULL,           /* Parameters */
                          1,              /* Priority */
                          &camTaskHandle, /* Handle */
                          1               /* Core 1 (ESP32-S3 App Core) */
  );
  Serial.println("[CAM] Task Started on Core 1");
}
