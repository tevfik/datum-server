package integration

import (
	"bytes"
	"datum-go/internal/api/commands"
	"datum-go/internal/api/devices"
	"datum-go/internal/mqtt"
	"datum-go/internal/processing"
	"datum-go/internal/storage"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"

	mqtt_client "github.com/eclipse/paho.mqtt.golang"
	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestMQTT_E2E_Flow(t *testing.T) {
	gin.SetMode(gin.TestMode)
	os.Setenv("MQTT_ALLOW_INSECURE", "true")

	// 1. Setup Storage
	tmpDir := t.TempDir()
	store, err := storage.New(
		fmt.Sprintf("%s/meta.db", tmpDir),
		fmt.Sprintf("%s/tsdata", tmpDir),
		24*time.Hour,
	)
	require.NoError(t, err)
	defer store.Close()
	require.NoError(t, store.InitializeSystem("E2E Test", false, 7))

	// 2. Setup MQTT Broker
	tp := processing.NewTelemetryProcessor(store)
	defer tp.Close()
	broker := mqtt.NewBroker(store, tp)
	err = broker.Start()
	require.NoError(t, err)
	defer broker.Stop()

	// 3. Setup API
	r := gin.New()
	deviceHandler := devices.NewHandler(store, broker, nil)
	commandHandler := commands.NewHandler(store, broker)

	// Mock User Auth Context
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user-1")
		c.Set("role", "admin")
		c.Next()
	})

	r.POST("/dev", deviceHandler.CreateDevice)
	r.GET("/dev", deviceHandler.ListDevices)
	r.POST("/dev/:device_id/cmd", commandHandler.SendCommand)

	// 4. Create Device via API
	deviceReq := devices.CreateDeviceRequest{
		Name: "Test Relay",
		Type: "relay_board",
	}
	body, _ := json.Marshal(deviceReq)
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/dev", bytes.NewBuffer(body))
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusCreated, w.Code)

	var deviceResp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &deviceResp)
	deviceID := deviceResp["device_id"].(string)
	apiKey := deviceResp["api_key"].(string)

	// 5. Connect simulated device via MQTT
	opts := mqtt_client.NewClientOptions().AddBroker("tcp://localhost:1883")
	opts.SetClientID(deviceID)
	opts.SetUsername(deviceID)
	opts.SetPassword(apiKey)
	opts.SetCleanSession(true)

	client := mqtt_client.NewClient(opts)
	token := client.Connect()
	if token.Wait() && token.Error() != nil {
		t.Fatalf("Failed to connect MQTT: %v", token.Error())
	}
	defer client.Disconnect(250)

	// Subscribe to commands — wait for SUBACK before sending command (prevents race)
	cmdReceived := make(chan []byte, 1)
	subToken := client.Subscribe(fmt.Sprintf("dev/%s/cmd", deviceID), 0, func(c mqtt_client.Client, m mqtt_client.Message) {
		cmdReceived <- m.Payload()
	})
	if !subToken.WaitTimeout(3*time.Second) || subToken.Error() != nil {
		t.Fatalf("MQTT subscribe failed: %v", subToken.Error())
	}

	// 6. Send Command via API
	cmdReq := commands.SendCommandRequest{
		Action: "set_property",
		Params: map[string]interface{}{"key": "relay_0", "value": true},
	}
	body, _ = json.Marshal(cmdReq)
	w = httptest.NewRecorder()
	req, _ = http.NewRequest("POST", fmt.Sprintf("/dev/%s/cmd", deviceID), bytes.NewBuffer(body))
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusAccepted, w.Code)

	// 7. Verify Command received by device
	select {
	case payload := <-cmdReceived:
		var cmd map[string]interface{}
		json.Unmarshal(payload, &cmd)
		assert.Equal(t, "set_property", cmd["action"])
		params := cmd["params"].(map[string]interface{})
		assert.Equal(t, "relay_0", params["key"])
		assert.Equal(t, true, params["value"])
	case <-time.After(2 * time.Second):
		t.Fatal("Command not received by device via MQTT")
	}

	// 8. Send Telemetry via MQTT (updates LastSeen)
	telemetry := map[string]interface{}{"relay_0": true, "temp": 25.5}
	telemetryBytes, _ := json.Marshal(telemetry)
	token = client.Publish(fmt.Sprintf("dev/%s/data", deviceID), 0, false, telemetryBytes)
	token.Wait()

	// Wait for processing (async TelemetryProcessor)
	time.Sleep(2 * time.Second)

	// 9. Verify Device is "online" via API
	w = httptest.NewRecorder()
	req, _ = http.NewRequest("GET", "/dev", nil)
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)

	var listResp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &listResp)
	devs := listResp["devices"].([]interface{})
	assert.Len(t, devs, 1)
	dev := devs[0].(map[string]interface{})
	assert.Equal(t, "online", dev["status"])

	if dev["shadow_state"] == nil {
		t.Fatal("shadow_state is nil")
	}
	shadow := dev["shadow_state"].(map[string]interface{})
	assert.Equal(t, true, shadow["relay_0"])
}
