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

	case "weather_station":
		return map[string]interface{}{
			"title": "Weather Station",
			"properties": map[string]interface{}{
				"temp": map[string]interface{}{
					"title":     "Air Temperature",
					"type":      "number",
					"unit":      "°C",
					"ui:widget": "timeseries",
				},
				"humidity": map[string]interface{}{
					"title":     "Relative Humidity",
					"type":      "number",
					"unit":      "%",
					"ui:widget": "chart",
				},
				"pressure": map[string]interface{}{
					"title":     "Barometric Pressure",
					"type":      "number",
					"unit":      "hPa",
					"ui:widget": "timeseries",
				},
				"wind_speed": map[string]interface{}{
					"title":     "Wind Speed",
					"type":      "number",
					"unit":      "km/h",
					"ui:widget": "gauge",
				},
				"wind_dir": map[string]interface{}{
					"title":     "Wind Direction",
					"type":      "number",
					"unit":      "°",
					"minimum":   0,
					"maximum":   360,
					"ui:widget": "compass",
				},
				"rain_mm": map[string]interface{}{
					"title":     "Rainfall",
					"type":      "number",
					"unit":      "mm",
					"ui:widget": "timeseries",
				},
				"uv_index": map[string]interface{}{
					"title":     "UV Index",
					"type":      "number",
					"unit":      "",
					"minimum":   0,
					"maximum":   11,
					"ui:widget": "gauge",
				},
			},
		}

	case "soil_sensor":
		return map[string]interface{}{
			"title": "Soil Sensor",
			"properties": map[string]interface{}{
				"soil_moisture": map[string]interface{}{
					"title":     "Soil Moisture",
					"type":      "number",
					"unit":      "%",
					"minimum":   0,
					"maximum":   100,
					"ui:widget": "gauge",
				},
				"soil_temp": map[string]interface{}{
					"title":     "Soil Temperature",
					"type":      "number",
					"unit":      "°C",
					"ui:widget": "timeseries",
				},
				"soil_ph": map[string]interface{}{
					"title":     "Soil pH",
					"type":      "number",
					"unit":      "",
					"minimum":   0,
					"maximum":   14,
					"ui:widget": "gauge",
				},
				"soil_ec": map[string]interface{}{
					"title":     "Electrical Conductivity",
					"type":      "number",
					"unit":      "µS/cm",
					"ui:widget": "timeseries",
				},
				"nitrogen": map[string]interface{}{
					"title":     "Nitrogen (N)",
					"type":      "number",
					"unit":      "mg/kg",
					"ui:widget": "chart",
				},
				"phosphorus": map[string]interface{}{
					"title":     "Phosphorus (P)",
					"type":      "number",
					"unit":      "mg/kg",
					"ui:widget": "chart",
				},
				"potassium": map[string]interface{}{
					"title":     "Potassium (K)",
					"type":      "number",
					"unit":      "mg/kg",
					"ui:widget": "chart",
				},
			},
		}

	case "irrigation_controller", "irrigation":
		return map[string]interface{}{
			"title": "Irrigation Controller",
			"properties": map[string]interface{}{
				"zone1_active": map[string]interface{}{
					"title":    "Zone 1 Valve",
					"type":     "boolean",
					"readOnly": false,
				},
				"zone2_active": map[string]interface{}{
					"title":    "Zone 2 Valve",
					"type":     "boolean",
					"readOnly": false,
				},
				"zone3_active": map[string]interface{}{
					"title":    "Zone 3 Valve",
					"type":     "boolean",
					"readOnly": false,
				},
				"zone4_active": map[string]interface{}{
					"title":    "Zone 4 Valve",
					"type":     "boolean",
					"readOnly": false,
				},
				"flow_rate": map[string]interface{}{
					"title":     "Flow Rate",
					"type":      "number",
					"unit":      "L/min",
					"ui:widget": "timeseries",
				},
				"total_volume": map[string]interface{}{
					"title":     "Total Volume Today",
					"type":      "number",
					"unit":      "L",
					"ui:widget": "chart",
				},
				"pressure": map[string]interface{}{
					"title":     "Line Pressure",
					"type":      "number",
					"unit":      "bar",
					"ui:widget": "gauge",
				},
				"schedule_enabled": map[string]interface{}{
					"title":    "Auto Schedule",
					"type":     "boolean",
					"readOnly": false,
				},
			},
		}

	case "sensor":
		return map[string]interface{}{
			"title": "Generic Sensor",
			"properties": map[string]interface{}{
				"value": map[string]interface{}{
					"title":     "Sensor Value",
					"type":      "number",
					"ui:widget": "timeseries",
				},
				"battery": map[string]interface{}{
					"title":     "Battery Level",
					"type":      "number",
					"unit":      "%",
					"minimum":   0,
					"maximum":   100,
					"ui:widget": "gauge",
				},
			},
		}

	case "actuator", "relay":
		return map[string]interface{}{
			"title": "Actuator / Relay",
			"properties": map[string]interface{}{
				"state": map[string]interface{}{
					"title":    "Output State",
					"type":     "boolean",
					"readOnly": false,
				},
				"auto_mode": map[string]interface{}{
					"title":    "Automatic Mode",
					"type":     "boolean",
					"readOnly": false,
				},
			},
		}

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

	case "weather_station":
		return []*rules.Rule{
			{
				Name:        "Frost Warning",
				Description: "Alert when air temperature drops below 2°C (frost risk)",
				DeviceID:    deviceID,
				Conditions: []rules.Condition{
					{
						Field:    "temp",
						Operator: rules.OpLT,
						Value:    2.0,
					},
				},
				Actions: []rules.RuleAction{
					{Type: rules.ActionLog},
					{Type: rules.ActionWebhook},
					{
						Type: rules.ActionMQTT,
						Config: map[string]interface{}{
							"topic": fmt.Sprintf("dev/%s/alert/frost", deviceID),
						},
					},
				},
			},
			{
				Name:        "High Wind Alert",
				Description: "Alert when wind speed exceeds 50 km/h",
				DeviceID:    deviceID,
				Conditions: []rules.Condition{
					{
						Field:    "wind_speed",
						Operator: rules.OpGT,
						Value:    50.0,
					},
				},
				Actions: []rules.RuleAction{
					{Type: rules.ActionLog},
					{Type: rules.ActionWebhook},
				},
			},
		}

	case "soil_sensor":
		return []*rules.Rule{
			{
				Name:        "Low Soil Moisture Alert",
				Description: "Alert when soil moisture drops below 25%",
				DeviceID:    deviceID,
				Conditions: []rules.Condition{
					{
						Field:    "soil_moisture",
						Operator: rules.OpLT,
						Value:    25.0,
					},
				},
				Actions: []rules.RuleAction{
					{Type: rules.ActionLog},
					{Type: rules.ActionWebhook},
					{
						Type: rules.ActionMQTT,
						Config: map[string]interface{}{
							"topic": fmt.Sprintf("dev/%s/alert/dry_soil", deviceID),
						},
					},
				},
			},
			{
				Name:        "Soil pH Out of Range",
				Description: "Alert when soil pH is outside the 5.5-7.5 optimal range",
				DeviceID:    deviceID,
				Conditions: []rules.Condition{
					{
						Field:    "soil_ph",
						Operator: rules.OpGT,
						Value:    7.5,
					},
				},
				Actions: []rules.RuleAction{
					{Type: rules.ActionLog},
					{Type: rules.ActionWebhook},
				},
			},
		}

	case "irrigation_controller", "irrigation":
		return []*rules.Rule{
			{
				Name:        "Low Pressure Alert",
				Description: "Alert when line pressure drops below 0.5 bar (possible leak)",
				DeviceID:    deviceID,
				Conditions: []rules.Condition{
					{
						Field:    "pressure",
						Operator: rules.OpLT,
						Value:    0.5,
					},
				},
				Actions: []rules.RuleAction{
					{Type: rules.ActionLog},
					{Type: rules.ActionWebhook},
					{
						Type: rules.ActionMQTT,
						Config: map[string]interface{}{
							"topic": fmt.Sprintf("dev/%s/alert/low_pressure", deviceID),
						},
					},
				},
			},
			{
				Name:        "High Flow Rate Alert",
				Description: "Alert when flow rate exceeds 100 L/min (possible pipe burst)",
				DeviceID:    deviceID,
				Conditions: []rules.Condition{
					{
						Field:    "flow_rate",
						Operator: rules.OpGT,
						Value:    100.0,
					},
				},
				Actions: []rules.RuleAction{
					{Type: rules.ActionLog},
					{Type: rules.ActionWebhook},
				},
			},
		}

	default:
		return nil
	}
}
