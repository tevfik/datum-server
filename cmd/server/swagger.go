package main

import (
	"embed"
	"io/fs"
	"net/http"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

//go:embed openapi.yaml
var openAPISpec string

//go:embed swagger-ui/*
var swaggerUIFS embed.FS

const swaggerUITemplate = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Datum API Documentation</title>
  <link rel="stylesheet" href="/docs/assets/swagger-ui.css" />
  <style>
    body { margin: 0; padding: 0; }
  </style>
</head>
<body>
  <div id="swagger-ui"></div>
  <script src="/docs/assets/swagger-ui-bundle.js"></script>
  <script src="/docs/assets/swagger-ui-standalone-preset.js"></script>
  <script>
    window.onload = () => {
      // Fetch openapi.yaml and forward the token query parameter if present.
      const specUrl = '/docs/openapi.yaml' + window.location.search;
      window.ui = SwaggerUIBundle({
        url: specUrl,
        dom_id: '#swagger-ui',
        deepLinking: true,
        presets: [
          SwaggerUIBundle.presets.apis,
          SwaggerUIStandalonePreset
        ],
        layout: "BaseLayout",
      });
    };
  </script>
</body>
</html>`

// setupSwagger registers documentation endpoints behind authentication.
func setupSwagger(r *gin.Engine, store storage.Provider) {
	// Serve static assets without authentication so browser can load them
	if subFS, err := fs.Sub(swaggerUIFS, "swagger-ui"); err == nil {
		r.StaticFS("/docs/assets", http.FS(subFS))
	}

	docs := r.Group("/")
	docs.Use(auth.UserAuthMiddleware(store))

	docs.GET("/docs", func(c *gin.Context) {
		c.Data(http.StatusOK, "text/html; charset=utf-8", []byte(swaggerUITemplate))
	})

	docs.GET("/docs/openapi.yaml", func(c *gin.Context) {
		c.Data(http.StatusOK, "application/yaml", []byte(openAPISpec))
	})

	// Legacy redirect
	r.GET("/swagger", auth.UserAuthMiddleware(store), func(c *gin.Context) {
		c.Redirect(http.StatusMovedPermanently, "/docs")
	})
}

