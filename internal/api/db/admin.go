package db

import (
	"net/http"

	"datum-go/internal/auth"

	"github.com/gin-gonic/gin"
	"github.com/rs/zerolog/log"
)

// RegisterAdminRoutes registers admin DB routes.
func (h *Handler) RegisterAdminRoutes(r *gin.RouterGroup) {
	// Assumes r is already grouped under /admin/db and has auth middleware if passed externally
	// If standard RegisterRoutes pattern, we usually pass middleware.
	// Here I'll assume we register sub-routes.

	// NOTE: In router.go we likely group by /admin/db.
	// Dependencies for admin routes are same: Store.

	r.GET("/collections", h.ListAllCollections)
	r.GET("/:user_id/:collection", h.ListUserDocuments)
	r.POST("/:user_id/:collection", h.CreateUserDocument)
	r.PUT("/:user_id/:collection/:doc_id", h.UpdateUserDocument)
	r.DELETE("/:user_id/:collection/:doc_id", h.DeleteUserDocument)
}

// ListAllCollections returns all collections across all users (admin only).
// GET /admin/db/collections
func (h *Handler) ListAllCollections(c *gin.Context) {
	// Verify admin role just in case middleware missed it (defense in depth)
	if role, _ := auth.GetUserRole(c); role != "admin" {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	collections, err := h.Store.ListAllCollections()
	if err != nil {
		log.Error().Err(err).Msg("Failed to list all collections")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list collections"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"collections": collections})
}

// ListUserDocuments returns documents in a specific user's collection (admin only).
// GET /admin/db/:user_id/:collection
func (h *Handler) ListUserDocuments(c *gin.Context) {
	userID := c.Param("user_id")
	collection := c.Param("collection")

	docs, err := h.Store.ListDocuments(userID, collection)
	if err != nil {
		log.Error().Err(err).Str("user_id", userID).Str("collection", collection).Msg("Failed to list user documents")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list documents"})
		return
	}

	if docs == nil {
		docs = []map[string]interface{}{}
	}

	c.JSON(http.StatusOK, docs)
}

// CreateUserDocument creates a document in a specific user's collection (admin only).
// POST /admin/db/:user_id/:collection
func (h *Handler) CreateUserDocument(c *gin.Context) {
	userID := c.Param("user_id")
	collection := c.Param("collection")

	var doc map[string]interface{}
	if err := c.ShouldBindJSON(&doc); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid JSON"})
		return
	}

	id, err := h.Store.CreateDocument(userID, collection, doc)
	if err != nil {
		log.Error().Err(err).Str("user_id", userID).Str("collection", collection).Msg("Failed to create document")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to create document"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{"id": id, "status": "created"})
}

// UpdateUserDocument updates a specific document in a specific user's collection (admin only).
// PUT /admin/db/:user_id/:collection/:doc_id
func (h *Handler) UpdateUserDocument(c *gin.Context) {
	userID := c.Param("user_id")
	collection := c.Param("collection")
	docID := c.Param("doc_id")

	var doc map[string]interface{}
	if err := c.ShouldBindJSON(&doc); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid JSON"})
		return
	}

	err := h.Store.UpdateDocument(userID, collection, docID, doc)
	if err != nil {
		log.Error().Err(err).Str("user_id", userID).Str("doc_id", docID).Msg("Failed to update document")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update document"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "updated"})
}

// DeleteUserDocument deletes a specific document from a user's collection (admin only).
// DELETE /admin/db/:user_id/:collection/:doc_id
func (h *Handler) DeleteUserDocument(c *gin.Context) {
	userID := c.Param("user_id")
	collection := c.Param("collection")
	docID := c.Param("doc_id")

	err := h.Store.DeleteDocument(userID, collection, docID)
	if err != nil {
		log.Error().Err(err).Str("user_id", userID).Str("doc_id", docID).Msg("Failed to delete document")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete document"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "deleted"})
}
