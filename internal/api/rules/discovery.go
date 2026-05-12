package rules

import (
	"net/http"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// DevicePropertyInfo represents a single property of a device for Blockly block generation.
type DevicePropertyInfo struct {
	Key      string `json:"key"`
	Title    string `json:"title"`
	Type     string `json:"type"`     // number, string, boolean
	Unit     string `json:"unit,omitempty"`
	ReadOnly bool   `json:"read_only"`
	Widget   string `json:"widget,omitempty"` // gauge, slider, chart, etc.
}

// DeviceInfo represents a device and its properties for Blockly discovery.
type DeviceInfo struct {
	DeviceID   string               `json:"device_id"`
	DeviceName string               `json:"device_name"`
	DeviceType string               `json:"device_type"`
	Properties []DevicePropertyInfo `json:"properties"`
}

// DiscoverDevices returns the user's devices with their Thing Description properties.
// This endpoint powers the dynamic Blockly block generation.
//
// GET /api/v1/rules/discovery
func (h *Handler) DiscoverDevices(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user_id required"})
		return
	}

	role, _ := auth.GetUserRole(c)

	var devices []storage.Device
	var err error

	if role == "admin" {
		devices, err = h.store.GetAllDevices()
	} else {
		devices, err = h.store.GetUserDevices(userID)
	}

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch devices"})
		return
	}

	result := make([]DeviceInfo, 0, len(devices))
	for _, dev := range devices {
		props := extractProperties(dev.ThingDescription)
		
		// Add basic common properties if not present
		hasStatus := false
		for _, p := range props {
			if p.Key == "status" {
				hasStatus = true
				break
			}
		}
		if !hasStatus {
			props = append(props, DevicePropertyInfo{
				Key:      "status",
				Title:    "Connection Status",
				Type:     "string",
				ReadOnly: true,
			})
		}

		info := DeviceInfo{
			DeviceID:   dev.ID,
			DeviceName: dev.Name,
			DeviceType: dev.Type,
			Properties: props,
		}
		result = append(result, info)
	}

	c.JSON(http.StatusOK, gin.H{
		"devices": result,
	})
}

// extractProperties parses a Thing Description to extract property information.
func extractProperties(td map[string]interface{}) []DevicePropertyInfo {
	if td == nil {
		return nil
	}

	props, ok := td["properties"].(map[string]interface{})
	if !ok {
		return nil
	}

	result := make([]DevicePropertyInfo, 0, len(props))
	for key, val := range props {
		propMap, ok := val.(map[string]interface{})
		if !ok {
			continue
		}

		info := DevicePropertyInfo{
			Key:   key,
			Title: getStringField(propMap, "title", key),
			Type:  getStringField(propMap, "type", "number"),
			Unit:  getStringField(propMap, "unit", ""),
		}

		if ro, ok := propMap["readOnly"].(bool); ok {
			info.ReadOnly = ro
		}

		if widget, ok := propMap["ui:widget"].(string); ok {
			info.Widget = widget
		}

		result = append(result, info)
	}

	return result
}

// getStringField safely extracts a string field from a map.
func getStringField(m map[string]interface{}, key, fallback string) string {
	if v, ok := m[key].(string); ok {
		return v
	}
	return fallback
}

// BlockDefinition represents a Blockly block definition for the frontend.
type BlockDefinition struct {
	Type       string      `json:"type"`
	Category   string      `json:"category"`
	Message    string      `json:"message0"`
	Args       interface{} `json:"args0,omitempty"`
	Output     string      `json:"output,omitempty"`
	Color      int         `json:"colour"`
	Tooltip    string      `json:"tooltip,omitempty"`
	HelpURL    string      `json:"helpUrl,omitempty"`
}

