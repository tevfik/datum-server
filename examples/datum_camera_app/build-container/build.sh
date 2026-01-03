#!/bin/bash
set -e

IMAGE_NAME="datum-flutter-builder"
TAG="latest"

echo "🐳 Building Docker Image: $IMAGE_NAME:$TAG"
docker build -t $IMAGE_NAME:$TAG .

echo "✅ Build Complete!"
echo "To use this container:"
echo "  docker run --rm -v \$(pwd)/..:/app $IMAGE_NAME:$TAG flutter build apk --release"
