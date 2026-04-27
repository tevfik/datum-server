package postgres

import (
	"database/sql"
	"datum-go/internal/storage"
	"encoding/json"
	"fmt"
	"time"

	"github.com/google/uuid"
)

// CreateDocument adds a new document to a collection
func (s *PostgresStore) CreateDocument(userID, collection string, doc map[string]interface{}) (string, error) {
	var docID string
	if id, ok := doc["_id"].(string); ok && id != "" {
		docID = id
	} else if id, ok := doc["id"].(string); ok && id != "" {
		docID = id
	} else {
		docID = uuid.New().String()
	}
	doc["_id"] = docID

	jsonData, err := json.Marshal(doc)
	if err != nil {
		return "", err
	}

	ctx, cancel := queryCtx()
	defer cancel()
	_, err = s.db.ExecContext(ctx, `
		INSERT INTO documents (id, user_id, collection, data, created_at, updated_at)
		VALUES ($1, $2, $3, $4, $5, $5)
	`, docID, userID, collection, jsonData, time.Now())

	if err != nil {
		return "", fmt.Errorf("failed to create document: %w", err)
	}
	return docID, nil
}

// ListDocuments retrieves all documents in a collection
func (s *PostgresStore) ListDocuments(userID, collection string) ([]map[string]interface{}, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	rows, err := s.db.QueryContext(ctx, `
		SELECT data FROM documents
		WHERE user_id = $1 AND collection = $2
		ORDER BY created_at DESC
	`, userID, collection)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var docs []map[string]interface{}
	for rows.Next() {
		var jsonData []byte
		if err := rows.Scan(&jsonData); err != nil {
			return nil, err
		}
		var doc map[string]interface{}
		if err := json.Unmarshal(jsonData, &doc); err != nil {
			continue
		}
		docs = append(docs, doc)
	}
	return docs, nil
}

// GetDocument retrieves a specific document
func (s *PostgresStore) GetDocument(userID, collection, docID string) (map[string]interface{}, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	var jsonData []byte
	err := s.db.QueryRowContext(ctx, `
		SELECT data FROM documents
		WHERE user_id = $1 AND collection = $2 AND id = $3
	`, userID, collection, docID).Scan(&jsonData)

	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("document not found")
		}
		return nil, err
	}

	var doc map[string]interface{}
	if err := json.Unmarshal(jsonData, &doc); err != nil {
		return nil, err
	}
	return doc, nil
}

// UpdateDocument updates an existing document
func (s *PostgresStore) UpdateDocument(userID, collection, docID string, doc map[string]interface{}) error {
	doc["_id"] = docID

	jsonData, err := json.Marshal(doc)
	if err != nil {
		return err
	}

	ctx, cancel := queryCtx()
	defer cancel()
	result, err := s.db.ExecContext(ctx, `
		UPDATE documents
		SET data = $1, updated_at = $2
		WHERE user_id = $3 AND collection = $4 AND id = $5
	`, jsonData, time.Now(), userID, collection, docID)

	if err != nil {
		return err
	}

	rows, err := result.RowsAffected()
	if err != nil {
		return err
	}
	if rows == 0 {
		return fmt.Errorf("document not found")
	}
	return nil
}

// DeleteDocument removes a document
func (s *PostgresStore) DeleteDocument(userID, collection, docID string) error {
	ctx, cancel := queryCtx()
	defer cancel()
	result, err := s.db.ExecContext(ctx, `
		DELETE FROM documents
		WHERE user_id = $1 AND collection = $2 AND id = $3
	`, userID, collection, docID)

	if err != nil {
		return err
	}

	rows, err := result.RowsAffected()
	if err != nil {
		return err
	}
	if rows == 0 {
		return fmt.Errorf("document not found")
	}
	return nil
}

// ListAllCollections returns validation counts for all collections by user
func (s *PostgresStore) ListAllCollections() ([]storage.CollectionInfo, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	rows, err := s.db.QueryContext(ctx, `
		SELECT user_id, collection, COUNT(*) as count
		FROM documents
		GROUP BY user_id, collection
	`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var collections []storage.CollectionInfo
	for rows.Next() {
		var info storage.CollectionInfo
		if err := rows.Scan(&info.UserID, &info.Collection, &info.DocCount); err != nil {
			return nil, err
		}
		collections = append(collections, info)
	}
	return collections, nil
}
