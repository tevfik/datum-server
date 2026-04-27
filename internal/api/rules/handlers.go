package rules

import (
	"net/http"

	"datum-go/internal/rules"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

// Handler manages rule engine API endpoints.
type Handler struct {
	engine *rules.Engine
}

// NewHandler creates a new rules API handler.
func NewHandler(engine *rules.Engine) *Handler {
	return &Handler{engine: engine}
}

// RegisterRoutes registers rule management routes on the given group.
func (h *Handler) RegisterRoutes(rg *gin.RouterGroup) {
	rg.GET("", h.ListRules)
	rg.POST("", h.CreateRule)
	rg.GET("/:rule_id", h.GetRule)
	rg.DELETE("/:rule_id", h.DeleteRule)
	rg.PUT("/:rule_id/enable", h.EnableRule)
	rg.PUT("/:rule_id/disable", h.DisableRule)
}

func (h *Handler) ListRules(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"rules": h.engine.ListRules()})
}

func (h *Handler) CreateRule(c *gin.Context) {
	var r rules.Rule
	if err := c.ShouldBindJSON(&r); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	if r.ID == "" {
		r.ID = "rule_" + uuid.New().String()[:8]
	}
	h.engine.AddRule(&r)
	c.JSON(http.StatusCreated, r)
}

func (h *Handler) GetRule(c *gin.Context) {
	id := c.Param("rule_id")
	r, ok := h.engine.GetRule(id)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "rule not found"})
		return
	}
	c.JSON(http.StatusOK, r)
}

func (h *Handler) DeleteRule(c *gin.Context) {
	id := c.Param("rule_id")
	h.engine.RemoveRule(id)
	c.JSON(http.StatusOK, gin.H{"status": "deleted"})
}

func (h *Handler) EnableRule(c *gin.Context) {
	id := c.Param("rule_id")
	r, ok := h.engine.GetRule(id)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "rule not found"})
		return
	}
	r.Enabled = true
	c.JSON(http.StatusOK, r)
}

func (h *Handler) DisableRule(c *gin.Context) {
	id := c.Param("rule_id")
	r, ok := h.engine.GetRule(id)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "rule not found"})
		return
	}
	r.Enabled = false
	c.JSON(http.StatusOK, r)
}
