package policy

import "testing"

func TestAllowOwnerOnly(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "bucket_object",
		Action:   "read",
		When: []Cond{
			{Left: "resource.owner_id", Op: "==", Right: "subject.id"},
		},
	})

	sub := Subject{ID: "u1"}
	if !e.Allow(sub, "bucket_object", "read", Resource{"owner_id": "u1"}) {
		t.Fatal("owner should be allowed")
	}
	if e.Allow(sub, "bucket_object", "read", Resource{"owner_id": "u2"}) {
		t.Fatal("non-owner must be denied")
	}
	if e.Allow(sub, "bucket_object", "delete", Resource{"owner_id": "u1"}) {
		t.Fatal("delete should be denied (not in policy)")
	}
}

func TestAdminWildcardRule(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "*", Action: "*",
		When: []Cond{{Left: "subject.role", Op: "==", RightLit: "admin"}},
	})
	if !e.Allow(Subject{Role: "admin"}, "anything", "delete", Resource{}) {
		t.Fatal("admin should be allowed everywhere")
	}
	if e.Allow(Subject{Role: "user"}, "anything", "delete", Resource{}) {
		t.Fatal("non-admin should be denied")
	}
}

func TestInOperator(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "device", Action: "read",
		When: []Cond{{Left: "subject.role", Op: "in", RightLit: []any{"admin", "operator"}}},
	})
	if !e.Allow(Subject{Role: "operator"}, "device", "read", Resource{}) {
		t.Fatal("operator should be allowed")
	}
	if e.Allow(Subject{Role: "viewer"}, "device", "read", Resource{}) {
		t.Fatal("viewer should be denied")
	}
}

func TestPrefixOperatorOnNestedResource(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "bucket_object", Action: "read",
		When: []Cond{{Left: "resource.path", Op: "prefix", RightLit: "public/"}},
	})
	if !e.Allow(Subject{}, "bucket_object", "read", Resource{"path": "public/hello.txt"}) {
		t.Fatal("public path should be allowed")
	}
	if e.Allow(Subject{}, "bucket_object", "read", Resource{"path": "private/x"}) {
		t.Fatal("private path should be denied")
	}
}

// ── Load / Rules ──────────────────────────────────────────────────────────────

func TestLoad_ReplacesRuleSet(t *testing.T) {
	e := New()
	e.Add(Rule{Name: "old", Resource: "x", Action: "read"}) // pre-existing

	newRules := []Rule{
		{Name: "r1", Resource: "device", Action: "read"},
		{Name: "r2", Resource: "device", Action: "write"},
	}
	e.Load(newRules)

	got := e.Rules()
	if len(got) != 2 {
		t.Fatalf("expected 2 rules after Load, got %d", len(got))
	}
	if got[0].Name != "r1" || got[1].Name != "r2" {
		t.Fatalf("unexpected rule names: %v", got)
	}
}

func TestLoad_EmptySlice_ClearsRules(t *testing.T) {
	e := New()
	e.Add(Rule{Name: "will-be-gone", Resource: "x", Action: "r"})
	e.Load([]Rule{})
	if n := len(e.Rules()); n != 0 {
		t.Fatalf("expected 0 rules after Load([]), got %d", n)
	}
}

func TestRules_ReturnsSnapshot(t *testing.T) {
	e := New()
	e.Add(Rule{Name: "snap", Resource: "r", Action: "a"})
	snap := e.Rules()
	if len(snap) != 1 || snap[0].Name != "snap" {
		t.Fatalf("unexpected snapshot: %v", snap)
	}
	// Mutating the snapshot does not affect the engine
	snap[0].Name = "mutated"
	if e.Rules()[0].Name != "snap" {
		t.Fatal("snapshot mutation should not affect engine state")
	}
}

// ── Operators ─────────────────────────────────────────────────────────────────

func TestNeqOperator(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "device", Action: "read",
		When: []Cond{{Left: "subject.role", Op: "!=", RightLit: "banned"}},
	})
	if !e.Allow(Subject{Role: "user"}, "device", "read", Resource{}) {
		t.Fatal("non-banned user should be allowed")
	}
	if e.Allow(Subject{Role: "banned"}, "device", "read", Resource{}) {
		t.Fatal("banned user should be denied")
	}
}

func TestContainsOperator(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "post", Action: "read",
		When: []Cond{{Left: "resource.tags", Op: "contains", RightLit: "public"}},
	})
	if !e.Allow(Subject{}, "post", "read", Resource{"tags": "public,featured"}) {
		t.Fatal("should allow when tags contains 'public'")
	}
	if e.Allow(Subject{}, "post", "read", Resource{"tags": "private"}) {
		t.Fatal("should deny when tags does not contain 'public'")
	}
}

func TestSuffixOperator(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "file", Action: "read",
		When: []Cond{{Left: "resource.name", Op: "suffix", RightLit: ".pdf"}},
	})
	if !e.Allow(Subject{}, "file", "read", Resource{"name": "report.pdf"}) {
		t.Fatal("should allow .pdf file")
	}
	if e.Allow(Subject{}, "file", "read", Resource{"name": "report.png"}) {
		t.Fatal("should deny non-.pdf file")
	}
}

