#!/bin/bash
################################################################################
# Convenience wrapper for build-all.sh
# Builds Pico (RP2040) and Pico 2 W (RP2350) by default.
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

export BUILD_BOARDS="${BUILD_BOARDS:-pico,pico2_w}"
exec ./build-all.sh
