package rules

import (
	"encoding/json"
	"testing"
)

func TestEvaluateSimpleGT(t *testing.T) {
	fired := false
	eng := NewEngine(nil, func(topic string, payload []byte) error {
		fired = true
		return nil
	})

	eng.AddRule(&Rule{
		ID:   "r1",
		Name: "High temp",
		Conditions: []Condition{
			{Field: "temperature", Operator: OpGT, Value: 30.0},
		},
		Actions: []RuleAction{
			{Type: ActionLog},
			{Type: ActionMQTT, Config: map[string]interface{}{"topic": "alerts/temp"}},
		},
	})

	// Should NOT fire
	eng.Evaluate("dev-1", map[string]interface{}{"temperature": 25.0})
	if fired {
		t.Fatal("rule should not have fired for temp=25")
	}

	// Should fire
	eng.Evaluate("dev-1", map[string]interface{}{"temperature": 35.0})
	if !fired {
		t.Fatal("rule should have fired for temp=35")
	}

	// Check fire count
	r, _ := eng.GetRule("r1")
	if r.FireCount != 1 {
		t.Fatalf("expected fire_count=1, got %d", r.FireCount)
	}
}

func TestEvaluateDeviceFilter(t *testing.T) {
	eng := NewEngine(nil, nil)

	eng.AddRule(&Rule{
		ID:       "r2",
		Name:     "Specific device",
		DeviceID: "dev-A",
		Conditions: []Condition{
			{Field: "humidity", Operator: OpLT, Value: 20.0},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	// Different device - should not fire
	eng.Evaluate("dev-B", map[string]interface{}{"humidity": 10.0})
	r, _ := eng.GetRule("r2")
	if r.FireCount != 0 {
		t.Fatal("rule should not fire for wrong device")
	}

	// Correct device - should fire
	eng.Evaluate("dev-A", map[string]interface{}{"humidity": 10.0})
	r, _ = eng.GetRule("r2")
	if r.FireCount != 1 {
		t.Fatal("rule should fire for matching device")
	}
}

func TestEvaluateMultipleConditions(t *testing.T) {
	eng := NewEngine(nil, nil)

	eng.AddRule(&Rule{
		ID:   "r3",
		Name: "Temp and humidity",
		Conditions: []Condition{
			{Field: "temperature", Operator: OpGTE, Value: 30.0},
			{Field: "humidity", Operator: OpLTE, Value: 50.0},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	// Only one condition met
	eng.Evaluate("dev-1", map[string]interface{}{"temperature": 35.0, "humidity": 60.0})
	r, _ := eng.GetRule("r3")
	if r.FireCount != 0 {
		t.Fatal("should not fire when only one condition is met")
	}

	// Both conditions met
	eng.Evaluate("dev-1", map[string]interface{}{"temperature": 35.0, "humidity": 40.0})
	r, _ = eng.GetRule("r3")
	if r.FireCount != 1 {
		t.Fatal("should fire when all conditions met")
	}
}

func TestEvaluateEQOperator(t *testing.T) {
	eng := NewEngine(nil, nil)

	eng.AddRule(&Rule{
		ID:   "r4",
		Name: "Status check",
		Conditions: []Condition{
			{Field: "status", Operator: OpEQ, Value: "error"},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	eng.Evaluate("dev-1", map[string]interface{}{"status": "ok"})
	r, _ := eng.GetRule("r4")
	if r.FireCount != 0 {
		t.Fatal("should not fire for status=ok")
	}

	eng.Evaluate("dev-1", map[string]interface{}{"status": "error"})
	r, _ = eng.GetRule("r4")
	if r.FireCount != 1 {
		t.Fatal("should fire for status=error")
	}
}

func TestLoadFromJSON(t *testing.T) {
	eng := NewEngine(nil, nil)

	rules := `[
		{
			"id": "json-1",
			"name": "From JSON",
			"conditions": [{"field": "v", "operator": "gt", "value": 10}],
			"actions": [{"type": "log"}]
		}
	]`

	if err := eng.LoadFromJSON([]byte(rules)); err != nil {
		t.Fatalf("LoadFromJSON failed: %v", err)
	}

	list := eng.ListRules()
	if len(list) != 1 {
		t.Fatalf("expected 1 rule, got %d", len(list))
	}
	if list[0].ID != "json-1" {
		t.Fatalf("expected id json-1, got %s", list[0].ID)
	}
}

func TestExportJSON(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "exp-1",
		Name: "Export test",
		Conditions: []Condition{
			{Field: "x", Operator: OpGT, Value: 5},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	data, err := eng.ExportJSON()
	if err != nil {
		t.Fatalf("ExportJSON failed: %v", err)
	}

	var rules []*Rule
	if err := json.Unmarshal(data, &rules); err != nil {
		t.Fatalf("failed to unmarshal exported JSON: %v", err)
	}
	if len(rules) != 1 || rules[0].ID != "exp-1" {
		t.Fatal("exported rules don't match")
	}
}

func TestRemoveRule(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{ID: "del-1", Name: "to delete"})

	eng.RemoveRule("del-1")
	if _, ok := eng.GetRule("del-1"); ok {
		t.Fatal("rule should be removed")
	}
}

func TestDisabledRuleSkipped(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "dis-1",
		Name: "Disabled",
		Conditions: []Condition{
			{Field: "v", Operator: OpGT, Value: 0},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	// Disable the rule
	eng.mu.Lock()
	eng.rules["dis-1"].Enabled = false
	eng.mu.Unlock()

	eng.Evaluate("dev-1", map[string]interface{}{"v": 100})
	r, _ := eng.GetRule("dis-1")
	if r.FireCount != 0 {
		t.Fatal("disabled rule should not fire")
	}
}

func TestCompareOperators(t *testing.T) {
	tests := []struct {
		actual   interface{}
		op       Operator
		expected interface{}
		want     bool
	}{
		{10.0, OpGT, 5.0, true},
		{5.0, OpGT, 10.0, false},
		{10.0, OpGTE, 10.0, true},
		{5.0, OpLT, 10.0, true},
		{10.0, OpLTE, 10.0, true},
		{"hello", OpEQ, "hello", true},
		{"hello", OpNEQ, "world", true},
		{"foobar", OpContains, "bar", true},
		{"foobar", OpContains, "baz", false},
		{int(42), OpEQ, float64(42), true},
	}

	for i, tt := range tests {
		got := compare(tt.actual, tt.op, tt.expected)
		if got != tt.want {
			t.Errorf("test %d: compare(%v, %s, %v) = %v, want %v",
				i, tt.actual, tt.op, tt.expected, got, tt.want)
		}
	}
}
