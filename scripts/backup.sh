#!/bin/bash
# Datum Backup Script
# Creates timestamped backups of database and time-series data

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DATA_DIR="$PROJECT_DIR/data"
BACKUP_DIR="$PROJECT_DIR/backups"
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
BACKUP_FILE="$BACKUP_DIR/datum-backup-$TIMESTAMP.tar.gz"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== Datum Backup Utility ===${NC}"
echo ""

# Check if data directory exists
if [ ! -d "$DATA_DIR" ]; then
    echo -e "${RED}❌ Error: Data directory not found at $DATA_DIR${NC}"
    exit 1
fi

# Create backup directory if it doesn't exist
mkdir -p "$BACKUP_DIR"

# Show data directory size
DATA_SIZE=$(du -sh "$DATA_DIR" | cut -f1)
echo -e "${GREEN}📊 Data directory size: $DATA_SIZE${NC}"
echo ""

# Create backup
echo -e "${YELLOW}📦 Creating backup...${NC}"
tar -czf "$BACKUP_FILE" -C "$PROJECT_DIR" data/

if [ $? -eq 0 ]; then
    BACKUP_SIZE=$(du -sh "$BACKUP_FILE" | cut -f1)
    echo -e "${GREEN}✅ Backup created successfully!${NC}"
    echo -e "${GREEN}   Location: $BACKUP_FILE${NC}"
    echo -e "${GREEN}   Size: $BACKUP_SIZE${NC}"
    echo ""
    
    # List recent backups
    echo -e "${YELLOW}Recent backups:${NC}"
    ls -lht "$BACKUP_DIR" | head -6
    
    # Cleanup old backups (keep last 7)
    echo ""
    echo -e "${YELLOW}🧹 Cleaning old backups (keeping last 7)...${NC}"
    cd "$BACKUP_DIR"
    ls -t datum-backup-*.tar.gz | tail -n +8 | xargs -r rm -f
    echo -e "${GREEN}✅ Cleanup complete${NC}"
else
    echo -e "${RED}❌ Backup failed!${NC}"
    exit 1
fi
