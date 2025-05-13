#!/bin/bash

set -eu  # Exit on error and undefined vars

DEBUG=0

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --debug)
            DEBUG=1
            ;;
        *)
            echo "Unknown argument: $arg"
            exit 1
            ;;
    esac
done

# --- Host build ---
rm -rf build-host
mkdir build-host
cd build-host

if [ "$DEBUG" -eq 1 ]; then
    echo "🔧 Configuring host build with FORTH_DEBUG..."
    cmake .. -DBUILD_PICO=OFF -DCMAKE_C_FLAGS="-DFORTH_DEBUG"
else
    cmake .. -DBUILD_PICO=OFF
fi

make forth_host
cd ..

# --- Pico build ---
rm -rf build-pico
mkdir build-pico && cd build-pico
cmake .. -DBUILD_PICO=ON -DPICO_BOARD=pico2
make forth_pico

