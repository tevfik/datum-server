package rules

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"datum-go/internal/rules"

	"github.com/gin-gonic/gin"
)

func setupTestRouter() (*gin.Engine, *rules.Engine) {
	gin.SetMode(gin.TestMode)
	eng := rules.NewEngine(nil, nil)
	h := NewHandler(eng)

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
