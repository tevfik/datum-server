package rules

import (
	"encoding/json"
	"fmt"
	"sync"
	"sync/atomic"
	"testing"

	"datum-go/internal/webhook"
)

// ── Logic types ──────────────────────────────────────────────────────────────

func TestEvaluateORLogic(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "or-1",
		Name: "OR rule",
		Logic: RuleLogic{
			Type: LogicConditions,
			// OR: temperature > 40 OR humidity > 90
			Conditions: []Condition{
				{Field: "temperature", Operator: OpGT, Value: 40.0},
				{Field: "humidity", Operator: OpGT, Value: 90.0},
			},
			LogicOp: "or",
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	// Neither — should NOT fire
	eng.Evaluate("d", map[string]interface{}{"temperature": 30.0, "humidity": 50.0})
	r, _ := eng.GetRule("or-1")
	if r.FireCount != 0 {
		t.Fatal("OR rule should not fire when no condition is met")
	}

	// First condition only — should fire
	eng.Evaluate("d", map[string]interface{}{"temperature": 45.0, "humidity": 50.0})
	r, _ = eng.GetRule("or-1")
	if r.FireCount != 1 {
		t.Fatal("OR rule should fire when first condition is met")
	}

	// Second condition only — should fire again
	eng.Evaluate("d", map[string]interface{}{"temperature": 30.0, "humidity": 95.0})
	r, _ = eng.GetRule("or-1")
	if r.FireCount != 2 {
		t.Fatal("OR rule should fire when second condition is met")
	}
}

func TestEvaluateEmptyConditions(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:      "empty-cond",
		Name:    "Empty conditions",
		Logic:   RuleLogic{Type: LogicConditions},
		Actions: []RuleAction{{Type: ActionLog}},
	})
	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	r, _ := eng.GetRule("empty-cond")
	if r.FireCount != 0 {
		t.Fatal("rule with empty conditions should never fire")
	}
}

func TestEvaluateLuaLogicPath(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "lua-1",
		Name: "Lua rule",
		Logic: RuleLogic{
			Type:      LogicLua,
			LuaScript: `return ctx.data.v > 10`,
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	eng.Evaluate("d", map[string]interface{}{"v": 5.0})
	r, _ := eng.GetRule("lua-1")
	if r.FireCount != 0 {
		t.Fatal("lua rule should not fire for v=5")
	}

	eng.Evaluate("d", map[string]interface{}{"v": 15.0})
	r, _ = eng.GetRule("lua-1")
	if r.FireCount != 1 {
		t.Fatal("lua rule should fire for v=15")
	}
}

func TestEvaluateBlocklyLogicPath(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "blockly-1",
		Name: "Blockly rule",
		Logic: RuleLogic{
			Type: LogicBlockly,
			Conditions: []Condition{
				{Field: "temp", Operator: OpGT, Value: 20.0},
			},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	eng.Evaluate("d", map[string]interface{}{"temp": 25.0})
	r, _ := eng.GetRule("blockly-1")
	if r.FireCount != 1 {
		t.Fatal("blockly rule should fire when compiled conditions match")
	}
}

func TestEvaluateLuaSyntaxErrorNoFire(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "lua-err",
		Name: "Bad lua",
		Logic: RuleLogic{
			Type:      LogicLua,
			LuaScript: `not valid lua !!!`,
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})
	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	r, _ := eng.GetRule("lua-err")
	if r.FireCount != 0 {
		t.Fatal("rule should not fire on lua error")
	}
}

func TestEvaluateLuaEmptyScript(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "lua-empty",
		Name: "Empty lua",
		Logic: RuleLogic{
			Type:      LogicLua,
			LuaScript: "",
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})
	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	r, _ := eng.GetRule("lua-empty")
	if r.FireCount != 0 {
		t.Fatal("empty lua script should not fire")
	}
}

// ── Manual trigger ────────────────────────────────────────────────────────────

