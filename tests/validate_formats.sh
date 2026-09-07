#!/usr/bin/env bash
# Validates that a triax-based binary's JSON, TAP, and JUnit XML output are
# well-formed, by feeding each through a real parser for that format rather
# than hand-rolling one. A nonzero exit from the binary under test is
# expected and ignored — only output *shape* is checked here, not outcome
# counts (triaxi_validate, in triax_selfverify.h, is what checks outcomes).
#
# Since every test here is wrapped in triaxi_validate, a normal (outer) run
# reports almost entirely "passed" outcomes at the top level — not much
# variety to validate formatting against. Passing a filter as the second
# argument instead invokes the binary in "child" mode (TRIAXI_VALIDATE_CHILD)
# against that filter, running the *subject* bodies directly — the same
# mechanism triaxi_validate uses internally — to get a real mix of
# pass/fail/crash/timeout content worth checking JSON/TAP/JUnit escaping
# against.
#
# Important: triaxi_validate_opts's per-test timeout/isolation are CLI
# overrides applied only by triaxi_validate_spawn's own child invocation
# (see triax_selfverify.h) — they are deliberately NOT compiled into the
# test's own attrs, so a filtered test run directly like this (bypassing
# that spawn) has no timeout at all unless one is supplied here too. Some
# filtered suites contain tests that sleep for 100+ seconds relying entirely
# on that external timeout, so --timeout/--isolation and a hard `timeout`
# wrapper are both required in child mode, not just a nicety.
set -euo pipefail

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  echo "usage: $0 <path-to-triax-binary> [filter]" >&2
  exit 2
fi

BIN="$1"
FILTER="${2:-}"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# Native (non-MSYS) Windows executables can't resolve Git Bash's POSIX-style
# temp paths (e.g. /tmp/tmp.XXXX) the way bash/python/xmllint can — passing
# one straight through as a --json=/--tap=/--junit= argument leaves the .exe
# unable to fopen() it, so the file is silently never written. cygpath -w
# converts to the equivalent native path (e.g. C:/Users/.../Temp/tmp.XXXX)
# for arguments the .exe itself must resolve; every other use of $WORKDIR
# below (reading the files back via python/xmllint/awk) keeps the original
# POSIX path.
native_path() {
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -w "$1"
  else
    printf '%s' "$1"
  fi
}
NATIVE_JSON="$(native_path "$WORKDIR/out.json")"
NATIVE_TAP="$(native_path "$WORKDIR/out.tap")"
NATIVE_XML="$(native_path "$WORKDIR/out.xml")"

# Portable watchdog (no dependency on GNU coreutils' `timeout`, which isn't
# guaranteed present on macOS/Windows CI runners): background the command,
# race it against a sleep, kill whichever loses.
run_with_watchdog() {
  local secs="$1"
  shift
  "$@" &
  local pid=$!
  (sleep "$secs" && kill -9 "$pid") >/dev/null 2>&1 &
  local watchdog=$!
  local status=0
  wait "$pid" 2>/dev/null || status=$?
  kill "$watchdog" >/dev/null 2>&1 || true
  wait "$watchdog" 2>/dev/null || true
  return "$status"
}

if [ -n "$FILTER" ]; then
  TRIAXI_VALIDATE_CHILD=1 run_with_watchdog 30 "$BIN" --text=none --json="$NATIVE_JSON" \
    --tap="$NATIVE_TAP" --junit="$NATIVE_XML" --timeout=500 --isolation=on \
    "$FILTER" >/dev/null 2>&1 || true
else
  run_with_watchdog 30 "$BIN" --text=none --json="$NATIVE_JSON" --tap="$NATIVE_TAP" \
    --junit="$NATIVE_XML" >/dev/null 2>&1 || true
fi

status=0

echo "== JSON =="
if ! python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$WORKDIR/out.json"; then
  echo "FAIL: $WORKDIR/out.json is not valid JSON" >&2
  status=1
else
  echo "OK"
fi

echo "== JUnit XML =="
if ! xmllint --noout "$WORKDIR/out.xml"; then
  echo "FAIL: $WORKDIR/out.xml is not well-formed XML" >&2
  status=1
else
  echo "OK"
fi

echo "== TAP =="
# TAP13: a run of "ok"/"not ok" lines (comments and blank lines allowed
# anywhere), with the "N..M" plan line either leading or trailing (triax
# emits it trailing).
if ! awk '
  /^#/ || /^[[:space:]]*$/ { next }
  /^[0-9]+\.\.[0-9]+$/ { plan++; next }
  /^(not )?ok[ \t]/ { tests++; next }
  { print "malformed TAP line " NR ": " $0; bad=1 }
  END {
    if (bad) exit 1
    if (plan != 1) { print "expected exactly one plan line, found " plan+0; exit 1 }
    if (tests < 1) { print "no ok/not ok lines found"; exit 1 }
  }
' "$WORKDIR/out.tap"; then
  echo "FAIL: $WORKDIR/out.tap is not valid TAP" >&2
  status=1
else
  echo "OK"
fi

exit "$status"
