#!/usr/bin/env bash
# VOLTA Resonance — Build + Install script
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

AU_INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

PLUGIN_NAME="VOLTA Resonance"

echo ""
echo -e "${CYAN}╔══════════════════════════════════════╗${NC}"
echo -e "${CYAN}║    VOLTA Resonance — Build & Install  ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════╝${NC}"
echo ""

# Check dependencies quickly
if ! xcode-select -p &>/dev/null; then
    echo -e "${RED}Error: Xcode Command Line Tools not found.${NC}"
    echo "Run: bash setup.sh"
    exit 1
fi
if ! command -v cmake &>/dev/null; then
    echo -e "${RED}Error: CMake not found.${NC}"
    echo "Run: bash setup.sh"
    exit 1
fi

cd "$SCRIPT_DIR"

# Step 1: Configure
echo -e "${BOLD}Step 1/4: Configuring project...${NC}"
echo "(First run downloads JUCE ~200MB — this may take a few minutes)"
echo ""
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    2>&1 | grep -v "^--" | grep -v "^$" || true
echo -e "${GREEN}✓ Configuration complete${NC}"
echo ""

# Step 2: Build
echo -e "${BOLD}Step 2/4: Building plugins...${NC}"
NPROC=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
cmake --build "$BUILD_DIR" --config Release -j"$NPROC"
echo -e "${GREEN}✓ Build complete${NC}"
echo ""

# Step 3: Codesign (with network entitlement so VoltaSync can reach the relay)
echo -e "${BOLD}Step 3/4: Code signing...${NC}"

AU_BUILD="$BUILD_DIR/VoltaResonance_artefacts/Release/AU/${PLUGIN_NAME}.component"
VST3_BUILD="$BUILD_DIR/VoltaResonance_artefacts/Release/VST3/${PLUGIN_NAME}.vst3"
ENTITLEMENTS="$SCRIPT_DIR/VoltaResonance.entitlements"

sign_plugin() {
    local path="$1"
    local label="$2"
    if [ -d "$path" ]; then
        if codesign --force --deep -s - \
            --entitlements "$ENTITLEMENTS" "$path" 2>/dev/null; then
            echo -e "${GREEN}✓ $label signed (with network entitlement)${NC}"
        else
            codesign --force --deep -s - "$path" 2>/dev/null && \
                echo -e "${YELLOW}⚠ $label signed (no entitlement — VoltaSync may not work in Logic)${NC}" || \
                echo -e "${YELLOW}⚠ $label signing skipped${NC}"
        fi
    fi
}

sign_plugin "$AU_BUILD"   "AU"
sign_plugin "$VST3_BUILD" "VST3"
echo ""

# Step 4: Install (manual fallback if COPY_PLUGIN_AFTER_BUILD didn't fire)
echo -e "${BOLD}Step 4/4: Installing plugins...${NC}"
mkdir -p "$AU_INSTALL_DIR"
mkdir -p "$VST3_INSTALL_DIR"

AU_INSTALLED=0
VST3_INSTALLED=0

# Check if already copied by JUCE
if [ -d "$AU_INSTALL_DIR/${PLUGIN_NAME}.component" ]; then
    echo -e "${GREEN}✓ AU already installed by build system${NC}"
    AU_INSTALLED=1
elif [ -d "$AU_BUILD" ]; then
    cp -R "$AU_BUILD" "$AU_INSTALL_DIR/"
    echo -e "${GREEN}✓ AU installed to $AU_INSTALL_DIR${NC}"
    AU_INSTALLED=1
else
    echo -e "${RED}✗ AU build artifact not found: $AU_BUILD${NC}"
fi

if [ -d "$VST3_INSTALL_DIR/${PLUGIN_NAME}.vst3" ]; then
    echo -e "${GREEN}✓ VST3 already installed by build system${NC}"
    VST3_INSTALLED=1
elif [ -d "$VST3_BUILD" ]; then
    cp -R "$VST3_BUILD" "$VST3_INSTALL_DIR/"
    echo -e "${GREEN}✓ VST3 installed to $VST3_INSTALL_DIR${NC}"
    VST3_INSTALLED=1
else
    echo -e "${RED}✗ VST3 build artifact not found: $VST3_BUILD${NC}"
fi

echo ""
echo -e "${CYAN}════════════════════════════════════════${NC}"
echo -e "${BOLD}Installation complete!${NC}"
echo -e "${CYAN}════════════════════════════════════════${NC}"
echo ""

if [ "$AU_INSTALLED" -eq 1 ]; then
    echo -e "${GREEN}✓ AU:   $AU_INSTALL_DIR/${PLUGIN_NAME}.component${NC}"
fi
if [ "$VST3_INSTALLED" -eq 1 ]; then
    echo -e "${GREEN}✓ VST3: $VST3_INSTALL_DIR/${PLUGIN_NAME}.vst3${NC}"
fi

echo ""
echo -e "${BOLD}Next steps:${NC}"
echo ""
echo "  Logic Pro:"
echo "    Logic Pro menu → Settings → Plug-in Manager → Rescan All"
echo "    New Software Instrument track → AU Instruments → Volta Labs → VOLTA Resonance"
echo ""
echo "  Ableton Live:"
echo "    Preferences → Plug-Ins → VST3 Plug-In Custom Folder → Rescan"
echo "    Add MIDI track → Instrument slot → VST3 → VOLTA Resonance"
echo ""
echo "  Play MIDI note 36 = KICK, 38 = SNARE, 42 = HH CLOSED, etc."
echo ""