func TestEvaluateManual_FiresWhenConditionMet(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "man-1",
		Name: "Manual",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 5.0}},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	fired := eng.EvaluateManual("man-1", "d", map[string]interface{}{"v": 10.0})
	if !fired {
		t.Fatal("EvaluateManual should return true when rule fires")
	}
	r, _ := eng.GetRule("man-1")
	if r.FireCount != 1 {
		t.Fatal("expected fire_count=1 after manual trigger")
	}
}

func TestEvaluateManual_NoFireWhenConditionNotMet(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "man-2",
		Name: "Manual no fire",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 5.0}},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	fired := eng.EvaluateManual("man-2", "d", map[string]interface{}{"v": 1.0})
	if fired {
		t.Fatal("EvaluateManual should return false when rule does not fire")
	}
}

func TestEvaluateManual_NoFireForUnknownRule(t *testing.T) {
	eng := NewEngine(nil, nil)
	fired := eng.EvaluateManual("nonexistent", "d", map[string]interface{}{})
	if fired {
		t.Fatal("EvaluateManual should return false for unknown rule")
	}
}

func TestEvaluateManual_NoFireForDisabledRule(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "man-dis",
		Name: "Disabled manual",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})
	eng.mu.Lock()
	eng.rules["man-dis"].Enabled = false
	eng.mu.Unlock()

	fired := eng.EvaluateManual("man-dis", "d", map[string]interface{}{"v": 100.0})
	if fired {
		t.Fatal("disabled rule should not fire via EvaluateManual")
	}
}

// ── Trigger types ─────────────────────────────────────────────────────────────

func TestScheduledRuleSkippedByEvaluate(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "sched-1",
		Name: "Scheduled rule",
		Trigger: RuleTrigger{
			Type:     TriggerScheduled,
			DeviceID: "d",
			Schedule: "@every 1m",
		},
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	// Telemetry path must NOT evaluate scheduled rules
	eng.Evaluate("d", map[string]interface{}{"v": 100.0})
	r, _ := eng.GetRule("sched-1")
	if r.FireCount != 0 {
		t.Fatal("scheduled rule should not be evaluated in telemetry path")
	}
}

func TestManualTriggerTypeSkippedByEvaluate(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "manual-type-1",
		Name: "Manual trigger type",
		Trigger: RuleTrigger{
			Type:     TriggerManual,
			DeviceID: "d",
		},
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	eng.Evaluate("d", map[string]interface{}{"v": 100.0})
	r, _ := eng.GetRule("manual-type-1")
	if r.FireCount != 0 {
		t.Fatal("manual trigger type rule should not be evaluated in telemetry path")
	}
}

func TestGetScheduledRules(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:      "s1",
		Name:    "scheduled",
		Trigger: RuleTrigger{Type: TriggerScheduled, Schedule: "@every 1m"},
	})
	eng.AddRule(&Rule{
		ID:      "s2",
		Name:    "on_data",
		Trigger: RuleTrigger{Type: TriggerOnData},
	})
	// Disabled scheduled rule should be excluded
	eng.AddRule(&Rule{
		ID:      "s3",
		Name:    "disabled scheduled",
		Trigger: RuleTrigger{Type: TriggerScheduled, Schedule: "@every 1h"},
	})
	eng.mu.Lock()
	eng.rules["s3"].Enabled = false
	eng.mu.Unlock()

	scheduled := eng.GetScheduledRules()
	if len(scheduled) != 1 {
		t.Fatalf("expected 1 scheduled rule, got %d", len(scheduled))
	}
	if scheduled[0].ID != "s1" {
		t.Fatalf("expected s1, got %s", scheduled[0].ID)
	}
}

// ── User ownership ─────────────────────────────────────────────────────────────

func TestListRulesForUser(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{ID: "u1", Name: "User A rule", OwnerID: "alice"})
	eng.AddRule(&Rule{ID: "u2", Name: "User B rule", OwnerID: "bob"})
	eng.AddRule(&Rule{ID: "u3", Name: "User A rule 2", OwnerID: "alice"})

	aliceRules := eng.ListRulesForUser("alice")
	if len(aliceRules) != 2 {
		t.Fatalf("expected 2 rules for alice, got %d", len(aliceRules))
	}

	bobRules := eng.ListRulesForUser("bob")
	if len(bobRules) != 1 {
		t.Fatalf("expected 1 rule for bob, got %d", len(bobRules))
	}

	noneRules := eng.ListRulesForUser("nobody")
	if len(noneRules) != 0 {
		t.Fatalf("expected 0 rules for nobody, got %d", len(noneRules))
	}
}

