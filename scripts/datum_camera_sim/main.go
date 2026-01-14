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
			"motion": map[string]interface{}{
				"title":    "Motion Detection",
				"type":     "boolean",
				"readOnly": true,
			},
			"led_color": map[string]interface{}{
				"title":     "LED Color",
				"type":      "string",
				"ui:widget": "color",
				"readOnly":  false,
			},
		},
		"actions": map[string]interface{}{
			"update_settings": map[string]interface{}{
				"title": "Update Settings",
				"input": map[string]interface{}{
					"type": "object",
					"properties": map[string]interface{}{
						"led_color": map[string]interface{}{"type": "string"},
					},
				},
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
	// Generate random data
	temp := 20.0 + rand.Float64()*10.0
	hum := 40.0 + rand.Float64()*20.0
	motion := rand.Intn(10) > 8 // 10% chance of motion

	payload := map[string]interface{}{
		"temperature": temp,
		"humidity":    hum,
		"motion":      motion,
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
		log.Printf("✅ Telemetry Sent: Temp=%.1f Hum=%.1f", payload["temperature"], payload["humidity"])
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
			var cmds []map[string]interface{}
			if err := json.NewDecoder(resp.Body).Decode(&cmds); err == nil && len(cmds) > 0 {
				for _, c := range cmds {
					log.Printf("🔔 Received Command: %v", c)
				}
			}
		}
		resp.Body.Close()
		time.Sleep(1 * time.Second) // Small backoff
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
	width, height := 320, 240
	img := image.NewRGBA(image.Rect(0, 0, width, height))

	// Background color cycles
	r := uint8((id * 5) % 255)
	g := uint8((id * 3) % 255)
	b := uint8((id * 7) % 255)
	col := color.RGBA{r, g, b, 255}
	draw.Draw(img, img.Bounds(), &image.Uniform{col}, image.Point{}, draw.Src)

	// Add Labels
	addLabel(img, 10, 20, fmt.Sprintf("SIM-CAM: %s", simDeviceID))
	addLabel(img, 10, 40, fmt.Sprintf("Frame: %d", id))
	addLabel(img, 10, 60, time.Now().Format("15:04:05"))

	var buf bytes.Buffer
	jpeg.Encode(&buf, img, &jpeg.Options{Quality: 70})
	return buf.Bytes()
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
		if rand.Intn(10) == 0 { // Log only occasionally
			log.Printf("❌ Stream Upload Failed: %v", err)
		}
		return
	}
	defer resp.Body.Close()
}
