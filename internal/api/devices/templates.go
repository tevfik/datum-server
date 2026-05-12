package devices

import (
	"fmt"

	"datum-go/internal/rules"
)

// GetTemplateForType returns a default Thing Description JSON structure for a given device type.
// If the type is not recognized or does not have a template, it returns nil.
func GetTemplateForType(deviceType string) map[string]interface{} {
	switch deviceType {
	case "demo", "smart_greenhouse", "greenhouse":
		return map[string]interface{}{
			"title": "Smart Greenhouse Demo",
			"properties": map[string]interface{}{
				"temp": map[string]interface{}{
					"title":     "Temperature",
					"type":      "number",
					"unit":      "°C",
					"ui:widget": "timeseries",
				},
				"hum": map[string]interface{}{
					"title":     "Humidity",
					"type":      "number",
					"unit":      "%",
					"ui:widget": "chart",
				},
				"soil": map[string]interface{}{
					"title":     "Soil Moisture",
					"type":      "number",
					"unit":      "%",
					"minimum":   0,
					"maximum":   100,
					"ui:widget": "gauge",
				},
				"fan_speed": map[string]interface{}{
					"title":     "Ventilation Fan",
					"type":      "number",
					"unit":      "RPM",
					"minimum":   0,
					"maximum":   3000,
					"readOnly":  false,
					"ui:widget": "slider",
				},
				"pump": map[string]interface{}{
					"title":    "Water Pump",
					"type":     "boolean",
					"readOnly": false,
				},
				"led_color": map[string]interface{}{
					"title":     "Grow Light Color",
					"type":      "string",
					"readOnly":  false,
					"ui:widget": "color",
				},
				"reset": map[string]interface{}{
					"title":     "System Reset",
					"type":      "boolean",
					"readOnly":  false,
					"ui:widget": "button",
				},
			},
		}
	// Add other templates here as needed
	default:
		return nil
	}
}

// GetDefaultRulesForType returns a list of default rules for a given device type.
func GetDefaultRulesForType(deviceType string, deviceID string) []*rules.Rule {
	switch deviceType {
	case "demo", "smart_greenhouse", "greenhouse":
		return []*rules.Rule{
			{
				Name:        "High Temperature Alert",
				Description: "Trigger an alert when temperature exceeds 35°C",
				DeviceID:    deviceID,
				Conditions: []rules.Condition{
					{
						Field:    "temp",
						Operator: rules.OpGT,
						Value:    35.0,
					},
				},
				Actions: []rules.RuleAction{
					{
						Type: rules.ActionLog,
					},
					{
						Type: rules.ActionWebhook,
					},
					{
						Type: rules.ActionMQTT,
						Config: map[string]interface{}{
							"topic": fmt.Sprintf("dev/%s/alert/high_temp", deviceID),
						},
					},
				},
			},
			{
				Name:        "Low Soil Moisture - Auto Pump",
				Description: "Automatically turn on the pump when soil moisture is below 20%",
				DeviceID:    deviceID,
				Conditions: []rules.Condition{
					{
						Field:    "soil",
						Operator: rules.OpLT,
						Value:    20.0,
					},
				},
				Actions: []rules.RuleAction{
					{
						Type: rules.ActionMQTT,
						Config: map[string]interface{}{
							// We send a desired state update to turn on the pump
							"topic": fmt.Sprintf("dev/%s/conf/set", deviceID),
						},
					},
				},
			},
		}
	default:
		return nil
	}
}
