#!/usr/bin/env bash
# Validates CLI-level behavior (--help, --list, --tags, --fail-fast, and
# argument-error handling) against tests/cli_fixture.c — a small, stable
# roster dedicated to this, since triax-selftest-{c,cpp}'s roster is
# expected to keep growing, which would make exact fail-fast-ordering and
# --list-output assertions fragile and unrelated to whatever else changed
# there. Every expectation here was verified empirically against a real
# build before being written into this script, not assumed from reading the
# source.
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <path-to-cli-fixture-binary>" >&2
  exit 2
fi

BIN="$1"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# Native (non-MSYS) Windows executables can't resolve Git Bash's POSIX-style
# temp paths (e.g. /tmp/tmp.XXXX) the way bash/python can — passing one
# straight through as a --json= argument leaves the .exe unable to fopen()
# it, so the file is silently never written. cygpath -w converts to the
# equivalent native path (e.g. C:/Users/.../Temp/tmp.XXXX) for arguments the
# .exe itself must resolve; every subsequent read of $WORKDIR below (via
# json_test_names/json_outcome_and_reason) keeps the original POSIX path.
native_path() {
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -w "$1"
  else
    printf '%s' "$1"
  fi
}

# Native python3 on Windows opens stdout in text mode, translating \n to
# \r\n; embedded \r survives bash's $(...) (which only strips the final
# trailing newline) and breaks exact-text comparisons against plain-LF
# strings even though both sides render identically in a log.
normalize_lf() {
  tr -d '\r'
}

status=0
check_num=0

# check DESC EXPECTED_EXIT ACTUAL_EXIT [grep-pattern] [file-to-grep]
check() {
  local desc="$1" expected_exit="$2" actual_exit="$3"
  check_num=$((check_num + 1))
  if [ "$expected_exit" != "$actual_exit" ]; then
    echo "FAIL [$desc]: expected exit $expected_exit, got $actual_exit" >&2
    status=1
    return
  fi
  if [ "$#" -ge 5 ]; then
    local pattern="$4" file="$5"
    if ! grep -q -- "$pattern" "$file"; then
      echo "FAIL [$desc]: expected output to contain '$pattern'" >&2
      echo "--- actual output ---" >&2
      cat "$file" >&2
      status=1
      return
    fi
  fi
  echo "OK   [$desc]"
}

run() {
  # Runs $BIN with the given args, capturing combined output and exit code
  # into $WORKDIR/out and returning the exit code via $LAST_EXIT. The
  # `|| LAST_EXIT=$?` form (not a bare `$?` on the next line) is required
  # under `set -e`: a failing command triggers errexit immediately, before
  # a subsequent assignment would ever run.
  LAST_EXIT=0
  "$BIN" "$@" >"$WORKDIR/out" 2>&1 || LAST_EXIT=$?
}

