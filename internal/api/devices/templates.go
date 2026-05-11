package devices

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
