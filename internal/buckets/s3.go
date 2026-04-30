package buckets

import (
	"context"
	"errors"
	"io"
)

// S3Backend is a placeholder for an S3 / MinIO-compatible remote backend.
//
// The interface is implemented so that selecting `buckets.backend = "s3"`
// at startup fails fast with a clear error message rather than silently
// degrading. Wiring the actual SDK (minio-go or aws-sdk-go-v2) is
// scheduled for a follow-up release; the surrounding REST API, CLI, and
// policy hooks are agnostic to which backend is active.
type S3Backend struct {
	Endpoint     string
	Region       string
	AccessKey    string
	SecretKey    string
	BucketPrefix string
}

// NewS3Backend returns a stub that always returns ErrNotImplemented. We
// still validate the most common config mistake (no endpoint) up front.
func NewS3Backend(endpoint, region, accessKey, secretKey, prefix string) (*S3Backend, error) {
	if endpoint == "" {
		return nil, errors.New("buckets/s3: BUCKETS_S3_ENDPOINT is required when backend=s3")
	}
	return &S3Backend{
		Endpoint:     endpoint,
		Region:       region,
		AccessKey:    accessKey,
		SecretKey:    secretKey,
		BucketPrefix: prefix,
	}, nil
}

func (S3Backend) Name() string { return "s3" }

func (s *S3Backend) Put(context.Context, string, string, io.Reader, PutOptions) (*Object, error) {
	return nil, ErrNotImplemented
}
func (s *S3Backend) Get(context.Context, string, string) (io.ReadCloser, *Object, error) {
	return nil, nil, ErrNotImplemented
}
func (s *S3Backend) Stat(context.Context, string, string) (*Object, error) {
	return nil, ErrNotImplemented
}
func (s *S3Backend) Delete(context.Context, string, string) error { return ErrNotImplemented }
func (s *S3Backend) List(context.Context, string, string, int) ([]Object, error) {
	return nil, ErrNotImplemented
}
func (s *S3Backend) Presign(context.Context, string, string, PresignOptions) (string, error) {
	return "", ErrNotImplemented
}
func (s *S3Backend) EnsureBucket(context.Context, string) error  { return ErrNotImplemented }
func (s *S3Backend) DeleteBucket(context.Context, string) error  { return ErrNotImplemented }
func (s *S3Backend) ListBuckets(context.Context) ([]string, error) { return nil, ErrNotImplemented }
