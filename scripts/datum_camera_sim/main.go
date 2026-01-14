package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"image"
	"image/color"
	"image/draw"
	"image/jpeg"
	"log"
	"math/rand"
	"net/http"
	"os"
	"sync"
	"time"

	"golang.org/x/image/font"
	"golang.org/x/image/font/basicfont"
	"golang.org/x/image/math/fixed"
)

var (
	serverURL   string
	simDeviceID string
	simApiKey   string
	simFPS      int
)

// Simulator State
type SimState struct {
	StreamResolution   string `json:"stream_resolution"`
	SnapshotResolution string `json:"snapshot_resolution"`
	LedOn              bool   `json:"led_on"`
	LedBrightness      int    `json:"led_brightness"`
	LedColor           string `json:"led_color"`
	MotionEnabled      bool   `json:"motion_enabled"`
	HMirror            bool   `json:"hmirror"`
	VFlip              bool   `json:"vflip"`
	mu                 sync.Mutex
}

var state = SimState{
	StreamResolution:   "VGA",
	SnapshotResolution: "UXGA",
	LedOn:              false,
	LedBrightness:      50,
	LedColor:           "#ff0000",
	MotionEnabled:      true,
	HMirror:            false,
	VFlip:              false,
}

func main() {
	flag.StringVar(&serverURL, "server", "http://localhost:8000", "Datum server URL")
	flag.StringVar(&simDeviceID, "device-id", "", "Target Device ID (required)")
	flag.StringVar(&simApiKey, "api-key", "", "Device API Key (required)")
	flag.IntVar(&simFPS, "fps", 1, "Frames Per Second for stream")
	flag.Parse()

	if simDeviceID == "" || simApiKey == "" {
		flag.Usage()
		os.Exit(1)
	}

	// Heuristic check for swapped arguments
	if len(simDeviceID) > 3 && simDeviceID[:3] == "sk_" {
		log.Printf("⚠️ WARNING: device-id '%s' starts with 'sk_'. Did you swap --device-id and --api-key?", simDeviceID)
	}
	if len(simApiKey) > 4 && simApiKey[:4] == "dev_" {
		log.Printf("⚠️ WARNING: api-key '%s' starts with 'dev_'. Did you swap --device-id and --api-key?", simApiKey)
	}

	log.Printf("🚀 Starting Standalone Camera Simulator")
	log.Printf("📱 Device: %s", simDeviceID)
	log.Printf("📡 Server: %s", serverURL)
	log.Printf("📸 FPS:    %d", simFPS)

	// Send Thing Description
	sendThingDescription()

	// Initial State Sync
	log.Printf("ℹ️ Initial State: Res=%s LED=%v Bri=%d%% Motion=%v Col=%s",
		state.StreamResolution, state.LedOn, state.LedBrightness, state.MotionEnabled, state.LedColor)

	// Start Telemetry Routine
	go runTelemetryLoop()

	// Start Command Polling Routine
	go runCommandPollingLoop()

	// Start Video Stream Routine (Blocking)
	runStreamLoop()
}

