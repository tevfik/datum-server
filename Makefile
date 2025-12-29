.PHONY: help build run stop clean test bench dev logs shell db-backup db-restore release

# Variables
GIT_VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo "1.0.0-dev")
VERSION ?= $(GIT_VERSION)
BUILD_DATE ?= $(shell date -u +"%Y-%m-%dT%H:%M:%SZ")
DEFAULT_SERVER_URL ?= http://localhost:8000
COMPOSE=docker compose -f docker/docker-compose.yml
COMPOSE_DEV=docker compose -f docker/docker-compose.yml -f docker/docker-compose.dev.yml
SERVER_BINARY=build/binaries/server
CLI_BINARY=build/binaries/datumctl

help: ## Show this help message
	@echo "Datum IoT Platform - Makefile Commands"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

# Docker commands
build: ## Build all Docker images
	docker compose -f docker/docker-compose.yml build

release: ## Build release Docker image (usage: make release VERSION=1.2.3 DEFAULT_SERVER_URL=http://api.example.com)
	docker build \
		--build-arg VERSION=$(VERSION) \
		--build-arg BUILD_DATE=$(BUILD_DATE) \
		--build-arg DEFAULT_SERVER_URL=$(DEFAULT_SERVER_URL) \
		-t datum-server:$(VERSION) \
		-t datum-server:latest \
		-f docker/Dockerfile .

run: ## Start all services
	$(COMPOSE) up -d
	@echo "✅ Services started"

stop: ## Stop all services
	$(COMPOSE) down

restart: ## Restart all services
	$(COMPOSE) restart

logs: ## Show logs (use SERVICE=datum-server to filter)
	@if [ -z "$(SERVICE)" ]; then \
		$(COMPOSE) logs -f; \
	else \
		$(COMPOSE) logs -f $(SERVICE); \
	fi

ps: ## Show running containers
	$(COMPOSE) ps

# Development commands
dev: ## Start services in development mode with hot reload
	$(COMPOSE_DEV) up -d
	@echo "🔧 Development mode active"

dev-logs: ## Show development logs
	$(COMPOSE_DEV) logs -f

# Database commands
db-backup: ## Backup database and time-series data
	@echo "📦 Creating backup..."
	@mkdir -p backups
	@tar -czf backups/backup-$$(date +%Y%m%d-%H%M%S).tar.gz data/
	@echo "✅ Backup created in backups/"

db-restore: ## Restore database from backup (use BACKUP=filename)
	@if [ -z "$(BACKUP)" ]; then \
		echo "❌ Error: Please specify BACKUP=filename"; \
		echo "Available backups:"; \
		ls -1 backups/; \
		exit 1; \
	fi
	@echo "🔄 Restoring from $(BACKUP)..."
	@$(COMPOSE) down
	@rm -rf data/
	@tar -xzf backups/$(BACKUP)
	@echo "✅ Restore completed"

db-clean: ## Clean all data (WARNING: destroys all data)
	@echo "⚠️  This will delete all data!"
	@read -p "Are you sure? [y/N] " -n 1 -r; \
	echo; \
	if [[ $$REPLY =~ ^[Yy]$$ ]]; then \
		$(COMPOSE) down -v; \
		rm -rf data/; \
		echo "✅ Data cleaned"; \
	fi

# Testing commands
test: ## Run all Go tests
	@echo "🧪 Running Go tests..."
	@mkdir -p build
	@go test ./... -v

test-coverage: ## Run tests with coverage
	@echo "🧪 Running tests with coverage..."
	@mkdir -p build
	@go test ./... -coverprofile=build/coverage.out
	@go tool cover -html=build/coverage.out -o build/coverage.html
	@echo "✅ Coverage report: build/coverage.html"

test-storage: ## Run storage tests with verbose output
	@echo "🧪 Running storage tests..."
	@go test ./internal/storage/... -v

test-auth: ## Run auth tests
	@echo "🧪 Running auth tests..."
	@go test ./internal/auth/... -v

bench: ## Run Go benchmarks
	@echo "⚡ Running Go benchmarks..."
	@go test ./... -bench=. -benchmem

test-load: ## Run HTTP load tests with Locust
	@echo "⚡ Running HTTP load tests..."
	@bash tests/run_load_test.sh

# Build commands
build-server: ## Build Go server binary locally
	@echo "🔨 Building Go server..."
	@mkdir -p build/binaries
	@go build -o $(SERVER_BINARY) ./cmd/server
	@echo "✅ Binary created: $(SERVER_BINARY)"

build-cli: ## Build datumctl CLI tool
	@echo "🔨 Building datumctl..."
	@mkdir -p build/binaries
	@go build -ldflags "-X main.Version=$(VERSION) -X main.DefaultServerURL=$(DEFAULT_SERVER_URL)" -o $(CLI_BINARY) ./cmd/datumctl
	@echo "✅ CLI tool created: $(CLI_BINARY)"

build-all: build-server build-cli ## Build server and CLI

run-server: build-server ## Build and run server locally
	@echo "🚀 Starting server..."
	@./$(SERVER_BINARY)



# Code quality
fmt: ## Format Go code
	@echo "🔧 Formatting Go code..."
	@go fmt ./...

lint: ## Lint Go code
	@echo "🔍 Linting Go code..."
	@go vet ./...

fmt-python: ## Format Python test scripts
	@echo "🔧 Formatting Python code..."
	@cd tests && black *.py 2>/dev/null || echo "⚠️  Black not installed"

# Utility commands
shell-server: ## Open shell in server container
	$(COMPOSE) exec datum-server sh

clean: ## Clean build artifacts and containers
	$(COMPOSE) down -v
	rm -rf build/
	rm -f datumctl server # Remove binaries if built in root
	find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	@echo "✅ Cleaned"

install-tools: ## Install development tools
	@echo "📦 Installing development tools..."
	@go install golang.org/x/tools/cmd/goimports@latest
	@pip3 install locust black 2>/dev/null || echo "⚠️  pip3 not found (required for load tests)"

# Health checks
health: ## Check service health (Development)
	@echo "🏥 Checking service health..."
	@curl -s http://localhost:8000/health | python3 -m json.tool || echo "❌ Server not responding (ensure you are running in dev mode or port 8000 is exposed)"

# Documentation
docs: ## Open API documentation in browser (Development)
	@echo "📖 Opening API docs..."
	@xdg-open http://localhost:8000/docs 2>/dev/null || open http://localhost:8000/docs 2>/dev/null || echo "Open http://localhost:8000/docs in your browser"

.DEFAULT_GOAL := help
