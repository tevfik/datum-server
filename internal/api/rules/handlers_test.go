package rules

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"datum-go/internal/rules"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// ── Mock store ────────────────────────────────────────────────────────────────

type testStore struct {
	storage.Provider // embed for interface satisfaction (nil for unneeded methods)
	latestData       map[string]*storage.DataPoint
	userDevices      []storage.Device
}

func newTestStore() *testStore {
	return &testStore{latestData: make(map[string]*storage.DataPoint)}
}

func (s *testStore) GetLatestData(deviceID string) (*storage.DataPoint, error) {
	dp, ok := s.latestData[deviceID]
	if !ok {
		return nil, nil
	}
	return dp, nil
}

func (s *testStore) GetUserDevices(userID string) ([]storage.Device, error) {
	return s.userDevices, nil
}

func (s *testStore) GetAllDevices() ([]storage.Device, error) {
	return s.userDevices, nil
}

func setupTestRouterWithStore(store storage.Provider) (*gin.Engine, *rules.Engine) {
	gin.SetMode(gin.TestMode)
	eng := rules.NewEngine(nil, nil)
	h := NewHandler(eng, store)

	r := gin.New()
	g := r.Group("/admin/rules")
	h.RegisterRoutes(g)
	return r, eng
}

func setupTestRouter() (*gin.Engine, *rules.Engine) {
	gin.SetMode(gin.TestMode)
	eng := rules.NewEngine(nil, nil)
	h := NewHandler(eng, nil) // Nil store for simple engine tests

	r := gin.New()
	g := r.Group("/admin/rules")
	h.RegisterRoutes(g)
	return r, eng
}

func TestCreateRule(t *testing.T) {
	r, _ := setupTestRouter()

	body := map[string]interface{}{
		"name": "Test Rule",
		"conditions": []map[string]interface{}{
			{"field": "temp", "operator": "gt", "value": 30},
		},
		"actions": []map[string]interface{}{
			{"type": "log"},
		},
	}
	b, _ := json.Marshal(body)

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/admin/rules", bytes.NewReader(b))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)

	if w.Code != http.StatusCreated {
		t.Fatalf("expected 201, got %d: %s", w.Code, w.Body.String())
	}

	var result rules.Rule
	json.Unmarshal(w.Body.Bytes(), &result)
	if result.Name != "Test Rule" {
		t.Fatalf("expected name 'Test Rule', got '%s'", result.Name)
	}
	if result.ID == "" {
		t.Fatal("expected auto-generated ID")
	}
}

func TestListRules(t *testing.T) {
	r, eng := setupTestRouter()
	eng.AddRule(&rules.Rule{ID: "r1", Name: "Rule 1"})
	eng.AddRule(&rules.Rule{ID: "r2", Name: "Rule 2"})

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/admin/rules", nil)
	r.ServeHTTP(w, req)

	if w.Code != 200 {
		t.Fatalf("expected 200, got %d", w.Code)
	}

	var result map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &result)
	rulesList, ok := result["rules"].([]interface{})
	if !ok || len(rulesList) != 2 {
		t.Fatalf("expected 2 rules, got %v", result)
	}
}

func TestGetRule(t *testing.T) {
	r, eng := setupTestRouter()
	eng.AddRule(&rules.Rule{ID: "r1", Name: "Rule 1"})

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/admin/rules/r1", nil)
	r.ServeHTTP(w, req)

	if w.Code != 200 {
		t.Fatalf("expected 200, got %d", w.Code)
	}
}

func TestGetRuleNotFound(t *testing.T) {
	r, _ := setupTestRouter()

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/admin/rules/nonexistent", nil)
	r.ServeHTTP(w, req)

	if w.Code != 404 {
		t.Fatalf("expected 404, got %d", w.Code)
	}
}

func TestDeleteRule(t *testing.T) {
	r, eng := setupTestRouter()
	eng.AddRule(&rules.Rule{ID: "to-del", Name: "Delete me"})

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("DELETE", "/admin/rules/to-del", nil)
	r.ServeHTTP(w, req)

	if w.Code != 200 {
		t.Fatalf("expected 200, got %d", w.Code)
	}

	if _, ok := eng.GetRule("to-del"); ok {
		t.Fatal("rule should be deleted")
	}
}

