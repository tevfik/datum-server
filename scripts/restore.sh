#!/bin/bash
# Datum Restore Script
# Restores database and time-series data from backup

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DATA_DIR="$PROJECT_DIR/data"
BACKUP_DIR="$PROJECT_DIR/backups"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== Datum Restore Utility ===${NC}"
echo ""

# Check if backup directory exists
if [ ! -d "$BACKUP_DIR" ]; then
    echo -e "${RED}❌ Error: Backup directory not found at $BACKUP_DIR${NC}"
    exit 1
fi

# List available backups
echo -e "${YELLOW}Available backups:${NC}"
BACKUPS=($(ls -t "$BACKUP_DIR"/datum-backup-*.tar.gz 2>/dev/null))

if [ ${#BACKUPS[@]} -eq 0 ]; then
    echo -e "${RED}❌ No backups found in $BACKUP_DIR${NC}"
    exit 1
fi

for i in "${!BACKUPS[@]}"; do
    BACKUP_FILE="${BACKUPS[$i]}"
    BACKUP_NAME=$(basename "$BACKUP_FILE")
    BACKUP_SIZE=$(du -sh "$BACKUP_FILE" | cut -f1)
    BACKUP_DATE=$(echo "$BACKUP_NAME" | sed 's/datum-backup-//' | sed 's/.tar.gz//')
    echo -e "  ${GREEN}[$((i+1))]${NC} $BACKUP_NAME (${BACKUP_SIZE})"
done
echo ""

# Get user selection
if [ -z "$1" ]; then
    read -p "Select backup to restore [1-${#BACKUPS[@]}]: " SELECTION
    BACKUP_FILE="${BACKUPS[$((SELECTION-1))]}"
else
    # Backup file provided as argument
    if [ -f "$1" ]; then
        BACKUP_FILE="$1"
    elif [ -f "$BACKUP_DIR/$1" ]; then
        BACKUP_FILE="$BACKUP_DIR/$1"
    else
        echo -e "${RED}❌ Error: Backup file not found: $1${NC}"
        exit 1
    fi
fi

if [ ! -f "$BACKUP_FILE" ]; then
    echo -e "${RED}❌ Error: Invalid selection${NC}"
    exit 1
fi

echo -e "${YELLOW}Selected backup: $(basename "$BACKUP_FILE")${NC}"
echo ""

# Confirm
read -p "⚠️  This will overwrite existing data. Continue? [y/N]: " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${RED}Restore cancelled${NC}"
    exit 1
fi

# Stop services
echo -e "${YELLOW}🛑 Stopping services...${NC}"
cd "$PROJECT_DIR"
docker-compose down 2>/dev/null || true

# Backup current data if it exists
if [ -d "$DATA_DIR" ]; then
    echo -e "${YELLOW}💾 Backing up current data...${NC}"
    mv "$DATA_DIR" "$DATA_DIR.bak-$(date +%Y%m%d-%H%M%S)"
fi

# Restore from backup
echo -e "${YELLOW}🔄 Restoring from backup...${NC}"
tar -xzf "$BACKUP_FILE" -C "$PROJECT_DIR"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Restore completed successfully!${NC}"
    echo ""
    echo -e "${YELLOW}Starting services...${NC}"
    docker-compose up -d
    echo -e "${GREEN}✅ Services started${NC}"
else
    echo -e "${RED}❌ Restore failed!${NC}"
    
    # Restore old data if available
    if [ -d "$DATA_DIR.bak"* ]; then
        echo -e "${YELLOW}🔄 Restoring previous data...${NC}"
        LATEST_BAK=$(ls -td "$DATA_DIR.bak"* | head -1)
        mv "$LATEST_BAK" "$DATA_DIR"
    fi
    exit 1
fi
