#!/usr/bin/env bash
# Extract the BckGndData PNG from a .sim1/.sim2 file.
#
# Usage:
#   ./scripts/extract-bckgnd.sh <path-to.sim1> [output.png]
#
# If output.png is omitted, the image is written next to the .sim file as
# <simfile-basename>_bckgnd.png.

set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 <path-to.sim1> [output.png]" >&2
    exit 1
fi

SIM_FILE="$1"

if [[ ! -f "$SIM_FILE" ]]; then
    echo "Error: sim file not found: $SIM_FILE" >&2
    exit 1
fi

if [[ $# -eq 2 ]]; then
    OUT_PNG="$2"
else
    SIM_DIR=$(dirname -- "$SIM_FILE")
    SIM_BASE=$(basename -- "$SIM_FILE")
    SIM_STEM="${SIM_BASE%.*}"
    OUT_PNG="${SIM_DIR}/${SIM_STEM}_bckgnd.png"
fi

HEX=$(grep -oP 'BckGndData="\K[0-9a-fA-F]+' "$SIM_FILE" | head -n 1 || true)

if [[ -z "$HEX" ]]; then
    echo "Error: no BckGndData hex string found in $SIM_FILE" >&2
    exit 2
fi

printf '%s' "$HEX" | xxd -r -p > "$OUT_PNG"

if ! file "$OUT_PNG" | grep -q 'PNG image data'; then
    echo "Warning: extracted file does not look like a PNG. Output: $OUT_PNG" >&2
    exit 3
fi

echo "Extracted: $OUT_PNG"
file "$OUT_PNG"
