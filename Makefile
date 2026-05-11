# ==============================================================================
# Datum IoT Platform — Makefile
# ==============================================================================
.DEFAULT_GOAL := help

# Load docker/.env for local overrides (SKIP_WEB, etc.)
-include docker/.env
export

# ── Variables ─────────────────────────────────────────────────────────────────
GIT_VERSION      := $(shell git describe --tags --always --dirty 2>/dev/null || echo "1.0.0-dev")
VERSION          ?= $(GIT_VERSION)
BUILD_DATE       ?= $(shell date -u +"%Y-%m-%dT%H:%M:%SZ")
DEFAULT_SERVER_URL ?= http://localhost:8000
SKIP_WEB         ?= 0

# Export so docker compose's `${VERSION}` / `${BUILD_DATE}` interpolation in
# docker-compose*.yml picks up the git-derived values automatically — even
# when the user runs `docker compose up --build` directly without going
# through `make deploy`.
export VERSION
export BUILD_DATE
export DEFAULT_SERVER_URL

COMPOSE          = docker compose --env-file docker/.env -f docker/docker-compose.yml
COMPOSE_DEV      = $(COMPOSE) -f docker/docker-compose.dev.yml
COMPOSE_EXT      = docker compose --env-file docker/.env -f docker/docker-compose.external.yml
SERVER_BINARY    = build/binaries/server
CLI_BINARY       = build/binaries/datumctl

# ── Help ──────────────────────────────────────────────────────────────────────
help: ## Show this help message
	@echo "Datum IoT Platform — Makefile"
	@echo ""
	@grep -hE '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

# ── Build ─────────────────────────────────────────────────────────────────────
build: build-all ## Build all binaries (Server + CLI + Web)

build-web: ## Build Web Dashboard (npm or Docker fallback)
	@echo "🎨 Building Web Dashboard..."
	@if command -v npm >/dev/null 2>&1; then \
		cd web && npm install && npm run build; \
	else \
		docker run --rm -u $$(id -u):$$(id -g) -v $$(pwd):/app -w /app/web node:20-alpine sh -c "npm ci && npm run build"; \
	fi
	@[ -f web/dist/index.html ] || { echo "❌ web/dist/index.html missing!"; exit 1; }
	@echo "✅ Web Dashboard built"

prepare-assets: ## Prepare web assets (skip with SKIP_WEB=1)
	@if [ "$(SKIP_WEB)" = "1" ]; then \
		echo "⚠️  SKIP_WEB=1 — API-only placeholder"; \
		rm -rf cmd/server/dist && mkdir -p cmd/server/dist; \
		echo '<!DOCTYPE html><html><body><h1>API-Only Mode</h1></body></html>' > cmd/server/dist/index.html; \
	else \
		$(MAKE) build-web; \
		rm -rf cmd/server/dist && cp -r web/dist cmd/server/dist; \
	fi

build-server: prepare-assets ## Build Go server binary
	@mkdir -p build/binaries
	@go build -ldflags "-X main.Version=$(VERSION) -X main.BuildDate=$(BUILD_DATE)" \
		-o $(SERVER_BINARY) ./cmd/server
	@echo "✅ Server: $(SERVER_BINARY) ($(VERSION))"

build-cli: ## Build datumctl CLI tool
	@mkdir -p build/binaries
	@go build -ldflags "-X main.Version=$(VERSION) -X main.DefaultServerURL=$(DEFAULT_SERVER_URL)" \
		-o $(CLI_BINARY) ./cmd/datumctl
	@echo "✅ CLI: $(CLI_BINARY)"

build-api-only: ## Build server without web UI
	@SKIP_WEB=1 $(MAKE) build-server

build-all: build-server build-cli ## Build server + CLI

