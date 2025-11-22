#!/usr/bin/env bash
set -euo pipefail

# Collect all built VST3 bundles into a single folder without deleting existing artefacts.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="${ROOT}/GLS_Project/Builds/UnixMakefiles"
TARGET="${ROOT}/GLS_Project/Builds/VST3_All"

mkdir -p "${TARGET}"

find "${SOURCE}" -path '*_artefacts/VST3/*.vst3' -type d -print0 | while IFS= read -r -d '' vst; do
  name="$(basename "${vst}")"
  dest="${TARGET}/${name}"
  mkdir -p "${dest}"
  rsync -a --delete "${vst}/" "${dest}/"
done

echo "Collected VST3 bundles into: ${TARGET}"