func TestEnableDisableRule(t *testing.T) {
	r, eng := setupTestRouter()
	eng.AddRule(&rules.Rule{ID: "toggle", Name: "Toggle"})

	// Disable
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("PUT", "/admin/rules/toggle/disable", nil)
	r.ServeHTTP(w, req)
	if w.Code != 200 {
		t.Fatalf("expected 200, got %d", w.Code)
	}

	rule, _ := eng.GetRule("toggle")
	if rule.Enabled {
		t.Fatal("rule should be disabled")
	}

	// Enable
	w = httptest.NewRecorder()
	req, _ = http.NewRequest("PUT", "/admin/rules/toggle/enable", nil)
	r.ServeHTTP(w, req)
	if w.Code != 200 {
		t.Fatalf("expected 200, got %d", w.Code)
	}

	rule, _ = eng.GetRule("toggle")
	if !rule.Enabled {
		t.Fatal("rule should be enabled")
	}
}

// ── UpdateRule ────────────────────────────────────────────────────────────────

func TestUpdateRule_NoAuth(t *testing.T) {
	// Without a user_id in context (no store), handler just updates engine.
	r, eng := setupTestRouter()
	eng.AddRule(&rules.Rule{ID: "upd-1", Name: "Original"})

	body := map[string]interface{}{"name": "Updated"}
	b, _ := json.Marshal(body)

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("PUT", "/admin/rules/upd-1", bytes.NewReader(b))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	updated, _ := eng.GetRule("upd-1")
	if updated.Name != "Updated" {
		t.Fatalf("expected name 'Updated', got '%s'", updated.Name)
	}
}

func TestUpdateRule_InvalidBody(t *testing.T) {
	r, eng := setupTestRouter()
	eng.AddRule(&rules.Rule{ID: "upd-bad", Name: "Existing"})

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("PUT", "/admin/rules/upd-bad", bytes.NewReader([]byte("not-json{")))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)

	if w.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d", w.Code)
	}
}

// ── TriggerRule ───────────────────────────────────────────────────────────────

func TestTriggerRule_Success(t *testing.T) {
	store := newTestStore()
	store.latestData["sensor-1"] = &storage.DataPoint{
		DeviceID:  "sensor-1",
		Timestamp: time.Now(),
		Data:      map[string]interface{}{"temp": 99.0},
	}
	r, eng := setupTestRouterWithStore(store)

	eng.AddRule(&rules.Rule{
		ID:   "trigger-ok",
		Name: "Trigger me",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerManual,
			DeviceID: "sensor-1",
		},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "temp", Operator: rules.OpGT, Value: 50.0}},
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/admin/rules/trigger-ok/trigger", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var result map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &result)
	if result["triggered"] != true {
		t.Fatal("expected triggered=true")
	}
}

func TestTriggerRule_NotFound(t *testing.T) {
	r, _ := setupTestRouterWithStore(newTestStore())

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/admin/rules/nonexistent/trigger", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusNotFound {
		t.Fatalf("expected 404, got %d", w.Code)
	}
}

func TestTriggerRule_NoDeviceID(t *testing.T) {
	r, eng := setupTestRouterWithStore(newTestStore())
	eng.AddRule(&rules.Rule{
		ID:   "no-dev",
		Name: "No device",
		// Trigger.DeviceID is empty and DeviceID is empty
	})

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/admin/rules/no-dev/trigger", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d: %s", w.Code, w.Body.String())
	}
}

func TestTriggerRule_NoDataAvailable(t *testing.T) {
	store := newTestStore() // no data for "sensor-x"
	r, eng := setupTestRouterWithStore(store)
	eng.AddRule(&rules.Rule{
		ID:   "nodata-rule",
		Name: "No data",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerManual,
			DeviceID: "sensor-x",
		},
	})

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/admin/rules/nodata-rule/trigger", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusNotFound {
		t.Fatalf("expected 404, got %d: %s", w.Code, w.Body.String())
	}
}

