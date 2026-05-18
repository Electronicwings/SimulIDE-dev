#!/usr/bin/env bash
# Replace the BckGndData PNG in a .sim1/.sim2 file with a new image.
#
# Usage:
#   ./scripts/replace-bckgnd.sh <path-to.sim1> [input.png]
#
# If input.png is omitted, looks for <simfile-basename>_bckgnd.png next to
# the .sim file (matching the default output of extract-bckgnd.sh).
#
# A backup is written to <simfile>.bak before modification.

set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 <path-to.sim1> [input.png]" >&2
    exit 1
fi

SIM_FILE="$1"

if [[ ! -f "$SIM_FILE" ]]; then
    echo "Error: sim file not found: $SIM_FILE" >&2
    exit 1
fi

if [[ $# -eq 2 ]]; then
    IN_PNG="$2"
else
    SIM_DIR=$(dirname -- "$SIM_FILE")
    SIM_BASE=$(basename -- "$SIM_FILE")
    SIM_STEM="${SIM_BASE%.*}"
    IN_PNG="${SIM_DIR}/${SIM_STEM}_bckgnd.png"
fi

if [[ ! -f "$IN_PNG" ]]; then
    echo "Error: png file not found: $IN_PNG" >&2
    exit 1
fi

if ! file "$IN_PNG" | grep -q 'PNG image data'; then
    echo "Error: $IN_PNG is not a PNG file" >&2
    exit 2
fi

if ! grep -q 'BckGndData="[0-9a-fA-F]\+"' "$SIM_FILE"; then
    echo "Error: no BckGndData attribute found in $SIM_FILE" >&2
    exit 3
fi

cp -- "$SIM_FILE" "${SIM_FILE}.bak"
echo "Backup written: ${SIM_FILE}.bak"

NEW_HEX=$(xxd -p -- "$IN_PNG" | tr -d '\n')

python3 - "$SIM_FILE" "$NEW_HEX" <<'PY'
import re, sys
sim_path, new_hex = sys.argv[1], sys.argv[2]
with open(sim_path, 'r', encoding='utf-8') as f:
    data = f.read()
new_data, n = re.subn(
    r'BckGndData="[0-9a-fA-F]+"',
    'BckGndData="' + new_hex + '"',
    data, count=1)
if n != 1:
    sys.exit("Failed to replace BckGndData (matched {} times)".format(n))
with open(sim_path, 'w', encoding='utf-8') as f:
    f.write(new_data)
PY

echo "Replaced BckGndData in: $SIM_FILE"
echo "  source PNG: $IN_PNG"
