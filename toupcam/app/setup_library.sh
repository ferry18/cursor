#!/bin/bash

# Setup script for libtoupcam.so library

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}Toupcam Library Setup${NC}"
echo ""

# Expected library location
LIB_DIR="../toupcamsdk/linux/arm64"
LIB_FILE="$LIB_DIR/libtoupcam.so"

# Create directory if it doesn't exist
if [ ! -d "$LIB_DIR" ]; then
    echo -e "${YELLOW}Creating library directory: $LIB_DIR${NC}"
    mkdir -p "$LIB_DIR"
fi

# Check if library exists
if [ -f "$LIB_FILE" ]; then
    echo -e "${GREEN}✅ Library found at: $LIB_FILE${NC}"
else
    echo -e "${RED}❌ Library not found at: $LIB_FILE${NC}"
    echo ""
    echo -e "${YELLOW}Please copy your libtoupcam.so file to:${NC}"
    echo -e "${YELLOW}  $(realpath $LIB_DIR)/${NC}"
    echo ""
    echo "You can copy it with:"
    echo "  cp /path/to/your/libtoupcam.so $LIB_DIR/"
    echo ""
    
    # Try to find the library elsewhere
    echo "Looking for libtoupcam.so in common locations..."
    
    # Check some common locations
    for location in \
        "$HOME/Downloads/libtoupcam.so" \
        "$HOME/libtoupcam.so" \
        "/usr/local/lib/libtoupcam.so" \
        "/usr/lib/libtoupcam.so" \
        "/opt/toupcam/libtoupcam.so"
    do
        if [ -f "$location" ]; then
            echo -e "${GREEN}Found at: $location${NC}"
            echo -e "${YELLOW}Would you like to copy it to the correct location? (y/n)${NC}"
            read -r response
            if [[ "$response" =~ ^[Yy]$ ]]; then
                cp "$location" "$LIB_FILE"
                echo -e "${GREEN}Library copied successfully!${NC}"
                break
            fi
        fi
    done
fi

# Check library architecture
if [ -f "$LIB_FILE" ]; then
    echo ""
    echo "Checking library architecture..."
    file_output=$(file "$LIB_FILE")
    if [[ "$file_output" == *"ARM aarch64"* ]] || [[ "$file_output" == *"ARM64"* ]]; then
        echo -e "${GREEN}✅ Library is ARM64 compatible${NC}"
    else
        echo -e "${RED}⚠️  Warning: Library may not be ARM64 compatible${NC}"
        echo "Library info: $file_output"
    fi
    
    # Check if it's a valid shared library
    if [[ "$file_output" == *"shared object"* ]] || [[ "$file_output" == *"dynamically linked"* ]]; then
        echo -e "${GREEN}✅ Valid shared library${NC}"
    else
        echo -e "${RED}❌ Not a valid shared library${NC}"
    fi
fi

echo ""
echo -e "${GREEN}Setup complete!${NC}"