func sendThingDescription() {
	td := map[string]interface{}{
		"@context": "https://www.w3.org/2019/wot/td/v1",
		"id":       "urn:dev:" + simDeviceID,
		"title":    "Simulated ESP32 Camera",
		"securityDefinitions": map[string]interface{}{
			"bearer_sec": map[string]interface{}{
				"scheme": "bearer",
			},
		},
		"security": []string{"bearer_sec"},
		"properties": map[string]interface{}{
			// Read-Only Telemetry
			"temperature": map[string]interface{}{
				"title":    "Temperature",
				"type":     "number",
				"unit":     "Celsius",
				"readOnly": true,
			},
			"humidity": map[string]interface{}{
				"title":    "Humidity",
				"type":     "number",
				"unit":     "%",
				"readOnly": true,
			},
			"rssi": map[string]interface{}{
				"title":    "WiFi Signal",
				"type":     "integer",
				"unit":     "dBm",
				"readOnly": true,
			},

			// Read-Write Settings
			"stream_resolution": map[string]interface{}{
				"title":    "Stream Resolution",
				"type":     "string",
				"enum":     []string{"UXGA", "SXGA", "XGA", "SVGA", "VGA", "CIF", "QVGA", "HQVGA", "QQVGA"},
				"readOnly": false,
			},
			"snapshot_resolution": map[string]interface{}{
				"title":    "Snapshot Resolution",
				"type":     "string",
				"enum":     []string{"UXGA", "SXGA", "XGA", "SVGA", "VGA", "CIF", "QVGA", "HQVGA", "QQVGA"},
				"readOnly": false,
			},
			"led_on": map[string]interface{}{
				"title":     "LED Flash",
				"type":      "boolean",
				"ui:widget": "switch",
				"readOnly":  false,
			},
			"led_brightness": map[string]interface{}{
				"title":     "LED Brightness",
				"type":      "integer",
				"minimum":   0,
				"maximum":   100,
				"unit":      "%",
				"ui:widget": "slider",
				"readOnly":  false,
			},
			"motion_enabled": map[string]interface{}{
				"title":     "Motion Detection",
				"type":      "boolean",
				"ui:widget": "switch",
				"readOnly":  false,
			},
			"led_color": map[string]interface{}{
				"title":     "LED Color",
				"type":      "string",
				"ui:widget": "color",
				"readOnly":  false,
			},
			"hmirror": map[string]interface{}{
				"title":     "Horizontal Mirror",
				"type":      "boolean",
				"ui:widget": "switch",
				"readOnly":  false,
			},
			"vflip": map[string]interface{}{
				"title":     "Vertical Flip",
				"type":      "boolean",
				"ui:widget": "switch",
				"readOnly":  false,
			},
		},
		"actions": map[string]interface{}{
			"snap": map[string]interface{}{
				"title": "Take Snapshot",
				"input": map[string]interface{}{
					"type": "object",
					"properties": map[string]interface{}{
						"resolution": map[string]interface{}{
							"type": "string",
							"enum": []string{"UXGA", "VGA"},
						},
					},
				},
			},
			"stream": map[string]interface{}{
				"title": "Control Stream",
				"input": map[string]interface{}{
					"type": "object",
					"properties": map[string]interface{}{
						"state": map[string]interface{}{
							"type": "string",
							"enum": []string{"on", "off"},
						},
					},
				},
			},
			"update_firmware": map[string]interface{}{
				"title": "Update Firmware",
				"input": map[string]interface{}{
					"type": "object",
					"properties": map[string]interface{}{
						"url": map[string]interface{}{
							"title": "Firmware URL",
							"type":  "string",
						},
					},
				},
			},
			"restart": map[string]interface{}{
				"title": "Restart Device",
			},
		},
	}

	url := fmt.Sprintf("%s/dev/%s/thing-description", serverURL, simDeviceID)
	jsonData, _ := json.Marshal(td)

	req, _ := http.NewRequest("PUT", url, bytes.NewBuffer(jsonData))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+simApiKey)

	client := &http.Client{Timeout: 5 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		log.Printf("❌ Failed to send Thing Description: %v", err)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode == 200 {
		log.Printf("✅ Thing Description Sent")
	} else {
		log.Printf("⚠️ Failed to send TD: %s", resp.Status)
	}
}

func runTelemetryLoop() {
	// Send initial data immediately
	sendTelemetry()

	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		sendTelemetry()
	}
}

func sendTelemetry() {
	// Snapshot state for consistent logging
	state.mu.Lock()
	motionActive := state.MotionEnabled
	state.mu.Unlock()

	// Generate random data
	temp := 20.0 + rand.Float64()*10.0
	hum := 40.0 + rand.Float64()*20.0

	// Only generate motion events if enabled
	motion := false
	if motionActive {
		motion = rand.Intn(10) > 8 // 10% chance
	}

	// Include all settings in telemetry (Shadow State)
	payload := map[string]interface{}{
		"temperature":         temp,
		"humidity":            hum,
		"motion":              motion,
		"rssi":                -50 - rand.Intn(20),
		"stream_resolution":   state.StreamResolution,
		"snapshot_resolution": state.SnapshotResolution,
		"led_on":              state.LedOn,
		"led_brightness":      state.LedBrightness,
		"led_color":           state.LedColor,
		"motion_enabled":      state.MotionEnabled,
		"hmirror":             state.HMirror,
		"vflip":               state.VFlip,
	}

	if motion {
		log.Println("🏃 Motion Detected! (Simulated)")
	}

	sendData(payload)
}

