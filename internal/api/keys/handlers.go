package keys

import (
	"net/http"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"
	"datum-go/internal/utils"

	"github.com/gin-gonic/gin"
)

type Handler struct {
	Store storage.Provider
}

func NewHandler(store storage.Provider) *Handler {
	return &Handler{Store: store}
}

type CreateKeyRequest struct {
	Name string `json:"name" binding:"required"`
}

type KeyResponse struct {
	ID        string    `json:"id"`
	Name      string    `json:"name"`
	Key       string    `json:"key,omitempty"` // Only shown on creation
	CreatedAt time.Time `json:"created_at"`
}

// CreateKeyHandler creates a new User API Key
// POST /auth/keys
func (h *Handler) CreateKeyHandler(c *gin.Context) {
	userID, err := auth.GetUserID(c)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	var req CreateKeyRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Generate ak_ key
	akKey, err := auth.GenerateUserAPIKey()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate key"})
		return
	}

	keyID := utils.GenerateID("key")
	apiKey := &storage.APIKey{
		ID:        keyID,
		UserID:    userID,
		Name:      req.Name,
		Key:       akKey,
		CreatedAt: time.Now(),
	}

	if err := h.Store.CreateUserAPIKey(apiKey); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusCreated, KeyResponse{
		ID:        apiKey.ID,
		Name:      apiKey.Name,
		Key:       apiKey.Key, // Show key once
		CreatedAt: apiKey.CreatedAt,
	})
}

// ListKeysHandler lists all keys for the user
// GET /auth/keys
func (h *Handler) ListKeysHandler(c *gin.Context) {
	userID, err := auth.GetUserID(c)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	keys, err := h.Store.GetUserAPIKeys(userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list keys"})
		return
	}

	var response []KeyResponse
	for _, k := range keys {
		response = append(response, KeyResponse{
			ID:        k.ID,
			Name:      k.Name,
			Key:       "ak_**************************" + k.Key[len(k.Key)-4:], // Masked
			CreatedAt: k.CreatedAt,
		})
	}
	// Return empty list if nil
	if response == nil {
		response = []KeyResponse{}
	}

	c.JSON(http.StatusOK, gin.H{"keys": response})
}

// DeleteKeyHandler revokes a key
// DELETE /auth/keys/:id
func (h *Handler) DeleteKeyHandler(c *gin.Context) {
	userID, err := auth.GetUserID(c)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	keyID := c.Param("id")
	if err := h.Store.DeleteUserAPIKey(userID, keyID); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Key deleted"})
}

// RegisterRoutes registers key management routes
func (h *Handler) RegisterRoutes(r *gin.RouterGroup) {
	r.POST("/keys", h.CreateKeyHandler)
	r.GET("/keys", h.ListKeysHandler)
	r.DELETE("/keys/:id", h.DeleteKeyHandler)
}
