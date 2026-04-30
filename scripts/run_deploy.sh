#!/usr/bin/env bash
# Deploy datum-server using the external Traefik compose.
# This is a thin wrapper around `make deploy` for backward compatibility.

cd "$(dirname "$0")/.." || exit 1

git pull

exec make deploy
