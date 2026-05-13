// Package integration provides end-to-end integration tests for the datum-server
// rule engine.  Tests stand up a real in-memory storage + Engine + Scheduler +
// TelemetryProcessor + notify Dispatcher and exercise the full pipeline from
// data ingestion through rule evaluation to notification dispatch.
//
// Run with:
//
//	go test ./tests/integration/ -v -tags integration
package integration

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"datum-go/internal/notify"
	"datum-go/internal/processing"
	"datum-go/internal/rules"
	"datum-go/internal/storage"
	"datum-go/internal/webhook"
)

// ─── helpers ──────────────────────────────────────────────────────────────────

func newTestStorage(t *testing.T) storage.Provider {
	t.Helper()
	tmpDir := t.TempDir()
	store, err := storage.New(
		":memory:",
		filepath.Join(tmpDir, "ts"),
		0,
	)
	if err != nil {
		t.Fatalf("storage.New: %v", err)
	}
	t.Cleanup(func() { store.Close() })
	store.InitializeSystem("TestPlatform", true, 30)
	return store
}

func addDevice(t *testing.T, store storage.Provider, id, userID string) {
	t.Helper()
	if err := store.CreateDevice(&storage.Device{
		ID:        id,
		UserID:    userID,
		Name:      id,
		Type:      "sensor",
		Status:    "active",
		APIKey:    "sk_" + id,
		CreatedAt: time.Now(),
	}); err != nil {
		t.Fatalf("CreateDevice(%s): %v", id, err)
	}
}

// ─── 1. OnData trigger → condition → log action ────────────────────────────

func TestRuleEngine_OnData_LogAction(t *testing.T) {
	store := newTestStorage(t)
	eng := rules.NewEngine(nil, nil)

	eng.AddRule(&rules.Rule{
		ID:   "r-log",
		Name: "Temp alert log",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerOnData,
			DeviceID: "sensor-1",
		},
		Logic: rules.RuleLogic{
			Type: rules.LogicConditions,
			Conditions: []rules.Condition{
				{Field: "temperature", Operator: rules.OpGT, Value: 30.0},
			},
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})

	addDevice(t, store, "sensor-1", "user-1")
	tp := processing.NewTelemetryProcessor(store)
	tp.SetRuleEngine(eng)
	defer tp.Close()

	_, err := tp.Process("sensor-1", map[string]interface{}{"temperature": 35.0})
	if err != nil {
		t.Fatalf("Process: %v", err)
	}
	time.Sleep(10 * time.Millisecond) // let workers flush

	r, ok := eng.GetRule("r-log")
	if !ok {
		t.Fatal("rule not found")
	}
	if r.FireCount != 1 {
		t.Fatalf("expected FireCount=1, got %d", r.FireCount)
	}
}

// ─── 2. OnData → no fire when condition not met ────────────────────────────

func TestRuleEngine_OnData_NoFire(t *testing.T) {
	store := newTestStorage(t)
	eng := rules.NewEngine(nil, nil)

	eng.AddRule(&rules.Rule{
		ID:   "r-nf",
		Name: "Below threshold",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerOnData,
			DeviceID: "sensor-nf",
		},
		Logic: rules.RuleLogic{
			Type: rules.LogicConditions,
			Conditions: []rules.Condition{
				{Field: "co2", Operator: rules.OpGT, Value: 1000.0},
			},
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})

	addDevice(t, store, "sensor-nf", "user-1")
	tp := processing.NewTelemetryProcessor(store)
	tp.SetRuleEngine(eng)
	defer tp.Close()

	tp.Process("sensor-nf", map[string]interface{}{"co2": 400.0})
	time.Sleep(10 * time.Millisecond)

	r, _ := eng.GetRule("r-nf")
	if r.FireCount != 0 {
		t.Fatalf("rule should not fire when condition not met, got FireCount=%d", r.FireCount)
	}
}

// ─── 3. OnData → MQTT action ────────────────────────────────────────────────

