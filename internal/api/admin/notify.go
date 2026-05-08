package admin

import (
	"net/http"

	"datum-go/internal/notify"

	"github.com/gin-gonic/gin"
)

// TestNotificationRequest defines the body for testing push notifications
type TestNotificationRequest struct {
	UserID   string `json:"user_id" binding:"required"`
	Title    string `json:"title"`
	Message  string `json:"message" binding:"required"`
	Priority string `json:"priority"`
}

// TestNotificationHandler dispatches a test notification to a specific user.
// This is used to verify SSE and other notification channels.
func (h *AdminHandler) TestNotificationHandler(c *gin.Context) {
	var req TestNotificationRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if h.Dispatcher == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "Notification dispatcher not initialized"})
		return
	}

	if req.Title == "" {
		req.Title = "Test Notification"
	}

	if req.Priority == "" {
		req.Priority = "high"
	}

	h.Dispatcher.Dispatch(notify.Notification{
		UserID:   req.UserID,
		Title:    req.Title,
		Message:  req.Message,
		Priority: req.Priority,
	})

	c.JSON(http.StatusOK, gin.H{
		"message": "Notification dispatched",
		"user_id": req.UserID,
		"topic":   "datum-" + req.UserID,
	})
}
