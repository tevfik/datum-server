package main

import (
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/rs/zerolog/log"
)

// StorageBucket represents a storage bucket info
type StorageBucket struct {
	Name         string    `json:"name"`
	Bucket       string    `json:"bucket"`
	ObjectsCount int       `json:"objects_count"`
	CreatedAt    time.Time `json:"created_at"`
}

// StorageObject represents an object/file within a bucket
type StorageObject struct {
	Name         string    `json:"name"`
	Key          string    `json:"key"`
	Size         int64     `json:"size"`
	LastModified time.Time `json:"last_modified"`
	ContentType  string    `json:"content_type,omitempty"`
}

// RegisterStorageRoutes registers storage and bucket browsing endpoints
func RegisterStorageRoutes(r *gin.Engine, baseDataDir string, authMiddleware gin.HandlerFunc) {
	storageRoot := filepath.Join(baseDataDir, "storage")
	// Ensure default buckets exist
	defaultBuckets := []string{"firmware", "snapshots", "recordings", "data"}
	for _, b := range defaultBuckets {
		_ = os.MkdirAll(filepath.Join(storageRoot, b), 0755)
	}

	// Link or copy any existing firmware directory if available
	firmwareDir := "./firmware"
	if info, err := os.Stat(firmwareDir); err == nil && info.IsDir() {
		// If firmware directory exists at root, ensure storage/firmware points or mirrors it
		// We can also include files from ./firmware when querying the 'firmware' bucket
	}

	handler := func(c *gin.Context) {
		// List buckets
		entries, err := os.ReadDir(storageRoot)
		if err != nil {
			log.Error().Err(err).Msg("Failed to read storage root directory")
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list buckets"})
			return
		}

		var buckets []StorageBucket
		for _, e := range entries {
			if e.IsDir() {
				bucketPath := filepath.Join(storageRoot, e.Name())
				fileEntries, _ := os.ReadDir(bucketPath)
				info, _ := e.Info()
				createdAt := time.Now()
				if info != nil {
					createdAt = info.ModTime()
				}
				buckets = append(buckets, StorageBucket{
					Name:         e.Name(),
					Bucket:       e.Name(),
					ObjectsCount: len(fileEntries),
					CreatedAt:    createdAt,
				})
			}
		}

		c.JSON(http.StatusOK, gin.H{
			"buckets": buckets,
			"count":   len(buckets),
		})
	}

	getBucketObjectsHandler := func(c *gin.Context) {
		bucketName := filepath.Clean(c.Param("bucket"))
		if strings.Contains(bucketName, "..") || strings.Contains(bucketName, "/") || strings.Contains(bucketName, "\\") {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid bucket name"})
			return
		}

		bucketPath := filepath.Join(storageRoot, bucketName)
		var objects []StorageObject

		// Read objects from storage/<bucket>
		if entries, err := os.ReadDir(bucketPath); err == nil {
			for _, e := range entries {
				if !e.IsDir() {
					info, err := e.Info()
					if err == nil {
						objects = append(objects, StorageObject{
							Name:         e.Name(),
							Key:          e.Name(),
							Size:         info.Size(),
							LastModified: info.ModTime(),
						})
					}
				}
			}
		}

		// If querying 'firmware' bucket, also check root ./firmware directory
		if bucketName == "firmware" {
			if fwEntries, err := os.ReadDir("./firmware"); err == nil {
				existingKeys := make(map[string]bool)
				for _, o := range objects {
					existingKeys[o.Key] = true
				}
				for _, e := range fwEntries {
					if !e.IsDir() && !existingKeys[e.Name()] {
						info, err := e.Info()
						if err == nil {
							objects = append(objects, StorageObject{
								Name:         e.Name(),
								Key:          e.Name(),
								Size:         info.Size(),
								LastModified: info.ModTime(),
							})
						}
					}
				}
			}
		}

		c.JSON(http.StatusOK, gin.H{
			"bucket":  bucketName,
			"objects": objects,
			"count":   len(objects),
		})
	}

	downloadObjectHandler := func(c *gin.Context) {
		bucketName := filepath.Clean(c.Param("bucket"))
		objectKey := filepath.Clean(c.Param("filepath"))
		if strings.Contains(bucketName, "..") || strings.Contains(objectKey, "..") {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid path"})
			return
		}

		filePath := filepath.Join(storageRoot, bucketName, objectKey)
		if _, err := os.Stat(filePath); os.IsNotExist(err) {
			if bucketName == "firmware" {
				fwPath := filepath.Join("./firmware", objectKey)
				if _, err2 := os.Stat(fwPath); err2 == nil {
					c.File(fwPath)
					return
				}
			}
			c.JSON(http.StatusNotFound, gin.H{"error": "Object not found"})
			return
		}

		c.File(filePath)
	}

	// Register routes on both /buckets and /storage for maximum compatibility
	for _, prefix := range []string{"/buckets", "/storage"} {
		grp := r.Group(prefix)
		if authMiddleware != nil {
			grp.Use(authMiddleware)
		}
		grp.GET("", handler)
		grp.GET("/:bucket", getBucketObjectsHandler)
		grp.GET("/:bucket/*filepath", downloadObjectHandler)
	}

	log.Info().Str("storage_root", storageRoot).Msg("Storage / Buckets routes registered")
}
