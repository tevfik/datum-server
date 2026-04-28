package buckets

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"net/url"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"
)

// LocalFS persists objects under <BaseDir>/<bucket>/<path>.
//
// Object metadata (content-type, owner, etc.) is stored in a sidecar JSON
// file at <BaseDir>/.meta/<bucket>/<path>.json so the on-disk layout
// remains human-friendly.
type LocalFS struct {
	BaseDir string
	// PresignKey signs presigned URLs (HMAC-SHA256). Keep stable across
	// restarts so previously-issued URLs stay valid.
	PresignKey []byte
	// PublicURL is the externally visible base URL for presigned downloads
	// (e.g. "https://api.example.com"). When empty, presigned URLs are
	// returned as paths only.
	PublicURL string
}

// NewLocalFS creates the backing directory if it does not exist yet.
func NewLocalFS(baseDir, publicURL string, presignKey []byte) (*LocalFS, error) {
	if baseDir == "" {
		baseDir = "./data/buckets"
	}
	if err := os.MkdirAll(baseDir, 0o755); err != nil {
		return nil, fmt.Errorf("buckets: create base dir: %w", err)
	}
	if err := os.MkdirAll(filepath.Join(baseDir, ".meta"), 0o755); err != nil {
		return nil, fmt.Errorf("buckets: create meta dir: %w", err)
	}
	if len(presignKey) == 0 {
		// Stable fallback derived from base dir; users should configure a real
		// key in production for cross-restart URL stability.
		h := sha256.Sum256([]byte("datum-buckets:" + baseDir))
		presignKey = h[:]
	}
	return &LocalFS{BaseDir: baseDir, PresignKey: presignKey, PublicURL: publicURL}, nil
}

func (LocalFS) Name() string { return "localfs" }

// safe returns the on-disk path for a logical (bucket, path), refusing
// any segment that would escape the bucket directory.
func (l *LocalFS) safe(bucket, path string) (string, error) {
	if !validName(bucket) {
		return "", fmt.Errorf("buckets: invalid bucket name %q", bucket)
	}
	// Reject obvious traversal attempts before cleaning normalises them away.
	for _, seg := range strings.Split(strings.ReplaceAll(path, "\\", "/"), "/") {
		if seg == ".." {
			return "", fmt.Errorf("buckets: invalid path %q", path)
		}
	}
	cleaned := filepath.Clean("/" + path)
	if strings.Contains(cleaned, "..") {
		return "", fmt.Errorf("buckets: invalid path %q", path)
	}
	return filepath.Join(l.BaseDir, bucket, cleaned), nil
}

func (l *LocalFS) metaPath(bucket, path string) string {
	cleaned := filepath.Clean("/" + path)
	return filepath.Join(l.BaseDir, ".meta", bucket, cleaned+".json")
}

func (l *LocalFS) EnsureBucket(_ context.Context, bucket string) error {
	if !validName(bucket) {
		return fmt.Errorf("buckets: invalid bucket name %q", bucket)
	}
	if err := os.MkdirAll(filepath.Join(l.BaseDir, bucket), 0o755); err != nil {
		return err
	}
	return os.MkdirAll(filepath.Join(l.BaseDir, ".meta", bucket), 0o755)
}

func (l *LocalFS) DeleteBucket(_ context.Context, bucket string) error {
	dir := filepath.Join(l.BaseDir, bucket)
	entries, err := os.ReadDir(dir)
	if err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return err
	}
	if len(entries) > 0 {
		return errors.New("buckets: bucket is not empty")
	}
	if err := os.RemoveAll(dir); err != nil {
		return err
	}
	return os.RemoveAll(filepath.Join(l.BaseDir, ".meta", bucket))
}

func (l *LocalFS) ListBuckets(_ context.Context) ([]string, error) {
	entries, err := os.ReadDir(l.BaseDir)
	if err != nil {
		return nil, err
	}
	out := make([]string, 0, len(entries))
	for _, e := range entries {
		name := e.Name()
		if e.IsDir() && !strings.HasPrefix(name, ".") {
			out = append(out, name)
		}
	}
	sort.Strings(out)
	return out, nil
}

func (l *LocalFS) Put(_ context.Context, bucket, path string, body io.Reader, opts PutOptions) (*Object, error) {
	if err := l.EnsureBucket(nil, bucket); err != nil {
		return nil, err
	}
	full, err := l.safe(bucket, path)
	if err != nil {
		return nil, err
	}
	if err := os.MkdirAll(filepath.Dir(full), 0o755); err != nil {
		return nil, err
	}
	tmp := full + ".tmp"
	f, err := os.OpenFile(tmp, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0o644)
	if err != nil {
		return nil, err
	}
	hash := sha256.New()
	mw := io.MultiWriter(f, hash)
	n, err := io.Copy(mw, body)
	if cerr := f.Close(); err == nil {
		err = cerr
	}
	if err != nil {
		os.Remove(tmp)
		return nil, err
	}
	if err := os.Rename(tmp, full); err != nil {
		return nil, err
	}
	obj := &Object{
		Bucket:       bucket,
		Path:         path,
		Size:         n,
		ContentType:  opts.ContentType,
		ETag:         hex.EncodeToString(hash.Sum(nil)[:16]),
		LastModified: time.Now(),
		OwnerID:      opts.OwnerID,
	}
	if err := l.writeMeta(bucket, path, obj); err != nil {
		return nil, err
	}
	return obj, nil
}

