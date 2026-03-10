#!/bin/bash
# Simple password reset script for Datum server

echo "═══════════════════════════════════════════"
echo "  Datum Server - Password Reset Helper"
echo "═══════════════════════════════════════════"
echo ""
echo "This script will help you reset your system if you forgot your password."
echo ""
echo "⚠️  WARNING: This will delete ALL data!"
echo ""
read -p "Continue? (type 'yes'): " confirm

if [ "$confirm" != "yes" ]; then
    echo "Cancelled"
    exit 1
fi

echo ""
echo "Step 1: Stopping server..."
if command -v docker-compose &> /dev/null; then
    docker-compose down
elif command -v docker &> /dev/null && docker ps | grep -q datum; then
    docker stop $(docker ps -q --filter ancestor=datum-server)
else
    echo "⚠️  Could not auto-stop server. Please stop it manually."
    read -p "Press Enter when server is stopped..."
fi

echo ""
echo "Step 2: Deleting database..."
if [ -f "/app/data/meta.db" ]; then
    rm -f /app/data/meta.db
    echo "✅ Deleted /app/data/meta.db"
elif [ -f "./data/meta.db" ]; then
    rm -f ./data/meta.db
    echo "✅ Deleted ./data/meta.db"
elif [ -f "../data/meta.db" ]; then
    rm -f ../data/meta.db
    echo "✅ Deleted ../data/meta.db"
else
    echo "⚠️  Could not find meta.db. Please delete it manually:"
    echo "   rm /app/data/meta.db"
    echo "   or"
    echo "   rm ./data/meta.db"
fi

echo ""
echo "Step 3: Restarting server..."
if command -v docker-compose &> /dev/null; then
    docker-compose up -d
    echo "✅ Server restarted with docker-compose"
else
    echo "⚠️  Please restart server manually"
fi

echo ""
echo "═══════════════════════════════════════════"
echo "  ✅ Reset complete!"
echo "═══════════════════════════════════════════"
echo ""
echo "Next steps:"
echo "1. Check status: datumctl status"
echo "2. Run setup with new password:"
echo ""
echo "   Option A - Interactive (Easiest):"
echo "   datumctl setup"
echo ""
echo "   Option B - Command line:"
echo "   datumctl setup --email admin@example.com --platform \"My IoT Platform\""
echo ""
echo "   Option C - Using curl:"
echo "   curl -X POST http://localhost:8000/system/setup \\"
echo "     -H 'Content-Type: application/json' \\"
echo "     -d '{"
echo "       \"platform_name\": \"My IoT Platform\","
echo "       \"admin_email\": \"admin@example.com\","
echo "       \"admin_password\": \"your-new-password\","
echo "       \"allow_register\": false"
echo "     }'"
echo ""
echo "Note: datumctl setup will automatically log you in after successful setup!"
echo ""
