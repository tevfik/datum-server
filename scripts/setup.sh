#!/bin/bash
# Quick setup script for Datumpy IoT Platform

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}"
echo "╔══════════════════════════════════════╗"
echo "║   Datumpy IoT Platform Setup        ║"
echo "╚══════════════════════════════════════╝"
echo -e "${NC}"

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo -e "${YELLOW}⚠️  Docker not found. Please install Docker first.${NC}"
    exit 1
fi

# Check if docker-compose is installed
if ! command -v docker-compose &> /dev/null; then
    echo -e "${YELLOW}⚠️  docker-compose not found. Please install docker-compose first.${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Docker and docker-compose found${NC}"
echo ""

# Check if .env exists
if [ ! -f .env ]; then
    echo -e "${YELLOW}📝 Creating .env file from template...${NC}"
    cp docker/.env.example .env
    
    # Generate random JWT secret
    JWT_SECRET=$(openssl rand -base64 32 2>/dev/null || cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)
    sed -i.bak "s/your-secret-key-min-32-chars-long-change-this-in-production/$JWT_SECRET/" .env
    
    # Generate random admin key
    ADMIN_KEY=$(openssl rand -base64 24 2>/dev/null || cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 24 | head -n 1)
    sed -i.bak "s/admin-secret-key-change-in-production/$ADMIN_KEY/" .env
    
    rm -f .env.bak
    echo -e "${GREEN}✅ .env file created with random secrets${NC}"
else
    echo -e "${GREEN}✅ .env file already exists${NC}"
fi
echo ""

# Create data directory if needed
mkdir -p data backups
echo -e "${GREEN}✅ Data directories created${NC}"
echo ""

# Build images
echo -e "${YELLOW}🔨 Building Docker images...${NC}"
docker-compose build

echo ""
echo -e "${GREEN}✅ Build complete!${NC}"
echo ""

# Start services
echo -e "${YELLOW}🚀 Starting services...${NC}"
docker-compose up -d

echo ""
echo -e "${GREEN}✅ Services started!${NC}"
echo ""

# Wait for services to be healthy
echo -e "${YELLOW}⏳ Waiting for services to be ready...${NC}"
sleep 5

# Check service health
API_HEALTH=$(curl -s http://localhost:8007/health 2>/dev/null || echo "")
if [[ $API_HEALTH == *"healthy"* ]]; then
    echo -e "${GREEN}✅ API is healthy${NC}"
else
    echo -e "${YELLOW}⚠️  API may still be starting up${NC}"
fi

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}🎉 Datumpy is ready!${NC}"
echo ""
echo -e "Access your services:"
echo -e "  ${BLUE}API:${NC}        http://localhost:8007"
echo -e "  ${BLUE}Analytics:${NC}  http://localhost:8001"
echo -e "  ${BLUE}Dashboard:${NC}  http://localhost:3000"
echo -e "  ${BLUE}API Docs:${NC}   http://localhost:8007/docs"
echo ""
echo -e "Useful commands:"
echo -e "  ${YELLOW}make logs${NC}       - View logs"
echo -e "  ${YELLOW}make stop${NC}       - Stop services"
echo -e "  ${YELLOW}make db-backup${NC}  - Backup database"
echo -e "  ${YELLOW}make help${NC}       - See all commands"
echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