func sendData(payload map[string]interface{}) {
	url := fmt.Sprintf("%s/dev/%s/data", serverURL, simDeviceID)
	jsonData, _ := json.Marshal(payload)

	req, _ := http.NewRequest("POST", url, bytes.NewBuffer(jsonData))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+simApiKey)

	client := &http.Client{Timeout: 5 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		log.Printf("❌ Telemetry Update Failed: %v", err)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		log.Printf("⚠️ Telemetry Error: %s", resp.Status)
	} else {
		// Concise log
		log.Printf("✅ Telemetry: Temp=%.1f Hum=%.1f", payload["temperature"], payload["humidity"])
	}
}

func runCommandPollingLoop() {
	client := &http.Client{Timeout: 40 * time.Second} // Long poll timeout

	for {
		url := fmt.Sprintf("%s/dev/%s/cmd/poll?wait=30", serverURL, simDeviceID)
		req, _ := http.NewRequest("GET", url, nil)
		req.Header.Set("Authorization", "Bearer "+simApiKey)

		resp, err := client.Do(req)
		if err != nil {
			log.Printf("❌ Command Poll Failed: %v", err)
			time.Sleep(5 * time.Second)
			continue
		}

		if resp.StatusCode == 200 {
			var result struct {
				Commands []map[string]interface{} `json:"commands"`
			}
			if err := json.NewDecoder(resp.Body).Decode(&result); err == nil && len(result.Commands) > 0 {
				for _, c := range result.Commands {
					handleCommand(c)
				}
			}
		}
		resp.Body.Close()
		time.Sleep(1 * time.Second) // Small backoff
	}
}

func handleCommand(cmd map[string]interface{}) {
	action, ok := cmd["action"].(string)
	if !ok {
		return
	}

	params, _ := cmd["params"].(map[string]interface{})
	log.Printf("🔔 Received Command: %s Params: %v", action, params)

	if action == "update_settings" && params != nil {
		state.mu.Lock()
		defer state.mu.Unlock()

		if v, ok := params["stream_resolution"].(string); ok {
			state.StreamResolution = v
			log.Printf("   -> Set Stream Resolution: %s", v)
		}
		if v, ok := params["snapshot_resolution"].(string); ok {
			state.SnapshotResolution = v
			log.Printf("   -> Set Snapshot Resolution: %s", v)
		}
		if v, ok := params["led_on"].(bool); ok {
			state.LedOn = v
			log.Printf("   -> Set LED: %v", v)
		}
		if v, ok := params["led_brightness"].(float64); ok {
			state.LedBrightness = int(v) // JSON numbers are float64
			log.Printf("   -> Set Brightness: %d%%", state.LedBrightness)
		}
		if v, ok := params["motion_enabled"].(bool); ok {
			state.MotionEnabled = v
			log.Printf("   -> Set Motion Enabled: %v", v)
		}
		if v, ok := params["led_color"].(string); ok {
			state.LedColor = v
			log.Printf("   -> Set Color: %s", v)
		}
		if v, ok := params["hmirror"].(bool); ok {
			state.HMirror = v
			log.Printf("   -> Set H-Mirror: %v", v)
		}
		if v, ok := params["vflip"].(bool); ok {
			state.VFlip = v
			log.Printf("   -> Set V-Flip: %v", v)
		}
	} else if action == "snap" {
		log.Println("📸 Action: SNAPSHOT requested")
	} else if action == "stream" {
		log.Printf("🎥 Action: STREAM control: %v", params)
	} else if action == "update_firmware" {
		log.Printf("📦 Action: FIRMWARE UPDATE requested: %v", params)
	} else if action == "restart" {
		log.Println("♻️  Action: REBOOT requested")
		time.Sleep(2 * time.Second)
		log.Println("🚀 Restarted!")
	}

	// Send Acknowledgement
	if cmdID, ok := cmd["command_id"].(string); ok {
		sendAck(cmdID)
	} else if cmdID, ok := cmd["id"].(string); ok {
		// Fallback
		sendAck(cmdID)
	}
}

