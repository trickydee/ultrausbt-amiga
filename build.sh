#!/bin/bash

# Build script for amigahid-pico
# This script initializes submodules, configures the build, and compiles the project

set -e  # Exit on error

echo "=== amigahid-pico Build Script ==="
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

# Step 2: Configure build with CMake
echo "Step 2: Configuring build with CMake..."
cmake -B build/ -S .
echo ""

# Step 3: Build the project
echo "Step 3: Building project..."
cd build
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
cd ..
echo ""

# Step 4: Check if build succeeded
if [ -f "build/amigahid-pico.uf2" ]; then
    echo "=== Build Successful! ==="
    echo "Output file: build/amigahid-pico.uf2"
    echo ""
    echo "To flash to your Pico:"
    echo "  1. Hold BOOTSEL button on Pico"
    echo "  2. Connect USB cable"
    echo "  3. Copy build/amigahid-pico.uf2 to the mounted volume"
else
    echo "=== Build Failed ==="
    echo "UF2 file not found at build/amigahid-pico.uf2"
    exit 1
fi

