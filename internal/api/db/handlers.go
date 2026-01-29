// Package db provides HTTP handlers for document database operations.
package db

import (
	"net/http"
	"strings"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/rs/zerolog/log"
)

// Handler provides database HTTP handlers.
type Handler struct {
	Store storage.Provider
}

// NewHandler creates a new database handler with dependencies.
func NewHandler(store storage.Provider) *Handler {
	return &Handler{
		Store: store,
	}
}

// RegisterRoutes registers user database routes.
func (h *Handler) RegisterRoutes(r *gin.RouterGroup) {
	r.POST("/:collection", h.CreateDoc)
	r.GET("/:collection", h.ListDocs)
	r.GET("/:collection/:id", h.GetDoc)
	r.PUT("/:collection/:id", h.UpdateDoc)
	r.DELETE("/:collection/:id", h.DeleteDoc)
}

// CreateDoc creates a user document.
// POST /db/:collection
func (h *Handler) CreateDoc(c *gin.Context) {
	userID := c.GetString("userID")
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

// ListDocs lists user documents in a collection.
// GET /db/:collection
func (h *Handler) ListDocs(c *gin.Context) {
	userID := c.GetString("userID")
	collection := c.Param("collection")

	docs, err := h.Store.ListDocuments(userID, collection)
	if err != nil {
		log.Error().Err(err).Str("user_id", userID).Str("collection", collection).Msg("Failed to list documents")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list documents"})
		return
	}

	if docs == nil {
		docs = []map[string]interface{}{}
	}

	c.JSON(http.StatusOK, docs)
}

// GetDoc gets a specific user document.
// GET /db/:collection/:id
func (h *Handler) GetDoc(c *gin.Context) {
	userID := c.GetString("userID")
	collection := c.Param("collection")
	docID := c.Param("id")

	doc, err := h.Store.GetDocument(userID, collection, docID)
	if err != nil {
		if strings.Contains(err.Error(), "not found") {
			c.JSON(http.StatusNotFound, gin.H{"error": "Document not found"})
			return
		}

		log.Error().Err(err).Str("user_id", userID).Str("doc_id", docID).Msg("Failed to get document")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to get document"})
		return
	}

	c.JSON(http.StatusOK, doc)
}

// UpdateDoc updates a specific user document.
// PUT /db/:collection/:id
func (h *Handler) UpdateDoc(c *gin.Context) {
	userID := c.GetString("userID")
	collection := c.Param("collection")
	docID := c.Param("id")

	var doc map[string]interface{}
	if err := c.ShouldBindJSON(&doc); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid JSON"})
		return
	}

	err := h.Store.UpdateDocument(userID, collection, docID, doc)
	if err != nil {
		if strings.Contains(err.Error(), "not found") {
			c.JSON(http.StatusNotFound, gin.H{"error": "Document not found"})
			return
		}
		log.Error().Err(err).Str("user_id", userID).Str("doc_id", docID).Msg("Failed to update document")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update document"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "updated"})
}

// DeleteDoc deletes a specific user document.
// DELETE /db/:collection/:id
func (h *Handler) DeleteDoc(c *gin.Context) {
	userID := c.GetString("userID")
	collection := c.Param("collection")
	docID := c.Param("id")

	err := h.Store.DeleteDocument(userID, collection, docID)
	if err != nil {
		if strings.Contains(err.Error(), "not found") {
			c.JSON(http.StatusNotFound, gin.H{"error": "Document not found"})
			return
		}
		log.Error().Err(err).Str("user_id", userID).Str("doc_id", docID).Msg("Failed to delete document")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete document"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "deleted"})
}
