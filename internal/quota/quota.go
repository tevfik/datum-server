// Package quota provides a lightweight per-user usage counter and limit
// enforcement layer. It is intentionally simple: counters live in-memory
// with periodic flush to the system_config blob; limits are configurable
// per "plan" (free / pro / unlimited) and a single env override.
//
// Why not Postgres counters? Quotas don't need transactional accuracy —
// "best-effort, eventually consistent" is fine. We only need to prevent
// runaway abuse, not bill to the cent. If this ever changes we can add
// a real counters table without changing the public API of this package.
package quota

import (
	"fmt"
	"sync"
	"time"
)

// Resource is a name we count. Add new ones as features land.
type Resource string

const (
	ResourceMCPCall      Resource = "mcp_call"
	ResourceAIChat       Resource = "ai_chat"
	ResourceDevice       Resource = "device"
	ResourceSpace        Resource = "space"
	ResourceAnalyticsEvt Resource = "analytics_event"
	ResourcePushNotify   Resource = "push_notify"
)

// Period is the rolling window for a counter.
type Period string

const (
	PeriodHour  Period = "hour"
	PeriodDay   Period = "day"
	PeriodMonth Period = "month"
	PeriodTotal Period = "total" // never resets — for hard caps like #devices
)

// Limit pairs a resource + period with a numeric cap. Zero means unlimited.
type Limit struct {
	Resource Resource `json:"resource"`
	Period   Period   `json:"period"`
	Max      int64    `json:"max"`
}

// Plan is a named bundle of limits. Default plans are defined below; an
// admin can override per-user via UpdateUserPlan().
type Plan struct {
	Name   string  `json:"name"`
	Limits []Limit `json:"limits"`
}

// DefaultPlans matches the user's intent: free works without artificial
// blocking until we decide to enforce; pro/unlimited are placeholders we
// can tune later. Free has generous-but-finite limits to catch abuse.
var DefaultPlans = map[string]Plan{
	"free": {Name: "free", Limits: []Limit{
		{Resource: ResourceMCPCall, Period: PeriodHour, Max: 1000},
		{Resource: ResourceAIChat, Period: PeriodDay, Max: 100},
		{Resource: ResourceDevice, Period: PeriodTotal, Max: 10},
		{Resource: ResourceSpace, Period: PeriodTotal, Max: 25},
		{Resource: ResourceAnalyticsEvt, Period: PeriodHour, Max: 10000},
		{Resource: ResourcePushNotify, Period: PeriodDay, Max: 200},
	}},
	"pro": {Name: "pro", Limits: []Limit{
		{Resource: ResourceMCPCall, Period: PeriodHour, Max: 10000},
		{Resource: ResourceAIChat, Period: PeriodDay, Max: 1000},
		{Resource: ResourceDevice, Period: PeriodTotal, Max: 100},
		{Resource: ResourceSpace, Period: PeriodTotal, Max: 250},
		{Resource: ResourceAnalyticsEvt, Period: PeriodHour, Max: 100000},
		{Resource: ResourcePushNotify, Period: PeriodDay, Max: 2000},
	}},
	"unlimited": {Name: "unlimited", Limits: nil}, // no checks
}

// Manager is the in-memory quota tracker.
type Manager struct {
	mu       sync.Mutex
	counts   map[counterKey]*counter
	userPlan map[string]string // user_id → plan name
	plans    map[string]Plan
}

type counterKey struct {
	UserID   string
	Resource Resource
	Period   Period
}

type counter struct {
	value   int64
	resetAt time.Time
}

// New creates a Manager seeded with the default plans.
func New() *Manager {
	return &Manager{
		counts:   make(map[counterKey]*counter),
		userPlan: make(map[string]string),
		plans:    cloneMap(DefaultPlans),
	}
}

func cloneMap(m map[string]Plan) map[string]Plan {
	out := make(map[string]Plan, len(m))
	for k, v := range m {
		out[k] = v
	}
	return out
}