build-linux: prepare-assets ## Cross-compile Linux AMD64
	@mkdir -p build/release
	@GOOS=linux GOARCH=amd64 go build -ldflags "-X main.Version=$(VERSION)" -o build/release/datum-server-linux-amd64 ./cmd/server
	@GOOS=linux GOARCH=amd64 go build -ldflags "-X main.Version=$(VERSION)" -o build/release/datumctl-linux-amd64 ./cmd/datumctl
	@echo "✅ Linux binaries: build/release/"

build-release: build-linux ## Build release binaries

release: ## Build release Docker image (make release VERSION=x.y.z)
	docker build \
		--build-arg VERSION=$(VERSION) \
		--build-arg BUILD_DATE=$(BUILD_DATE) \
		--build-arg DEFAULT_SERVER_URL=$(DEFAULT_SERVER_URL) \
		-t datum-server:$(VERSION) -t datum-server:latest \
		-f docker/Dockerfile .

# ── Docker (local full-stack) ────────────────────────────────────────────────
run: ## Start all services (Traefik + Postgres + Server)
	$(COMPOSE) up -d
	@echo "✅ Services started"

stop: ## Stop all services
	$(COMPOSE) down

restart: ## Restart all services
	$(COMPOSE) restart

logs: ## Show logs (SERVICE=datum-server to filter)
	@if [ -z "$(SERVICE)" ]; then $(COMPOSE) logs -f; else $(COMPOSE) logs -f $(SERVICE); fi

ps: ## Show running containers
	$(COMPOSE) ps

shell-server: ## Shell into server container
	$(COMPOSE) exec datum-server sh

# ── Deploy (external Traefik + existing postgres) ─────────────────────────────
# Run this on the server directly (ssh bezgin → cd /opt/docker/iot/datum-server).
# Uses docker-compose.external.yml which has only the datum-server service —
# postgres is already managed by the outer iot stack, so it won't be touched.
# DATABASE_URL must be set in docker/.env (pointing to iot-postgres).
deploy: ## Build and (re)start datum-server — run on server, postgres not touched
	@echo "🚀 Deploying $(VERSION) ($(BUILD_DATE))..."
	$(COMPOSE_EXT) build \
		--build-arg VERSION="$(VERSION)" \
		--build-arg BUILD_DATE="$(BUILD_DATE)"
	$(COMPOSE_EXT) up -d --force-recreate
	@echo "✅ Deployed"

deploy-logs: ## Tail datum-server logs
	$(COMPOSE_EXT) logs -f

deploy-stop: ## Stop datum-server (leaves postgres running)
	$(COMPOSE_EXT) stop

deploy-ps: ## Show deployment status
	$(COMPOSE_EXT) ps

deploy-health: ## Check health endpoint
	@curl -sf https://datum.bezg.in/health | python3 -m json.tool

# ── Development ───────────────────────────────────────────────────────────────
dev: ## Start in dev mode with hot reload
	$(COMPOSE_DEV) up -d
	@echo "🔧 Dev mode active"

dev-logs: ## Show dev logs
	$(COMPOSE_DEV) logs -f

run-server: build-server ## Build and run server locally (no Docker)
	@./$(SERVER_BINARY)

# ── Test ──────────────────────────────────────────────────────────────────────
test: ## Run all Go tests
	@go test ./... -v

test-coverage: ## Run tests with coverage report
	@mkdir -p build
	@go test ./... -coverprofile=build/coverage.out
	@go tool cover -html=build/coverage.out -o build/coverage.html
	@echo "✅ Coverage: build/coverage.html"

coverage-check: ## Fail if coverage < MIN_COVERAGE (default 30%)
	@MIN_COVERAGE=$${MIN_COVERAGE:-30}; \
	mkdir -p build; \
	go test ./... -coverprofile=build/coverage.out > /dev/null; \
	pct=$$(go tool cover -func=build/coverage.out | tail -1 | awk '{print $$3}' | tr -d '%'); \
	echo "📊 Coverage: $$pct% (min: $${MIN_COVERAGE}%)"; \
	awk -v p=$$pct -v t=$$MIN_COVERAGE 'BEGIN{ exit (p+0 < t+0) }'