json_test_names() {
  # Prints "suite::test" for every test entry in a triax JSON report.
  python3 -c "
import json
d = json.load(open('$1'))
for s in d['suites']:
    for t in s['tests']:
        print(f\"{s['name']}::{t['name']}\")
" | normalize_lf
}

json_outcome_and_reason() {
  # Prints "<outcome> <reason-or-->" for the first test entry in a triax JSON
  # report. Used for fx_c::malformed_params below, where the interesting
  # part is the specific TRIAXI_Error, not just pass/fail.
  python3 -c "
import json
d = json.load(open('$1'))
t = d['suites'][0]['tests'][0]
print(t['outcome'], t.get('termination', {}).get('reason', '-'))
" | normalize_lf
}

# ── --help / --list ─────────────────────────────────────────────────────────

run --help
check "help exits 0" 0 "$LAST_EXIT" "Usage: triax" "$WORKDIR/out"

run -h
check "-h exits 0" 0 "$LAST_EXIT" "Usage: triax" "$WORKDIR/out"

run --list
check "list exits 0" 0 "$LAST_EXIT" "fx_a::one" "$WORKDIR/out"
list_output="$(normalize_lf < "$WORKDIR/out")"
if [ "$list_output" != "$(printf 'fx_a::one\nfx_a::two\nfx_b::first\nfx_b::second\nfx_c::malformed_params')" ]; then
  echo "FAIL [list output]: unexpected content:" >&2
  echo "$list_output" >&2
  status=1
else
  echo "OK   [list output matches exactly]"
fi

run -l
check "-l exits 0" 0 "$LAST_EXIT" "fx_a::two" "$WORKDIR/out"

# ── --tags ───────────────────────────────────────────────────────────────────

run --tags=alpha --no-color --json="$(native_path "$WORKDIR/tags_alpha.json")"
names="$(json_test_names "$WORKDIR/tags_alpha.json" | sort | tr '\n' ',')"
if [ "$names" = "fx_a::one,fx_a::two," ]; then
  echo "OK   [--tags=alpha selects fx_a::one, fx_a::two (suite-level tag)]"
else
  echo "FAIL [--tags=alpha]: got '$names'" >&2
  status=1
fi

run --tags=beta --no-color --json="$(native_path "$WORKDIR/tags_beta.json")"
names="$(json_test_names "$WORKDIR/tags_beta.json" | sort | tr '\n' ',')"
if [ "$names" = "fx_a::two," ]; then
  echo "OK   [--tags=beta selects only fx_a::two (test-level tag)]"
else
  echo "FAIL [--tags=beta]: got '$names'" >&2
  status=1
fi

run --tags=nonexistent --no-color --json="$(native_path "$WORKDIR/tags_none.json")"
names="$(json_test_names "$WORKDIR/tags_none.json" | tr '\n' ',')"
if [ -z "$names" ]; then
  echo "OK   [--tags=nonexistent selects nothing]"
else
  echo "FAIL [--tags=nonexistent]: got '$names'" >&2
  status=1
fi

# ── --fail-fast ──────────────────────────────────────────────────────────────

run fx_b --fail-fast --no-color --json="$(native_path "$WORKDIR/failfast.json")"
names="$(json_test_names "$WORKDIR/failfast.json" | tr '\n' ',')"
if [ "$names" = "fx_b::first," ]; then
  echo "OK   [--fail-fast stops after fx_b::first fails, never launches fx_b::second]"
else
  echo "FAIL [--fail-fast]: got '$names'" >&2
  status=1
fi

# ── --break / --debug ────────────────────────────────────────────────────────

# fx_b::first fails a plain assertion. With --break, the framework raises
# SIGTRAP right after logging that failure; with no debugger attached, that
# should be reported as a clean "ucrashed" outcome (breakpoint fault) rather
# than crashing the runner itself with an internal consistency-check error —
# this exact combination previously hit a triaxi_fatal() (see triax.h's
# triaxi_log_res: the signal used to interrupt state being reset back to
# TRIAXI_STATE_TEST after the result was already logged).
run fx_b::first --break --no-color --json="$(native_path "$WORKDIR/break.json")"
check "--break on a failing assertion exits nonzero" 1 "$LAST_EXIT"
result="$(json_outcome_and_reason "$WORKDIR/break.json")"
# The exact signal description (2nd word onward) is platform-specific
# (e.g. "Trace/BPT trap: 5" on macOS vs "Trace/breakpoint trap" on Linux) —
# only the outcome itself is checked here.
if [ "${result%% *}" = "ucrashed" ]; then
  echo "OK   [--break on a failing assertion reports ucrashed, not an internal error]"
else
  echo "FAIL [--break]: got '$result'" >&2
  status=1
fi

# --debug forces isolation off, timeout off, and implies --break — same
# underlying signal-timing hazard, checked independently since it goes
# through triaxi_run_ctx_make's forced settings rather than a plain
# --isolation=off override.
run fx_b::first --debug --no-color --json="$(native_path "$WORKDIR/debug.json")"
check "--debug on a failing assertion exits nonzero" 1 "$LAST_EXIT"
result="$(json_outcome_and_reason "$WORKDIR/debug.json")"
if [ "${result%% *}" = "ucrashed" ]; then
  echo "OK   [--debug on a failing assertion reports ucrashed, not an internal error]"
else
  echo "FAIL [--debug]: got '$result'" >&2
  status=1
fi

# ── invalid per-test configuration (TRIAXI_Error) ───────────────────────────

# fx_c::malformed_params is rejected before its body ever dispatches, for
# every invocation — including this direct, unfiltered-of-other-tests one —
# so (unlike everything triaxi_validate wraps in tests/c/errors.c) there's no
# safe way to exercise it from inside the same process that also needs to
# report an overall pass. Checked here instead, from outside the process.
run fx_c::malformed_params --no-color --json="$(native_path "$WORKDIR/malformed.json")"
check "fx_c::malformed_params exits nonzero" 1 "$LAST_EXIT"
result="$(json_outcome_and_reason "$WORKDIR/malformed.json")"
if [ "$result" = "test_error malformed_params" ]; then
  echo "OK   [fx_c::malformed_params reports test_error/malformed_params]"
else
  echo "FAIL [fx_c::malformed_params]: got '$result'" >&2
  status=1
fi

# ── CLI error handling ───────────────────────────────────────────────────────

run --bogus-flag
check "unknown flag" 2 "$LAST_EXIT" "unrecognized flag" "$WORKDIR/out"

run --isolation=maybe
check "invalid --isolation value" 2 "$LAST_EXIT" "invalid value" "$WORKDIR/out"

run --verbosity=loud
check "invalid --verbosity value" 2 "$LAST_EXIT" "invalid value" "$WORKDIR/out"

run --timeout=notanumber
check "invalid --timeout value" 2 "$LAST_EXIT" "invalid value" "$WORKDIR/out"

run --jobs=999
check "--jobs exceeding max" 2 "$LAST_EXIT" "invalid value" "$WORKDIR/out"

run --json=a.json --json=b.json
check "duplicate output stream" 2 "$LAST_EXIT" "set more than once" "$WORKDIR/out"

# shellcheck disable=SC2046 # word-splitting is intentional: 256 distinct args
run $(python3 -c "print(' '.join(f'f{i}' for i in range(256)))")
check "too many filters (256 > max 255)" 2 "$LAST_EXIT" "Too many filter" "$WORKDIR/out"

echo
echo "$check_num checks run"
exit "$status"
