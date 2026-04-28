# Policy Engine (Row-Level Security)

> Phase 5 deliverable. The `internal/policy` package implements an
> in-memory, side-effect-free RLS-style allow/deny engine inspired by
> PostgREST and Supabase RLS.

## Concepts

| Concept    | Description                                                  |
|------------|--------------------------------------------------------------|
| `Subject`  | The caller — user/device with `id`, `role`, `email`, `Extra` |
| `Resource` | The row being accessed (a generic `map[string]any`)          |
| `Rule`     | Grants `Action` on `Resource` when ALL `When` conditions hold |
| `Cond`     | A single comparison: `<Left op Right|RightLit>`              |

A request is **allowed** when ANY rule matches. Rules with `Resource: "*"`
or `Action: "*"` are treated as wildcards.

## Operators

| Op                | Meaning                                                |
|-------------------|--------------------------------------------------------|
| `==`, `eq`        | String-stringified equality                            |
| `!=`, `ne`        | Inequality                                             |
| `in`              | `Left` is one of `RightLit []any`                      |
| `contains`        | substring                                              |
| `prefix`/`suffix` | string prefix / suffix                                 |
| `true`            | `Left` evaluates to literal "true"                     |

Paths used in `Left`/`Right` resolve against the subject and resource:

- `subject.id`, `subject.role`, `subject.email`, `subject.<key>` (from `Extra`)
- `resource.<key>` (supports nested `resource.meta.owner`)

## Examples

```go
e := policy.New()

// Owner-only read on bucket objects
e.Add(policy.Rule{
    Resource: "bucket_object",
    Action:   "read",
    When: []policy.Cond{
        {Left: "resource.owner_id", Op: "==", Right: "subject.id"},
    },
})

// Admin can do anything
e.Add(policy.Rule{
    Resource: "*", Action: "*",
    When: []policy.Cond{{Left: "subject.role", Op: "==", RightLit: "admin"}},
})

// Public path readable by everyone
e.Add(policy.Rule{
    Resource: "bucket_object", Action: "read",
    When: []policy.Cond{{Left: "resource.path", Op: "prefix", RightLit: "public/"}},
})
```

## Use from a handler

```go
sub := policy.Subject{ID: claims.UserID, Role: claims.Role}
row := policy.Resource{"owner_id": obj.OwnerID, "path": obj.Path}
if !engine.Allow(sub, "bucket_object", "read", row) {
    c.AbortWithStatus(http.StatusForbidden)
    return
}
```

## Roadmap

- Persistence: rules are currently in-memory. A JSON loader matching
  `internal/rules` will be added in a future phase.
- Per-table rule scoping: today rules are evaluated against any
  `Resource` string passed by the handler. Convention: use lowercase
  singular nouns (`device`, `bucket_object`, `command`).
- Transport-level integration: REST handlers will receive a thin
  `RequirePolicy(resource, action)` middleware in Phase 6.
