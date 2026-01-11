#!/usr/bin/env bash

git pull
make clean
make build-all
make build release
docker compose --env-file ./docker/.env -f ./docker/docker-compose.external.yml up -d --force-recreate
exit 0
