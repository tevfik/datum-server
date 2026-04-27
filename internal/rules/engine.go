package rules

import (
	"encoding/json"
	"fmt"
	"sync"
	"time"

	"datum-go/internal/logger"
	"datum-go/internal/webhook"
)

// Operator defines the comparison type.
type Operator string

const (
	OpGT       Operator = "gt"
	OpGTE      Operator = "gte"
	OpLT       Operator = "lt"
	OpLTE      Operator = "lte"
	OpEQ       Operator = "eq"
	OpNEQ      Operator = "neq"
	OpContains Operator = "contains"
)

// ActionType defines what happens when a rule matches.
type ActionType string

const (
	ActionWebhook ActionType = "webhook"
	ActionLog     ActionType = "log"
	ActionMQTT    ActionType = "mqtt"
)

// Condition defines a single comparison.
type Condition struct {
	Field    string      `json:"field"`
	Operator Operator    `json:"operator"`
	Value    interface{} `json:"value"`
}

// RuleAction defines what to do when a rule fires.
type RuleAction struct {
	Type   ActionType             `json:"type"`
	Config map[string]interface{} `json:"config,omitempty"`
}

// Rule is a user-defined data processing rule.
type Rule struct {
	ID          string       `json:"id"`
	Name        string       `json:"name"`
	Description string       `json:"description,omitempty"`
	DeviceID    string       `json:"device_id,omitempty"` // empty = all devices
	Conditions  []Condition  `json:"conditions"`
	Actions     []RuleAction `json:"actions"`
	Enabled     bool         `json:"enabled"`
	CreatedAt   time.Time    `json:"created_at"`
	LastFired   time.Time    `json:"last_fired,omitempty"`
	FireCount   uint64       `json:"fire_count"`
}

// Engine evaluates incoming data against rules.
type Engine struct {
	mu          sync.RWMutex
	rules       map[string]*Rule
	webhookDisp *webhook.Dispatcher
	mqttPublish func(topic string, payload []byte) error
}

// NewEngine creates a rule engine.
func NewEngine(webhookDisp *webhook.Dispatcher, mqttPublish func(string, []byte) error) *Engine {
	return &Engine{
		rules:       make(map[string]*Rule),
		webhookDisp: webhookDisp,
		mqttPublish: mqttPublish,
	}
}

// AddRule registers a new rule.
func (e *Engine) AddRule(r *Rule) {
	e.mu.Lock()
	defer e.mu.Unlock()
	r.CreatedAt = time.Now()
	r.Enabled = true
	e.rules[r.ID] = r
}

// RemoveRule deletes a rule.
func (e *Engine) RemoveRule(id string) {
	e.mu.Lock()
	defer e.mu.Unlock()
	delete(e.rules, id)
}

// GetRule returns a rule by ID.
func (e *Engine) GetRule(id string) (*Rule, bool) {
	e.mu.RLock()
	defer e.mu.RUnlock()
	r, ok := e.rules[id]
	return r, ok
}

// ListRules returns all rules.
func (e *Engine) ListRules() []*Rule {
	e.mu.RLock()
	defer e.mu.RUnlock()
	out := make([]*Rule, 0, len(e.rules))
	for _, r := range e.rules {
		out = append(out, r)
	}
	return out
}

// Evaluate checks all rules against the incoming data point.
func (e *Engine) Evaluate(deviceID string, data map[string]interface{}) {
	e.mu.RLock()
	rules := make([]*Rule, 0)
	for _, r := range e.rules {
		if !r.Enabled {
			continue
		}
		if r.DeviceID != "" && r.DeviceID != deviceID {
			continue
		}
		rules = append(rules, r)
	}
	e.mu.RUnlock()

	for _, r := range rules {
		if e.matchAll(r.Conditions, data) {
			e.fire(r, deviceID, data)
		}
	}
}

