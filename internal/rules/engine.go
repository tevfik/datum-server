package rules

import (
	"encoding/json"
	"fmt"
	"sync"
	"time"

	"datum-go/internal/logger"
	"datum-go/internal/notify"
	"datum-go/internal/webhook"
)

// ── Operator ────────────────────────────────────────────────────────────────

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

// ── Action Types ────────────────────────────────────────────────────────────

// ActionType defines what happens when a rule matches.
type ActionType string

const (
	ActionWebhook ActionType = "webhook"
	ActionLog     ActionType = "log"
	ActionMQTT    ActionType = "mqtt"
	ActionNotify  ActionType = "notify"  // Push notification
	ActionCommand ActionType = "command" // Send command to device
)

// ── Trigger Types ───────────────────────────────────────────────────────────

// TriggerType defines when a rule is evaluated.
type TriggerType string

const (
	TriggerOnData    TriggerType = "on_data"   // Evaluated when telemetry arrives (default)
	TriggerScheduled TriggerType = "scheduled" // Evaluated on a cron schedule
	TriggerManual    TriggerType = "manual"    // Only evaluated when manually triggered via API
)

// ── Logic Types ─────────────────────────────────────────────────────────────

// LogicType defines how the rule conditions are expressed.
type LogicType string

const (
	LogicConditions LogicType = "conditions" // Simple JSON condition array (default)
	LogicBlockly    LogicType = "blockly"    // Blockly visual editor output
	LogicLua        LogicType = "lua"        // Custom Lua script
)

// ── Data Structures ─────────────────────────────────────────────────────────

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

// RuleTrigger defines when/how the rule is evaluated.
type RuleTrigger struct {
	Type     TriggerType `json:"type"`                // on_data, scheduled, manual
	Schedule string      `json:"schedule,omitempty"`  // Cron expression for scheduled triggers
	DeviceID string      `json:"device_id,omitempty"` // Specific device to watch (empty = all)
}

// RuleLogic defines how conditions are expressed.
type RuleLogic struct {
	Type        LogicType              `json:"type"`                   // conditions, blockly, lua
	Conditions  []Condition            `json:"conditions,omitempty"`   // For type=conditions or blockly-compiled
	LogicOp     string                 `json:"logic_op,omitempty"`     // "and" (default) or "or"
	BlocklyJSON map[string]interface{} `json:"blockly_json,omitempty"` // Blockly workspace state
	LuaScript   string                 `json:"lua_script,omitempty"`   // For type=lua
}

// Rule is a user-defined data processing rule.
type Rule struct {
	ID          string       `json:"id"`
	OwnerID     string       `json:"owner_id,omitempty"` // User who owns the rule
	Name        string       `json:"name"`
	Description string       `json:"description,omitempty"`
	DeviceID    string       `json:"device_id,omitempty"` // Shortcut: empty = all devices (backward compat)
	Trigger     RuleTrigger  `json:"trigger"`
	Logic       RuleLogic    `json:"logic"`
	Conditions  []Condition  `json:"conditions,omitempty"` // Backward compat shortcut
	Actions     []RuleAction `json:"actions"`
	Enabled     bool         `json:"enabled"`
	CreatedAt   time.Time    `json:"created_at"`
	UpdatedAt   time.Time    `json:"updated_at,omitempty"`
	LastFired   time.Time    `json:"last_fired,omitempty"`
	FireCount   uint64       `json:"fire_count"`
}

// effectiveConditions returns the conditions to evaluate, preferring
// Logic.Conditions over the legacy top-level Conditions field.
func (r *Rule) effectiveConditions() []Condition {
	if len(r.Logic.Conditions) > 0 {
		return r.Logic.Conditions
	}
	return r.Conditions
}

// effectiveDeviceID returns the device filter, preferring Trigger.DeviceID
// over the legacy top-level DeviceID field.
func (r *Rule) effectiveDeviceID() string {
	if r.Trigger.DeviceID != "" {
		return r.Trigger.DeviceID
	}
	return r.DeviceID
}

