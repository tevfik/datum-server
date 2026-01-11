package main

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
	"github.com/rs/zerolog/log"
)

// DBHandler encapsulates the storage provider
type DBHandler struct {
	store interface {
		CreateDocument(userID, collection string, doc map[string]interface{}) (string, error)
		ListDocuments(userID, collection string) ([]map[string]interface{}, error)
		GetDocument(userID, collection, docID string) (map[string]interface{}, error)
		UpdateDocument(userID, collection, docID string, doc map[string]interface{}) error
		DeleteDocument(userID, collection, docID string) error
	}
}

// RegisterDBRoutes registers the /db endpoints
func RegisterDBRoutes(r *gin.RouterGroup, store interface {
	CreateDocument(userID, collection string, doc map[string]interface{}) (string, error)
	ListDocuments(userID, collection string) ([]map[string]interface{}, error)
	GetDocument(userID, collection, docID string) (map[string]interface{}, error)
	UpdateDocument(userID, collection, docID string, doc map[string]interface{}) error
	DeleteDocument(userID, collection, docID string) error
}) {
	h := &DBHandler{store: store}

	// Collections API
	// Example: POST /db/todos, GET /db/todos
	group := r.Group("/db")
	{
		group.POST("/:collection", h.createDoc)
		group.GET("/:collection", h.listDocs)
		group.GET("/:collection/:id", h.getDoc)
		group.PUT("/:collection/:id", h.updateDoc)
		group.DELETE("/:collection/:id", h.deleteDoc)
	}
}

func (h *DBHandler) createDoc(c *gin.Context) {
	userID := c.GetString("userID")
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

func (h *DBHandler) listDocs(c *gin.Context) {
	userID := c.GetString("userID")
	collection := c.Param("collection")

	docs, err := h.store.ListDocuments(userID, collection)
	if err != nil {
		log.Error().Err(err).Str("user_id", userID).Str("collection", collection).Msg("Failed to list documents")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list documents"})
		return
	}

	// Helper to ensure empty list is not null in JSON
	if docs == nil {
		docs = []map[string]interface{}{}
	}

	c.JSON(http.StatusOK, docs)
}

func (h *DBHandler) getDoc(c *gin.Context) {
	userID := c.GetString("userID")
	collection := c.Param("collection")
	docID := c.Param("id")

	doc, err := h.store.GetDocument(userID, collection, docID)
	if err != nil {
		// Differentiate between not found and other errors if possible
		// For buntdb simplicity we mostly assume strict lookups fail if not found
		// But in a real app check specific error types.
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

func (h *DBHandler) updateDoc(c *gin.Context) {
	userID := c.GetString("userID")
	collection := c.Param("collection")
	docID := c.Param("id")

	var doc map[string]interface{}
	if err := c.ShouldBindJSON(&doc); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid JSON"})
		return
	}

	err := h.store.UpdateDocument(userID, collection, docID, doc)
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

func (h *DBHandler) deleteDoc(c *gin.Context) {
	userID := c.GetString("userID")
	collection := c.Param("collection")
	docID := c.Param("id")

	err := h.store.DeleteDocument(userID, collection, docID)
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
