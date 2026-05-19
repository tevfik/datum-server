package storage

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ============ Document Store Tests ============

func TestCreateDocument(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	doc := map[string]interface{}{
		"name":  "Test Document",
		"value": 42,
	}

	id, err := s.CreateDocument("usr_doc_001", "notes", doc)
	require.NoError(t, err)
	assert.NotEmpty(t, id)
}

func TestCreateDocument_WithExistingID(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	doc := map[string]interface{}{
		"id":   "custom_id_001",
		"name": "Named Doc",
	}

	id, err := s.CreateDocument("usr_doc_002", "notes", doc)
	require.NoError(t, err)
	assert.Equal(t, "custom_id_001", id)
}

func TestListDocuments(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_list_001"

	for i := 0; i < 3; i++ {
		doc := map[string]interface{}{
			"index": i,
			"text":  "doc content",
		}
		_, err := s.CreateDocument(userID, "items", doc)
		require.NoError(t, err)
	}

	docs, err := s.ListDocuments(userID, "items")
	require.NoError(t, err)
	assert.Len(t, docs, 3)
}

func TestListDocuments_Empty(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	docs, err := s.ListDocuments("usr_empty_001", "nonexistent_collection")
	require.NoError(t, err)
	assert.Empty(t, docs)
}

func TestListDocuments_IsolatedByUser(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	// User 1 creates docs
	_, err := s.CreateDocument("usr_iso_001", "notes", map[string]interface{}{"data": "user1"})
	require.NoError(t, err)

	// User 2 creates docs in same collection name
	_, err = s.CreateDocument("usr_iso_002", "notes", map[string]interface{}{"data": "user2"})
	require.NoError(t, err)

	docs1, err := s.ListDocuments("usr_iso_001", "notes")
	require.NoError(t, err)
	assert.Len(t, docs1, 1)
	assert.Equal(t, "user1", docs1[0]["data"])

	docs2, err := s.ListDocuments("usr_iso_002", "notes")
	require.NoError(t, err)
	assert.Len(t, docs2, 1)
	assert.Equal(t, "user2", docs2[0]["data"])
}

func TestGetDocument(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_get_001"
	doc := map[string]interface{}{
		"id":    "doc_get_001",
		"title": "Hello World",
		"count": 7,
	}

	_, err := s.CreateDocument(userID, "articles", doc)
	require.NoError(t, err)

	retrieved, err := s.GetDocument(userID, "articles", "doc_get_001")
	require.NoError(t, err)
	assert.Equal(t, "Hello World", retrieved["title"])
	assert.Equal(t, float64(7), retrieved["count"])
	assert.Equal(t, userID, retrieved["_owner_id"])
	assert.NotEmpty(t, retrieved["_created_at"])
}

func TestGetDocument_NotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := s.GetDocument("usr_gd_001", "notes", "nonexistent_doc")
	assert.Error(t, err)
}

func TestGetDocument_WrongUser(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := s.CreateDocument("usr_gu_001", "notes", map[string]interface{}{"id": "gutest_001", "text": "secret"})
	require.NoError(t, err)

	// Different user cannot access
	_, err = s.GetDocument("usr_gu_002", "notes", "gutest_001")
	assert.Error(t, err)
}

func TestUpdateDocument(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_upd_001"
	doc := map[string]interface{}{
		"id":    "upd_doc_001",
		"title": "Original Title",
		"count": 1,
	}

	_, err := s.CreateDocument(userID, "notes", doc)
	require.NoError(t, err)

	update := map[string]interface{}{
		"title": "Updated Title",
		"count": 99,
	}

	err = s.UpdateDocument(userID, "notes", "upd_doc_001", update)
	require.NoError(t, err)

	retrieved, err := s.GetDocument(userID, "notes", "upd_doc_001")
	require.NoError(t, err)
	assert.Equal(t, "Updated Title", retrieved["title"])
	assert.Equal(t, float64(99), retrieved["count"])
	// ID should be preserved (immutable)
	assert.Equal(t, "upd_doc_001", retrieved["id"])
}