func TestRuleEngine_OnData_MQTTAction(t *testing.T) {
	var mu sync.Mutex
	mqttCalls := make(map[string][]byte)

	mqttPublish := func(topic string, payload []byte) error {
		mu.Lock()
		mqttCalls[topic] = payload
		mu.Unlock()
		return nil
	}

	eng := rules.NewEngine(nil, mqttPublish)
	store := newTestStorage(t)
	addDevice(t, store, "sensor-mqtt", "user-1")

	eng.AddRule(&rules.Rule{
		ID:   "r-mqtt",
		Name: "MQTT alert",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerOnData,
			DeviceID: "sensor-mqtt",
		},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "pressure", Operator: rules.OpGT, Value: 1013.0}},
		},
		Actions: []rules.RuleAction{
			{Type: rules.ActionMQTT, Config: map[string]interface{}{"topic": "alerts/pressure"}},
		},
	})

	tp := processing.NewTelemetryProcessor(store)
	tp.SetRuleEngine(eng)
	defer tp.Close()

	tp.Process("sensor-mqtt", map[string]interface{}{"pressure": 1020.0})
	time.Sleep(20 * time.Millisecond)

	mu.Lock()
	payload, sent := mqttCalls["alerts/pressure"]
	mu.Unlock()

	if !sent {
		t.Fatal("expected MQTT message on 'alerts/pressure'")
	}
	var msg map[string]interface{}
	if err := json.Unmarshal(payload, &msg); err != nil {
		t.Fatalf("non-JSON MQTT payload: %v", err)
	}
	if msg["rule_id"] != "r-mqtt" {
		t.Fatalf("unexpected rule_id in payload: %v", msg["rule_id"])
	}
}

// ─── 4. OnData → Webhook action ─────────────────────────────────────────────

func TestRuleEngine_OnData_WebhookAction(t *testing.T) {
	received := make(chan struct{}, 1)
	webhookSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		received <- struct{}{}
		w.WriteHeader(http.StatusOK)
	}))
	defer webhookSrv.Close()

	// Wire a real webhook.Dispatcher; register the test HTTP server as subscriber.
	wDisp := webhook.NewDispatcher()
	defer wDisp.Close()
	wDisp.Subscribe(&webhook.Subscription{
		ID:     "test-sub",
		URL:    webhookSrv.URL + "/hook",
		Events: []webhook.EventType{webhook.EventRuleTriggered},
	})

	eng := rules.NewEngine(wDisp, nil)
	store := newTestStorage(t)
	addDevice(t, store, "sensor-wh", "user-1")

	eng.AddRule(&rules.Rule{
		ID:   "r-wh",
		Name: "Webhook rule",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerOnData,
			DeviceID: "sensor-wh",
		},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "battery", Operator: rules.OpLT, Value: 20.0}},
		},
		Actions: []rules.RuleAction{
			{Type: rules.ActionWebhook},
		},
	})

	tp := processing.NewTelemetryProcessor(store)
	tp.SetRuleEngine(eng)
	defer tp.Close()

	tp.Process("sensor-wh", map[string]interface{}{"battery": 15.0})

	select {
	case <-received:
	case <-time.After(3 * time.Second):
		t.Fatal("webhook not called within timeout")
	}
}

// ─── 5. OnData → Notify action ────────────────────────────────────────────

func TestRuleEngine_OnData_NotifyAction(t *testing.T) {
	var notifyCalled int32
	ch := &captureChannel{onSend: func(n notify.Notification) {
		atomic.AddInt32(&notifyCalled, 1)
	}}
	d := notify.NewDispatcher()
	d.Register(ch)
	d.SetDefaultChannels([]string{"capture"})

	eng := rules.NewEngine(nil, nil)
	eng.SetNotifyDispatcher(d)

	store := newTestStorage(t)
	addDevice(t, store, "sensor-notify", "user-notify")

	eng.AddRule(&rules.Rule{
		ID:      "r-notify",
		Name:    "Notify rule",
		OwnerID: "user-notify",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerOnData,
			DeviceID: "sensor-notify",
		},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "smoke", Operator: rules.OpGT, Value: 0.5}},
		},
		Actions: []rules.RuleAction{
			{Type: rules.ActionNotify, Config: map[string]interface{}{
				"title":   "Smoke Alert",
				"message": "Smoke detected",
			}},
		},
	})

	tp := processing.NewTelemetryProcessor(store)
	tp.SetRuleEngine(eng)
	defer tp.Close()

	tp.Process("sensor-notify", map[string]interface{}{"smoke": 0.8})
	time.Sleep(200 * time.Millisecond)

	if atomic.LoadInt32(&notifyCalled) != 1 {
		t.Fatalf("expected 1 notification, got %d", atomic.LoadInt32(&notifyCalled))
	}
}

// ─── 6. Lua logic ─────────────────────────────────────────────────────────

