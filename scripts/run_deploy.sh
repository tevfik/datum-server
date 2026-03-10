#!/usr/bin/env bash

# Navigate to project root (one level up from scripts/)
cd "$(dirname "$0")/.." || exit 1

# Pull latest changes
git pull

# Remove old build artifacts from host (optional)
# make clean # Disabled to prevent data loss (removes named volumes)

# Calculate version and build date
VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo "1.0.0-dev")
BUILD_DATE=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
echo "Deploying Version: $VERSION ($BUILD_DATE)"

echo "Building Docker images..."
# Build Docker image using build args to inject version info.
# --no-cache is redundant if build-args change, but good for safety.
docker compose --env-file ./docker/.env -f ./docker/docker-compose.external.yml build \
    --pull --no-cache \
    --build-arg VERSION="$VERSION" \
    --build-arg BUILD_DATE="$BUILD_DATE"

echo "Deploying services..."
# Start services
docker compose --env-file ./docker/.env -f ./docker/docker-compose.external.yml up -d --force-recreate --remove-orphans

exit 0
