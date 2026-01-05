package main

import (
	_ "embed"
	"net/http"
	"os"

	"github.com/gin-gonic/gin"
)

//go:embed openapi.yaml
var openAPISpec string

// Swagger UI HTML template
const swaggerUIHTML = `<!DOCTYPE html>
<html>
<head>
    <title>Datum API Documentation</title>
    <link rel="stylesheet" type="text/css" href="https://unpkg.com/swagger-ui-dist@5/swagger-ui.css">
    <style>
        body { margin: 0; padding: 0; }
        .swagger-ui .topbar { display: none; }
    </style>
</head>
<body>
    <div id="swagger-ui"></div>
    <script src="https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js"></script>
    <script>
        SwaggerUIBundle({
            url: '/docs/openapi.yaml',
            dom_id: '#swagger-ui',
            presets: [SwaggerUIBundle.presets.apis, SwaggerUIBundle.SwaggerUIStandalonePreset],
            layout: "BaseLayout",
            deepLinking: true,
            showExtensions: true,
            showCommonExtensions: true
        });
    </script>
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
