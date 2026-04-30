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
