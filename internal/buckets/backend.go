// Package buckets provides Datum-server's object storage abstraction
// (the "Storage" half of a Supabase-style backend).
//
// A Backend persists opaque blobs keyed by `(bucket, path)`. Two backends
// ship in the default build:
//
//   - localfs: stores objects under a directory on the host filesystem.
//     This is the default and requires no external dependencies.
//   - s3:     placeholder for an S3 / MinIO-compatible remote backend.
//     Wired in a follow-up; selecting it today returns an error so
//     the misconfiguration is visible at startup.
//
// The HTTP layer in `internal/api/buckets` exposes a small REST surface
// on top of any Backend; CLI helpers in `cmd/datumctl/bucket.go` mirror it.
package buckets

import (
	"context"
	"errors"
	"io"
	"time"
)

// ErrNotFound is returned when an object or bucket does not exist.
var ErrNotFound = errors.New("buckets: not found")

// ErrNotImplemented is returned by backend stubs that exist for interface
// completeness but are not yet wired (e.g. S3 in this release).
var ErrNotImplemented = errors.New("buckets: backend not implemented")

// Object describes a stored blob without including its body.
type Object struct {
	Bucket       string    `json:"bucket"`
	Path         string    `json:"path"`
	Size         int64     `json:"size"`
	ContentType  string    `json:"content_type,omitempty"`
	ETag         string    `json:"etag,omitempty"`
	LastModified time.Time `json:"last_modified"`
	OwnerID      string    `json:"owner_id,omitempty"`
}

// PutOptions controls how a Put call writes its object.
type PutOptions struct {
	ContentType string
	OwnerID     string
	// Public marks the object as readable without authentication.
	Public bool
	// Metadata is an opaque user-defined key/value bag (best-effort
	// persistence; localfs ignores it for now).
	Metadata map[string]string
}

// PresignOptions controls Backend.Presign.
type PresignOptions struct {
	// Expires is how long the issued URL stays valid. 15 min default.
	Expires time.Duration
	// Method is "GET" (download) or "PUT" (upload). Defaults to GET.
	Method string
}

// Backend is the contract every object-storage driver implements.
type Backend interface {
	// Name identifies the backend in logs and config (e.g. "localfs", "s3").
	Name() string

	// Put writes body to (bucket, path), returning the resulting Object
	// (including final size & ETag).
	Put(ctx context.Context, bucket, path string, body io.Reader, opts PutOptions) (*Object, error)

	// Get returns a ReadCloser for the object body and its metadata.
	// Caller MUST close the body.
	Get(ctx context.Context, bucket, path string) (io.ReadCloser, *Object, error)

	// Stat returns metadata only (no body).
	Stat(ctx context.Context, bucket, path string) (*Object, error)

	// Delete removes a single object. Missing object is not an error.
	Delete(ctx context.Context, bucket, path string) error

	// List enumerates objects under a prefix (recursive). Pass an empty
	// prefix to list the entire bucket.
	List(ctx context.Context, bucket, prefix string, limit int) ([]Object, error)

	// Presign returns a URL the client can use directly to download/upload
	// without authenticating to datum-server. Backends that do not support
	// presigning return ErrNotImplemented.
	Presign(ctx context.Context, bucket, path string, opts PresignOptions) (string, error)

	// EnsureBucket creates the bucket if missing. Idempotent.
	EnsureBucket(ctx context.Context, bucket string) error

	// DeleteBucket removes an *empty* bucket. Returns an error if it still
	// contains objects.
	DeleteBucket(ctx context.Context, bucket string) error

	// ListBuckets returns the names of every bucket the backend knows about.
	ListBuckets(ctx context.Context) ([]string, error)
}