// GetBlockDefinitions returns the available Blockly block definitions.
// The frontend uses these to populate the Blockly toolbox.
//
// GET /api/v1/rules/blocks
func (h *Handler) GetBlockDefinitions(c *gin.Context) {
	blocks := []BlockDefinition{
		// ── Trigger blocks ──
		{
			Type:     "trigger_on_data",
			Category: "Triggers",
			Message:  "When device %1 receives data",
			Color:    120,
			Tooltip:  "Triggers when new telemetry data arrives from a device",
		},
		{
			Type:     "trigger_scheduled",
			Category: "Triggers",
			Message:  "Every %1 minutes",
			Color:    120,
			Tooltip:  "Triggers on a time schedule",
		},
		// ── Condition blocks ──
		{
			Type:     "condition_compare",
			Category: "Conditions",
			Message:  "%1 %2 %3",
			Color:    210,
			Tooltip:  "Compare a device property value",
		},
		{
			Type:     "logic_and",
			Category: "Conditions",
			Message:  "%1 AND %2",
			Color:    210,
			Tooltip:  "Both conditions must be true",
		},
		{
			Type:     "logic_or",
			Category: "Conditions",
			Message:  "%1 OR %2",
			Color:    210,
			Tooltip:  "Either condition must be true",
		},
		{
			Type:     "logic_not",
			Category: "Conditions",
			Message:  "NOT %1",
			Color:    210,
			Tooltip:  "Inverts the condition",
		},
		// ── Device blocks ──
		{
			Type:     "device_property",
			Category: "Devices",
			Message:  "Device %1 Property %2",
			Output:   "Number",
			Color:    65,
			Tooltip:  "Get a property value from a device",
		},
		// ── Action blocks ──
		{
			Type:     "action_log",
			Category: "Actions",
			Message:  "Log event: %1",
			Color:    330,
			Tooltip:  "Write an event to the server log",
		},
		{
			Type:     "action_notify",
			Category: "Actions",
			Message:  "Send notification: %1",
			Color:    330,
			Tooltip:  "Send a push notification",
		},
		{
			Type:     "action_mqtt",
			Category: "Actions",
			Message:  "MQTT publish to %1 with %2",
			Color:    330,
			Tooltip:  "Publish a message to an MQTT topic",
		},
		{
			Type:     "action_command",
			Category: "Actions",
			Message:  "Send command to %1: %2",
			Color:    330,
			Tooltip:  "Send a command to a device",
		},
		// ── Advanced blocks ──
		{
			Type:     "lua_script",
			Category: "Advanced",
			Message:  "Lua Script %1",
			Color:    0,
			Tooltip:  "Write custom Lua code for complex logic",
		},
	}

	// Get available operators
	operators := []map[string]string{
		{"value": "gt", "label": "> (Greater than)"},
		{"value": "gte", "label": ">= (Greater or equal)"},
		{"value": "lt", "label": "< (Less than)"},
		{"value": "lte", "label": "<= (Less or equal)"},
		{"value": "eq", "label": "== (Equal)"},
		{"value": "neq", "label": "!= (Not equal)"},
		{"value": "contains", "label": "Contains (text)"},
	}

	// Get available action types
	actionTypes := []map[string]string{
		{"value": "log", "label": "Log Event"},
		{"value": "notify", "label": "Send Notification"},
		{"value": "mqtt", "label": "MQTT Publish"},
		{"value": "webhook", "label": "Webhook"},
		{"value": "command", "label": "Device Command"},
	}

	// Get available trigger types
	triggerTypes := []map[string]string{
		{"value": "on_data", "label": "When data arrives"},
		{"value": "scheduled", "label": "On schedule (cron)"},
		{"value": "manual", "label": "Manual trigger only"},
	}

	c.JSON(http.StatusOK, gin.H{
		"blocks":        blocks,
		"operators":     operators,
		"action_types":  actionTypes,
		"trigger_types": triggerTypes,
	})
}

// getDevicesForUser is a helper used by discovery (avoids circular dependency).
func getDevicesForUser(store storage.Provider, userID string) ([]storage.Device, error) {
	return store.GetUserDevices(userID)
}