// ── EffectiveX helpers ─────────────────────────────────────────────────────────

func TestEffectiveConditions_FallsBackToTopLevel(t *testing.T) {
	r := &Rule{
		Conditions: []Condition{{Field: "a", Operator: OpGT, Value: 1.0}},
		Logic:      RuleLogic{},
	}
	if len(r.effectiveConditions()) != 1 {
		t.Fatal("expected top-level conditions when Logic.Conditions is empty")
	}
}

func TestEffectiveConditions_PrefersLogicConditions(t *testing.T) {
	r := &Rule{
		Conditions: []Condition{{Field: "a", Operator: OpGT, Value: 1.0}},
		Logic: RuleLogic{
			Conditions: []Condition{
				{Field: "b", Operator: OpLT, Value: 5.0},
				{Field: "c", Operator: OpEQ, Value: "x"},
			},
		},
	}
	conds := r.effectiveConditions()
	if len(conds) != 2 || conds[0].Field != "b" {
		t.Fatal("expected Logic.Conditions to take precedence")
	}
}

func TestEffectiveDeviceID_FallsBackToTopLevel(t *testing.T) {
	r := &Rule{DeviceID: "dev-top", Trigger: RuleTrigger{}}
	if r.effectiveDeviceID() != "dev-top" {
		t.Fatal("expected top-level DeviceID fallback")
	}
}

func TestEffectiveDeviceID_PreferesTrigger(t *testing.T) {
	r := &Rule{DeviceID: "dev-top", Trigger: RuleTrigger{DeviceID: "dev-trigger"}}
	if r.effectiveDeviceID() != "dev-trigger" {
		t.Fatal("expected Trigger.DeviceID to take precedence")
	}
}

func TestEffectiveTriggerType_DefaultsToOnData(t *testing.T) {
	r := &Rule{Trigger: RuleTrigger{}}
	if r.effectiveTriggerType() != TriggerOnData {
		t.Fatalf("expected TriggerOnData default, got %s", r.effectiveTriggerType())
	}
}

// ── Actions ────────────────────────────────────────────────────────────────────

func TestFireActionWebhook(t *testing.T) {
	fired := make(chan string, 1)
	disp := webhook.NewDispatcher()
	defer disp.Close()

	disp.Subscribe(&webhook.Subscription{
		ID:  "sub-1",
		URL: "http://example.com/wh",
		Events: []webhook.EventType{
			webhook.EventRuleTriggered,
		},
	})

	eng := NewEngine(disp, nil)
	eng.AddRule(&Rule{
		ID:   "wh-1",
		Name: "Webhook action",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{{Type: ActionWebhook}},
	})

	_ = fired
	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	r, _ := eng.GetRule("wh-1")
	if r.FireCount != 1 {
		t.Fatal("expected rule to fire and increment FireCount")
	}
}

func TestFireActionMQTT(t *testing.T) {
	var publishedTopic atomic.Value
	var publishCount int32

	eng := NewEngine(nil, func(topic string, payload []byte) error {
		publishedTopic.Store(topic)
		atomic.AddInt32(&publishCount, 1)
		return nil
	})

	// New structured form: mqtt_device + mqtt_cmd → scoped topic.
	eng.AddRule(&Rule{
		ID:   "mqtt-action",
		Name: "MQTT action",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			{Type: ActionMQTT, Config: map[string]interface{}{"mqtt_device": "fan-01", "mqtt_cmd": "set"}},
		},
	})

	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	if atomic.LoadInt32(&publishCount) != 1 {
		t.Fatal("expected exactly 1 MQTT publish")
	}
	if publishedTopic.Load().(string) != "dev/fan-01/cmd/set" {
		t.Fatalf("expected scoped topic 'dev/fan-01/cmd/set', got '%v'", publishedTopic.Load())
	}
}