test-storage: ## Run storage tests
	@go test ./internal/storage/... -v

test-auth: ## Run auth tests
	@go test ./internal/auth/... -v

test-integration: build-all ## Run integration tests
	@SERVER_BINARY=$(SERVER_BINARY) CLI_BINARY=$(CLI_BINARY) bash tests/integration_test.sh

bench: ## Run Go benchmarks
	@go test ./... -bench=. -benchmem

test-load: ## Run HTTP load tests (Locust)
	@bash tests/run_load_test.sh

# ── Database ──────────────────────────────────────────────────────────────────
db-backup: ## Backup data/ directory
	@mkdir -p backups
	@tar -czf backups/backup-$$(date +%Y%m%d-%H%M%S).tar.gz data/
	@echo "✅ Backup created"

db-restore: ## Restore backup (BACKUP=filename)
	@[ -n "$(BACKUP)" ] || { echo "❌ BACKUP= required"; ls -1 backups/ 2>/dev/null; exit 1; }
	@$(COMPOSE) down
	@rm -rf data/
	@tar -xzf backups/$(BACKUP)
	@echo "✅ Restored from $(BACKUP)"

db-clean: ## Delete all data (interactive confirm)
	@echo "⚠️  This will delete all data!"
	@read -p "Are you sure? [y/N] " -n 1 -r; echo; \
	if [[ $$REPLY =~ ^[Yy]$$ ]]; then $(COMPOSE) down -v; rm -rf data/; echo "✅ Cleaned"; fi

# ── Code Quality ──────────────────────────────────────────────────────────────
fmt: ## Format Go code
	@go fmt ./...

lint: ## Lint Go code
	@go vet ./...

validate-openapi: ## Validate openapi.yaml
	@python3 scripts/validate_openapi.py && echo "✅ openapi.yaml valid"

# ── Utilities ─────────────────────────────────────────────────────────────────
health: ## Check server health
	@curl -sf http://localhost:8000/health | python3 -m json.tool || echo "❌ Server not responding"

docker-build: ## Build Docker images (local compose)
	$(COMPOSE) build

config-check: ## Show effective config from docker/.env
	@echo "── Storage ──"
	@grep -E '^(STORAGE_BACKEND|DATABASE_URL)=' docker/.env 2>/dev/null || echo "(not set — defaults to embedded)"
	@echo ""
	@echo "── Server ──"
	@grep -E '^(PORT|SERVER_URL|DOMAIN)=' docker/.env 2>/dev/null
	@echo ""
	@echo "── Auth ──"
	@grep -E '^JWT_' docker/.env 2>/dev/null | sed 's/=.*/=***/'

docs: ## Open API docs in browser
	@xdg-open http://localhost:8000/docs 2>/dev/null || open http://localhost:8000/docs 2>/dev/null || echo "Open http://localhost:8000/docs"

install-tools: ## Install dev tools
	@go install golang.org/x/tools/cmd/goimports@latest
	@pip3 install locust black 2>/dev/null || echo "⚠️  pip3 not found"

clean: ## Clean build artifacts (keeps data)
	rm -rf build/
	rm -f datumctl server
	find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	@echo "✅ Cleaned"

clean-all: clean ## Clean everything including containers and volumes
	$(COMPOSE) down -v 2>/dev/null || true
	@echo "✅ All cleaned"

.PHONY: help build build-web prepare-assets build-server build-cli build-api-only build-all \
	build-linux build-release release \
	run stop restart logs ps shell-server \
	deploy deploy-logs deploy-stop deploy-ps deploy-health \
	dev dev-logs run-server \
	test test-coverage coverage-check test-storage test-auth test-integration bench test-load \
	db-backup db-restore db-clean \
	fmt lint validate-openapi \
	health docker-build config-check docs install-tools clean clean-all
