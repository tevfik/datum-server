/**
 * ESP32 Camera Streamer for Datum Server
 * 
 * Supports multiple ESP32 camera boards and sensors:
 * - ESP32-CAM (AI-Thinker) with OV2640
 * - ESP32-S3-CAM with OV2640/OV3660
 * - Freenove ESP32-S3 WROOM CAM with OV2640
 * 
 * Features:
 * - Real-time frame streaming to Datum server
 * - Command-based stream control (start/stop/capture)
 * - Automatic WiFi reconnection
 * - Configurable resolution and quality
 * 
 * Author: Datum IoT Platform
 * License: MIT
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include "Arduino.h"

// ============================================================================
// Board Selection - Uncomment your board
// ============================================================================

// #define CAMERA_MODEL_AI_THINKER      // ESP32-CAM AI-Thinker (OV2640)
#define CAMERA_MODEL_ESP32S3_CAM        // ESP32-S3-CAM (OV2640 or OV3660)
// #define CAMERA_MODEL_FREENOVE_S3      // Freenove ESP32-S3 WROOM CAM

// ============================================================================
// Sensor Selection (for ESP32-S3 boards)
// ============================================================================
#define CAMERA_SENSOR_OV2640  // Default 2MP sensor
// #define CAMERA_SENSOR_OV3660  // 3MP sensor upgrade

// ============================================================================
// Configuration
// ============================================================================

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Datum server configuration
const char* serverURL = "http://your-server.com";  // No trailing slash
const char* deviceID = "esp32-cam-01";
const char* apiKey = "your-device-api-key";  // Device API key (sk_xxx or dk_xxx)

// Stream settings
#define STREAM_FPS 10            // Frames per second (1-30)
#define JPEG_QUALITY 12          // 0-63, lower = better quality, more bandwidth
#define FRAME_SIZE FRAMESIZE_VGA // Default resolution

// ============================================================================
// Pin Definitions - AI-Thinker ESP32-CAM
// ============================================================================
#if defined(CAMERA_MODEL_AI_THINKER)
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
  #define LED_GPIO_NUM       4  // Flash LED

// ============================================================================
// Pin Definitions - ESP32-S3-CAM (e.g., Seeed XIAO, Waveshare)
// ============================================================================
#elif defined(CAMERA_MODEL_ESP32S3_CAM)
  #define PWDN_GPIO_NUM     -1  // Not used on most S3 boards
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM     10
  #define SIOD_GPIO_NUM     40
  #define SIOC_GPIO_NUM     39
  #define Y9_GPIO_NUM       48
  #define Y8_GPIO_NUM       11
  #define Y7_GPIO_NUM       12
  #define Y6_GPIO_NUM       14
  #define Y5_GPIO_NUM       16
  #define Y4_GPIO_NUM       18
  #define Y3_GPIO_NUM       17
  #define Y2_GPIO_NUM       15
  #define VSYNC_GPIO_NUM    38
  #define HREF_GPIO_NUM     47
  #define PCLK_GPIO_NUM     13
  #define LED_GPIO_NUM      21  // Built-in LED (varies by board)

// ============================================================================
// Pin Definitions - Freenove ESP32-S3 WROOM CAM
// ============================================================================
#elif defined(CAMERA_MODEL_FREENOVE_S3)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM     15
  #define SIOD_GPIO_NUM      4
  #define SIOC_GPIO_NUM      5
  #define Y9_GPIO_NUM       16
  #define Y8_GPIO_NUM       17
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       12
  #define Y5_GPIO_NUM       10
  #define Y4_GPIO_NUM        8
  #define Y3_GPIO_NUM        9
  #define Y2_GPIO_NUM       11
  #define VSYNC_GPIO_NUM     6
  #define HREF_GPIO_NUM      7
  #define PCLK_GPIO_NUM     13
  #define LED_GPIO_NUM      48  // RGB LED

#else
  #error "Camera model not selected. Please define one of the CAMERA_MODEL_* macros."
#endif

// ============================================================================
// Global Variables
// ============================================================================
bool streaming = false;
unsigned long lastFrameTime = 0;
unsigned long commandCheckInterval = 5000;  // Check for commands every 5 seconds
unsigned long lastCommandCheck = 0;
unsigned long wifiReconnectInterval = 30000;  // WiFi reconnect interval
unsigned long lastWifiCheck = 0;
int frameCount = 0;
int errorCount = 0;

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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;  // Reduce latency
  
  // Frame buffer configuration
  if (psramFound()) {
    Serial.println("PSRAM found, using high resolution");
    config.frame_size = FRAME_SIZE;
    config.jpeg_quality = JPEG_QUALITY;
    config.fb_count = 2;  // Double buffering for smooth streaming
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    Serial.println("No PSRAM, using lower resolution");
    config.frame_size = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 15;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  // Get sensor reference
  sensor_t* s = esp_camera_sensor_get();
  if (s == NULL) {
    Serial.println("Failed to get camera sensor");
    return false;
  }

  // Print sensor info
  Serial.printf("Camera PID: 0x%02X VER: 0x%02X MIDL: 0x%02X MIDH: 0x%02X\n",
                s->id.PID, s->id.VER, s->id.MIDL, s->id.MIDH);

  // Common sensor settings
  s->set_brightness(s, 0);      // -2 to 2
  s->set_contrast(s, 0);        // -2 to 2
  s->set_saturation(s, 0);      // -2 to 2
  s->set_whitebal(s, 1);        // Auto white balance
  s->set_awb_gain(s, 1);        // AWB gain
  s->set_exposure_ctrl(s, 1);   // Auto exposure
  s->set_gain_ctrl(s, 1);       // Auto gain
  s->set_hmirror(s, 0);         // Horizontal mirror
  s->set_vflip(s, 0);           // Vertical flip

#if defined(CAMERA_SENSOR_OV3660)
  // OV3660 specific settings (3MP sensor)
  if (s->id.PID == OV3660_PID) {
    Serial.println("OV3660 sensor detected");
    s->set_vflip(s, 1);         // OV3660 is typically mounted upside down
    s->set_brightness(s, 1);    // Slightly brighter for OV3660
    s->set_saturation(s, -1);   // OV3660 tends to oversaturate
    // Higher resolution for 3MP sensor
    if (psramFound()) {
      s->set_framesize(s, FRAMESIZE_UXGA);  // 1600x1200
    }
  }
#endif

  // OV2640 specific settings (2MP sensor)
  if (s->id.PID == OV2640_PID) {
    Serial.println("OV2640 sensor detected");
    s->set_special_effect(s, 0);  // No effect
    s->set_raw_gma(s, 1);         // Gamma correction
    s->set_lenc(s, 1);            // Lens correction
    s->set_dcw(s, 1);             // Downsize enable
  }

  Serial.println("Camera initialized successfully");
  return true;
}

// ============================================================================
// WiFi Connection with Reconnection Support
// ============================================================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void checkWiFiConnection() {
  if (millis() - lastWifiCheck < wifiReconnectInterval) {
    return;
  }
  lastWifiCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.disconnect();
    delay(1000);
    connectWiFi();
  }
}

// ============================================================================
// Command Polling
// ============================================================================
void checkCommands() {
  if (millis() - lastCommandCheck < commandCheckInterval) {
    return;
  }
  lastCommandCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  String url = String(serverURL) + "/device/" + deviceID + "/commands";
  
  httpCommand.begin(url);
  httpCommand.addHeader("Authorization", String("Bearer ") + apiKey);
  httpCommand.setTimeout(5000);
  
  int httpCode = httpCommand.GET();
  
  if (httpCode == 200) {
    String payload = httpCommand.getString();
    Serial.println("Commands: " + payload);
    
    // Parse commands (simple string matching for reliability)
    if (payload.indexOf("\"action\":\"start-stream\"") > 0) {
      Serial.println(">>> Starting stream");
      streaming = true;
      frameCount = 0;
      errorCount = 0;
    }
    else if (payload.indexOf("\"action\":\"stop-stream\"") > 0) {
      Serial.println(">>> Stopping stream");
      streaming = false;
    }
    else if (payload.indexOf("\"action\":\"capture-frame\"") > 0) {
      Serial.println(">>> Capturing single frame");
      sendFrame();
    }
    else if (payload.indexOf("\"action\":\"set-quality\"") > 0) {
      // Extract quality value
      int qualityStart = payload.indexOf("\"quality\":") + 10;
      int quality = payload.substring(qualityStart).toInt();
      if (quality >= 0 && quality <= 63) {
        sensor_t* s = esp_camera_sensor_get();
        s->set_quality(s, quality);
        Serial.printf(">>> Set quality to %d\n", quality);
      }
    }
    else if (payload.indexOf("\"action\":\"set-resolution\"") > 0) {
      // Handle resolution change
      sensor_t* s = esp_camera_sensor_get();
      if (payload.indexOf("\"QVGA\"") > 0) {
        s->set_framesize(s, FRAMESIZE_QVGA);
      } else if (payload.indexOf("\"VGA\"") > 0) {
        s->set_framesize(s, FRAMESIZE_VGA);
      } else if (payload.indexOf("\"SVGA\"") > 0) {
        s->set_framesize(s, FRAMESIZE_SVGA);
      } else if (payload.indexOf("\"XGA\"") > 0) {
        s->set_framesize(s, FRAMESIZE_XGA);
      }
      Serial.println(">>> Resolution changed");
    }
    else if (payload.indexOf("\"action\":\"flash-led\"") > 0) {
      #ifdef LED_GPIO_NUM
      pinMode(LED_GPIO_NUM, OUTPUT);
      digitalWrite(LED_GPIO_NUM, HIGH);
      delay(100);
      digitalWrite(LED_GPIO_NUM, LOW);
      Serial.println(">>> Flash LED triggered");
      #endif
    }
    else if (payload.indexOf("\"action\":\"restart\"") > 0) {
      Serial.println(">>> Restarting device");
      delay(1000);
      ESP.restart();
    }
  }
  else if (httpCode < 0) {
    Serial.printf("Command check failed: %s\n", httpCommand.errorToString(httpCode).c_str());
  }
  
  httpCommand.end();
}

// ============================================================================
// Frame Upload
// ============================================================================
bool sendFrame() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    errorCount++;
    return false;
  }

  // Validate JPEG
  if (fb->len < 100 || fb->buf[0] != 0xFF || fb->buf[1] != 0xD8) {
    Serial.println("Invalid JPEG frame, skipping");
    esp_camera_fb_return(fb);
    errorCount++;
    return false;
  }

  String url = String(serverURL) + "/device/" + deviceID + "/stream/frame";
  
  httpStream.begin(url);
  httpStream.addHeader("Authorization", String("Bearer ") + apiKey);
  httpStream.addHeader("Content-Type", "image/jpeg");
  httpStream.setTimeout(5000);
  
  int httpCode = httpStream.POST(fb->buf, fb->len);
  
  bool success = (httpCode >= 200 && httpCode < 300);
  
  if (success) {
    frameCount++;
    if (frameCount % 100 == 0) {
      Serial.printf("Frames sent: %d, Size: %d bytes, RSSI: %d dBm\n", 
                    frameCount, fb->len, WiFi.RSSI());
    }
  } else {
    errorCount++;
    Serial.printf("Frame failed: HTTP %d - %s\n", 
                  httpCode, httpStream.errorToString(httpCode).c_str());
    
    // Stop streaming after too many errors
    if (errorCount > 10) {
      Serial.println("Too many errors, stopping stream");
      streaming = false;
      errorCount = 0;
    }
  }
  
  httpStream.end();
  esp_camera_fb_return(fb);
  
  return success;
}

// ============================================================================
// Streaming Loop with Frame Rate Control
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
  delay(1000);  // Wait for serial to initialize
  
  Serial.println("\n");
  Serial.println("========================================");
  Serial.println("   ESP32 Camera Streamer for Datum");
  Serial.println("========================================");
  Serial.printf("SDK Version: %s\n", ESP.getSdkVersion());
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("PSRAM Size: %d bytes\n", ESP.getPsramSize());

  // Initialize camera
  if (!initCamera()) {
    Serial.println("FATAL: Camera initialization failed!");
    Serial.println("Check camera connection and restart");
    while (1) {
      delay(1000);
    }
  }

  // Initialize LED
  #ifdef LED_GPIO_NUM
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);
  #endif

  // Connect to WiFi
  connectWiFi();
  
  // Print ready message
  Serial.println("\n========================================");
  Serial.println("   Device Ready!");
  Serial.println("========================================");
  Serial.printf("Server: %s\n", serverURL);
  Serial.printf("Device ID: %s\n", deviceID);
  Serial.printf("Stream FPS: %d\n", STREAM_FPS);
  Serial.printf("JPEG Quality: %d\n", JPEG_QUALITY);
  Serial.println("\nCommands:");
  Serial.println("  datumctl command send " + String(deviceID) + " start-stream");
  Serial.println("  datumctl command send " + String(deviceID) + " stop-stream");
  Serial.println("  datumctl command send " + String(deviceID) + " capture-frame");
  Serial.println("  datumctl command send " + String(deviceID) + " flash-led");
  Serial.println("  datumctl command send " + String(deviceID) + " restart");
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
  checkWiFiConnection();
  checkCommands();
  streamLoop();
  
  // Small delay to prevent watchdog timeout
  delay(1);
}