// effectiveTriggerType returns the trigger type, defaulting to on_data.
func (r *Rule) effectiveTriggerType() TriggerType {
	if r.Trigger.Type != "" {
		return r.Trigger.Type
	}
	return TriggerOnData
}

// ── Engine ───────────────────────────────────────────────────────────────────

// Engine evaluates incoming data against rules.
type Engine struct {
	mu          sync.RWMutex
	rules       map[string]*Rule
	webhookDisp *webhook.Dispatcher
	notifyDisp  *notify.Dispatcher
	mqttPublish func(topic string, payload []byte) error
	luaEval     *LuaEvaluator
}

// NewEngine creates a rule engine.
func NewEngine(webhookDisp *webhook.Dispatcher, mqttPublish func(string, []byte) error) *Engine {
	return &Engine{
		rules:       make(map[string]*Rule),
		webhookDisp: webhookDisp,
		mqttPublish: mqttPublish,
		luaEval:     NewLuaEvaluator(),
	}
}

// SetNotifyDispatcher attaches a push notification dispatcher to the engine.
// When set, ActionNotify rules will deliver real push notifications.
func (e *Engine) SetNotifyDispatcher(d *notify.Dispatcher) {
	e.mu.Lock()
	e.notifyDisp = d
	e.mu.Unlock()
}

// MustGetRule returns a rule by ID or panics — for use in tests only.
func (e *Engine) MustGetRule(id string) *Rule {
	r, ok := e.GetRule(id)
	if !ok {
		panic("rule not found: " + id)
	}
	return r
}

