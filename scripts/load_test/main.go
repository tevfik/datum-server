package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net/http"
	"os"
	"sync"
	"sync/atomic"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
)

// DeviceCreds holds the credentials for a test device
type DeviceCreds struct {
	ID         string
	Key        string
	MQTTClient mqtt.Client
}

// Stats tracks the load test results
type Stats struct {
	TotalRequests int64
	Success       int64
	Errors        int64
	BytesSent     int64
	TotalLatency  int64 // Microseconds
}

var stats Stats

func main() {
	baseURL := flag.String("url", "http://localhost:8080", "Target Server URL (HTTP)")
	mqttBroker := flag.String("mqtt-broker", "tcp://localhost:1883", "MQTT Broker URL")
	protocol := flag.String("proto", "http", "Protocol: http, mqtt, mixed")
	numUsers := flag.Int("users", 5, "Number of users to simulate")
	devicesPer := flag.Int("devices", 2, "Number of devices per user")
	duration := flag.Duration("duration", 30*time.Second, "Test duration")

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "\nDatum Load Test Runner\n")
		fmt.Fprintf(os.Stderr, "----------------------\n")
		fmt.Fprintf(os.Stderr, "Simulates device traffic to stress test the Datum Server.\n\n")
		fmt.Fprintf(os.Stderr, "Usage: %s [options]\n\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "Options:\n")
		flag.PrintDefaults()
		fmt.Fprintf(os.Stderr, "\nExamples:\n")
		fmt.Fprintf(os.Stderr, "  %s -proto mqtt -users 10\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "  %s -url http://localhost:8080 -proto mixed\n\n", os.Args[0])
	}

	flag.Parse()

	log.Printf("Starting Load Test against %s (Proto: %s)", *baseURL, *protocol)
	if *protocol != "http" {
		log.Printf("MQTT Broker: %s", *mqttBroker)
	}
	log.Printf("Setup: %d Users, %d Devices/User (Total %d devices)", *numUsers, *devicesPer, *numUsers**devicesPer)

	client := &http.Client{
		Timeout: 5 * time.Second,
		Transport: &http.Transport{
			MaxIdleConns:        2000,
			MaxIdleConnsPerHost: 2000,
			IdleConnTimeout:     90 * time.Second,
		},
	}

	// 1. Setup Phase
	log.Println("--- Setup Phase: Registering Users & Devices (HTTP) ---")
	devices := setupTestEntities(client, *baseURL, *numUsers, *devicesPer)
	if len(devices) == 0 {
		log.Fatal("No devices created. Exiting.")
	}

	// Connect MQTT clients if needed
	if *protocol == "mqtt" || *protocol == "mixed" {
		log.Println("--- Connecting MQTT Clients ---")
		connectMQTTClients(devices, *mqttBroker)
	}

	log.Printf("Successfully registered %d devices.", len(devices))

	// 2. Load Phase
	log.Println("--- Load Phase: Starting Traffic ---")
	log.Println(" Sending random telemetry (temp, humidity) every ~100ms per device...")

	start := time.Now()
	var wg sync.WaitGroup

	stopCh := make(chan struct{})
	time.AfterFunc(*duration, func() {
		close(stopCh)
	})

	for _, dev := range devices {
		wg.Add(1)
		go func(d DeviceCreds) {
			defer wg.Done()
			// Close MQTT on exit
			defer func() {
				if d.MQTTClient != nil && d.MQTTClient.IsConnected() {
					d.MQTTClient.Disconnect(250)
				}
			}()

			ticker := time.NewTicker(time.Duration(100+rand.Intn(50)) * time.Millisecond) // Jitter
			defer ticker.Stop()

			for {
				select {
				case <-stopCh:
					return
				case <-ticker.C:
					// Determine protocol for this message
					useMQTT := false
					if *protocol == "mqtt" {
						useMQTT = true
					} else if *protocol == "mixed" {
						useMQTT = (rand.Float32() < 0.5)
					}

					if useMQTT {
						sendTelemetryMQTT(d)
					} else {
						sendTelemetryHTTP(client, *baseURL, d)
					}
				}
			}
		}(dev)
	}

	wg.Wait()
	elapsed := time.Since(start)

	// 3. Report Phase
	printReport(elapsed)
}

func connectMQTTClients(devices []DeviceCreds, broker string) {
	// To avoid flooding the broker with connects, limit concurrency
	sem := make(chan bool, 50) // Max 50 concurrent connects
	var connected int32

	var wg sync.WaitGroup
	for i := range devices {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()
			sem <- true
			defer func() { <-sem }()

			dev := &devices[idx]
			opts := mqtt.NewClientOptions()
			opts.AddBroker(broker)
			opts.SetClientID(dev.ID)
			opts.SetUsername(dev.ID)
			opts.SetPassword(dev.Key)
			opts.SetCleanSession(true)
			opts.SetConnectTimeout(5 * time.Second)

			c := mqtt.NewClient(opts)
			if token := c.Connect(); token.Wait() && token.Error() != nil {
				log.Printf("MQTT Connect Error (%s): %v", dev.ID, token.Error())
			} else {
				dev.MQTTClient = c
				atomic.AddInt32(&connected, 1)
			}
		}(i)
	}
	wg.Wait()
	log.Printf("Connected %d/%d MQTT clients", connected, len(devices))
}

