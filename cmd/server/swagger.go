package main

import (
	_ "embed"
	"net/http"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

//go:embed openapi.yaml
var openAPISpec string

// Scalar API reference HTML — token is forwarded as `?token=` so the
// embedded OpenAPI fetch survives the SPA's auth wall.
// Uses the modern Scalar.createApiReference() JS API (the old data-url
// attribute approach was deprecated and no longer works with latest CDN).
const swaggerUIHTML = `<!DOCTYPE html>
<html>
<head>
    <title>Datum API Documentation</title>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
</head>
<body>
    <div id="app"></div>
    <script src="https://cdn.jsdelivr.net/npm/@scalar/api-reference"></script>
    <script>
      // Propagate ?token=… to the spec fetch so the protected route accepts it.
      var qs = window.location.search;
      var specURL = '/docs/openapi.yaml' + (qs || '');
      Scalar.createApiReference('#app', {
        url: specURL,
        hideDownloadButton: false,
      });
    </script>
</body>
</html>`

// setupSwagger registers documentation endpoints behind authentication.
//
// Both `/docs` and `/docs/openapi.yaml` require a valid JWT or API key. The
// token may be provided either via `Authorization: Bearer …` header or via
// `?token=…` query parameter (so a SPA can deep-link from a logged-in
// session). Anonymous access is no longer permitted.
func setupSwagger(r *gin.Engine, store storage.Provider) {
	docs := r.Group("/")
	docs.Use(auth.UserAuthMiddleware(store))

	docs.GET("/docs", func(c *gin.Context) {
		c.Data(http.StatusOK, "text/html; charset=utf-8", []byte(swaggerUIHTML))
	})
	docs.GET("/docs/openapi.yaml", func(c *gin.Context) {
		c.Data(http.StatusOK, "application/x-yaml", []byte(openAPISpec))
	})

	// Legacy redirect — also auth-gated.
	r.GET("/swagger", auth.UserAuthMiddleware(store), func(c *gin.Context) {
		c.Redirect(http.StatusMovedPermanently, "/docs")
	})
}