// SetUserPlan assigns a plan name to a user. Unknown plans default to "free".
func (m *Manager) SetUserPlan(userID, plan string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.userPlan[userID] = plan
}

// PlanFor returns the user's plan; defaults to "free" if unset.
func (m *Manager) PlanFor(userID string) Plan {
	m.mu.Lock()
	defer m.mu.Unlock()
	name := m.userPlan[userID]
	if name == "" {
		name = "free"
	}
	if p, ok := m.plans[name]; ok {
		return p
	}
	return m.plans["free"]
}

// Check returns nil if the user can perform `n` more units of the given
// resource right now. Returns *Error otherwise.
//
// This is a non-incrementing check — call Increment after a successful
// action. We split the API so the caller can do a "preview" without
// committing the counter.
func (m *Manager) Check(userID string, res Resource, n int64) error {
	plan := m.PlanFor(userID)
	for _, l := range plan.Limits {
		if l.Resource != res || l.Max == 0 {
			continue
		}
		used := m.usage(userID, res, l.Period)
		if used+n > l.Max {
			return &Error{
				Resource: res,
				Period:   l.Period,
				Limit:    l.Max,
				Used:     used,
			}
		}
	}
	return nil
}

// Increment bumps the counter for every period applicable to res. Safe
// to call even for resources without a configured limit (it just keeps
// stats for /auth/me/quota).
func (m *Manager) Increment(userID string, res Resource, n int64) {
	now := time.Now().UTC()
	m.mu.Lock()
	defer m.mu.Unlock()
	for _, p := range []Period{PeriodHour, PeriodDay, PeriodMonth, PeriodTotal} {
		k := counterKey{UserID: userID, Resource: res, Period: p}
		c, ok := m.counts[k]
		if !ok {
			c = &counter{resetAt: nextReset(now, p)}
			m.counts[k] = c
		}
		if p != PeriodTotal && now.After(c.resetAt) {
			c.value = 0
			c.resetAt = nextReset(now, p)
		}
		c.value += n
	}
}



// Usage returns the user's current per-resource usage across all periods.
func (m *Manager) Usage(userID string) map[Resource]map[Period]int64 {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := make(map[Resource]map[Period]int64)
	now := time.Now().UTC()
	for k, c := range m.counts {
		if k.UserID != userID {
			continue
		}
		val := c.value
		if k.Period != PeriodTotal && now.After(c.resetAt) {
			val = 0
		}
		if _, ok := out[k.Resource]; !ok {
			out[k.Resource] = map[Period]int64{}
		}
		out[k.Resource][k.Period] = val
	}
	return out
}

// usage is the unlocked variant for internal use inside Check.
func (m *Manager) usage(userID string, res Resource, period Period) int64 {
	m.mu.Lock()
	defer m.mu.Unlock()
	k := counterKey{UserID: userID, Resource: res, Period: period}
	c, ok := m.counts[k]
	if !ok {
		return 0
	}
	now := time.Now().UTC()
	if period != PeriodTotal && now.After(c.resetAt) {
		return 0
	}
	return c.value
}

// nextReset returns the wall-clock time at which a counter for `p` should
// roll over.
func nextReset(now time.Time, p Period) time.Time {
	switch p {
	case PeriodHour:
		return now.Truncate(time.Hour).Add(time.Hour)
	case PeriodDay:
		y, m, d := now.Date()
		return time.Date(y, m, d, 0, 0, 0, 0, time.UTC).Add(24 * time.Hour)
	case PeriodMonth:
		y, m, _ := now.Date()
		return time.Date(y, m+1, 1, 0, 0, 0, 0, time.UTC)
	default: // total
		return time.Time{}
	}
}

// Error represents a rejected quota check. It implements `error` and
// carries enough context for the API layer to render a useful 429
// response with Retry-After.
type Error struct {
	Resource Resource
	Period   Period
	Limit    int64
	Used     int64
}

func (e *Error) Error() string {
	return fmt.Sprintf("quota exceeded: %s/%s used %d of %d", e.Resource, e.Period, e.Used, e.Limit)
}
