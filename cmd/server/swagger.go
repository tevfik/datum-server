package main

import (
	_ "embed"
	"net/http"

	"github.com/gin-gonic/gin"
)

//go:embed openapi.yaml
var openAPISpec string

// Scalar UI HTML template
const swaggerUIHTML = `<!doctype html>
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
    <script
      id="api-reference"
      data-url="/docs/openapi.yaml"
      data-proxy-url="https://proxy.scalar.com"></script>
    <script src="https://cdn.jsdelivr.net/npm/@scalar/api-reference"></script>
  </body>
</html>`

// setupSwagger adds API documentation endpoints
func setupSwagger(r *gin.Engine) {
	// Swagger UI
	r.GET("/docs", func(c *gin.Context) {
		c.Data(http.StatusOK, "text/html; charset=utf-8", []byte(swaggerUIHTML))
	})

	// OpenAPI spec
	r.GET("/docs/openapi.yaml", func(c *gin.Context) {
		c.Data(http.StatusOK, "application/x-yaml", []byte(openAPISpec))
	})

	// Redirect /swagger to /docs
	r.GET("/swagger", func(c *gin.Context) {
		c.Redirect(http.StatusMovedPermanently, "/docs")
	})
}