func TestRuleEngine_LuaLogic(t *testing.T) {
	eng := rules.NewEngine(nil, nil)
	store := newTestStorage(t)
	addDevice(t, store, "sensor-lua", "user-1")

	eng.AddRule(&rules.Rule{
		ID:   "r-lua",
		Name: "Lua temperature+humidity check",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerOnData,
			DeviceID: "sensor-lua",
		},
		Logic: rules.RuleLogic{
			Type:      rules.LogicLua,
			LuaScript: `local t = ctx.data.temperature; local h = ctx.data.humidity; return t > 30 and h < 40`,
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})

	tp := processing.NewTelemetryProcessor(store)
	tp.SetRuleEngine(eng)
	defer tp.Close()

	// Should fire
	tp.Process("sensor-lua", map[string]interface{}{"temperature": 35.0, "humidity": 30.0})
	time.Sleep(10 * time.Millisecond)

	r, _ := eng.GetRule("r-lua")
	if r.FireCount != 1 {
		t.Fatalf("Lua rule should fire, FireCount=%d", r.FireCount)
	}

	// Should not fire
	tp.Process("sensor-lua", map[string]interface{}{"temperature": 25.0, "humidity": 50.0})
	time.Sleep(10 * time.Millisecond)

	r, _ = eng.GetRule("r-lua")
	if r.FireCount != 1 {
		t.Fatalf("Lua rule should not fire again, FireCount=%d", r.FireCount)
	}
}

// ─── 7. Rule disabled → no fire ─────────────────────────────────────────

func TestRuleEngine_DisabledRule_NoFire(t *testing.T) {
	eng := rules.NewEngine(nil, nil)
	store := newTestStorage(t)
	addDevice(t, store, "sensor-dis", "user-1")

	eng.AddRule(&rules.Rule{
		ID:      "r-dis",
		Name:    "Disabled",
		Trigger: rules.RuleTrigger{Type: rules.TriggerOnData, DeviceID: "sensor-dis"},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "v", Operator: rules.OpGT, Value: 0.0}},
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})
	// AddRule defaults to enabled=true, so explicitly disable
	r, _ := eng.GetRule("r-dis")
	r.Enabled = false

	tp := processing.NewTelemetryProcessor(store)
	tp.SetRuleEngine(eng)
	defer tp.Close()

	tp.Process("sensor-dis", map[string]interface{}{"v": 99.0})
	time.Sleep(10 * time.Millisecond)

	rAfter, _ := eng.GetRule("r-dis")
	if rAfter.FireCount != 0 {
		t.Fatalf("disabled rule should not fire, FireCount=%d", rAfter.FireCount)
	}
}

// ─── 8. Scheduler → periodic rule evaluation ────────────────────────────

func TestScheduler_PeriodicEvaluation(t *testing.T) {
	eng := rules.NewEngine(nil, nil)
	store := newTestStorage(t)
	addDevice(t, store, "sensor-sched", "user-1")

	// Store data that will satisfy the rule condition
	store.StoreData(&storage.DataPoint{
		DeviceID:  "sensor-sched",
		Timestamp: time.Now(),
		Data:      map[string]interface{}{"level": 75.0},
	})

	eng.AddRule(&rules.Rule{
		ID:   "r-sched",
		Name: "Scheduled check",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerScheduled,
			DeviceID: "sensor-sched",
			Schedule: "@every 1s", // cron library rounds sub-second to 1s
		},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "level", Operator: rules.OpGT, Value: 50.0}},
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})

	sched := rules.NewScheduler(eng, store)
	sched.Start()
	defer sched.Stop()

	sched.RegisterRule(eng.MustGetRule("r-sched"))

	// Wait for at least 2 scheduled evaluations
	time.Sleep(2500 * time.Millisecond)

	r, _ := eng.GetRule("r-sched")
	if r.FireCount < 1 {
		t.Fatalf("scheduled rule should have fired at least once, FireCount=%d", r.FireCount)
	}
}

// ─── 9. Manual trigger via EvaluateManual ────────────────────────────────

func TestRuleEngine_ManualTrigger(t *testing.T) {
	eng := rules.NewEngine(nil, nil)

	eng.AddRule(&rules.Rule{
		ID:   "r-man",
		Name: "Manual rule",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerManual,
			DeviceID: "sensor-man",
		},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "v", Operator: rules.OpGT, Value: 0.0}},
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})

	fired := eng.EvaluateManual("r-man", "sensor-man", map[string]interface{}{"v": 1.0})
	if !fired {
		t.Fatal("manual trigger should fire the rule")
	}

	r, _ := eng.GetRule("r-man")
	if r.FireCount != 1 {
		t.Fatalf("FireCount should be 1, got %d", r.FireCount)
	}
}