func setupTestEntities(client *http.Client, baseURL string, numUsers, devicesPer int) []DeviceCreds {
	var devices []DeviceCreds

	// Pre-seed random for unique emails
	rnd := rand.New(rand.NewSource(time.Now().UnixNano()))

	for u := 0; u < numUsers; u++ {
		email := fmt.Sprintf("loaduser_%d_%d@test.com", time.Now().Unix(), rnd.Intn(100000))
		password := "Password123!"

		// Register
		regBody := map[string]string{"email": email, "password": password}
		status, _ := postJSON(client, baseURL+"/auth/register", regBody, nil)
		if status != 201 && status != 409 {
			log.Printf("Failed to register user %s: status %d", email, status)
			continue
		}

		// Login
		var loginResp map[string]interface{}
		status, _ = postJSON(client, baseURL+"/auth/login", regBody, &loginResp)
		if status != 200 {
			log.Printf("Failed to login user %s: status %d", email, status)
			continue
		}

		var token string
		if t, ok := loginResp["token"].(string); ok {
			token = t
		} else {
			log.Printf("No token in login response for %s", email)
			continue
		}

		// Create Devices
		for d := 0; d < devicesPer; d++ {
			devName := fmt.Sprintf("LoadDevice_%d", d)
			devBody := map[string]string{
				"name": devName,
				"type": "sensor",
			}
			headers := map[string]string{"Authorization": "Bearer " + token}

			var devResp map[string]interface{}
			status, _ := postJSONWithHeaders(client, baseURL+"/devices", devBody, &devResp, headers)
			if status != 201 {
				log.Printf("Failed to create device %s: status %d", devName, status)
				continue
			}

			key, kOk := devResp["api_key"].(string)
			id, iOk := devResp["device_id"].(string)

			if kOk && iOk {
				devices = append(devices, DeviceCreds{ID: id, Key: key})
			} else {
				log.Printf("Invalid device response: %v", devResp)
			}
		}
		// Small delay to avoid overwhelming DB during setup
		time.Sleep(10 * time.Millisecond)
	}
	return devices
}

func sendTelemetryHTTP(client *http.Client, baseURL string, dev DeviceCreds) {
	url := fmt.Sprintf("%s/data/%s", baseURL, dev.ID)

	payload := map[string]interface{}{
		"temperature": 20.0 + rand.Float64()*10.0,
		"humidity":    40.0 + rand.Float64()*20.0,
		"battery":     rand.Intn(100),
		"status":      "testing_http",
	}

	start := time.Now()

	headers := map[string]string{
		"X-API-Key":    dev.Key,
		"Content-Type": "application/json",
	}

	status, _ := postJSONWithHeaders(client, url, payload, nil, headers)

	latency := time.Since(start).Microseconds()

	atomic.AddInt64(&stats.TotalRequests, 1)
	atomic.AddInt64(&stats.TotalLatency, latency)

	if status == 200 || status == 201 {
		atomic.AddInt64(&stats.Success, 1)
	} else {
		atomic.AddInt64(&stats.Errors, 1)
	}
}

func sendTelemetryMQTT(dev DeviceCreds) {
	if dev.MQTTClient == nil || !dev.MQTTClient.IsConnected() {
		atomic.AddInt64(&stats.Errors, 1)
		return
	}

	topic := fmt.Sprintf("data/%s", dev.ID)
	payload := fmt.Sprintf(`{"temperature":%.2f,"humidity":%.2f,"status":"testing_mqtt"}`,
		20.0+rand.Float64()*10.0, 40.0+rand.Float64()*20.0)

	start := time.Now()
	token := dev.MQTTClient.Publish(topic, 0, false, payload)
	token.Wait()

	latency := time.Since(start).Microseconds()
	atomic.AddInt64(&stats.TotalRequests, 1)
	atomic.AddInt64(&stats.TotalLatency, latency)

	if token.Error() != nil {
		atomic.AddInt64(&stats.Errors, 1)
	} else {
		atomic.AddInt64(&stats.Success, 1)
	}
}

func printReport(elapsed time.Duration) {
	total := atomic.LoadInt64(&stats.TotalRequests)
	success := atomic.LoadInt64(&stats.Success)
	errors := atomic.LoadInt64(&stats.Errors)
	latencyTotal := atomic.LoadInt64(&stats.TotalLatency)

	rps := float64(total) / elapsed.Seconds()
	avgLatency := float64(0)
	if total > 0 {
		avgLatency = float64(latencyTotal) / float64(total) / 1000.0 // Milliseconds
	}

	fmt.Println("\n==================================================")
	fmt.Println("             LOAD TEST REPORT")
	fmt.Println("==================================================")
	fmt.Printf("Total Duration    : %v\n", elapsed)
	fmt.Printf("Total Requests    : %d\n", total)
	fmt.Printf("Success           : %d\n", success)
	fmt.Printf("Errors            : %d\n", errors)
	fmt.Printf("Requests/Sec      : %.2f\n", rps)
	fmt.Printf("Avg Latency       : %.2f ms\n", avgLatency)
	fmt.Println("==================================================")
}

func postJSON(client *http.Client, url string, data interface{}, result interface{}) (int, []byte) {
	return postJSONWithHeaders(client, url, data, result, nil)
}

func postJSONWithHeaders(client *http.Client, url string, data interface{}, result interface{}, headers map[string]string) (int, []byte) {
	jsonBytes, _ := json.Marshal(data)
	req, _ := http.NewRequest("POST", url, bytes.NewBuffer(jsonBytes))

	for k, v := range headers {
		req.Header.Set(k, v)
	}

	resp, err := client.Do(req)
	if err != nil {
		return 0, nil
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)
	if result != nil {
		json.Unmarshal(body, result)
	}
	return resp.StatusCode, body
}
