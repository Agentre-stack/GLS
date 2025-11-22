#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_VALIDATOR="$HOME/vst3sdk/cmake-build/bin/Release/validator"
VST3VALIDATOR="${VST3VALIDATOR:-$DEFAULT_VALIDATOR}"
VST3_SEARCH_ROOT="${VST3_SEARCH_ROOT:-$ROOT_DIR/GLS_Project/Builds}"

if [[ ! -x "$VST3VALIDATOR" ]]; then
  echo "ERROR: vst3validator not found or not executable at: $VST3VALIDATOR" >&2
  exit 1
fi

if [[ ! -d "$VST3_SEARCH_ROOT" ]]; then
  echo "ERROR: Build directory not found: $VST3_SEARCH_ROOT" >&2
  exit 1
fi

VST3_PLUGINS=()
while IFS= read -r plugin; do
  VST3_PLUGINS+=("$plugin")
done < <(find "$VST3_SEARCH_ROOT" -type d -name "*.vst3" 2>/dev/null | sort)

if [[ ${#VST3_PLUGINS[@]} -eq 0 ]]; then
  echo "ERROR: No .vst3 bundles found under $VST3_SEARCH_ROOT" >&2
  exit 1
fi

echo "Running VST3 validation using $VST3VALIDATOR"

for plugin in "${VST3_PLUGINS[@]}"; do
  echo "----------------------------------------"
  echo "Validating: $plugin"
  if ! "$VST3VALIDATOR" "$plugin"; then
    echo "WARNING: validator returned non-zero for $plugin" >&2
  fi
done

echo "Validation pass complete."
