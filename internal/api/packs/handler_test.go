package packsapi

import (
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
)

// fakeGleann returns a tiny stub for the gleann packs API.
func fakeGleann() *httptest.Server {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/packs", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.Header().Set("ETag", `"abc123"`)
		_, _ = w.Write([]byte(`{"packs":[{"id":"crops-tr","version":"1.0.0"}],"count":1}`))
	})
	mux.HandleFunc("/api/packs/crops-tr", func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("If-None-Match") == `"manifest-etag"` {
			w.WriteHeader(http.StatusNotModified)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		w.Header().Set("ETag", `"manifest-etag"`)
		_, _ = w.Write([]byte(`{"id":"crops-tr","version":"1.0.0"}`))
	})
	mux.HandleFunc("/api/packs/crops-tr/data", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.Header().Set("ETag", `"data-etag"`)
		_, _ = w.Write([]byte(`{"items":[{"id":"tomato"}]}`))
	})
	mux.HandleFunc("/api/packs/crops-tr/items/tomato", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(`{"id":"tomato","name_tr":"Domates"}`))
	})
	mux.HandleFunc("/api/packs/crops-tr/search", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(`{"items":[],"count":0,"query":"` + r.URL.Query().Get("q") + `"}`))
	})
	return httptest.NewServer(mux)
}

func newTestRouter(t *testing.T) (*gin.Engine, func()) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	stub := fakeGleann()
	t.Setenv("GLEANN_URL", stub.URL)
	r := gin.New()
	g := r.Group("/")
	New().RegisterRoutes(g)
	return r, stub.Close
}

func do(t *testing.T, r *gin.Engine, method, path string, headers map[string]string) *httptest.ResponseRecorder {
	t.Helper()
	req := httptest.NewRequest(method, path, nil)
	for k, v := range headers {
		req.Header.Set(k, v)
	}
	rec := httptest.NewRecorder()
	r.ServeHTTP(rec, req)
	return rec
}

func TestList(t *testing.T) {
	r, stop := newTestRouter(t)
	defer stop()
	rec := do(t, r, "GET", "/packs", nil)
	if rec.Code != 200 || !strings.Contains(rec.Body.String(), "crops-tr") {
		t.Fatalf("body=%s code=%d", rec.Body.String(), rec.Code)
	}
}

func TestManifestAnd304(t *testing.T) {
	r, stop := newTestRouter(t)
	defer stop()
	rec := do(t, r, "GET", "/packs/crops-tr", nil)
	if rec.Code != 200 {
		t.Fatalf("code=%d", rec.Code)
	}
	etag := rec.Header().Get("ETag")
	if etag == "" {
		t.Fatal("missing etag")
	}
	rec = do(t, r, "GET", "/packs/crops-tr", map[string]string{"If-None-Match": etag})
	if rec.Code != http.StatusNotModified {
		t.Errorf("want 304, got %d", rec.Code)
	}
}

func TestData(t *testing.T) {
	r, stop := newTestRouter(t)
	defer stop()
	rec := do(t, r, "GET", "/packs/crops-tr/data", nil)
	if rec.Code != 200 || !strings.Contains(rec.Body.String(), "tomato") {
		t.Fatalf("body=%s code=%d", rec.Body.String(), rec.Code)
	}
}

func TestItem(t *testing.T) {
	r, stop := newTestRouter(t)
	defer stop()
	rec := do(t, r, "GET", "/packs/crops-tr/items/tomato", nil)
	if rec.Code != 200 || !strings.Contains(rec.Body.String(), "Domates") {
		t.Fatalf("body=%s", rec.Body.String())
	}
}

func TestSearchPropagatesQuery(t *testing.T) {
	r, stop := newTestRouter(t)
	defer stop()
	rec := do(t, r, "GET", "/packs/crops-tr/search?q=biber", nil)
	if rec.Code != 200 || !strings.Contains(rec.Body.String(), "biber") {
		t.Fatalf("body=%s", rec.Body.String())
	}
}

func TestUnreachableUpstream(t *testing.T) {
	gin.SetMode(gin.TestMode)
	t.Setenv("GLEANN_URL", "http://127.0.0.1:1") // closed port
	os.Setenv("GLEANN_URL", "http://127.0.0.1:1")
	r := gin.New()
	g := r.Group("/")
	New().RegisterRoutes(g)
	rec := do(t, r, "GET", "/packs", nil)
	if rec.Code != http.StatusBadGateway {
		t.Errorf("want 502, got %d", rec.Code)
	}
}