func (e *Engine) matchAll(conditions []Condition, data map[string]interface{}) bool {
	for _, c := range conditions {
		val, ok := data[c.Field]
		if !ok {
			return false
		}
		if !compare(val, c.Operator, c.Value) {
			return false
		}
	}
	return true
}

func compare(actual interface{}, op Operator, expected interface{}) bool {
	av := toFloat64(actual)
	ev := toFloat64(expected)

	switch op {
	case OpGT:
		return av > ev
	case OpGTE:
		return av >= ev
	case OpLT:
		return av < ev
	case OpLTE:
		return av <= ev
	case OpEQ:
		return fmt.Sprintf("%v", actual) == fmt.Sprintf("%v", expected)
	case OpNEQ:
		return fmt.Sprintf("%v", actual) != fmt.Sprintf("%v", expected)
	case OpContains:
		s, ok1 := actual.(string)
		e, ok2 := expected.(string)
		if ok1 && ok2 {
			return len(s) > 0 && len(e) > 0 && containsStr(s, e)
		}
		return false
	default:
		return false
	}
}

func containsStr(s, sub string) bool {
	return len(s) >= len(sub) && (s == sub || len(s) > 0 && findSubstring(s, sub))
}

func findSubstring(s, sub string) bool {
	for i := 0; i <= len(s)-len(sub); i++ {
		if s[i:i+len(sub)] == sub {
			return true
		}
	}
	return false
}

func toFloat64(v interface{}) float64 {
	switch n := v.(type) {
	case float64:
		return n
	case float32:
		return float64(n)
	case int:
		return float64(n)
	case int64:
		return float64(n)
	case json.Number:
		f, _ := n.Float64()
		return f
	default:
		return 0
	}
}

func (e *Engine) fire(r *Rule, deviceID string, data map[string]interface{}) {
	log := logger.GetLogger()

	e.mu.Lock()
	r.LastFired = time.Now()
	r.FireCount++
	e.mu.Unlock()

	for _, action := range r.Actions {
		switch action.Type {
		case ActionLog:
			log.Info().
				Str("rule_id", r.ID).
				Str("rule_name", r.Name).
				Str("device_id", deviceID).
				Interface("data", data).
				Msg("RULE_FIRED")

		case ActionWebhook:
			if e.webhookDisp != nil {
				e.webhookDisp.Emit(webhook.Event{
					ID:       fmt.Sprintf("rule_%s_%d", r.ID, r.FireCount),
					Type:     webhook.EventRuleTriggered,
					DeviceID: deviceID,
					Data: map[string]interface{}{
						"rule_id":   r.ID,
						"rule_name": r.Name,
						"payload":   data,
					},
				})
			}

		case ActionMQTT:
			if e.mqttPublish != nil {
				topic, _ := action.Config["topic"].(string)
				if topic == "" {
					topic = fmt.Sprintf("dev/%s/alert", deviceID)
				}
				payload, _ := json.Marshal(map[string]interface{}{
					"rule_id":   r.ID,
					"rule_name": r.Name,
					"device_id": deviceID,
					"data":      data,
					"fired_at":  time.Now().Format(time.RFC3339),
				})
				if err := e.mqttPublish(topic, payload); err != nil {
					log.Error().Err(err).Str("rule_id", r.ID).Msg("rule: MQTT publish failed")
				}
			}
		}
	}
}

// LoadFromJSON loads rules from a JSON byte slice.
func (e *Engine) LoadFromJSON(data []byte) error {
	var rules []*Rule
	if err := json.Unmarshal(data, &rules); err != nil {
		return err
	}
	e.mu.Lock()
	defer e.mu.Unlock()
	for _, r := range rules {
		r.Enabled = true
		if r.CreatedAt.IsZero() {
			r.CreatedAt = time.Now()
		}
		e.rules[r.ID] = r
	}
	return nil
}

// ExportJSON exports all rules as JSON.
func (e *Engine) ExportJSON() ([]byte, error) {
	return json.MarshalIndent(e.ListRules(), "", "  ")
}
