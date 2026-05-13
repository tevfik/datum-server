package rules

import (
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"datum-go/internal/storage"
)

// minimalStore is a storage.Provider stub that satisfies the interface
// with no-op implementations. Only GetLatestData is functional for scheduler tests.
type minimalStore struct {
	storage.Provider // embed to satisfy unexported methods if any
	mu               sync.Mutex
	latestData       map[string]*storage.DataPoint
	callCount        int32
}

func newMinimalStore() *minimalStore {
	return &minimalStore{latestData: make(map[string]*storage.DataPoint)}
}

func (m *minimalStore) setLatestData(deviceID string, data map[string]interface{}) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.latestData[deviceID] = &storage.DataPoint{
		DeviceID:  deviceID,
		Timestamp: time.Now(),
		Data:      data,
	}
}

func (m *minimalStore) GetLatestData(deviceID string) (*storage.DataPoint, error) {
	atomic.AddInt32(&m.callCount, 1)
	m.mu.Lock()
	defer m.mu.Unlock()
	dp, ok := m.latestData[deviceID]
	if !ok {
		return nil, nil
	}
	return dp, nil
}

// ── Scheduler construction ────────────────────────────────────────────────────

func TestNewScheduler(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()
	sched := NewScheduler(eng, store)
	if sched == nil {
		t.Fatal("NewScheduler returned nil")
	}
	if sched.engine != eng {
		t.Fatal("scheduler engine mismatch")
	}
	if sched.store == nil {
		t.Fatal("scheduler store is nil")
	}
}

// ── Start / Stop ──────────────────────────────────────────────────────────────

func TestSchedulerStart_NoRules(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()
	sched := NewScheduler(eng, store)
	// Start with no rules — should not panic
	sched.Start()
	sched.Stop()
}

func TestSchedulerStart_RegistersExistingRules(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()

	eng.AddRule(&Rule{
		ID:   "s-existing",
		Name: "Pre-existing scheduled",
		Trigger: RuleTrigger{
			Type:     TriggerScheduled,
			DeviceID: "dev-1",
			Schedule: "@every 10s",
		},
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	sched := NewScheduler(eng, store)
	sched.Start()
	defer sched.Stop()

	sched.mu.Lock()
	n := len(sched.jobs)
	sched.mu.Unlock()

	if n != 1 {
		t.Fatalf("expected 1 registered job, got %d", n)
	}
}

// ── RegisterRule / UnregisterRule ─────────────────────────────────────────────

func TestScheduler_RegisterRule(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()
	sched := NewScheduler(eng, store)
	sched.Start()
	defer sched.Stop()

	r := &Rule{
		ID:   "reg-1",
		Name: "Reg test",
		Trigger: RuleTrigger{
			Type:     TriggerScheduled,
			DeviceID: "dev-1",
			Schedule: "@every 60s",
		},
	}
	eng.AddRule(r)
	if err := sched.RegisterRule(r); err != nil {
		t.Fatalf("RegisterRule failed: %v", err)
	}

	sched.mu.Lock()
	_, ok := sched.jobs["reg-1"]
	sched.mu.Unlock()

	if !ok {
		t.Fatal("rule should be registered in jobs map")
	}
}

func TestScheduler_RegisterRule_EmptySchedule(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()
	sched := NewScheduler(eng, store)
	sched.Start()
	defer sched.Stop()

	r := &Rule{
		ID:      "reg-empty",
		Name:    "No schedule",
		Trigger: RuleTrigger{Type: TriggerScheduled, Schedule: ""},
	}
	eng.AddRule(r)
	if err := sched.RegisterRule(r); err != nil {
		t.Fatalf("RegisterRule with empty schedule should not error: %v", err)
	}

	sched.mu.Lock()
	_, ok := sched.jobs["reg-empty"]
	sched.mu.Unlock()

	if ok {
		t.Fatal("rule with empty schedule should not be in jobs map")
	}
}

func TestScheduler_RegisterRule_InvalidCron(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()
	sched := NewScheduler(eng, store)
	sched.Start()
	defer sched.Stop()

	r := &Rule{
		ID:      "reg-bad-cron",
		Name:    "Bad cron",
		Trigger: RuleTrigger{Type: TriggerScheduled, Schedule: "not-a-valid-cron"},
	}
	eng.AddRule(r)
	err := sched.RegisterRule(r)
	if err == nil {
		t.Fatal("expected error for invalid cron expression")
	}
}

func TestScheduler_UnregisterRule(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()
	sched := NewScheduler(eng, store)
	sched.Start()
	defer sched.Stop()

	r := &Rule{
		ID:   "unreg-1",
		Name: "Unreg test",
		Trigger: RuleTrigger{
			Type:     TriggerScheduled,
			DeviceID: "dev-1",
			Schedule: "@every 60s",
		},
	}
	eng.AddRule(r)
	if err := sched.RegisterRule(r); err != nil {
		t.Fatalf("RegisterRule failed: %v", err)
	}

	sched.UnregisterRule("unreg-1")

	sched.mu.Lock()
	_, ok := sched.jobs["unreg-1"]
	sched.mu.Unlock()

	if ok {
		t.Fatal("rule should be removed from jobs map after UnregisterRule")
	}
}

func TestScheduler_UnregisterUnknownRule(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()
	sched := NewScheduler(eng, store)
	sched.Start()
	defer sched.Stop()

	// Should not panic
	sched.UnregisterRule("nonexistent")
}

func TestScheduler_ReRegisterUpdatesJob(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()
	sched := NewScheduler(eng, store)
	sched.Start()
	defer sched.Stop()

	r := &Rule{
		ID:   "rereg",
		Name: "Re-reg",
		Trigger: RuleTrigger{
			Type:     TriggerScheduled,
			DeviceID: "dev-1",
			Schedule: "@every 60s",
		},
	}
	eng.AddRule(r)
	if err := sched.RegisterRule(r); err != nil {
		t.Fatalf("first RegisterRule failed: %v", err)
	}

	sched.mu.Lock()
	firstID := sched.jobs["rereg"]
	sched.mu.Unlock()

	// Re-register with new schedule
	r.Trigger.Schedule = "@every 120s"
	if err := sched.RegisterRule(r); err != nil {
		t.Fatalf("second RegisterRule failed: %v", err)
	}

	sched.mu.Lock()
	secondID := sched.jobs["rereg"]
	sched.mu.Unlock()

	if firstID == secondID {
		t.Fatal("re-register should replace the cron entry (different EntryID)")
	}
}

// ── evaluateScheduledRule ──────────────────────────────────────────────────────

func TestScheduler_EvaluateScheduledRule_NoData(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore() // no data set

	r := &Rule{
		ID:   "es-nodata",
		Name: "Evaluate no data",
		Trigger: RuleTrigger{
			Type:     TriggerScheduled,
			DeviceID: "dev-missing",
			Schedule: "@every 60s",
		},
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	}
	eng.AddRule(r)
	sched := NewScheduler(eng, store)

	// evaluateScheduledRule should not fire (no data available)
	sched.evaluateScheduledRule("es-nodata")
	rule, _ := eng.GetRule("es-nodata")
	if rule.FireCount != 0 {
		t.Fatal("rule should not fire when no data is available")
	}
}

func TestScheduler_EvaluateScheduledRule_WithData(t *testing.T) {
	var firedTopic atomic.Value
	eng := NewEngine(nil, func(topic string, payload []byte) error {
		firedTopic.Store(topic)
		return nil
	})
	store := newMinimalStore()
	store.setLatestData("dev-1", map[string]interface{}{"v": 50.0})

	r := &Rule{
		ID:   "es-data",
		Name: "Evaluate with data",
		Trigger: RuleTrigger{
			Type:     TriggerScheduled,
			DeviceID: "dev-1",
			Schedule: "@every 60s",
		},
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 10.0}},
		},
		Actions: []RuleAction{
			{Type: ActionMQTT, Config: map[string]interface{}{"topic": "sched/alert"}},
		},
	}
	eng.AddRule(r)
	sched := NewScheduler(eng, store)

	sched.evaluateScheduledRule("es-data")
	rule, _ := eng.GetRule("es-data")
	if rule.FireCount != 1 {
		t.Fatal("rule should fire when data condition is met")
	}
}

