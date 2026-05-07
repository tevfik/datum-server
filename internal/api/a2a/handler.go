// Package a2aapi is datum-server's gateway in front of gleann's
// A2A (Agent-to-Agent) protocol surface.
//
// It proxies chat requests to gleann, adding user authentication
// and optional per-user quota checks.
//
// Routes registered (all require user auth):
//
//	POST /a2a/chat           → forward to gleann A2A message:send
//	GET  /a2a/agent-card     → forward gleann's agent card
package a2aapi

import (
	"io"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
)

const defaultGleannURL = "http://localhost:8080"

// Handler proxies A2A requests to gleann.
type Handler struct {
	gleannURL string
	client    *http.Client
}

// New builds a Handler reading the upstream URL from GLEANN_URL.
func New() *Handler {
	base := strings.TrimRight(os.Getenv("GLEANN_URL"), "/")
	if base == "" {
		base = defaultGleannURL
	}
	return &Handler{
		gleannURL: base,
		client:    &http.Client{Timeout: 60 * time.Second}, // chat can be slow
	}
}

// RegisterRoutes mounts the A2A routes on the given group.
// The group is expected to already have user-auth middleware applied.
func (h *Handler) RegisterRoutes(g *gin.RouterGroup) {
	g.POST("/a2a/chat", h.chat)
	g.GET("/a2a/agent-card", h.agentCard)
}

// chat forwards a message:send request to gleann.
func (h *Handler) chat(c *gin.Context) {
	target := h.gleannURL + "/a2a/v1/message:send"

	body, err := io.ReadAll(io.LimitReader(c.Request.Body, 64*1024))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "bad request body"})
		return
	}

	req, err := http.NewRequestWithContext(
		c.Request.Context(), http.MethodPost, target, strings.NewReader(string(body)),
	)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"error": "failed to build upstream request"})
		return
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := h.client.Do(req)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"error": "gleann unreachable"})
		return
	}
	defer resp.Body.Close()

	c.Status(resp.StatusCode)
	for _, key := range []string{"Content-Type", "X-Request-Id"} {
		if v := resp.Header.Get(key); v != "" {
			c.Header(key, v)
		}
	}
	io.Copy(c.Writer, resp.Body)
}

// agentCard returns gleann's agent-card (skill discovery).
func (h *Handler) agentCard(c *gin.Context) {
	target := h.gleannURL + "/.well-known/agent-card.json"

	req, err := http.NewRequestWithContext(
		c.Request.Context(), http.MethodGet, target, nil,
	)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"error": "failed to build upstream request"})
		return
	}

	resp, err := h.client.Do(req)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"error": "gleann unreachable"})
		return
	}
	defer resp.Body.Close()

	c.Status(resp.StatusCode)
	c.Header("Content-Type", "application/json")
	io.Copy(c.Writer, resp.Body)
}