func (l *LocalFS) Get(_ context.Context, bucket, path string) (io.ReadCloser, *Object, error) {
	full, err := l.safe(bucket, path)
	if err != nil {
		return nil, nil, err
	}
	f, err := os.Open(full)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil, ErrNotFound
		}
		return nil, nil, err
	}
	obj, err := l.readMeta(bucket, path)
	if err != nil {
		f.Close()
		return nil, nil, err
	}
	return f, obj, nil
}

func (l *LocalFS) Stat(_ context.Context, bucket, path string) (*Object, error) {
	full, err := l.safe(bucket, path)
	if err != nil {
		return nil, err
	}
	if _, err := os.Stat(full); err != nil {
		if os.IsNotExist(err) {
			return nil, ErrNotFound
		}
		return nil, err
	}
	return l.readMeta(bucket, path)
}

func (l *LocalFS) Delete(_ context.Context, bucket, path string) error {
	full, err := l.safe(bucket, path)
	if err != nil {
		return err
	}
	if err := os.Remove(full); err != nil && !os.IsNotExist(err) {
		return err
	}
	if err := os.Remove(l.metaPath(bucket, path)); err != nil && !os.IsNotExist(err) {
		return err
	}
	return nil
}

func (l *LocalFS) List(_ context.Context, bucket, prefix string, limit int) ([]Object, error) {
	if !validName(bucket) {
		return nil, fmt.Errorf("buckets: invalid bucket name %q", bucket)
	}
	root := filepath.Join(l.BaseDir, bucket)
	out := make([]Object, 0, 32)
	err := filepath.Walk(root, func(p string, info os.FileInfo, err error) error {
		if err != nil {
			if os.IsNotExist(err) {
				return nil
			}
			return err
		}
		if info.IsDir() || strings.HasSuffix(p, ".tmp") {
			return nil
		}
		rel, err := filepath.Rel(root, p)
		if err != nil {
			return err
		}
		rel = filepath.ToSlash(rel)
		if prefix != "" && !strings.HasPrefix(rel, strings.TrimPrefix(prefix, "/")) {
			return nil
		}
		obj, err := l.readMeta(bucket, rel)
		if err != nil {
			obj = &Object{
				Bucket:       bucket,
				Path:         rel,
				Size:         info.Size(),
				LastModified: info.ModTime(),
			}
		}
		out = append(out, *obj)
		if limit > 0 && len(out) >= limit {
			return io.EOF
		}
		return nil
	})
	if err == io.EOF {
		err = nil
	}
	return out, err
}

// Presign returns a relative URL signed with the LocalFS HMAC key.
//
// The URL form is `<PublicURL>/storage/{bucket}/{path}?expires=<unix>&sig=<hex>`.
// Verification is done by VerifyPresignedURL — used by the HTTP layer when
// serving downloads without re-authenticating the user.
func (l *LocalFS) Presign(_ context.Context, bucket, path string, opts PresignOptions) (string, error) {
	method := strings.ToUpper(opts.Method)
	if method == "" {
		method = "GET"
	}
	if method != "GET" && method != "PUT" {
		return "", fmt.Errorf("buckets: unsupported presign method %q", method)
	}
	expires := opts.Expires
	if expires <= 0 {
		expires = 15 * time.Minute
	}
	exp := time.Now().Add(expires).Unix()
	canonical := fmt.Sprintf("%s\n%s\n%s\n%d", method, bucket, path, exp)
	mac := hmac.New(sha256.New, l.PresignKey)
	mac.Write([]byte(canonical))
	sig := hex.EncodeToString(mac.Sum(nil))
	q := url.Values{}
	q.Set("expires", strconv.FormatInt(exp, 10))
	q.Set("sig", sig)
	if method != "GET" {
		q.Set("method", method)
	}
	rel := "/storage/" + bucket + "/" + strings.TrimPrefix(path, "/") + "?" + q.Encode()
	if l.PublicURL != "" {
		return strings.TrimRight(l.PublicURL, "/") + rel, nil
	}
	return rel, nil
}

// VerifyPresignedURL checks signature + expiry and returns true if valid.
func (l *LocalFS) VerifyPresignedURL(method, bucket, path, expiresStr, sig string) bool {
	if method == "" {
		method = "GET"
	}
	method = strings.ToUpper(method)
	exp, err := strconv.ParseInt(expiresStr, 10, 64)
	if err != nil || time.Now().Unix() > exp {
		return false
	}
	canonical := fmt.Sprintf("%s\n%s\n%s\n%d", method, bucket, path, exp)
	mac := hmac.New(sha256.New, l.PresignKey)
	mac.Write([]byte(canonical))
	want := hex.EncodeToString(mac.Sum(nil))
	return hmac.Equal([]byte(want), []byte(sig))
}

// validName guards against directory traversal and accidental control chars
// in a bucket / segment name.
func validName(s string) bool {
	if s == "" || len(s) > 128 || strings.ContainsAny(s, "/\\..\x00") {
		return false
	}
	for _, r := range s {
		switch {
		case r >= 'a' && r <= 'z',
			r >= 'A' && r <= 'Z',
			r >= '0' && r <= '9',
			r == '-' || r == '_':
		default:
			return false
		}
	}
	return true
}
