// Default MCP tool set for datum-server. Read-only by design.
//
// Each tool is a thin wrapper over storage.Provider that scopes every
// query to the authenticated user. There is no business logic here —
// that lives in the existing handlers / storage layer. This file is
// purely the protocol adapter.

package mcp

import (
	"fmt"
	"time"

	"github.com/gin-gonic/gin"

	"datum-go/internal/storage"
)

// schema is a tiny helper for constructing JSON Schema objects.
func schema(props map[string]interface{}, required ...string) map[string]interface{} {
	s := map[string]interface{}{
		"type":       "object",
		"properties": props,
	}
	if len(required) > 0 {
		s["required"] = required
	}
	return s
}

func strProp(desc string) map[string]interface{} {
	return map[string]interface{}{"type": "string", "description": desc}
}
func intProp(desc string) map[string]interface{} {
	return map[string]interface{}{"type": "integer", "description": desc}
}

func defaultTools(store storage.Provider) map[string]Tool {
	tools := map[string]Tool{}

	// ── list_devices ──────────────────────────────────────────────────
	tools["list_devices"] = Tool{
		Name:        "list_devices",
		Description: "List all devices owned by the authenticated user.",
		InputSchema: schema(nil),
		Run: func(c *gin.Context, _ map[string]interface{}) (interface{}, error) {
			uid, err := userID(c)
			if err != nil {
				return nil, err
			}
			devs, err := store.GetUserDevices(uid)
			if err != nil {
				return nil, err
			}
			return gin.H{"devices": devs, "count": len(devs)}, nil
		},
	}

	// ── get_device ────────────────────────────────────────────────────
	tools["get_device"] = Tool{
		Name:        "get_device",
		Description: "Fetch one device by id (must belong to the user).",
		InputSchema: schema(map[string]interface{}{
			"device_id": strProp("Device id (e.g. dev_abc123)"),
		}, "device_id"),
		Run: func(c *gin.Context, args map[string]interface{}) (interface{}, error) {
			uid, err := userID(c)
			if err != nil {
				return nil, err
			}
			id, err := argString(args, "device_id", true)
			if err != nil {
				return nil, err
			}
			d, err := store.GetDevice(id)
			if err != nil {
				return nil, err
			}
			if d == nil || d.UserID != uid {
				return nil, fmt.Errorf("device not found")
			}
			return d, nil
		},
	}

	// ── recent_telemetry ──────────────────────────────────────────────
	tools["recent_telemetry"] = Tool{
		Name:        "recent_telemetry",
		Description: "Return the last N data points for a device (default 50, max 1000).",
		InputSchema: schema(map[string]interface{}{
			"device_id": strProp("Device id"),
			"n":         intProp("How many readings to return (1..1000)"),
		}, "device_id"),
		Run: func(c *gin.Context, args map[string]interface{}) (interface{}, error) {
			uid, err := userID(c)
			if err != nil {
				return nil, err
			}
			id, err := argString(args, "device_id", true)
			if err != nil {
				return nil, err
			}
			n, err := argInt(args, "n", 50, 1000)
			if err != nil {
				return nil, err
			}
			// Ownership check.
			d, err := store.GetDevice(id)
			if err != nil || d == nil || d.UserID != uid {
				return nil, fmt.Errorf("device not found")
			}
			points, err := store.GetDataHistory(id, n)
			if err != nil {
				return nil, err
			}
			return gin.H{"device_id": id, "points": points, "count": len(points)}, nil
		},
	}

	// ── telemetry_range ───────────────────────────────────────────────
	tools["telemetry_range"] = Tool{
		Name:        "telemetry_range",
		Description: "Return data points for a device between two RFC3339 timestamps.",
		InputSchema: schema(map[string]interface{}{
			"device_id": strProp("Device id"),
			"from":      strProp("Start time (RFC3339)"),
			"to":        strProp("End time (RFC3339)"),
			"n":         intProp("Cap on number of points (default 1000, max 10000)"),
		}, "device_id", "from", "to"),
		Run: func(c *gin.Context, args map[string]interface{}) (interface{}, error) {
			uid, err := userID(c)
			if err != nil {
				return nil, err
			}
			id, err := argString(args, "device_id", true)
			if err != nil {
				return nil, err
			}
			from, _ := argString(args, "from", true)
			to, _ := argString(args, "to", true)
			tFrom, err := time.Parse(time.RFC3339, from)
			if err != nil {
				return nil, fmt.Errorf("from: %w", err)
			}
			tTo, err := time.Parse(time.RFC3339, to)
			if err != nil {
				return nil, fmt.Errorf("to: %w", err)
			}
			n, err := argInt(args, "n", 1000, 10000)
			if err != nil {
				return nil, err
			}
			d, err := store.GetDevice(id)
			if err != nil || d == nil || d.UserID != uid {
				return nil, fmt.Errorf("device not found")
			}
			points, err := store.GetDataHistoryWithRange(id, tFrom, tTo, n)
			if err != nil {
				return nil, err
			}
			return gin.H{"device_id": id, "from": from, "to": to, "points": points, "count": len(points)}, nil
		},
	}

	// ── list_collections ──────────────────────────────────────────────
	tools["list_collections"] = Tool{
		Name:        "list_collections",
		Description: "List document collections (e.g. spaces, plants, calendar_entries) for the user.",
		InputSchema: schema(nil),
		Run: func(c *gin.Context, _ map[string]interface{}) (interface{}, error) {
			uid, err := userID(c)
			if err != nil {
				return nil, err
			}
			all, err := store.ListAllCollections()
			if err != nil {
				return nil, err
			}
			mine := make([]storage.CollectionInfo, 0)
			for _, ci := range all {
				if ci.UserID == uid {
					mine = append(mine, ci)
				}
			}
			return gin.H{"collections": mine}, nil
		},
	}

	// ── list_documents ────────────────────────────────────────────────
	tools["list_documents"] = Tool{
		Name:        "list_documents",
		Description: "List documents in a collection (e.g. spaces, plants).",
		InputSchema: schema(map[string]interface{}{
			"collection": strProp("Collection name"),
		}, "collection"),
		Run: func(c *gin.Context, args map[string]interface{}) (interface{}, error) {
			uid, err := userID(c)
			if err != nil {
				return nil, err
			}
			col, err := argString(args, "collection", true)
			if err != nil {
				return nil, err
			}
			docs, err := store.ListDocuments(uid, col)
			if err != nil {
				return nil, err
			}
			return gin.H{"collection": col, "documents": docs, "count": len(docs)}, nil
		},
	}

	// ── get_document ──────────────────────────────────────────────────
	tools["get_document"] = Tool{
		Name:        "get_document",
		Description: "Fetch a single document by id from a collection.",
		InputSchema: schema(map[string]interface{}{
			"collection":  strProp("Collection name"),
			"document_id": strProp("Document id"),
		}, "collection", "document_id"),
		Run: func(c *gin.Context, args map[string]interface{}) (interface{}, error) {
			uid, err := userID(c)
			if err != nil {
				return nil, err
			}
			col, _ := argString(args, "collection", true)
			id, _ := argString(args, "document_id", true)
			doc, err := store.GetDocument(uid, col, id)
			if err != nil {
				return nil, err
			}
			return doc, nil
		},
	}

	// ── search_documents ──────────────────────────────────────────────
	tools["search_documents"] = Tool{
		Name:        "search_documents",
		Description: "Substring search across documents in a collection (case-insensitive). Naive — for richer search, index client-side.",
		InputSchema: schema(map[string]interface{}{
			"collection": strProp("Collection name"),
			"query":      strProp("Substring to search for in any string field"),
			"limit":      intProp("Max results (default 20, max 200)"),
		}, "collection", "query"),
		Run: func(c *gin.Context, args map[string]interface{}) (interface{}, error) {
			uid, err := userID(c)
			if err != nil {
				return nil, err
			}
			col, _ := argString(args, "collection", true)
			q, _ := argString(args, "query", true)
			limit, _ := argInt(args, "limit", 20, 200)
			docs, err := store.ListDocuments(uid, col)
			if err != nil {
				return nil, err
			}
			matches := matchDocuments(docs, q, limit)
			return gin.H{"collection": col, "query": q, "matches": matches, "count": len(matches)}, nil
		},
	}

	// ── whoami ────────────────────────────────────────────────────────
	tools["whoami"] = Tool{
		Name:        "whoami",
		Description: "Return the authenticated user's id, email, and role.",
		InputSchema: schema(nil),
		Run: func(c *gin.Context, _ map[string]interface{}) (interface{}, error) {
			uid, err := userID(c)
			if err != nil {
				return nil, err
			}
			email, _ := c.Get("email")
			role, _ := c.Get("role")
			return gin.H{"user_id": uid, "email": email, "role": role}, nil
		},
	}

	return tools
}

