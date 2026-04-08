#!/usr/bin/env bash
# Deploy VOLTA Sync relay to Fly.io (free tier, always-on)
set -euo pipefail

CYAN='\033[0;36m'; GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'

echo ""
echo -e "${CYAN}VOLTA Sync — Deploy to Fly.io${NC}"
echo ""

# 1. Check flyctl is installed
if ! command -v flyctl &>/dev/null; then
  echo -e "${RED}flyctl not found. Install it:${NC}"
  echo "  curl -L https://fly.io/install.sh | sh"
  echo "  Then add ~/.fly/bin to your PATH and re-run this script."
  exit 1
fi

cd "$(dirname "$0")"

# 2. Login if needed
if ! flyctl auth whoami &>/dev/null; then
  echo "Not logged in — opening browser for Fly.io signup/login..."
  flyctl auth login
fi

# 3. Create app (only needed first time; safe to re-run)
echo -e "${CYAN}Creating app (skipped if already exists)...${NC}"
flyctl apps create volta-sync 2>/dev/null || true

# 4. Deploy
echo -e "${CYAN}Deploying...${NC}"
flyctl deploy

echo ""
echo -e "${GREEN}✓ VOLTA Sync relay is live at:${NC}"
echo "  wss://volta-sync.fly.dev"
echo ""
echo "Update VoltaSync.h  →  SERVER_HOST = \"volta-sync.fly.dev\""
echo "Then rebuild the plugin: bash ../build.sh"