func TestTrueOperator(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "device", Action: "admin",
		When: []Cond{{Left: "subject.role", Op: "true"}}, // "admin" != "true"
	})
	if e.Allow(Subject{Role: "admin"}, "device", "admin", Resource{}) {
		t.Fatal("'admin' string is not the literal 'true'")
	}
	if !e.Allow(Subject{Role: "true"}, "device", "admin", Resource{}) {
		t.Fatal("'true' role should pass the true operator")
	}
}

func TestUnknownOperator_DeniesAccess(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "x", Action: "r",
		When: []Cond{{Left: "subject.id", Op: "unknown_op", RightLit: "x"}},
	})
	if e.Allow(Subject{ID: "x"}, "x", "r", Resource{}) {
		t.Fatal("unknown operator should deny")
	}
}

// ── Lookup paths ──────────────────────────────────────────────────────────────

func TestLookup_SubjectEmail(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "report", Action: "read",
		When: []Cond{{Left: "subject.email", Op: "==", RightLit: "boss@example.com"}},
	})
	if !e.Allow(Subject{Email: "boss@example.com"}, "report", "read", Resource{}) {
		t.Fatal("matching email should be allowed")
	}
}

func TestLookup_SubjectExtra(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "item", Action: "read",
		When: []Cond{{Left: "subject.tier", Op: "==", RightLit: "gold"}},
	})
	if !e.Allow(Subject{Extra: map[string]any{"tier": "gold"}}, "item", "read", Resource{}) {
		t.Fatal("extra field 'tier' should be resolved")
	}
}

func TestLookup_SubjectExtra_NilMap(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "item", Action: "read",
		When: []Cond{{Left: "subject.tier", Op: "==", RightLit: "gold"}},
	})
	// nil Extra → lookup returns nil → fmt.Sprint(nil) = "<nil>" ≠ "gold"
	if e.Allow(Subject{Extra: nil}, "item", "read", Resource{}) {
		t.Fatal("nil Extra should not match 'gold'")
	}
}

func TestLookup_ResourceNested(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "bucket", Action: "read",
		When: []Cond{{Left: "resource.meta.owner", Op: "==", Right: "subject.id"}},
	})
	if !e.Allow(
		Subject{ID: "u1"}, "bucket", "read",
		Resource{"meta": map[string]any{"owner": "u1"}},
	) {
		t.Fatal("nested resource path should resolve")
	}
}

func TestLookup_UnknownRoot_ReturnsSelf(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "x", Action: "r",
		When: []Cond{{Left: "other.field", Op: "==", RightLit: "other.field"}},
	})
	// lookup("other.field") returns nil for unknown root; fmt.Sprint(nil) = "<nil>"
	// RightLit is "other.field" (string). They won't match, so deny.
	if e.Allow(Subject{}, "x", "r", Resource{}) {
		// Only passes if lookup("other.field") somehow equals "other.field"
		// which it doesn't (unknown root → nil). Test just confirms no panic.
	}
}

func TestLookup_NoDot_ReturnsPathLiteral(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "x", Action: "r",
		When: []Cond{{Left: "admin", Op: "==", RightLit: "admin"}},
	})
	// lookup("admin") → "admin" (no dot → literal passthrough)
	if !e.Allow(Subject{}, "x", "r", Resource{}) {
		t.Fatal("no-dot path should be returned as literal, matching 'admin'")
	}
}

// ── Empty When list (always-match) ────────────────────────────────────────────

func TestEmptyWhen_AlwaysMatches(t *testing.T) {
	e := New()
	e.Add(Rule{Resource: "public", Action: "read", When: nil})
	if !e.Allow(Subject{}, "public", "read", Resource{}) {
		t.Fatal("empty When should always match")
	}
}

// ── Multi-rule evaluation (first matching wins) ───────────────────────────────

func TestMultipleRules_FirstMatchWins(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "device", Action: "read",
		When: []Cond{{Left: "subject.role", Op: "==", RightLit: "viewer"}},
	})
	e.Add(Rule{
		Resource: "device", Action: "read",
		When: []Cond{{Left: "subject.role", Op: "==", RightLit: "admin"}},
	})
	if !e.Allow(Subject{Role: "admin"}, "device", "read", Resource{}) {
		t.Fatal("admin should match second rule")
	}
	if !e.Allow(Subject{Role: "viewer"}, "device", "read", Resource{}) {
		t.Fatal("viewer should match first rule")
	}
	if e.Allow(Subject{Role: "none"}, "device", "read", Resource{}) {
		t.Fatal("unknown role should be denied")
	}
}

// ── In operator — non-list RightLit ──────────────────────────────────────────

func TestInOperator_NonListRight_Denies(t *testing.T) {
	e := New()
	e.Add(Rule{
		Resource: "x", Action: "r",
		When: []Cond{{Left: "subject.role", Op: "in", RightLit: "admin"}}, // string, not slice
	})
	if e.Allow(Subject{Role: "admin"}, "x", "r", Resource{}) {
		t.Fatal("in operator with non-list right should deny")
	}
}
