// Package quotaapi exposes the in-memory quota manager over HTTP.
//
//	GET  /auth/me/quota                — current user usage + plan limits
//	GET  /admin/quotas/plans           — list available plans
//	GET  /admin/quotas/users/:user_id  — usage for one user
//	PUT  /admin/quotas/users/:user_id  — set plan name for a user
package quotaapi

import (
	"net/http"

	"github.com/gin-gonic/gin"

	"datum-go/internal/quota"
)

type Handler struct {
	M *quota.Manager
}

func New(m *quota.Manager) *Handler {
	return &Handler{M: m}
}

// RegisterUserRoutes wires the user-facing routes (under existing /auth group).
func (h *Handler) RegisterUserRoutes(g *gin.RouterGroup) {
	g.GET("/me/quota", h.MyQuota)
}

// RegisterAdminRoutes wires the admin routes (under /admin group).
func (h *Handler) RegisterAdminRoutes(g *gin.RouterGroup) {
	g.GET("/quotas/plans", h.ListPlans)
	g.GET("/quotas/users/:user_id", h.UserUsage)
	g.PUT("/quotas/users/:user_id", h.SetUserPlan)
}

// MyQuota returns the caller's usage + plan.
func (h *Handler) MyQuota(c *gin.Context) {
	uid, _ := c.Get("user_id")
	uidStr, _ := uid.(string)
	if uidStr == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "no user"})
		return
	}
	c.JSON(http.StatusOK, gin.H{
		"plan":  h.M.PlanFor(uidStr),
		"usage": h.M.Usage(uidStr),
	})
}

func (h *Handler) ListPlans(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{"plans": quota.DefaultPlans})
}

func (h *Handler) UserUsage(c *gin.Context) {
	uid := c.Param("user_id")
	c.JSON(http.StatusOK, gin.H{
		"user_id": uid,
		"plan":    h.M.PlanFor(uid),
		"usage":   h.M.Usage(uid),
	})
}

func (h *Handler) SetUserPlan(c *gin.Context) {
	uid := c.Param("user_id")
	var body struct {
		Plan string `json:"plan" binding:"required"`
	}
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	if _, ok := quota.DefaultPlans[body.Plan]; !ok {
		c.JSON(http.StatusBadRequest, gin.H{"error": "unknown plan"})
		return
	}
	h.M.SetUserPlan(uid, body.Plan)
	c.JSON(http.StatusOK, gin.H{"user_id": uid, "plan": body.Plan})
}
