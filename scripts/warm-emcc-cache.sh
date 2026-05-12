#!/usr/bin/env bash
# Pre-populate emscripten's struct-info cache without pthread flags.
#
# Why: emcc 1.39.8's cache-regeneration probe is built with whatever
# EMCC_CFLAGS/EMCC_CXXFLAGS are in the environment. With USE_PTHREADS=1
# the probe references the PThread runtime at top level, which is
# undefined under bare Node 18+ and throws ReferenceError. Stripping
# the pthread flag for the probe sidesteps the bug; the cached JSON it
# produces is identical and gets reused by the real (pthreaded) build.
#
# Run this once after any cache wipe / fresh emsdk install. Idempotent:
# if the cache is already populated, the trivial compile is a no-op.
#
# Requires emsdk activated and emsdk_env.sh sourced in the current shell.

set -euo pipefail

if ! command -v emcc >/dev/null 2>&1; then
    echo "ERROR: emcc not on PATH — source <emsdk>/emsdk_env.sh first" >&2
    exit 1
fi

SAVED_CFLAGS="${EMCC_CFLAGS:-}"
SAVED_CXXFLAGS="${EMCC_CXXFLAGS:-}"
unset EMCC_CFLAGS EMCC_CXXFLAGS

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

echo 'int main(){return 0;}' > "$TMPDIR/probe.c"
emcc "$TMPDIR/probe.c" -o "$TMPDIR/probe.js" >/dev/null

export EMCC_CFLAGS="$SAVED_CFLAGS"
export EMCC_CXXFLAGS="$SAVED_CXXFLAGS"

echo "emcc cache warmed under Node $("${NODE_JS:-node}" --version 2>/dev/null || echo unknown)"
