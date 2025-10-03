#!/bin/bash
# all.sh
# Builds AMOURANTH RTX for both Linux and Windows
# Usage: ./all.sh [clean]

set -e

# Run Linux build
echo "Starting Linux build..."
./linux.sh "$1"

# Run Windows build
echo "Starting Windows build..."
./windows.sh "$1"

echo "All builds completed."