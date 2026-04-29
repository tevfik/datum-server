// Package mcp implements a minimal Model Context Protocol server that
// exposes the user's datum-server data to any MCP client (gleann, Claude
// Desktop, Cursor, ekiyo Asistan, …).
//
// Wire protocol: JSON-RPC 2.0 over a single HTTP POST endpoint
// (Streamable HTTP transport). Every request is authenticated via the
// regular user-auth middleware, so the bearer token can be either a JWT
// or an `ak_…` user API key. Tools execute under the user's identity and
// only see the user's own data.
//
// See docs/P1_GLEANN_INTEGRATION.md for the design rationale.
package mcp

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"

	"datum-go/internal/audit"
	"datum-go/internal/quota"
	"datum-go/internal/storage"
)

// ProtocolVersion is the MCP spec version this server implements.
const ProtocolVersion = "2025-06-18"

// ServerInfo is advertised in the `initialize` response.
type ServerInfo struct {
	Name    string `json:"name"`
	Version string `json:"version"`
}

// Tool is one callable function exposed over MCP.
type Tool struct {
	Name        string                 `json:"name"`
	Description string                 `json:"description"`
	InputSchema map[string]interface{} `json:"inputSchema"`

	// Run executes the tool. ctx contains "user_id" + "role" set by the
	// auth middleware. args are the raw JSON-decoded params.
	Run func(c *gin.Context, args map[string]interface{}) (interface{}, error) `json:"-"`
}

// Server holds the MCP handler state.
type Server struct {
	store storage.Provider
	tools map[string]Tool
	info  ServerInfo
	quota *quota.Manager // optional
}

// NewServer constructs an MCP server with the default tool set.
// quota may be nil to disable quota enforcement (tests).
func NewServer(store storage.Provider, version string, q *quota.Manager) *Server {
	s := &Server{
		store: store,
		quota: q,
		info: ServerInfo{
			Name:    "datum-server",
			Version: version,
		},
	}
	s.tools = defaultTools(store)
	return s
}

// RegisterRoutes wires the /mcp endpoint into the given gin engine.
// The handler MUST be wrapped in user-auth middleware by the caller.
func (s *Server) RegisterRoutes(g *gin.RouterGroup) {
	g.POST("", s.handleRPC)
	// .well-known endpoint exposed unauthenticated by the caller.
}

// WellKnown returns the discovery document advertised at
// /.well-known/mcp.json. It does not require auth.
func (s *Server) WellKnown(publicURL string) gin.HandlerFunc {
	return func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"name":             s.info.Name,
			"version":          s.info.Version,
			"protocol_version": ProtocolVersion,
			"endpoint":         publicURL + "/mcp",
			"auth": gin.H{
				"type":   "bearer",
				"hint":   "JWT issued by /auth/login OR an `ak_…` API key from /auth/keys",
				"scopes": []string{"mcp:read", "mcp:notify", "mcp:write"},
			},
		})
	}
}

// ── JSON-RPC ───────────────────────────────────────────────────────────────

type rpcRequest struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id,omitempty"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params,omitempty"`
}

type rpcError struct {
	Code    int         `json:"code"`
	Message string      `json:"message"`
	Data    interface{} `json:"data,omitempty"`
}

type rpcResponse struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id,omitempty"`
	Result  interface{}     `json:"result,omitempty"`
	Error   *rpcError       `json:"error,omitempty"`
}

const (
	errParseError     = -32700
	errInvalidRequest = -32600
	errMethodNotFound = -32601
	errInvalidParams  = -32602
	errInternalError  = -32603
)

