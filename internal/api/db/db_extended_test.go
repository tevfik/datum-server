package db

import (
	"bytes"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

// ---------- GetStats ----------

func TestGetStats_Success(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.GET("/admin/database/stats", handler.GetStats)

	req, _ := http.NewRequest("GET", "/admin/database/stats", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// ---------- UpdateDoc NotFound ----------

func TestUpdateDoc_NotFound(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(userMiddleware("user_upd_nf"))
	r.PUT("/db/:collection/:id", handler.UpdateDoc)

	body := bytes.NewBufferString("{}")
	req, _ := http.NewRequest("PUT", "/db/notes/nonexistent_id", body)
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// ---------- DeleteDoc NotFound ----------

func TestDeleteDoc_NotFound(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(userMiddleware("user_del_nf"))
	r.DELETE("/db/:collection/:id", handler.DeleteDoc)

	req, _ := http.NewRequest("DELETE", "/db/notes/nonexistent_id", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// ---------- UpdateDoc InvalidJSON ----------

func TestUpdateDoc_InvalidJSON(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(userMiddleware("user_upd_inv"))
	r.PUT("/db/:collection/:id", handler.UpdateDoc)

	body := bytes.NewBufferString("not-json")
	req, _ := http.NewRequest("PUT", "/db/notes/some_id", body)
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}
