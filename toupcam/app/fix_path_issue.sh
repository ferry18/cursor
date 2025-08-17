#!/bin/bash

# Script to fix path issues with special characters

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Path Issue Detector${NC}"
echo ""

# Get current directory
CURRENT_DIR=$(pwd)
echo "Current directory: $CURRENT_DIR"

# Check if path contains problematic characters
if [[ "$CURRENT_DIR" =~ [\(\)\[\]\{\}\&\;\'\"\<\>\|] ]]; then
    echo -e "${RED}ERROR: Your project path contains special characters!${NC}"
    echo "Characters like (), [], {}, &, ;, ', \", <, >, | can cause build issues."
    echo ""
    echo -e "${YELLOW}Solution: Copy the project to a path without special characters${NC}"
    echo ""
    echo "Suggested new location: $HOME/toupcam-app"
    echo ""
    echo "To fix this, run these commands from the parent directory:"
    echo ""
    echo "  cd $(dirname "$(dirname "$CURRENT_DIR")")"
    echo "  cp -r \"$(basename "$(dirname "$CURRENT_DIR")")\" $HOME/toupcam-app"
    echo "  cd $HOME/toupcam-app/toupcam/app"
    echo "  ./build_with_qt5.sh"
    echo ""
    exit 1
else
    echo -e "${GREEN}Path looks good - no special characters detected${NC}"
fi