// ─── 10. Multi-rule fan-out (same device, different rules) ───────────────

func TestRuleEngine_MultiRule_FanOut(t *testing.T) {
	var count int32
	mqttPublish := func(topic string, payload []byte) error {
		atomic.AddInt32(&count, 1)
		return nil
	}

	eng := rules.NewEngine(nil, mqttPublish)

	for i := 1; i <= 3; i++ {
		eng.AddRule(&rules.Rule{
			ID:   fmt.Sprintf("r-fan-%d", i),
			Name: fmt.Sprintf("Fan rule %d", i),
			Trigger: rules.RuleTrigger{
				Type:     rules.TriggerOnData,
				DeviceID: "sensor-fan",
			},
			Logic: rules.RuleLogic{
				Type:       rules.LogicConditions,
				Conditions: []rules.Condition{{Field: "v", Operator: rules.OpGT, Value: 0.0}},
			},
			Actions: []rules.RuleAction{
				{Type: rules.ActionMQTT, Config: map[string]interface{}{"topic": fmt.Sprintf("fan/rule%d", i)}},
			},
		})
	}

	eng.Evaluate("sensor-fan", map[string]interface{}{"v": 5.0})
	time.Sleep(50 * time.Millisecond)

	if atomic.LoadInt32(&count) != 3 {
		t.Fatalf("expected 3 MQTT publishes (one per rule), got %d", atomic.LoadInt32(&count))
	}
}

// ─── 11. DeviceID scoping: wrong device does not fire rule ───────────────

func TestRuleEngine_DeviceScoping(t *testing.T) {
	eng := rules.NewEngine(nil, nil)

	eng.AddRule(&rules.Rule{
		ID:   "r-scope",
		Name: "Device scoped",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerOnData,
			DeviceID: "device-A",
		},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "v", Operator: rules.OpGT, Value: 0.0}},
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})

	// Data from device-B should NOT fire the rule for device-A
	eng.Evaluate("device-B", map[string]interface{}{"v": 99.0})

	r, _ := eng.GetRule("r-scope")
	if r.FireCount != 0 {
		t.Fatalf("rule scoped to device-A should not fire for device-B data, FireCount=%d", r.FireCount)
	}

	// Data from device-A SHOULD fire
	eng.Evaluate("device-A", map[string]interface{}{"v": 99.0})

	r, _ = eng.GetRule("r-scope")
	if r.FireCount != 1 {
		t.Fatalf("rule should fire for device-A data, FireCount=%d", r.FireCount)
	}
}

// ─── 12. Rule with OR logic ─────────────────────────────────────────────

func TestRuleEngine_ORLogic(t *testing.T) {
	eng := rules.NewEngine(nil, nil)

	eng.AddRule(&rules.Rule{
		ID:   "r-or",
		Name: "OR condition",
		Trigger: rules.RuleTrigger{
			Type:     rules.TriggerOnData,
			DeviceID: "sensor-or",
		},
		Logic: rules.RuleLogic{
			Type:    rules.LogicConditions,
			LogicOp: "or",
			Conditions: []rules.Condition{
				{Field: "temp", Operator: rules.OpGT, Value: 40.0},
				{Field: "humidity", Operator: rules.OpLT, Value: 10.0},
			},
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})

	// Only temp matches
	eng.Evaluate("sensor-or", map[string]interface{}{"temp": 45.0, "humidity": 50.0})
	r, _ := eng.GetRule("r-or")
	if r.FireCount != 1 {
		t.Fatalf("OR: expected fire when first condition matches, FireCount=%d", r.FireCount)
	}

	// Only humidity matches
	eng.Evaluate("sensor-or", map[string]interface{}{"temp": 20.0, "humidity": 5.0})
	r, _ = eng.GetRule("r-or")
	if r.FireCount != 2 {
		t.Fatalf("OR: expected fire when second condition matches, FireCount=%d", r.FireCount)
	}

	// Neither matches
	eng.Evaluate("sensor-or", map[string]interface{}{"temp": 20.0, "humidity": 50.0})
	r, _ = eng.GetRule("r-or")
	if r.FireCount != 2 {
		t.Fatalf("OR: should not fire when neither condition matches, FireCount=%d", r.FireCount)
	}
}

// ─── 13. Concurrent data points: no race conditions ──────────────────────

func TestRuleEngine_ConcurrentEvaluations(t *testing.T) {
	eng := rules.NewEngine(nil, nil)

	eng.AddRule(&rules.Rule{
		ID:      "r-conc",
		Name:    "Concurrent rule",
		Trigger: rules.RuleTrigger{Type: rules.TriggerOnData, DeviceID: "sensor-conc"},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "v", Operator: rules.OpGT, Value: 0.0}},
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})

	const goroutines = 50
	var wg sync.WaitGroup
	wg.Add(goroutines)
	for i := 0; i < goroutines; i++ {
		go func(n int) {
			defer wg.Done()
			eng.Evaluate("sensor-conc", map[string]interface{}{"v": float64(n + 1)})
		}(i)
	}
	wg.Wait()

	r, _ := eng.GetRule("r-conc")
	if r.FireCount != goroutines {
		t.Fatalf("expected %d fires, got %d", goroutines, r.FireCount)
	}
}