func TestScheduler_EvaluateScheduledRule_NoDeviceID(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()

	eng.AddRule(&Rule{
		ID:   "es-nodev",
		Name: "No device",
		Trigger: RuleTrigger{
			Type:     TriggerScheduled,
			Schedule: "@every 60s",
			// DeviceID intentionally empty
		},
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})

	sched := NewScheduler(eng, store)
	// Should not panic; skips evaluation when no device_id
	sched.evaluateScheduledRule("es-nodev")

	rule, _ := eng.GetRule("es-nodev")
	if rule.FireCount != 0 {
		t.Fatal("rule without device_id should be skipped in scheduler")
	}
}

func TestScheduler_EvaluateScheduledRule_UnknownRule(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()
	sched := NewScheduler(eng, store)
	// Should not panic
	sched.evaluateScheduledRule("nonexistent")
}

func TestScheduler_EvaluateScheduledRule_DisabledRule(t *testing.T) {
	eng := NewEngine(nil, nil)
	store := newMinimalStore()
	store.setLatestData("dev-1", map[string]interface{}{"v": 50.0})

	eng.AddRule(&Rule{
		ID:   "es-disabled",
		Name: "Disabled scheduled",
		Trigger: RuleTrigger{
			Type:     TriggerScheduled,
			DeviceID: "dev-1",
			Schedule: "@every 60s",
		},
		Logic: RuleLogic{
			Type:       LogicConditions,
			Conditions: []Condition{{Field: "v", Operator: OpGT, Value: 0.0}},
		},
		Actions: []RuleAction{{Type: ActionLog}},
	})
	eng.mu.Lock()
	eng.rules["es-disabled"].Enabled = false
	eng.mu.Unlock()

	sched := NewScheduler(eng, store)
	sched.evaluateScheduledRule("es-disabled")

	rule, _ := eng.GetRule("es-disabled")
	if rule.FireCount != 0 {
		t.Fatal("disabled rule should not fire in scheduler")
	}
}
