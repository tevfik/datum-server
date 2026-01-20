package main

import (
	"net/http"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/rs/zerolog/log"
)

// DBAdminHandler handles admin operations for document collections
type DBAdminHandler struct {
	store storage.Provider
}

// NewDBAdminHandler creates a new admin handler for collections
func NewDBAdminHandler(store storage.Provider) *DBAdminHandler {
	return &DBAdminHandler{store: store}
}

// RegisterRoutes registers admin DB routes
func (h *DBAdminHandler) RegisterRoutes(r *gin.Engine) {
	adminGroup := r.Group("/admin/db")
	adminGroup.Use(auth.UserAuthMiddleware(h.store))
	adminGroup.Use(auth.AdminMiddleware(h.store))
	{
		adminGroup.GET("/collections", h.listAllCollections)
		adminGroup.GET("/:user_id/:collection", h.listUserDocuments)
		adminGroup.POST("/:user_id/:collection", h.createUserDocument)
		adminGroup.PUT("/:user_id/:collection/:doc_id", h.updateUserDocument)
		adminGroup.DELETE("/:user_id/:collection/:doc_id", h.deleteUserDocument)
	}
}

// listAllCollections returns all collections across all users (admin only)
func (h *DBAdminHandler) listAllCollections(c *gin.Context) {
	collections, err := h.store.ListAllCollections()
	if err != nil {
		log.Error().Err(err).Msg("Failed to list all collections")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list collections"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"collections": collections})
}

// listUserDocuments returns documents in a specific user's collection (admin only)
func (h *DBAdminHandler) listUserDocuments(c *gin.Context) {
	userID := c.Param("user_id")
	collection := c.Param("collection")

	docs, err := h.store.ListDocuments(userID, collection)
	if err != nil {
		log.Error().Err(err).Str("user_id", userID).Str("collection", collection).Msg("Failed to list user documents")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list documents"})
		return
	}

	// Ensure empty list is not null
	if docs == nil {
		docs = []map[string]interface{}{}
	}

	c.JSON(http.StatusOK, docs)
}

// createUserDocument creates a document in a specific user's collection (admin only)
func (h *DBAdminHandler) createUserDocument(c *gin.Context) {
	userID := c.Param("user_id")
	collection := c.Param("collection")

	var doc map[string]interface{}
	if err := c.ShouldBindJSON(&doc); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid JSON"})
		return
	}

	id, err := h.store.CreateDocument(userID, collection, doc)
	if err != nil {
		log.Error().Err(err).Str("user_id", userID).Str("collection", collection).Msg("Failed to create document")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to create document"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{"id": id, "status": "created"})
}

// updateUserDocument updates a specific document in a specific user's collection (admin only)
func (h *DBAdminHandler) updateUserDocument(c *gin.Context) {
	userID := c.Param("user_id")
	collection := c.Param("collection")
	docID := c.Param("doc_id")

	var doc map[string]interface{}
	if err := c.ShouldBindJSON(&doc); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid JSON"})
		return
	}

	err := h.store.UpdateDocument(userID, collection, docID, doc)
	if err != nil {
		log.Error().Err(err).Str("user_id", userID).Str("doc_id", docID).Msg("Failed to update document")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update document"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "updated"})
}

// deleteUserDocument deletes a specific document from a user's collection (admin only)
func (h *DBAdminHandler) deleteUserDocument(c *gin.Context) {
	userID := c.Param("user_id")
	collection := c.Param("collection")
	docID := c.Param("doc_id")

	err := h.store.DeleteDocument(userID, collection, docID)
	if err != nil {
		log.Error().Err(err).Str("user_id", userID).Str("doc_id", docID).Msg("Failed to delete document")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete document"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "deleted"})
}
