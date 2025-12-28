#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include "Arduino.h"

// ============================================================================
// Configuration
// ============================================================================

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Datum server configuration
const char* serverURL = "http://your-server.com";  // No trailing slash
const char* deviceID = "esp32-cam-01";
const char* apiKey = "your-device-api-key";

// Camera settings
#define CAMERA_MODEL_AI_THINKER  // ESP32-CAM AI-Thinker
#define STREAM_FPS 10            // Frames per second
#define JPEG_QUALITY 12          // 0-63, lower = better quality

// ============================================================================
// Camera Pins (AI-Thinker ESP32-CAM)
// ============================================================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ============================================================================
// Global Variables
// ============================================================================
bool streaming = false;
unsigned long lastFrameTime = 0;
unsigned long commandCheckInterval = 5000;  // Check for commands every 5 seconds
unsigned long lastCommandCheck = 0;

HTTPClient httpCommand;
HTTPClient httpStream;

// ============================================================================
// Camera Initialization
// ============================================================================
bool initCamera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Image resolution
  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;  // 640x480
    config.jpeg_quality = JPEG_QUALITY;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  // Sensor settings
  sensor_t* s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2
  s->set_special_effect(s, 0); // 0-6
  s->set_whitebal(s, 1);       // 0=disable, 1=enable
  s->set_awb_gain(s, 1);       // 0=disable, 1=enable
  s->set_wb_mode(s, 0);        // 0-4
  s->set_exposure_ctrl(s, 1);  // 0=disable, 1=enable
  s->set_aec2(s, 0);           // 0=disable, 1=enable
  s->set_ae_level(s, 0);       // -2 to 2
  s->set_aec_value(s, 300);    // 0-1200
  s->set_gain_ctrl(s, 1);      // 0=disable, 1=enable
  s->set_agc_gain(s, 0);       // 0-30
  s->set_gainceiling(s, (gainceiling_t)0); // 0-6
  s->set_bpc(s, 0);            // 0=disable, 1=enable
  s->set_wpc(s, 1);            // 0=disable, 1=enable
  s->set_raw_gma(s, 1);        // 0=disable, 1=enable
  s->set_lenc(s, 1);           // 0=disable, 1=enable
  s->set_hmirror(s, 0);        // 0=disable, 1=enable
  s->set_vflip(s, 0);          // 0=disable, 1=enable
  s->set_dcw(s, 1);            // 0=disable, 1=enable
  s->set_colorbar(s, 0);       // 0=disable, 1=enable

  Serial.println("Camera initialized successfully");
  return true;
}

// ============================================================================
// Command Polling
// ============================================================================
void checkCommands() {
  if (millis() - lastCommandCheck < commandCheckInterval) {
    return;
  }
  lastCommandCheck = millis();

  String url = String(serverURL) + "/device/" + deviceID + "/commands";
  
  httpCommand.begin(url);
  httpCommand.addHeader("X-API-Key", apiKey);
  
  int httpCode = httpCommand.GET();
  
  if (httpCode == 200) {
    String payload = httpCommand.getString();
    Serial.println("Commands received: " + payload);
    
    // Parse JSON (simple string parsing for demo)
    if (payload.indexOf("\"action\":\"start-stream\"") > 0) {
      Serial.println("Starting stream...");
      streaming = true;
    }
    else if (payload.indexOf("\"action\":\"stop-stream\"") > 0) {
      Serial.println("Stopping stream...");
      streaming = false;
    }
    else if (payload.indexOf("\"action\":\"capture-frame\"") > 0) {
      Serial.println("Capturing single frame...");
      sendFrame();
    }
  }
  
  httpCommand.end();
}

// ============================================================================
// Frame Upload
// ============================================================================
bool sendFrame() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }

  String url = String(serverURL) + "/device/" + deviceID + "/stream/frame";
  
  httpStream.begin(url);
  httpStream.addHeader("X-API-Key", apiKey);
  httpStream.addHeader("Content-Type", "image/jpeg");
  
  int httpCode = httpStream.POST(fb->buf, fb->len);
  
  bool success = (httpCode == 200);
  
  if (success) {
    Serial.printf("Frame sent: %d bytes, HTTP %d\n", fb->len, httpCode);
  } else {
    Serial.printf("Frame send failed: HTTP %d\n", httpCode);
  }
  
  httpStream.end();
  esp_camera_fb_return(fb);
  
  return success;
}

// ============================================================================
// Streaming Loop
// ============================================================================
void streamLoop() {
  if (!streaming) {
    return;
  }

  unsigned long currentTime = millis();
  unsigned long frameInterval = 1000 / STREAM_FPS;
  
  if (currentTime - lastFrameTime >= frameInterval) {
    lastFrameTime = currentTime;
    sendFrame();
  }
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\nESP32-CAM Datum Streamer");
  Serial.println("========================");

  // Initialize camera
  if (!initCamera()) {
    Serial.println("Camera initialization failed!");
    while (1) delay(1000);
  }

  // Connect to WiFi
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("Server: %s\n", serverURL);
  Serial.printf("Device ID: %s\n", deviceID);
  Serial.println("\nReady. Send commands via datumctl:");
  Serial.println("  datumctl command send " + String(deviceID) + " start-stream");
  Serial.println("  datumctl command send " + String(deviceID) + " capture-frame");
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
  checkCommands();
  streamLoop();
  delay(10);
}