func TestUpdateDocument_PreservesOwner(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_upd_002"
	_, err := s.CreateDocument(userID, "notes", map[string]interface{}{"id": "upd_owner_001", "text": "original"})
	require.NoError(t, err)

	// Try to change owner_id via update
	err = s.UpdateDocument(userID, "notes", "upd_owner_001", map[string]interface{}{"_owner_id": "attacker_id", "text": "modified"})
	require.NoError(t, err)

	retrieved, err := s.GetDocument(userID, "notes", "upd_owner_001")
	require.NoError(t, err)
	// Owner should remain original
	assert.Equal(t, userID, retrieved["_owner_id"])
}

func TestUpdateDocument_NotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	err := s.UpdateDocument("usr_upd_003", "notes", "nonexistent_doc", map[string]interface{}{"x": 1})
	assert.Error(t, err)
}

func TestDeleteDocument(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_del_001"
	_, err := s.CreateDocument(userID, "trash", map[string]interface{}{"id": "del_doc_001", "data": "to delete"})
	require.NoError(t, err)

	err = s.DeleteDocument(userID, "trash", "del_doc_001")
	require.NoError(t, err)

	_, err = s.GetDocument(userID, "trash", "del_doc_001")
	assert.Error(t, err)
}

func TestDeleteDocument_NotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	err := s.DeleteDocument("usr_del_002", "notes", "nonexistent_doc")
	assert.Error(t, err)
}

func TestDeleteDocument_UpdatesCounter(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_del_003"
	_, err := s.CreateDocument(userID, "counted", map[string]interface{}{"id": "cnt_001", "x": 1})
	require.NoError(t, err)
	_, err = s.CreateDocument(userID, "counted", map[string]interface{}{"id": "cnt_002", "x": 2})
	require.NoError(t, err)

	// Before delete: 2 docs in collection
	collections, err := s.ListAllCollections()
	require.NoError(t, err)
	var found *CollectionInfo
	for i := range collections {
		if collections[i].UserID == userID && collections[i].Collection == "counted" {
			found = &collections[i]
			break
		}
	}
	require.NotNil(t, found)
	assert.Equal(t, 2, found.DocCount)

	// Delete one
	require.NoError(t, s.DeleteDocument(userID, "counted", "cnt_001"))

	// After delete: 1 doc
	collections, err = s.ListAllCollections()
	require.NoError(t, err)
	found = nil
	for i := range collections {
		if collections[i].UserID == userID && collections[i].Collection == "counted" {
			found = &collections[i]
			break
		}
	}
	require.NotNil(t, found)
	assert.Equal(t, 1, found.DocCount)
}

func TestListAllCollections(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	// Start with empty
	cols, err := s.ListAllCollections()
	require.NoError(t, err)
	assert.Empty(t, cols)

	// Create docs in different collections for different users
	_, err = s.CreateDocument("usr_col_001", "sensors", map[string]interface{}{"data": "a"})
	require.NoError(t, err)
	_, err = s.CreateDocument("usr_col_001", "sensors", map[string]interface{}{"data": "b"})
	require.NoError(t, err)
	_, err = s.CreateDocument("usr_col_002", "logs", map[string]interface{}{"log": "entry"})
	require.NoError(t, err)

	cols, err = s.ListAllCollections()
	require.NoError(t, err)
	assert.GreaterOrEqual(t, len(cols), 2)

	// Find the sensors collection for user_col_001
	var sensorCol *CollectionInfo
	for i := range cols {
		if cols[i].UserID == "usr_col_001" && cols[i].Collection == "sensors" {
			sensorCol = &cols[i]
			break
		}
	}
	require.NotNil(t, sensorCol)
	assert.Equal(t, 2, sensorCol.DocCount)
}

// ============ CleanupPublicData Tests ============

func TestCleanupPublicData(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	// CleanupPublicData is currently a placeholder that returns 0, nil
	count, err := s.CleanupPublicData(7 * 24 * time.Hour)
	assert.NoError(t, err)
	assert.Equal(t, 0, count)
}
