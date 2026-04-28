#!/usr/bin/env bash
#
# fetch-components.sh
#
# Downloads SimulIDE's component catalog (components.txt) and every
# referenced component-set zip into resources/data/components/.
#
# By default the zips are kept as-is (the in-app Installer will extract
# them on demand at first run).
#
# Pass --extract (or -e) to extract each zip into a sibling folder and
# delete the zip. With this option the WASM build ships every component
# set already "installed" — no in-app install step needed.
#
# Usage:
#   ./scripts/fetch-components.sh           # download only
#   ./scripts/fetch-components.sh -e        # download + extract + remove zips
#   ./scripts/fetch-components.sh --extract
#
# Re-run whenever components.txt changes upstream (e.g. new MCU set
# added) to keep the bundled catalog in sync.

set -euo pipefail

EXTRACT=0
for arg in "$@"; do
    case "$arg" in
        -e|--extract) EXTRACT=1 ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *)
            echo "Unknown argument: $arg (use -h for help)" >&2
            exit 2 ;;
    esac
done

URL_BASE="https://simulide.com/p/direct_downloads/components"

# Resolve repo root from this script's location, regardless of where it's invoked.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST="$REPO_ROOT/resources/data/components"

if (( EXTRACT )); then
    command -v unzip >/dev/null || {
        echo "ERROR: 'unzip' is required for --extract (apt install unzip / brew install unzip)" >&2
        exit 1
    }
fi

mkdir -p "$DEST"
cd "$DEST"

echo "Fetching component catalog into $DEST"

# 1. Catalog
echo "  components.txt"
curl -fsSL --max-time 30 "$URL_BASE/dloadset.php?file=components.txt" -o components.txt

# 2. Every zip referenced in the catalog. Each line is
#    "<name>; <description>; <zipfile>; <version>; [deps]"
#    or a section header (e.g. "MCUs and CPUs:").
zips=$(awk -F';' '/\.zip/ { gsub(/^[ \t]+|[ \t]+$/, "", $3); print $3 }' components.txt)

if [[ -z "$zips" ]]; then
    echo "ERROR: no .zip entries found in components.txt — aborting." >&2
    exit 1
fi

for zip in $zips; do
    echo "  $zip"
    curl -fsSL --max-time 120 "$URL_BASE/dloadset.php?file=$zip" -o "$zip"

    if (( EXTRACT )); then
        # The zip's top-level entry is the set name folder
        # (e.g. Arduino.zip -> Arduino/). Overwrite any existing extraction.
        set_name="${zip%.zip}"
        rm -rf "$set_name"
        unzip -q -o "$zip" -d .
        rm -f "$zip"
    fi
done

echo
if (( EXTRACT )); then
    echo "Done. Extracted component sets in $DEST:"
    ls -d */ 2>/dev/null || echo "  (no folders extracted!)"
else
    echo "Done. Zips in $DEST (re-run with --extract to expand them):"
    ls -1 *.zip 2>/dev/null || echo "  (no zips found!)"
fi
