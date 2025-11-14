#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/GLS_Project/Builds/UnixMakefiles"
MODULE_CACHE="$ROOT_DIR/GLS_Project/Builds/ModuleCache"

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++
export CLANG_MODULE_CACHE_PATH="$MODULE_CACHE"

mkdir -p "$BUILD_DIR" "$MODULE_CACHE"

cd "$ROOT_DIR/GLS_Project"
cmake -B "$BUILD_DIR" -G "Unix Makefiles" \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_CXX_COMPILER="$CXX"
cmake --build "$BUILD_DIR" -- -j8