// ─── 14. Full pipeline: HTTP API → engine → storage ──────────────────────

func TestRuleEngine_FullHTTPPipeline(t *testing.T) {
	store := newTestStorage(t)
	addDevice(t, store, "sensor-http", "user-http")

	eng := rules.NewEngine(nil, nil)

	// Add rule directly (simulating the API layer)
	eng.AddRule(&rules.Rule{
		ID:      "r-http",
		Name:    "HTTP pipeline",
		OwnerID: "user-http",
		Trigger: rules.RuleTrigger{Type: rules.TriggerOnData, DeviceID: "sensor-http"},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "value", Operator: rules.OpGTE, Value: 100.0}},
		},
		Actions: []rules.RuleAction{{Type: rules.ActionLog}},
	})

	// Simulate telemetry ingestion
	tp := processing.NewTelemetryProcessor(store)
	tp.SetRuleEngine(eng)
	defer tp.Close()

	for i := 0; i < 5; i++ {
		v := float64(i * 30) // 0, 30, 60, 90, 120
		tp.Process("sensor-http", map[string]interface{}{"value": v})
	}
	time.Sleep(20 * time.Millisecond)

	// Only 120 >= 100, so FireCount should be 1
	r, _ := eng.GetRule("r-http")
	if r.FireCount != 1 {
		t.Fatalf("expected 1 fire (only value=120 matches), got %d", r.FireCount)
	}
}

// ─── 15. Command action → MQTT publish ───────────────────────────────────

func TestRuleEngine_CommandAction(t *testing.T) {
	var mu sync.Mutex
	cmdTopics := make(map[string][]byte)

	mqttPublish := func(topic string, payload []byte) error {
		mu.Lock()
		cmdTopics[topic] = payload
		mu.Unlock()
		return nil
	}

	store := newTestStorage(t)
	addDevice(t, store, "actuator-1", "user-cmd")

	eng := rules.NewEngine(nil, mqttPublish)
	eng.AddRule(&rules.Rule{
		ID:      "r-cmd",
		Name:    "Command rule",
		Trigger: rules.RuleTrigger{Type: rules.TriggerOnData, DeviceID: "actuator-1"},
		Logic: rules.RuleLogic{
			Type:       rules.LogicConditions,
			Conditions: []rules.Condition{{Field: "motion", Operator: rules.OpGT, Value: 0.0}},
		},
		Actions: []rules.RuleAction{
			{Type: rules.ActionCommand, Config: map[string]interface{}{
				"target_device": "actuator-1",
				"payload":       `{"action":"activate"}`,
			}},
		},
	})

	tp := processing.NewTelemetryProcessor(store)
	tp.SetRuleEngine(eng)
	defer tp.Close()

	tp.Process("actuator-1", map[string]interface{}{"motion": 1.0})
	time.Sleep(50 * time.Millisecond)

	expectedTopic := "dev/actuator-1/cmd/set"
	mu.Lock()
	payload, published := cmdTopics[expectedTopic]
	mu.Unlock()

	if !published {
		t.Fatalf("expected MQTT command on topic %q", expectedTopic)
	}
	if string(payload) != `{"action":"activate"}` {
		t.Fatalf("unexpected command payload: %s", payload)
	}
}

// ─── capture channel helper ───────────────────────────────────────────────────

type captureChannel struct {
	onSend func(notify.Notification)
}

func (c *captureChannel) Name() string { return "capture" }
func (c *captureChannel) Send(_ context.Context, n notify.Notification) error {
	if c.onSend != nil {
		c.onSend(n)
	}
	return nil
}
