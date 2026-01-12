#!/usr/bin/env bash

# Pull latest changes
git pull

# Remove old build artifacts from host (optional, just cleanup)
make clean

echo "Building Docker images (forcing no-cache to ensure updates)..."
# Build Docker image using the Dockerfile. 
# We use --no-cache to ensure the latest code is compiled inside Docker.
docker compose --env-file ./docker/.env -f ./docker/docker-compose.external.yml build --pull --no-cache

echo "Deploying new version..."
# Start services
docker compose --env-file ./docker/.env -f ./docker/docker-compose.external.yml up -d --force-recreate --remove-orphans

exit 0