// matchDocuments returns docs containing q (case-insensitive) in any
// string field, capped at limit.
func matchDocuments(docs []map[string]interface{}, q string, limit int) []map[string]interface{} {
	if q == "" {
		if len(docs) > limit {
			return docs[:limit]
		}
		return docs
	}
	q = toLower(q)
	out := make([]map[string]interface{}, 0, limit)
	for _, d := range docs {
		if matchesAny(d, q) {
			out = append(out, d)
			if len(out) >= limit {
				break
			}
		}
	}
	return out
}

func matchesAny(d map[string]interface{}, q string) bool {
	for _, v := range d {
		if s, ok := v.(string); ok && containsFold(s, q) {
			return true
		}
		if m, ok := v.(map[string]interface{}); ok && matchesAny(m, q) {
			return true
		}
	}
	return false
}

func containsFold(s, sub string) bool {
	return len(sub) == 0 || indexFold(toLower(s), sub) >= 0
}

func toLower(s string) string {
	b := []byte(s)
	for i, c := range b {
		if c >= 'A' && c <= 'Z' {
			b[i] = c + 32
		}
	}
	return string(b)
}

func indexFold(haystack, needle string) int {
	if len(needle) == 0 {
		return 0
	}
	for i := 0; i+len(needle) <= len(haystack); i++ {
		if haystack[i:i+len(needle)] == needle {
			return i
		}
	}
	return -1
}
