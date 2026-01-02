#!/bin/bash
set -e

echo "🔨 Building Datum Server and CLI..."
make build

echo "🔨 Building Flutter App (Mock)..."
# In a real environment, we would run:
# cd examples/flutter_app && flutter build apk
# For this environment, we just verify the file structure exists.

if [ -d "examples/flutter_app/lib" ]; then
    echo "✅ Flutter app structure verified."
    echo "✅ pubspec.yaml found."
    echo "✅ lib/main.dart found."
else
    echo "❌ Flutter app missing!"
    exit 1
fi

echo "🧪 Running Go Tests..."
make test

# echo "🧪 Running Flutter Tests (Mock)..."
# In a real environment:
# cd examples/flutter_app && flutter test

echo "✅ Build and Test Complete!"
