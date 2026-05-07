#!/bin/bash

# Build script for amigahid-pico (Pico 2 W only)
# This script builds only for Pico 2 W (RP2350) with Bluetooth support

set -e  # Exit on error

echo "=== amigahid-pico Build Script (Pico 2 W) ==="
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

# Step 2: Configure build with CMake
echo "Step 2: Configuring build for Pico 2 W (RP2350)..."
cmake -B build-pico2w -S . -DPICO_BOARD=pico2_w
echo ""

# Step 3: Build the project
echo "Step 3: Building Pico 2 W (RP2350)..."
cd build-pico2w
make -j"$CORES"
cd ..
echo ""

# Step 4: Check if build succeeded
echo "=== Build Summary ==="
echo ""

if [ -f "build-pico2w/amigahid-pico.uf2" ]; then
    echo "✓ Pico 2 W (RP2350) build successful"
    echo "  Output file: build-pico2w/amigahid-pico.uf2"
    ls -lh build-pico2w/amigahid-pico.uf2 | awk '{print "  Size: " $5}'
    echo ""
    echo "To flash to your Pico 2 W:"
    echo "  1. Hold BOOTSEL button on Pico 2 W"
    echo "  2. Connect USB cable"
    echo "  3. Copy build-pico2w/amigahid-pico.uf2 to the mounted volume"
    echo ""
    exit 0
else
    echo "✗ Pico 2 W (RP2350) build failed"
    echo "  UF2 file not found at build-pico2w/amigahid-pico.uf2"
    echo ""
    exit 1
fi

