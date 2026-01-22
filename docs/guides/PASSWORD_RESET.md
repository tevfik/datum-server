# Password Reset Guide

## If You Forgot Your Password

There are several ways to reset passwords or regain access to your Datum system.

## Option 1: Complete System Reset (Fresh Start)

If you forgot the admin password and want to start fresh:

### Step 1: Stop the server
```bash
docker-compose down
# or if running directly:
# kill the server process
```

### Step 2: Delete the database files
```bash
# From datum-server directory
rm -rf ./data/meta.db
rm -rf ./data/tsdata/*
```

### Step 3: Restart and setup again
```bash
docker-compose up -d

# Then run setup
datumctl status
# Should show: "initialized": false

# Run setup to create new admin
curl -X POST http://localhost:8000/sys/setup \
  -H "Content-Type: application/json" \
  -d '{
    "platform_name": "My IoT Platform",
    "admin_email": "admin@example.com",
    "admin_password": "your-new-password",
    "allow_register": false,
    "data_retention": 30
  }'
```

## Option 2: Reset via Database Access (Advanced)

If you have direct access to the server and can modify the database:

### Method A: Using datumctl admin command

```bash
# If you have another admin user or can temporarily get access:
datumctl login --email other-admin@example.com
datumctl admin reset-password admin@example.com --new-password "newpass123"
```

### Method B: Direct database manipulation

1. Stop the server
2. Delete `./data/meta.db`
3. Restart - system will be uninitialized
4. Run setup again

## Option 3: If You Have Token/API Key

If you saved your JWT token or have an API key:

```bash
# Using token
datumctl --token YOUR_JWT_TOKEN admin reset-password admin@example.com

# Using API key
datumctl --api-key YOUR_API_KEY admin reset-password admin@example.com
```

## Option 4: Create New Admin via Direct API (If Registered)

If there's another user with admin role:

```bash
# Login as another admin
datumctl login --email other-admin@example.com

# Reset the forgotten password
datumctl admin reset-password forgotten-user@example.com
```

## Option 5: Docker Volume Reset (Clean Slate)

If using Docker and want to completely reset:

```bash
# Stop containers
docker-compose down

# Remove volumes (deletes ALL data!)
docker volume rm datum-server_data

# Start fresh
docker-compose up -d

# Run setup
datumctl status  # Should show uninitialized
# Then setup as new
```

## Prevention: Backup Your Credentials

To avoid this in the future:

### Save your token after login
```bash
datumctl login --email admin@example.com
# Token is saved to ~/.datumctl.yaml

# Backup this file!
cp ~/.datumctl.yaml ~/.datumctl.yaml.backup
```

### Create a second admin account
```bash
datumctl admin create-user
# Email: backup-admin@example.com
# Role: admin
```

### Use API keys for automation
```bash
# Device API keys are generated when you create a device
datumctl device create --name "My Device"
# Save the returned API key (dk_...) in a secure place
```

## Quick Reset Script

Save this as `reset-datum.sh`:

```bash
#!/bin/bash
echo "⚠️  This will DELETE ALL DATA!"
read -p "Type 'yes' to continue: " confirm

if [ "$confirm" != "yes" ]; then
    echo "Cancelled"
    exit 1
fi

echo "Stopping services..."
docker-compose down

echo "Deleting database..."
rm -rf ./data/meta.db
rm -rf ./data/tsdata/*

echo "Starting services..."
docker-compose up -d

echo "✅ System reset! Run setup:"
echo "datumctl status"
```

## Summary

**Easiest method**: Delete `data/meta.db`, restart server, run setup again

**Data preservation**: If you need to keep device data but reset users, use the admin API to reset passwords (requires another admin access)

**Complete reset**: Use `datumctl admin reset-system` if you have admin access

**Emergency**: Direct file deletion is always an option - the system will automatically reinitialize
