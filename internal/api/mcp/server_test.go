package mcp

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupMCPTest(t *testing.T) (storage.Provider, *gin.Engine, string, func()) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	tmp := t.TempDir()
	store, err := storage.New(
		filepath.Join(tmp, "meta.db"),
		filepath.Join(tmp, "tsdata"),
		24*time.Hour,
	)
	require.NoError(t, err)

	// Seed a user.
	uid := "u_mcp_test"
	require.NoError(t, store.CreateUser(&storage.User{
		ID:           uid,
		Email:        "mcp@test.local",
		PasswordHash: "$2a$10$abcdefghijklmnopqrstuv",
		Role:         "user",
	}))
	// One device.
	require.NoError(t, store.CreateDevice(&storage.Device{
		ID:     "dev_mcp_1",
		UserID: uid,
		Name:   "Sensor",
		Status: "active",
	}))
	// One document.
	_, err = store.CreateDocument(uid, "spaces", map[string]interface{}{
		"id":   "sp_1",
		"name": "Balcony",
		"type": "balcony",
	})
	require.NoError(t, err)

	srv := NewServer(store, "test", nil)
	r := gin.New()
	// Inject auth context manually so we don't drag in JWT plumbing.
	g := r.Group("/mcp", func(c *gin.Context) {
		c.Set("user_id", uid)
		c.Set("email", "mcp@test.local")
		c.Set("role", "user")
		c.Next()
	})
	srv.RegisterRoutes(g)

	return store, r, uid, func() { store.Close() }
}

func rpc(t *testing.T, r *gin.Engine, method string, params interface{}) *rpcResponse {
	t.Helper()
	body := map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      1,
		"method":  method,
	}
	if params != nil {
		body["params"] = params
	}
	buf, _ := json.Marshal(body)
	req := httptest.NewRequest(http.MethodPost, "/mcp", bytes.NewReader(buf))
	req.Header.Set("Content-Type", "application/json")
	rec := httptest.NewRecorder()
	r.ServeHTTP(rec, req)
	require.Equal(t, http.StatusOK, rec.Code, "body: %s", rec.Body.String())
	var resp rpcResponse
	require.NoError(t, json.Unmarshal(rec.Body.Bytes(), &resp))
	return &resp
}

func TestInitialize(t *testing.T) {
	_, r, _, cleanup := setupMCPTest(t)
	defer cleanup()
	resp := rpc(t, r, "initialize", nil)
	require.Nil(t, resp.Error)
	res := resp.Result.(map[string]interface{})
	assert.Equal(t, ProtocolVersion, res["protocolVersion"])
	info := res["serverInfo"].(map[string]interface{})
	assert.Equal(t, "datum-server", info["name"])
}

func TestToolsList(t *testing.T) {
	_, r, _, cleanup := setupMCPTest(t)
	defer cleanup()
	resp := rpc(t, r, "tools/list", nil)
	require.Nil(t, resp.Error)
	res := resp.Result.(map[string]interface{})
	tools := res["tools"].([]interface{})
	assert.GreaterOrEqual(t, len(tools), 8, "expected at least 8 tools")
	// Sanity: every tool has a name and description.
	for _, x := range tools {
		tool := x.(map[string]interface{})
		assert.NotEmpty(t, tool["name"])
		assert.NotEmpty(t, tool["description"])
	}
}

func callTool(t *testing.T, r *gin.Engine, name string, args map[string]interface{}) (map[string]interface{}, *rpcError) {
	t.Helper()
	resp := rpc(t, r, "tools/call", map[string]interface{}{"name": name, "arguments": args})
	if resp.Error != nil {
		return nil, resp.Error
	}
	res := resp.Result.(map[string]interface{})
	content := res["content"].([]interface{})[0].(map[string]interface{})
	var out map[string]interface{}
	require.NoError(t, json.Unmarshal([]byte(content["text"].(string)), &out))
	return out, nil
}

func TestToolWhoami(t *testing.T) {
	_, r, uid, cleanup := setupMCPTest(t)
	defer cleanup()
	out, errResp := callTool(t, r, "whoami", nil)
	require.Nil(t, errResp)
	assert.Equal(t, uid, out["user_id"])
}

func TestToolListDevices(t *testing.T) {
	_, r, _, cleanup := setupMCPTest(t)
	defer cleanup()
	out, errResp := callTool(t, r, "list_devices", nil)
	require.Nil(t, errResp)
	assert.EqualValues(t, 1, out["count"])
}

func TestToolGetDevice(t *testing.T) {
	_, r, _, cleanup := setupMCPTest(t)
	defer cleanup()
	out, errResp := callTool(t, r, "get_device", map[string]interface{}{"device_id": "dev_mcp_1"})
	require.Nil(t, errResp)
	assert.Equal(t, "Sensor", out["name"])

	// Wrong device: returns rpc error.
	_, errResp = callTool(t, r, "get_device", map[string]interface{}{"device_id": "dev_other"})
	require.NotNil(t, errResp)
}

func TestToolListDocuments(t *testing.T) {
	_, r, _, cleanup := setupMCPTest(t)
	defer cleanup()
	out, errResp := callTool(t, r, "list_documents", map[string]interface{}{"collection": "spaces"})
	require.Nil(t, errResp)
	assert.EqualValues(t, 1, out["count"])
}

func TestToolSearchDocuments(t *testing.T) {
	_, r, _, cleanup := setupMCPTest(t)
	defer cleanup()
	out, errResp := callTool(t, r, "search_documents", map[string]interface{}{
		"collection": "spaces", "query": "balc",
	})
	require.Nil(t, errResp)
	assert.EqualValues(t, 1, out["count"])

	out, errResp = callTool(t, r, "search_documents", map[string]interface{}{
		"collection": "spaces", "query": "ZZZNOMATCH",
	})
	require.Nil(t, errResp)
	assert.EqualValues(t, 0, out["count"])
}

func TestUnknownMethod(t *testing.T) {
	_, r, _, cleanup := setupMCPTest(t)
	defer cleanup()
	resp := rpc(t, r, "frobnicate", nil)
	require.NotNil(t, resp.Error)
	assert.Equal(t, errMethodNotFound, resp.Error.Code)
}

func TestUnknownTool(t *testing.T) {
	_, r, _, cleanup := setupMCPTest(t)
	defer cleanup()
	_, errResp := callTool(t, r, "no_such_tool", nil)
	require.NotNil(t, errResp)
}

func TestPing(t *testing.T) {
	_, r, _, cleanup := setupMCPTest(t)
	defer cleanup()
	resp := rpc(t, r, "ping", nil)
	require.Nil(t, resp.Error)
}