func TestFireActionMQTT_LegacyTopicAllowed(t *testing.T) {
	// Legacy "topic" with "dev/" prefix should still work.
	var publishedTopic atomic.Value

	eng := NewEngine(nil, func(topic string, payload []byte) error {
		publishedTopic.Store(topic)
		return nil
	})

	eng.AddRule(&Rule{
		ID:   "mqtt-legacy",
		Name: "MQTT legacy",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			{Type: ActionMQTT, Config: map[string]interface{}{"topic": "dev/sensor-01/cmd/reboot"}},
		},
	})

	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	if publishedTopic.Load().(string) != "dev/sensor-01/cmd/reboot" {
		t.Fatalf("expected 'dev/sensor-01/cmd/reboot', got '%v'", publishedTopic.Load())
	}
}

func TestFireActionMQTT_LegacyTopicBlocked(t *testing.T) {
	// Legacy "topic" without "dev/" prefix must be rejected → falls back to default.
	var publishedTopic atomic.Value

	eng := NewEngine(nil, func(topic string, payload []byte) error {
		publishedTopic.Store(topic)
		return nil
	})

	eng.AddRule(&Rule{
		ID:   "mqtt-blocked",
		Name: "MQTT blocked unsafe topic",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			// "system/admin/reset" does not start with "dev/" — should be rejected.
			{Type: ActionMQTT, Config: map[string]interface{}{"topic": "system/admin/reset"}},
		},
	})

	eng.Evaluate("my-device", map[string]interface{}{"v": 1.0})
	got := publishedTopic.Load().(string)
	if got == "system/admin/reset" {
		t.Fatal("unsafe topic should have been rejected, not published as-is")
	}
	// Must fall back to the safe default.
	if got != "dev/my-device/alert" {
		t.Fatalf("expected fallback 'dev/my-device/alert', got '%v'", got)
	}
}

func TestFireActionMQTT_ScopedDefaultCmd(t *testing.T) {
	// mqtt_device present but mqtt_cmd empty → cmd defaults to "set".
	var publishedTopic atomic.Value

	eng := NewEngine(nil, func(topic string, payload []byte) error {
		publishedTopic.Store(topic)
		return nil
	})

	eng.AddRule(&Rule{
		ID: "mqtt-default-cmd",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			{Type: ActionMQTT, Config: map[string]interface{}{"mqtt_device": "actuator-5"}},
		},
	})

	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	if publishedTopic.Load().(string) != "dev/actuator-5/cmd/set" {
		t.Fatalf("expected 'dev/actuator-5/cmd/set', got '%v'", publishedTopic.Load())
	}
}

func TestFireActionMQTT_CustomPayload(t *testing.T) {
	// Valid JSON payload in config should be passed through.
	var receivedPayload []byte

	eng := NewEngine(nil, func(topic string, payload []byte) error {
		receivedPayload = payload
		return nil
	})

	eng.AddRule(&Rule{
		ID: "mqtt-custom-payload",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			{Type: ActionMQTT, Config: map[string]interface{}{
				"mqtt_device": "fan-01",
				"mqtt_cmd":    "set",
				"payload":     `{"fan_speed": 3000}`,
			}},
		},
	})

	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	var parsed map[string]interface{}
	if err := json.Unmarshal(receivedPayload, &parsed); err != nil {
		t.Fatalf("payload is not valid JSON: %v", err)
	}
	if parsed["fan_speed"] != float64(3000) {
		t.Fatalf("expected fan_speed=3000, got %v", parsed["fan_speed"])
	}
}

