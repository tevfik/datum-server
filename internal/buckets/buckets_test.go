package buckets

import (
	"bytes"
	"context"
	"io"
	"strings"
	"testing"
	"time"
)

func newTestFS(t *testing.T) *LocalFS {
	t.Helper()
	dir := t.TempDir()
	fs, err := NewLocalFS(dir, "https://example.com", []byte("test-key"))
	if err != nil {
		t.Fatal(err)
	}
	return fs
}

func TestLocalFSPutGetStatDelete(t *testing.T) {
	fs := newTestFS(t)
	ctx := context.Background()

	obj, err := fs.Put(ctx, "avatars", "u/1.png",
		bytes.NewBufferString("hello"),
		PutOptions{ContentType: "image/png", OwnerID: "usr_1"})
	if err != nil {
		t.Fatal(err)
	}
	if obj.Size != 5 || obj.ETag == "" {
		t.Fatalf("unexpected obj: %+v", obj)
	}

	rc, meta, err := fs.Get(ctx, "avatars", "u/1.png")
	if err != nil {
		t.Fatal(err)
	}
	defer rc.Close()
	body, _ := io.ReadAll(rc)
	if string(body) != "hello" {
		t.Fatalf("body=%q", string(body))
	}
	if meta.ContentType != "image/png" || meta.OwnerID != "usr_1" {
		t.Fatalf("meta drifted: %+v", meta)
	}

	stat, err := fs.Stat(ctx, "avatars", "u/1.png")
	if err != nil || stat.Size != 5 {
		t.Fatalf("stat err=%v obj=%+v", err, stat)
	}

	if err := fs.Delete(ctx, "avatars", "u/1.png"); err != nil {
		t.Fatal(err)
	}
	if _, err := fs.Stat(ctx, "avatars", "u/1.png"); err == nil {
		t.Fatal("expected not-found after delete")
	}
}

func TestLocalFSListAndBuckets(t *testing.T) {
	fs := newTestFS(t)
	ctx := context.Background()

	for i, p := range []string{"a/1.txt", "a/2.txt", "b/1.txt"} {
		_, err := fs.Put(ctx, "files", p, bytes.NewReader([]byte{byte(i)}), PutOptions{})
		if err != nil {
			t.Fatal(err)
		}
	}
	objs, err := fs.List(ctx, "files", "a/", 0)
	if err != nil {
		t.Fatal(err)
	}
	if len(objs) != 2 {
		t.Fatalf("expected 2 under a/, got %d (%+v)", len(objs), objs)
	}

	buckets, err := fs.ListBuckets(ctx)
	if err != nil || len(buckets) != 1 || buckets[0] != "files" {
		t.Fatalf("ListBuckets => %v err=%v", buckets, err)
	}
}

func TestLocalFSPresign(t *testing.T) {
	fs := newTestFS(t)
	ctx := context.Background()
	_, err := fs.Put(ctx, "docs", "secret.txt", strings.NewReader("hi"), PutOptions{})
	if err != nil {
		t.Fatal(err)
	}
	urlStr, err := fs.Presign(ctx, "docs", "secret.txt", PresignOptions{Expires: time.Minute})
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(urlStr, "expires=") || !strings.Contains(urlStr, "sig=") {
		t.Fatalf("missing expected query params: %s", urlStr)
	}

	// Re-derive params and verify
	q := urlStr[strings.Index(urlStr, "?")+1:]
	params := map[string]string{}
	for _, kv := range strings.Split(q, "&") {
		i := strings.Index(kv, "=")
		params[kv[:i]] = kv[i+1:]
	}
	if !fs.VerifyPresignedURL("GET", "docs", "secret.txt", params["expires"], params["sig"]) {
		t.Fatalf("VerifyPresignedURL returned false for valid URL: %s", urlStr)
	}
	if fs.VerifyPresignedURL("GET", "docs", "secret.txt", params["expires"], "deadbeef") {
		t.Fatalf("VerifyPresignedURL accepted bad signature")
	}
}

func TestLocalFSRejectsTraversal(t *testing.T) {
	fs := newTestFS(t)
	ctx := context.Background()
	_, err := fs.Put(ctx, "docs", "../escape", strings.NewReader("x"), PutOptions{})
	if err == nil {
		t.Fatal("expected traversal to be rejected")
	}
	_, err = fs.Put(ctx, "../bad", "file", strings.NewReader("x"), PutOptions{})
	if err == nil {
		t.Fatal("expected invalid bucket to be rejected")
	}
}

func TestS3StubReturnsNotImplemented(t *testing.T) {
	s, err := NewS3Backend("https://minio.example.com", "us-east-1", "k", "s", "")
	if err != nil {
		t.Fatal(err)
	}
	if _, _, err := s.Get(context.Background(), "x", "y"); err != ErrNotImplemented {
		t.Fatalf("expected ErrNotImplemented, got %v", err)
	}
	if _, err := NewS3Backend("", "", "", "", ""); err == nil {
		t.Fatal("expected missing endpoint error")
	}
}