func (s *Server) handleRPC(c *gin.Context) {
	var req rpcRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		writeRPC(c, nil, nil, &rpcError{Code: errParseError, Message: "parse error: " + err.Error()})
		return
	}
	if req.JSONRPC != "2.0" {
		writeRPC(c, req.ID, nil, &rpcError{Code: errInvalidRequest, Message: "jsonrpc must be 2.0"})
		return
	}

	switch req.Method {
	case "initialize":
		writeRPC(c, req.ID, gin.H{
			"protocolVersion": ProtocolVersion,
			"serverInfo":      s.info,
			"capabilities": gin.H{
				"tools": gin.H{"listChanged": false},
			},
		}, nil)

	case "tools/list":
		list := make([]Tool, 0, len(s.tools))
		for _, t := range s.tools {
			list = append(list, t)
		}
		writeRPC(c, req.ID, gin.H{"tools": list}, nil)

	case "tools/call":
		var p struct {
			Name      string                 `json:"name"`
			Arguments map[string]interface{} `json:"arguments"`
		}
		if err := json.Unmarshal(req.Params, &p); err != nil {
			writeRPC(c, req.ID, nil, &rpcError{Code: errInvalidParams, Message: err.Error()})
			return
		}
		tool, ok := s.tools[p.Name]
		if !ok {
			writeRPC(c, req.ID, nil, &rpcError{Code: errMethodNotFound, Message: "unknown tool: " + p.Name})
			s.audit(c, p.Name, p.Arguments, "unknown_tool")
			return
		}
		// Quota check: every tool counts as one MCP call.
		if s.quota != nil {
			if uid, _ := c.Get("user_id"); uid != nil {
				if uidStr, _ := uid.(string); uidStr != "" {
					if err := s.quota.Check(uidStr, quota.ResourceMCPCall, 1); err != nil {
						writeRPC(c, req.ID, nil, &rpcError{Code: errInternalError, Message: err.Error()})
						s.audit(c, p.Name, p.Arguments, "quota_exceeded")
						return
					}
					s.quota.Increment(uidStr, quota.ResourceMCPCall, 1)
				}
			}
		}
		out, err := tool.Run(c, p.Arguments)
		if err != nil {
			writeRPC(c, req.ID, nil, &rpcError{Code: errInternalError, Message: err.Error()})
			s.audit(c, p.Name, p.Arguments, "error:"+err.Error())
			return
		}
		// MCP "tool result" wraps content in a typed array. We return JSON.
		payload, _ := json.Marshal(out)
		writeRPC(c, req.ID, gin.H{
			"content": []gin.H{{
				"type": "text",
				"text": string(payload),
			}},
			"isError": false,
		}, nil)
		s.audit(c, p.Name, p.Arguments, "ok")

	case "ping":
		writeRPC(c, req.ID, gin.H{}, nil)

	default:
		writeRPC(c, req.ID, nil, &rpcError{Code: errMethodNotFound, Message: "unknown method: " + req.Method})
	}
}

func writeRPC(c *gin.Context, id json.RawMessage, result interface{}, err *rpcError) {
	resp := rpcResponse{JSONRPC: "2.0", ID: id, Result: result, Error: err}
	c.JSON(http.StatusOK, resp)
}

// audit emits an audit log entry for one tool invocation.
func (s *Server) audit(c *gin.Context, tool string, args map[string]interface{}, status string) {
	uid, _ := c.Get("user_id")
	role, _ := c.Get("role")
	uidStr, _ := uid.(string)
	roleStr, _ := role.(string)

	// Hash the args so we can correlate calls without leaking PII.
	argsHash := ""
	if len(args) > 0 {
		b, _ := json.Marshal(args)
		h := sha256.Sum256(b)
		argsHash = hex.EncodeToString(h[:8])
	}

	audit.Log(audit.Entry{
		Action:    audit.Action("mcp.tool_call"),
		ActorID:   uidStr,
		ActorRole: roleStr,
		TargetID:  tool,
		IP:        c.ClientIP(),
		Details: map[string]interface{}{
			"args_hash": argsHash,
			"status":    status,
			"ts":        time.Now().UTC().Format(time.RFC3339),
		},
	})
}

// ── Helpers exposed for tests ─────────────────────────────────────────────

// ErrUnauthenticated is returned when user_id is missing from context.
var ErrUnauthenticated = errors.New("unauthenticated")

func userID(c *gin.Context) (string, error) {
	v, _ := c.Get("user_id")
	s, _ := v.(string)
	if s == "" {
		return "", ErrUnauthenticated
	}
	return s, nil
}

func argString(args map[string]interface{}, key string, required bool) (string, error) {
	v, ok := args[key]
	if !ok {
		if required {
			return "", fmt.Errorf("missing required argument %q", key)
		}
		return "", nil
	}
	s, ok := v.(string)
	if !ok {
		return "", fmt.Errorf("argument %q must be a string", key)
	}
	return s, nil
}

func argInt(args map[string]interface{}, key string, def, max int) (int, error) {
	v, ok := args[key]
	if !ok {
		return def, nil
	}
	f, ok := v.(float64)
	if !ok {
		return 0, fmt.Errorf("argument %q must be a number", key)
	}
	n := int(f)
	if max > 0 && n > max {
		n = max
	}
	if n < 0 {
		n = def
	}
	return n, nil
}