func TestFireActionMQTT_InvalidPayloadDropped(t *testing.T) {
	// Invalid JSON payload should be dropped and auto-generated payload used instead.
	var receivedPayload []byte

	eng := NewEngine(nil, func(topic string, payload []byte) error {
		receivedPayload = payload
		return nil
	})

	eng.AddRule(&Rule{
		ID: "mqtt-bad-payload",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			{Type: ActionMQTT, Config: map[string]interface{}{
				"mqtt_device": "fan-01",
				"mqtt_cmd":    "set",
				"payload":     `not valid json <<`,
			}},
		},
	})

	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	var parsed map[string]interface{}
	if err := json.Unmarshal(receivedPayload, &parsed); err != nil {
		t.Fatalf("should have fallen back to auto-generated JSON payload: %v", err)
	}
	if _, hasRuleID := parsed["rule_id"]; !hasRuleID {
		t.Fatal("auto-generated payload should contain rule_id")
	}
}

func TestFireActionMQTT_DefaultTopic(t *testing.T) {
	var publishedTopic atomic.Value

	eng := NewEngine(nil, func(topic string, payload []byte) error {
		publishedTopic.Store(topic)
		return nil
	})

	eng.AddRule(&Rule{
		ID:   "mqtt-default-topic",
		Name: "MQTT default topic",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			{Type: ActionMQTT, Config: map[string]interface{}{}}, // no topic or device
		},
	})

	eng.Evaluate("my-device", map[string]interface{}{"v": 1.0})
	expected := "dev/my-device/alert"
	if publishedTopic.Load().(string) != expected {
		t.Fatalf("expected default topic '%s', got '%v'", expected, publishedTopic.Load())
	}
}

func TestFireActionMQTT_PublishError(t *testing.T) {
	// Publish error should not panic; rule still increments FireCount
	eng := NewEngine(nil, func(topic string, payload []byte) error {
		return fmt.Errorf("mqtt error")
	})

	eng.AddRule(&Rule{
		ID:   "mqtt-err",
		Name: "MQTT error",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			{Type: ActionMQTT, Config: map[string]interface{}{"topic": "t"}},
		},
	})

	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	r, _ := eng.GetRule("mqtt-err")
	if r.FireCount != 1 {
		t.Fatal("fire count should be 1 even if MQTT publish fails")
	}
}

func TestFireActionCommand(t *testing.T) {
	var publishedTopic atomic.Value

	eng := NewEngine(nil, func(topic string, payload []byte) error {
		publishedTopic.Store(topic)
		return nil
	})

	eng.AddRule(&Rule{
		ID:   "cmd-action",
		Name: "Command action",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			{Type: ActionCommand, Config: map[string]interface{}{
				"target_device": "relay-01",
				"payload":       `{"state":"on"}`,
			}},
		},
	})

	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	expected := "dev/relay-01/cmd/set"
	if publishedTopic.Load().(string) != expected {
		t.Fatalf("expected command topic '%s', got '%v'", expected, publishedTopic.Load())
	}
}

func TestFireActionCommand_DefaultsToTriggeringDevice(t *testing.T) {
	var publishedTopic atomic.Value

	eng := NewEngine(nil, func(topic string, payload []byte) error {
		publishedTopic.Store(topic)
		return nil
	})

	eng.AddRule(&Rule{
		ID:   "cmd-default",
		Name: "Command default device",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			{Type: ActionCommand, Config: map[string]interface{}{}},
		},
	})

	eng.Evaluate("source-device", map[string]interface{}{"v": 1.0})
	expected := "dev/source-device/cmd/set"
	if publishedTopic.Load().(string) != expected {
		t.Fatalf("expected '%s', got '%v'", expected, publishedTopic.Load())
	}
}

func TestFireActionNotify(t *testing.T) {
	disp := webhook.NewDispatcher()
	defer disp.Close()

	eng := NewEngine(disp, nil)
	eng.AddRule(&Rule{
		ID:   "notify-action",
		Name: "Notify action",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{
			{Type: ActionNotify, Config: map[string]interface{}{
				"title":   "Alert",
				"message": "Something happened",
			}},
		},
	})

	eng.Evaluate("d", map[string]interface{}{"v": 1.0})
	r, _ := eng.GetRule("notify-action")
	if r.FireCount != 1 {
		t.Fatal("notify action should fire")
	}
}

