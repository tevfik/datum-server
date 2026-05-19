package devices

import (
	"testing"
)

func TestGetTemplateForType_KnownTypes(t *testing.T) {
	knownTypes := []string{
		"demo",
		"smart_greenhouse",
		"greenhouse",
		"weather_station",
		"soil_sensor",
		"irrigation_controller",
		"irrigation",
		"sensor",
		"actuator",
		"relay",
	}
	for _, dt := range knownTypes {
		t.Run(dt, func(t *testing.T) {
			tmpl := GetTemplateForType(dt)
			if tmpl == nil {
				t.Errorf("GetTemplateForType(%q) = nil, expected non-nil template", dt)
				return
			}
			if _, ok := tmpl["title"]; !ok {
				t.Errorf("template for %q missing 'title' field", dt)
			}
			if _, ok := tmpl["properties"]; !ok {
				t.Errorf("template for %q missing 'properties' field", dt)
			}
			props, ok := tmpl["properties"].(map[string]interface{})
			if !ok || len(props) == 0 {
				t.Errorf("template for %q has empty properties", dt)
			}
		})
	}
}

func TestGetTemplateForType_UnknownType(t *testing.T) {
	tmpl := GetTemplateForType("unknown_device_xyz")
	if tmpl != nil {
		t.Errorf("GetTemplateForType(unknown) expected nil, got %v", tmpl)
	}
}

func TestGetTemplateForType_GreenhouseProperties(t *testing.T) {
	tmpl := GetTemplateForType("greenhouse")
	props := tmpl["properties"].(map[string]interface{})
	required := []string{"temp", "hum", "soil", "pump"}
	for _, p := range required {
		if _, ok := props[p]; !ok {
			t.Errorf("greenhouse template missing property %q", p)
		}
	}
}

func TestGetTemplateForType_WeatherStationProperties(t *testing.T) {
	tmpl := GetTemplateForType("weather_station")
	props := tmpl["properties"].(map[string]interface{})
	required := []string{"temp", "humidity", "pressure", "wind_speed", "rain_mm"}
	for _, p := range required {
		if _, ok := props[p]; !ok {
			t.Errorf("weather_station template missing property %q", p)
		}
	}
}

func TestGetTemplateForType_SoilSensorProperties(t *testing.T) {
	tmpl := GetTemplateForType("soil_sensor")
	props := tmpl["properties"].(map[string]interface{})
	required := []string{"soil_moisture", "soil_temp", "soil_ph", "nitrogen", "phosphorus", "potassium"}
	for _, p := range required {
		if _, ok := props[p]; !ok {
			t.Errorf("soil_sensor template missing property %q", p)
		}
	}
}

func TestGetTemplateForType_IrrigationProperties(t *testing.T) {
	tmpl := GetTemplateForType("irrigation_controller")
	props := tmpl["properties"].(map[string]interface{})
	required := []string{"zone1_active", "zone2_active", "flow_rate", "pressure", "schedule_enabled"}
	for _, p := range required {
		if _, ok := props[p]; !ok {
			t.Errorf("irrigation_controller template missing property %q", p)
		}
	}
}

func TestGetDefaultRulesForType_WeatherStation(t *testing.T) {
	rules := GetDefaultRulesForType("weather_station", "dev_test_01")
	if len(rules) == 0 {
		t.Fatal("expected default rules for weather_station")
	}
	hasFrostRule := false
	for _, r := range rules {
		if r.DeviceID != "dev_test_01" {
			t.Errorf("rule %q has wrong deviceID: %v", r.Name, r.DeviceID)
		}
		if r.Name == "Frost Warning" {
			hasFrostRule = true
		}
	}
	if !hasFrostRule {
		t.Error("expected 'Frost Warning' rule for weather_station")
	}
}

func TestGetDefaultRulesForType_SoilSensor(t *testing.T) {
	rules := GetDefaultRulesForType("soil_sensor", "dev_soil_01")
	if len(rules) == 0 {
		t.Fatal("expected default rules for soil_sensor")
	}
	for _, r := range rules {
		if len(r.Conditions) == 0 {
			t.Errorf("rule %q has no conditions", r.Name)
		}
		if len(r.Actions) == 0 {
			t.Errorf("rule %q has no actions", r.Name)
		}
	}
}

func TestGetDefaultRulesForType_Irrigation(t *testing.T) {
	rules := GetDefaultRulesForType("irrigation", "dev_irr_01")
	if len(rules) == 0 {
		t.Fatal("expected default rules for irrigation")
	}
}

func TestGetDefaultRulesForType_Unknown(t *testing.T) {
	rules := GetDefaultRulesForType("unknown_xyz", "dev_x")
	if rules != nil {
		t.Errorf("expected nil rules for unknown type, got %v", rules)
	}
}
