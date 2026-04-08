#!/usr/bin/env bash
# VOLTA Resonance — Dependency checker
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo ""
echo -e "${CYAN}╔══════════════════════════════════════╗${NC}"
echo -e "${CYAN}║     VOLTA Resonance — Setup Check    ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════╝${NC}"
echo ""

MISSING=0

# 1. Xcode Command Line Tools
echo -n "Checking Xcode Command Line Tools... "
if xcode-select -p &>/dev/null; then
    echo -e "${GREEN}✓ Found$(NC)"
else
    echo -e "${RED}✗ Missing${NC}"
    echo ""
    echo -e "${YELLOW}  Install Xcode Command Line Tools by running:${NC}"
    echo "    xcode-select --install"
    echo "  Then re-run this script."
    MISSING=1
fi

# 2. CMake
echo -n "Checking CMake... "
if command -v cmake &>/dev/null; then
    CMAKE_VER=$(cmake --version | head -1)
    echo -e "${GREEN}✓ $CMAKE_VER${NC}"
else
    echo -e "${RED}✗ Missing${NC}"
    echo ""
    echo -e "${YELLOW}  Install CMake by running:${NC}"
    if command -v brew &>/dev/null; then
        echo "    brew install cmake"
    else
        echo "    First install Homebrew from https://brew.sh, then run:"
        echo "    brew install cmake"
    fi
    MISSING=1
fi

# 3. Git
echo -n "Checking Git... "
if command -v git &>/dev/null; then
    GIT_VER=$(git --version)
    echo -e "${GREEN}✓ $GIT_VER${NC}"
else
    echo -e "${RED}✗ Missing${NC}"
    echo ""
    echo -e "${YELLOW}  Install Git by running:${NC}"
    echo "    xcode-select --install"
    echo "  (Git comes with Xcode Command Line Tools)"
    MISSING=1
fi

# 4. Check macOS version
echo -n "Checking macOS version... "
MACOS_VER=$(sw_vers -productVersion)
MACOS_MAJOR=$(echo "$MACOS_VER" | cut -d. -f1)
if [ "$MACOS_MAJOR" -ge 11 ]; then
    echo -e "${GREEN}✓ macOS $MACOS_VER${NC}"
else
    echo -e "${YELLOW}⚠ macOS $MACOS_VER (Big Sur 11.0+ recommended)${NC}"
fi

echo ""
if [ "$MISSING" -eq 0 ]; then
    echo -e "${GREEN}All dependencies found! Run:${NC}"
    echo ""
    echo "    bash build.sh"
    echo ""
else
    echo -e "${RED}Please install missing dependencies above, then re-run this script.${NC}"
    exit 1
fi