func TestTriggerRule_NilStore(t *testing.T) {
	r, eng := setupTestRouter() // nil store
	eng.AddRule(&rules.Rule{
		ID:   "nilstore",
		Name: "Nil store",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerManual,
			DeviceID: "sensor-1",
		},
	})

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/admin/rules/nilstore/trigger", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusInternalServerError {
		t.Fatalf("expected 500, got %d: %s", w.Code, w.Body.String())
	}
}

// ── DiscoverDevices ───────────────────────────────────────────────────────────

func TestDiscoverDevices_NoUserID(t *testing.T) {
	r, _ := setupTestRouterWithStore(newTestStore())

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/admin/rules/discovery", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d", w.Code)
	}
}

func setupTestRouterWithUserAndStore(store storage.Provider, userID string) *gin.Engine {
	gin.SetMode(gin.TestMode)
	eng := rules.NewEngine(nil, nil)
	h := NewHandler(eng, store)

	router := gin.New()
	router.Use(func(c *gin.Context) {
		c.Set("user_id", userID)
		c.Next()
	})
	g := router.Group("/admin/rules")
	h.RegisterRoutes(g)
	return router
}

func TestDiscoverDevices_WithDevices(t *testing.T) {
	store := newTestStore()
	store.userDevices = []storage.Device{
		{ID: "d1", Name: "Sensor A", Type: "sensor"},
		{ID: "d2", Name: "Actuator B", Type: "actuator"},
	}

	r := setupTestRouterWithUserAndStore(store, "user-123")

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/admin/rules/discovery", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var result map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &result)
	devs, ok := result["devices"].([]interface{})
	if !ok || len(devs) != 2 {
		t.Fatalf("expected 2 devices, got %v", result)
	}
}

// TestDiscoverDevices_ShadowStateFallback verifies that when a device has no
// ThingDescription, properties are inferred from its ShadowState telemetry keys.
func TestDiscoverDevices_ShadowStateFallback(t *testing.T) {
	store := newTestStore()
	store.userDevices = []storage.Device{
		{
			ID:   "custom-sensor",
			Name: "Custom Sensor",
			Type: "custom",
			ShadowState: map[string]interface{}{
				"temperature": 22.5,
				"humidity":    65.0,
				"active":      true,
			},
		},
	}
	r := setupTestRouterWithUserAndStore(store, "user-1")

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/admin/rules/discovery", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var result map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &result)
	devs := result["devices"].([]interface{})
	dev := devs[0].(map[string]interface{})
	props := dev["properties"].([]interface{})

	// Should have 3 shadow keys + 1 status fallback = 4
	if len(props) < 4 {
		t.Fatalf("expected at least 4 properties (3 shadow + status), got %d: %v", len(props), props)
	}

	// Verify type inference: temperature and humidity should be "number"
	propMap := make(map[string]string)
	for _, p := range props {
		pm := p.(map[string]interface{})
		propMap[pm["key"].(string)] = pm["type"].(string)
	}
	if propMap["temperature"] != "number" {
		t.Errorf("temperature type: want number, got %s", propMap["temperature"])
	}
	if propMap["active"] != "boolean" {
		t.Errorf("active type: want boolean, got %s", propMap["active"])
	}
}

func TestDiscoverDevices_Empty(t *testing.T) {
	store := newTestStore() // no devices
	r := setupTestRouterWithUserAndStore(store, "user-xyz")

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/admin/rules/discovery", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
}

// ── GetBlockDefinitions ───────────────────────────────────────────────────────

func TestGetBlockDefinitions(t *testing.T) {
	r, _ := setupTestRouter()

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/admin/rules/blocks", nil)
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", w.Code, w.Body.String())
	}

	var result map[string]interface{}
	if err := json.Unmarshal(w.Body.Bytes(), &result); err != nil {
		t.Fatalf("invalid JSON response: %v", err)
	}
	blocks, ok := result["blocks"].([]interface{})
	if !ok || len(blocks) == 0 {
		t.Fatalf("expected non-empty blocks array, got %v", result)
	}
	if _, ok := result["operators"]; !ok {
		t.Fatal("expected operators in response")
	}
	if _, ok := result["action_types"]; !ok {
		t.Fatal("expected action_types in response")
	}
	if _, ok := result["trigger_types"]; !ok {
		t.Fatal("expected trigger_types in response")
	}
}