// AddRule registers a new rule.
func (e *Engine) AddRule(r *Rule) {
	e.mu.Lock()
	defer e.mu.Unlock()
	if r.CreatedAt.IsZero() {
		r.CreatedAt = time.Now()
	}
	if !r.Enabled {
		// Default to enabled for new rules unless explicitly set
		r.Enabled = true
	}
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

// ListRulesForUser returns rules owned by a specific user.
func (e *Engine) ListRulesForUser(userID string) []*Rule {
	e.mu.RLock()
	defer e.mu.RUnlock()
	out := make([]*Rule, 0)
	for _, r := range e.rules {
		if r.OwnerID == userID {
			out = append(out, r)
		}
	}
	return out
}

// ── Evaluation ──────────────────────────────────────────────────────────────

// Evaluate checks all on_data rules against the incoming data point.
func (e *Engine) Evaluate(deviceID string, data map[string]interface{}) {
	e.mu.RLock()
	rules := make([]*Rule, 0)
	for _, r := range e.rules {
		if !r.Enabled {
			continue
		}
		// Only evaluate on_data (default) trigger rules in the telemetry path
		if r.effectiveTriggerType() != TriggerOnData {
			continue
		}
		did := r.effectiveDeviceID()
		if did != "" && did != deviceID {
			continue
		}
		rules = append(rules, r)
	}
	e.mu.RUnlock()

	for _, r := range rules {
		if e.evaluateRule(r, deviceID, data) {
			e.fire(r, deviceID, data)
		}
	}
}

// EvaluateManual explicitly evaluates a single rule against given data.
// Used for manual triggers and scheduled triggers.
func (e *Engine) EvaluateManual(ruleID string, deviceID string, data map[string]interface{}) bool {
	r, ok := e.GetRule(ruleID)
	if !ok || !r.Enabled {
		return false
	}
	if e.evaluateRule(r, deviceID, data) {
		e.fire(r, deviceID, data)
		return true
	}
	return false
}

// evaluateRule checks a single rule's conditions/logic against data.
func (e *Engine) evaluateRule(r *Rule, deviceID string, data map[string]interface{}) bool {
	switch r.Logic.Type {
	case LogicLua:
		if r.Logic.LuaScript != "" && e.luaEval != nil {
			result, err := e.luaEval.Evaluate(r.Logic.LuaScript, deviceID, data)
			if err != nil {
				logger.GetLogger().Warn().
					Err(err).
					Str("rule_id", r.ID).
					Msg("Lua evaluation failed")
				return false
			}
			return result
		}
		return false

	case LogicBlockly:
		// Blockly rules compile down to conditions; evaluate them
		return e.matchConditions(r.effectiveConditions(), r.Logic.LogicOp, data)

	default:
		// LogicConditions or unset — use standard condition matching
		return e.matchConditions(r.effectiveConditions(), r.Logic.LogicOp, data)
	}
}

// matchConditions evaluates a list of conditions with AND or OR logic.
func (e *Engine) matchConditions(conditions []Condition, logicOp string, data map[string]interface{}) bool {
	if len(conditions) == 0 {
		return false
	}

	if logicOp == "or" {
		// OR: any condition matching is enough
		for _, c := range conditions {
			val, ok := data[c.Field]
			if ok && compare(val, c.Operator, c.Value) {
				return true
			}
		}
		return false
	}

	// AND (default): all conditions must match
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

// ── Comparison ──────────────────────────────────────────────────────────────

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

// ── Actions ─────────────────────────────────────────────────────────────────

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

		case ActionNotify:
			e.mu.RLock()
			nd := e.notifyDisp
			e.mu.RUnlock()

			title, _ := action.Config["title"].(string)
			message, _ := action.Config["message"].(string)
			if title == "" {
				title = "Rule Alert: " + r.Name
			}
			if message == "" {
				message = fmt.Sprintf("Rule '%s' was triggered by device %s", r.Name, deviceID)
			}
			priority, _ := action.Config["priority"].(string)
			if priority == "" {
				priority = notify.PriorityDefault
			}

			// Dispatch push notification if dispatcher is available
			if nd != nil {
				nd.NotifyUser(r.OwnerID, title, message, priority)
			}

			// Also emit as a webhook event for SSE/webhook subscribers
			if e.webhookDisp != nil {
				e.webhookDisp.Emit(webhook.Event{
					ID:       fmt.Sprintf("rule_notify_%s_%d", r.ID, r.FireCount),
					Type:     webhook.EventRuleTriggered,
					DeviceID: deviceID,
					Data: map[string]interface{}{
						"rule_id":   r.ID,
						"rule_name": r.Name,
						"type":      "notification",
						"title":     title,
						"message":   message,
					},
				})
			}

		case ActionCommand:
			// Send command to a device via MQTT
			if e.mqttPublish != nil {
				targetDevice, _ := action.Config["target_device"].(string)
				if targetDevice == "" {
					targetDevice = deviceID
				}
				cmdPayload, _ := action.Config["payload"].(string)
				if cmdPayload == "" {
					cmdPayload = "{}"
				}
				topic := fmt.Sprintf("dev/%s/cmd/set", targetDevice)
				if err := e.mqttPublish(topic, []byte(cmdPayload)); err != nil {
					log.Error().Err(err).Str("rule_id", r.ID).Msg("rule: command publish failed")
				}
			}
		}
	}
}

// ── Serialization ───────────────────────────────────────────────────────────

// LoadFromJSON loads rules from a JSON byte slice.
func (e *Engine) LoadFromJSON(data []byte) error {
	var rules []*Rule
	if err := json.Unmarshal(data, &rules); err != nil {
		return err
	}
	e.mu.Lock()
	defer e.mu.Unlock()
	for _, r := range rules {
		if !r.Enabled {
			r.Enabled = true
		}
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

// GetScheduledRules returns rules that have scheduled triggers.
func (e *Engine) GetScheduledRules() []*Rule {
	e.mu.RLock()
	defer e.mu.RUnlock()
	out := make([]*Rule, 0)
	for _, r := range e.rules {
		if r.Enabled && r.effectiveTriggerType() == TriggerScheduled {
			out = append(out, r)
		}
	}
	return out
}