func sendAck(cmdID string) {
	url := fmt.Sprintf("%s/dev/%s/cmd/%s/ack", serverURL, simDeviceID, cmdID)

	payload := map[string]interface{}{
		"status": "executed",
		"result": map[string]string{"msg": "Simulated execution success"},
	}
	jsonData, _ := json.Marshal(payload)

	req, _ := http.NewRequest("POST", url, bytes.NewBuffer(jsonData))
	req.Header.Set("Authorization", "Bearer "+simApiKey)
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{Timeout: 5 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		log.Printf("❌ Failed to ACK command %s: %v", cmdID, err)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode == 200 {
		log.Printf("✅ ACK Sent for Command: %s", cmdID)
	} else {
		log.Printf("⚠️ Failed to ACK Command %s: %s", cmdID, resp.Status)
	}
}

func runStreamLoop() {
	ticker := time.NewTicker(time.Duration(1000/simFPS) * time.Millisecond)
	defer ticker.Stop()

	frameID := 0
	for range ticker.C {
		frameID++
		imgData := generateFrame(frameID)

		uploadFrame(imgData)
	}
}

func generateFrame(id int) []byte {
	// Frame size based on resolution (simplified)
	width, height := 320, 240
	if state.StreamResolution == "SVGA" {
		width, height = 800, 600
	} else if state.StreamResolution == "VGA" {
		width, height = 640, 480
	}

	img := image.NewRGBA(image.Rect(0, 0, width, height))

	// Background color (Affected by LED Color if LED is ON!)
	var col color.RGBA
	if state.LedOn {
		// Parse Hex Color
		c, err := parseHexColor(state.LedColor)
		if err == nil {
			// Mix with time-based cycle
			col = c
		} else {
			col = color.RGBA{255, 255, 255, 255}
		}
		// Apply brightness
		col.R = uint8(float64(col.R) * (float64(state.LedBrightness) / 100.0))
		col.G = uint8(float64(col.G) * (float64(state.LedBrightness) / 100.0))
		col.B = uint8(float64(col.B) * (float64(state.LedBrightness) / 100.0))
		col.A = 255
	} else {
		// Default Dark cycling background
		r := uint8((id * 5) % 100)
		g := uint8((id * 3) % 100)
		b := uint8((id * 7) % 100)
		col = color.RGBA{r, g, b, 255}
	}

	draw.Draw(img, img.Bounds(), &image.Uniform{col}, image.Point{}, draw.Src)

	// Invert logic for Flip/Mirror would go here (complex for raw bytes)
	// For simulation, we just toggle text position
	x, y := 10, 20
	if state.HMirror {
		x = width - 150
	}
	if state.VFlip {
		y = height - 40
	}

	// Add Labels
	addLabel(img, x, y, fmt.Sprintf("SIM-CAM: %s", simDeviceID))
	addLabel(img, x, y+20, fmt.Sprintf("Res: %s | FPS: %d", state.StreamResolution, simFPS))
	addLabel(img, x, y+40, fmt.Sprintf("Motion: %v | LED: %v", state.MotionEnabled, state.LedOn))

	var buf bytes.Buffer
	jpeg.Encode(&buf, img, &jpeg.Options{Quality: 70})
	return buf.Bytes()
}

func parseHexColor(s string) (c color.RGBA, err error) {
	c.A = 0xff
	if s[0] != '#' {
		return c, fmt.Errorf("invalid format")
	}
	hexToByte := func(b byte) byte {
		switch {
		case b >= '0' && b <= '9':
			return b - '0'
		case b >= 'a' && b <= 'f':
			return b - 'a' + 10
		case b >= 'A' && b <= 'F':
			return b - 'A' + 10
		}
		return 0
	}
	switch len(s) {
	case 7:
		c.R = hexToByte(s[1])<<4 + hexToByte(s[2])
		c.G = hexToByte(s[3])<<4 + hexToByte(s[4])
		c.B = hexToByte(s[5])<<4 + hexToByte(s[6])
	case 4:
		c.R = hexToByte(s[1]) * 17
		c.G = hexToByte(s[2]) * 17
		c.B = hexToByte(s[3]) * 17
	default:
		return c, fmt.Errorf("invalid length")
	}
	return
}

func addLabel(img *image.RGBA, x, y int, label string) {
	col := color.RGBA{255, 255, 255, 255}
	point := fixed.Point26_6{X: fixed.I(x), Y: fixed.I(y)}
	d := &font.Drawer{
		Dst:  img,
		Src:  image.NewUniform(col),
		Face: basicfont.Face7x13,
		Dot:  point,
	}
	d.DrawString(label)
}

func uploadFrame(data []byte) {
	url := fmt.Sprintf("%s/dev/%s/stream/frame", serverURL, simDeviceID)
	req, _ := http.NewRequest("POST", url, bytes.NewBuffer(data))
	req.Header.Set("Content-Type", "image/jpeg")
	req.Header.Set("Authorization", "Bearer "+simApiKey)

	client := &http.Client{Timeout: 2 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		if rand.Intn(20) == 0 { // Log rarer
			log.Printf("❌ Stream Upload Failed: %v", err)
		}
		return
	}
	defer resp.Body.Close()
}
