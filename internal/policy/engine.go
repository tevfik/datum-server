// Package policy implements a lightweight Row-Level-Security style engine
// inspired by PostgREST/Supabase RLS. Policies are evaluated in-memory
// against a Subject (user/device claims) and a Resource (the row being
// accessed), returning Allow / Deny.
//
// Built-in operators (intentionally minimal):
//
//	==, !=, in, contains, prefix, suffix, true
//
// Examples:
//
//	// owner-only read on a buckets row
//	policy.Rule{
//	    Resource: "bucket_object",
//	    Action:   "read",
//	    When: []policy.Cond{
//	        {Left: "resource.owner_id", Op: "==", Right: "subject.id"},
//	    },
//	}
//
//	// admin can do anything
//	policy.Rule{Resource: "*", Action: "*", When: []policy.Cond{
//	    {Left: "subject.role", Op: "==", RightLit: "admin"},
//	}}
//
// The engine is intentionally side-effect free. Use it from API handlers:
//
//	if !engine.Allow(subject, "device", "delete", deviceRow) { return 403 }
package policy

import (
	"fmt"
	"strings"
	"sync"
)

// Subject describes the calling identity.
type Subject struct {
	ID    string
	Role  string
	Email string
	Extra map[string]any
}

// Resource is the row being accessed (a generic map for flexibility).
type Resource map[string]any

// Cond is one row in a Rule.When list. Either Right (path) or RightLit
// (literal) is filled.
type Cond struct {
	Left     string `json:"left"`
	Op       string `json:"op"`
	Right    string `json:"right,omitempty"`
	RightLit any    `json:"right_lit,omitempty"`
}

// Rule grants Action on Resource when ALL conditions match. An empty When
// list always matches (useful for `subject.role == admin → allow *`).
type Rule struct {
	Name     string `json:"name"`
	Resource string `json:"resource"` // "*" matches any
	Action   string `json:"action"`   // "*" matches any
	When     []Cond `json:"when"`
}

// Engine is the rule store + evaluator.
type Engine struct {
	mu    sync.RWMutex
	rules []Rule
}

// New constructs an empty Engine.
func New() *Engine { return &Engine{} }

// Load replaces the rule set.
func (e *Engine) Load(rules []Rule) {
	e.mu.Lock()
	cp := make([]Rule, len(rules))
	copy(cp, rules)
	e.rules = cp
	e.mu.Unlock()
}

// Add appends a single rule.
func (e *Engine) Add(r Rule) {
	e.mu.Lock()
	e.rules = append(e.rules, r)
	e.mu.Unlock()
}

// Rules returns a snapshot.
func (e *Engine) Rules() []Rule {
	e.mu.RLock()
	defer e.mu.RUnlock()
	out := make([]Rule, len(e.rules))
	copy(out, e.rules)
	return out
}

// Allow returns true when at least one rule matches the subject/resource/action.
func (e *Engine) Allow(sub Subject, resource, action string, row Resource) bool {
	e.mu.RLock()
	defer e.mu.RUnlock()
	for _, r := range e.rules {
		if !match(r.Resource, resource) || !match(r.Action, action) {
			continue
		}
		if evalAll(r.When, sub, row) {
			return true
		}
	}
	return false
}

func match(pattern, v string) bool {
	return pattern == "*" || pattern == v
}

func evalAll(conds []Cond, sub Subject, row Resource) bool {
	for _, c := range conds {
		if !evalCond(c, sub, row) {
			return false
		}
	}
	return true
}

func evalCond(c Cond, sub Subject, row Resource) bool {
	left := lookup(c.Left, sub, row)
	var right any
	if c.Right != "" {
		right = lookup(c.Right, sub, row)
	} else {
		right = c.RightLit
	}
	switch strings.ToLower(c.Op) {
	case "==", "eq":
		return fmt.Sprint(left) == fmt.Sprint(right)
	case "!=", "ne":
		return fmt.Sprint(left) != fmt.Sprint(right)
	case "in":
		if list, ok := right.([]any); ok {
			ls := fmt.Sprint(left)
			for _, v := range list {
				if fmt.Sprint(v) == ls {
					return true
				}
			}
		}
		return false
	case "contains":
		return strings.Contains(fmt.Sprint(left), fmt.Sprint(right))
	case "prefix":
		return strings.HasPrefix(fmt.Sprint(left), fmt.Sprint(right))
	case "suffix":
		return strings.HasSuffix(fmt.Sprint(left), fmt.Sprint(right))
	case "true":
		return fmt.Sprint(left) == "true"
	}
	return false
}

// lookup resolves "subject.x" and "resource.y" paths.
func lookup(path string, sub Subject, row Resource) any {
	if !strings.ContainsRune(path, '.') {
		return path
	}
	root, key, _ := strings.Cut(path, ".")
	switch root {
	case "subject":
		switch key {
		case "id":
			return sub.ID
		case "role":
			return sub.Role
		case "email":
			return sub.Email
		default:
			if sub.Extra != nil {
				return sub.Extra[key]
			}
			return nil
		}
	case "resource":
		// Allow nested keys like resource.meta.owner via repeated cut.
		var cur any = map[string]any(row)
		for _, p := range strings.Split(key, ".") {
			m, ok := cur.(map[string]any)
			if !ok {
				return nil
			}
			cur = m[p]
		}
		return cur
	}
	return nil
}