// ── toFloat64 edge cases ──────────────────────────────────────────────────────

func TestToFloat64(t *testing.T) {
	tests := []struct {
		in   interface{}
		want float64
	}{
		{float64(3.14), 3.14},
		{float32(1.5), float64(float32(1.5))},
		{int(42), 42},
		{int64(100), 100},
		{json.Number("7.5"), 7.5},
		{"string", 0},
		{nil, 0},
		{true, 0},
	}
	for _, tt := range tests {
		got := toFloat64(tt.in)
		if got != tt.want {
			t.Errorf("toFloat64(%v) = %v, want %v", tt.in, got, tt.want)
		}
	}
}

// ── matchConditions: missing field ────────────────────────────────────────────

func TestMatchConditions_MissingFieldAND(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "missing-field",
		Name: "Missing field",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "nonexistent", Operator: OpGT, Value: 0.0}},
			LogicOp:    "and",
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})
	eng.Evaluate("d", map[string]interface{}{"other": 100.0})
	r, _ := eng.GetRule("missing-field")
	if r.FireCount != 0 {
		t.Fatal("rule should not fire when field is missing (AND)")
	}
}

func TestMatchConditions_MissingFieldOR(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "missing-field-or",
		Name: "Missing field OR",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "nonexistent", Operator: OpGT, Value: 0.0}},
			LogicOp:    "or",
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})
	eng.Evaluate("d", map[string]interface{}{"other": 100.0})
	r, _ := eng.GetRule("missing-field-or")
	if r.FireCount != 0 {
		t.Fatal("rule should not fire when field is missing (OR)")
	}
}

// ── LoadFromJSON edge cases ────────────────────────────────────────────────────

func TestLoadFromJSON_InvalidJSON(t *testing.T) {
	eng := NewEngine(nil, nil)
	err := eng.LoadFromJSON([]byte("{bad json"))
	if err == nil {
		t.Fatal("expected error for invalid JSON")
	}
}

func TestLoadFromJSON_SetsEnabledByDefault(t *testing.T) {
	eng := NewEngine(nil, nil)
	data := `[{"id":"j1","name":"J1","enabled":false,"conditions":[{"field":"v","operator":"gt","value":0}],"actions":[{"type":"log"}]}]`
	if err := eng.LoadFromJSON([]byte(data)); err != nil {
		t.Fatal(err)
	}
	r, ok := eng.GetRule("j1")
	if !ok {
		t.Fatal("rule j1 not found")
	}
	if !r.Enabled {
		t.Fatal("LoadFromJSON should set Enabled=true even if false in JSON")
	}
}

// ── Concurrency ──────────────────────────────────────────────────────────────

func TestEngine_ConcurrentEvaluate(t *testing.T) {
	eng := NewEngine(nil, nil)
	for i := 0; i < 100; i++ {
		id := fmt.Sprintf("r%d", i)
		eng.AddRule(&Rule{
			ID:   id,
			Name: id,
			Logic: RuleLogic{
				Type:       LogicConditions,
				Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
			},
			Actions: []RuleAction{{Type: ActionLog}},
		})
	}

	var wg sync.WaitGroup
	for g := 0; g < 20; g++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			eng.Evaluate("d", map[string]interface{}{"v": 1.0})
		}()
	}
	wg.Wait()
}

// ── NEQ operator ──────────────────────────────────────────────────────────────

func TestEvaluateNEQOperator(t *testing.T) {
	eng := NewEngine(nil, nil)
	eng.AddRule(&Rule{
		ID:   "neq-1",
		Name: "NEQ test",
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "status", Operator: OpNEQ, Value: "ok"}},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	eng.Evaluate("d", map[string]interface{}{"status": "ok"})
	r, _ := eng.GetRule("neq-1")
	if r.FireCount != 0 {
		t.Fatal("NEQ should not fire when values are equal")
	}

	eng.Evaluate("d", map[string]interface{}{"status": "error"})
	r, _ = eng.GetRule("neq-1")
	if r.FireCount != 1 {
		t.Fatal("NEQ should fire when values differ")
	}
}
