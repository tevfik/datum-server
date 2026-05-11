package main

import (
	_ "embed"
	"encoding/json"
	"net/http"
	"strings"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

//go:embed openapi.yaml
var openAPISpec string

const swaggerUITemplate = `<!DOCTYPE html>
<html>
<head>
    <title>Datum API Documentation</title>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style>
      body { margin: 0; }
    </style>
</head>
<body>
    <div id="app"></div>
    <script src="https://cdn.jsdelivr.net/npm/@scalar/api-reference"></script>
    <script>
      (function() {
        const specContent = {{SPEC_CONTENT}};
        Scalar.createApiReference('#app', {
          spec: {
            content: specContent,
          },
          hideDownloadButton: false,
        });
      })();
    </script>
</body>
</html>`

// setupSwagger registers documentation endpoints behind authentication.
func setupSwagger(r *gin.Engine, store storage.Provider) {
	// Pre-generate the HTML with the spec embedded
	specJSON, _ := json.Marshal(openAPISpec)
	finalHTML := strings.Replace(swaggerUITemplate, "{{SPEC_CONTENT}}", string(specJSON), 1)

	docs := r.Group("/")
	docs.Use(auth.UserAuthMiddleware(store))

	docs.GET("/docs", func(c *gin.Context) {
		c.Data(http.StatusOK, "text/html; charset=utf-8", []byte(finalHTML))
	})

	docs.GET("/docs/openapi.yaml", func(c *gin.Context) {
		c.Data(http.StatusOK, "application/yaml", []byte(openAPISpec))
	})

	// Legacy redirect
	r.GET("/swagger", auth.UserAuthMiddleware(store), func(c *gin.Context) {
		c.Redirect(http.StatusMovedPermanently, "/docs")
	})
}
