#!/bin/bash

# Build script for amigahid-pico
# This script builds for all supported boards: Pico (RP2040), Pico W (RP2040 with CYW43), and Pico 2 W (RP2350)

set -e  # Exit on error

echo "=== amigahid-pico Build Script (All Boards) ==="
echo ""

# Check if we're in the right directory (look for CMakeLists.txt)
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the project root."
    exit 1
fi

# Step 1: Initialize submodules
echo "Step 1: Initializing git submodules..."
if [ ! -d "pico-sdk" ] || [ ! -f "pico-sdk/.git" ]; then
    echo "  Initializing submodules (this may take a while for TinyUSB)..."
    git submodule update --init --recursive
else
    echo "  Submodules already initialized, skipping..."
fi
echo ""

# Get number of CPU cores for parallel builds
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Function to build for a specific board
build_for_board() {
    local BOARD=$1
    local BUILD_DIR=$2
    local PLATFORM_NAME=$3
    
    echo "=== Building for $PLATFORM_NAME ($BOARD) ==="
    echo ""
    
    # Configure build with CMake
    echo "Configuring build for $PLATFORM_NAME..."
    cmake -B "$BUILD_DIR" -S . -DPICO_BOARD="$BOARD"
    echo ""
    
    # Build the project
    echo "Building $PLATFORM_NAME..."
    cd "$BUILD_DIR"
    make -j"$CORES"
    cd ..
    echo ""
}

# Step 2: Build for Pico (RP2040)
echo "Step 2: Building for Pico (RP2040)..."
build_for_board "pico" "build" "Pico (RP2040)"

# Step 3: Build for Pico W (RP2040 with CYW43)
echo "Step 3: Building for Pico W (RP2040 with CYW43)..."
build_for_board "pico_w" "build-picow" "Pico W (RP2040 with CYW43)"

# Step 4: Build for Pico 2 W (RP2350)
echo "Step 4: Building for Pico 2 W (RP2350)..."
build_for_board "pico2_w" "build-pico2w" "Pico 2 W (RP2350)"

# Step 5: Check if builds succeeded
echo "=== Build Summary ==="
echo ""

BUILD_SUCCESS=true

if [ -f "build/amigahid-pico.uf2" ]; then
    echo "✓ Pico (RP2040) build successful"
    echo "  Output file: build/amigahid-pico.uf2"
    ls -lh build/amigahid-pico.uf2 | awk '{print "  Size: " $5}'
else
    echo "✗ Pico (RP2040) build failed"
    echo "  UF2 file not found at build/amigahid-pico.uf2"
    BUILD_SUCCESS=false
fi

echo ""

if [ -f "build-picow/amigahid-pico.uf2" ]; then
    echo "✓ Pico W (RP2040 with CYW43) build successful"
    echo "  Output file: build-picow/amigahid-pico.uf2"
    ls -lh build-picow/amigahid-pico.uf2 | awk '{print "  Size: " $5}'
else
    echo "✗ Pico W (RP2040 with CYW43) build failed"
    echo "  UF2 file not found at build-picow/amigahid-pico.uf2"
    BUILD_SUCCESS=false
fi

echo ""

if [ -f "build-pico2w/amigahid-pico.uf2" ]; then
    echo "✓ Pico 2 W (RP2350) build successful"
    echo "  Output file: build-pico2w/amigahid-pico.uf2"
    ls -lh build-pico2w/amigahid-pico.uf2 | awk '{print "  Size: " $5}'
else
    echo "✗ Pico 2 W (RP2350) build failed"
    echo "  UF2 file not found at build-pico2w/amigahid-pico.uf2"
    BUILD_SUCCESS=false
fi

echo ""
echo "To flash to your board:"
echo "  1. Hold BOOTSEL button on your Pico"
echo "  2. Connect USB cable"
echo "  3. Copy the appropriate .uf2 file to the mounted volume:"
echo "     - Pico (RP2040): build/amigahid-pico.uf2"
echo "     - Pico W (RP2040 with CYW43): build-picow/amigahid-pico.uf2"
echo "     - Pico 2 W (RP2350): build-pico2w/amigahid-pico.uf2"
echo ""

if [ "$BUILD_SUCCESS" = false ]; then
    exit 1
fi

