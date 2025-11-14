#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/GLS_Project/Builds/Xcode"
VST3_DIR="$BUILD_DIR/Debug/VST3"

VST3VALIDATOR="/path/to/vst3validator" # <-- update to your local install path

if [[ ! -x "$VST3VALIDATOR" ]]; then
  echo "ERROR: vst3validator not found or not executable at: $VST3VALIDATOR" >&2
  exit 1
fi

if [[ ! -d "$VST3_DIR" ]]; then
  echo "ERROR: VST3 directory not found: $VST3_DIR" >&2
  exit 1
fi

shopt -s nullglob

echo "Running VST3 validation in: $VST3_DIR"

for plugin in "$VST3_DIR"/*.vst3; do
  echo "----------------------------------------"
  echo "Validating: $plugin"
  "$VST3VALIDATOR" "$plugin" || echo "WARNING: validator returned non-zero for $plugin"
done

echo "Validation pass complete."
