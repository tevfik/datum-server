package main

import (
	_ "embed"
	"net/http"
	"os"

	"github.com/gin-gonic/gin"
)

//go:embed openapi.yaml
var openAPISpec string

// Scalar API reference HTML
const swaggerUIHTML = `<!DOCTYPE html>
<html>
<head>
    <title>Datum API Documentation</title>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
</head>
<body>
    <script id="api-reference" data-url="/docs/openapi.yaml"></script>
    <script src="https://cdn.jsdelivr.net/npm/@scalar/api-reference"></script>
</body>
</html>`

// setupSwagger adds API documentation endpoints
func setupSwagger(r *gin.Engine) {
	var docs *gin.RouterGroup

	// Check if Basic Auth credentials are configured
	user := os.Getenv("DOCS_USER")
	pass := os.Getenv("DOCS_PASS")

	if user != "" && pass != "" {
		docs = r.Group("/", gin.BasicAuth(gin.Accounts{user: pass}))
	} else {
		docs = r.Group("/")
	}

	// Swagger UI
	docs.GET("/docs", func(c *gin.Context) {
		c.Data(http.StatusOK, "text/html; charset=utf-8", []byte(swaggerUIHTML))
	})

	// OpenAPI spec
	docs.GET("/docs/openapi.yaml", func(c *gin.Context) {
		c.Data(http.StatusOK, "application/x-yaml", []byte(openAPISpec))
	})

	// Redirect /swagger to /docs
	r.GET("/swagger", func(c *gin.Context) {
		c.Redirect(http.StatusMovedPermanently, "/docs")
	})
}
