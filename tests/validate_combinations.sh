#!/usr/bin/env bash
# Pre-release combination coverage: each individual feature (isolation,
# jobs, timeouts, parameters, capture, crashes, fixtures, --debug, each
# reporter) already has its own tests elsewhere. This script is only for
# verifying their INTERACTIONS, against tests/combo_fixture.{c,cpp} and
# tests/multitu_{c,cpp}_{a,b}.{c,cpp} — a bug specific to a combination
# (e.g. a timeout in one parallel slot blocking a sibling, or two
# concurrently-running tests' captured stdout getting cross-attributed)
# would not show up in any single-axis test. Every expectation here was
# verified empirically against a real build before being written in.
set -euo pipefail

if [ "$#" -ne 4 ]; then
  echo "usage: $0 <combo-fixture-bin> <combo-fixture-cpp-bin> <multitu-c-bin> <multitu-cpp-bin>" >&2
  exit 2
fi

COMBO="$1"
COMBO_CPP="$2"
MULTITU_C="$3"
MULTITU_CPP="$4"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# Native (non-MSYS) Windows executables can't resolve Git Bash's POSIX-style
# temp paths (e.g. /tmp/tmp.XXXX) the way bash/python can — passing one
# straight through as a --json=/--tap=/--junit= argument leaves the .exe
# unable to fopen() it, so the file is silently never written. cygpath -w
# converts to the equivalent native path (e.g. C:/Users/.../Temp/tmp.XXXX)
# for arguments the .exe itself must resolve; every subsequent read of
# $WORKDIR below (via python/xmllint) keeps the original POSIX path.
native_path() {
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -w "$1"
  else
    printf '%s' "$1"
  fi
}

status=0
check_num=0

# check DESC CONDITION_DESCRIBING_COMMAND...
# Records pass/fail for a boolean shell condition, uniformly, so every check
# below (however it computes its boolean) reports consistently.
ok() {
  check_num=$((check_num + 1))
  echo "OK   [$1]"
}
fail() {
  check_num=$((check_num + 1))
  echo "FAIL [$1]: $2" >&2
  status=1
}

json_outcomes() {
  # Prints "name outcome" for every test in a triax JSON report's first suite.
  python3 -c "
import json, sys
sys.stdout.reconfigure(newline='\n')
d = json.load(open(sys.argv[1]))
for t in d['suites'][0]['tests']:
    print(t['name'], t['outcome'])
" "$(native_path "$1")"
}

json_outcomes_by_invocation() {
  # Prints "invocation outcome" for every invocation in a JSON report's first
  # (only) test — for parameterized-test checks, where name is shared.
  python3 -c "
import json, sys
sys.stdout.reconfigure(newline='\n')
d = json.load(open(sys.argv[1]))
for t in d['suites'][0]['tests']:
    print(t['invocation'], t['outcome'])
" "$(native_path "$1")"
}

json_capture() {
  # Prints "name stdout stderr" for every test in a JSON report's first suite.
  python3 -c "
import json, sys
sys.stdout.reconfigure(newline='\n')
d = json.load(open(sys.argv[1]))
for t in d['suites'][0]['tests']:
    print(t['name'], t.get('stdout', ''), t.get('stderr', ''))
" "$(native_path "$1")"
}

json_phase_outcomes() {
  # Prints "name outcome phase-or-dash" for every test.
  python3 -c "
import json, sys
sys.stdout.reconfigure(newline='\n')
d = json.load(open(sys.argv[1]))
for t in d['suites'][0]['tests']:
    print(t['name'], t['outcome'], t.get('phase', '-'))
" "$(native_path "$1")"
}

# check_map DESC ACTUAL_TEXT EXPECTED_TEXT
# Both are newline-separated "key value..." records; compared as sorted sets
# so invocation/scheduling order under parallel jobs doesn't matter.
check_map() {
  local desc="$1" actual="$2" expected="$3"
  local sorted_actual sorted_expected
  # Strip \r unconditionally: native python3 on Windows opens stdout in text
  # mode (\n -> \r\n), and bash's $(...) only strips the final trailing
  # newline, not embedded \r — leaving "actual" and "expected" render
  # identically in a log but compare byte-unequal.
  sorted_actual="$(echo "$actual" | tr -d '\r' | sort)"
  sorted_expected="$(echo "$expected" | tr -d '\r' | sort)"
  if [ "$sorted_actual" = "$sorted_expected" ]; then
    ok "$desc"
  else
    fail "$desc" "expected:
$sorted_expected
got:
$sorted_actual"
  fi
}

# ── isolation x jobs ─────────────────────────────────────────────────────────
# Fixed mix of pass/fail/crash, all isolated, forced into overlapping
# concurrent batches by --jobs=4 (8 tests, 4 slots -> 2 batches).
"$COMBO" --no-color --jobs=4 --json="$(native_path "$WORKDIR/isojobs.json")" combo_isojobs >/dev/null 2>&1 || true
check_map "isolation x jobs: all 8 concurrent isolated outcomes correct" \
  "$(json_outcomes "$WORKDIR/isojobs.json")" \
  "pass1 passed
fail1 failed
crash1 ucrashed
pass2 passed
fail2 failed
pass3 passed
crash2 ucrashed
pass4 passed"

# ── timeouts x jobs ──────────────────────────────────────────────────────────
# One short-timeout/long-sleep test alongside three measurably-slow-but-
# passing ones, all under --jobs=4 (one batch). If the timeout ever blocked
# its slot instead of being reaped concurrently, wall time would grow with
# the sum (300 + 3*200 = 900ms) instead of staying near the max (~300ms).
start_ms=$(($(date +%s%N) / 1000000))
"$COMBO" --no-color --jobs=4 --json="$(native_path "$WORKDIR/timeoutjobs.json")" combo_timeoutjobs \
  >/dev/null 2>&1 || true
end_ms=$(($(date +%s%N) / 1000000))
elapsed=$((end_ms - start_ms))
check_map "timeout x jobs: outcomes correct" \
  "$(json_outcomes "$WORKDIR/timeoutjobs.json")" \
  "slow_timeout timeout
fast1 passed
fast2 passed
fast3 passed"
if [ "$elapsed" -lt 600 ]; then
  ok "timeout x jobs: total time (${elapsed}ms) shows concurrent execution, not serialized (<600ms)"
else
  fail "timeout x jobs" "took ${elapsed}ms, expected well under 600ms if slots run concurrently"
fi

# ── parameters x jobs ────────────────────────────────────────────────────────
# One 8-element parameterized test, run under --jobs=4 so invocations
# execute concurrently across slots — checks each invocation's own
# parameter/outcome isn't swapped with a concurrently-running sibling.
"$COMBO" --no-color --jobs=4 --json="$(native_path "$WORKDIR/paramjobs.json")" combo_paramjobs \
  >/dev/null 2>&1 || true
check_map "parameters x jobs: even indices pass, odd fail, none swapped" \
  "$(json_outcomes_by_invocation "$WORKDIR/paramjobs.json")" \
  "0 passed
1 failed
2 passed
3 failed
4 passed
5 failed
6 passed
7 failed"

# ── capture x jobs ───────────────────────────────────────────────────────────
# Several concurrently-running tests each print a marker unique to
# themselves — checks per-slot capture isn't shared/cross-contaminated.
"$COMBO" --no-color --jobs=4 --json="$(native_path "$WORKDIR/capturejobs.json")" combo_capturejobs \
  >/dev/null 2>&1 || true
check_map "capture x jobs: each test's captured output is only its own" \
  "$(json_capture "$WORKDIR/capturejobs.json")" \
  "out1 MARKER-OUT-1 MARKER-ERR-1
out2 MARKER-OUT-2 MARKER-ERR-2
out3 MARKER-OUT-3 MARKER-ERR-3
out4 MARKER-OUT-4 MARKER-ERR-4"

# ── crashes x jobs ───────────────────────────────────────────────────────────
# Several concurrently-crashing tests (all SIGABRT — see combo_fixture.c's
# comment on why SIGFPE/SIGSEGV aren't portable here) — checks each crashing
# slot's own outcome isn't cross-attributed to a sibling crashing nearby.
"$COMBO" --no-color --jobs=5 --json="$(native_path "$WORKDIR/crashjobs.json")" combo_crashjobs \
  >/dev/null 2>&1 || true
check_map "crashes x jobs: each concurrent crash classified independently" \
  "$(json_outcomes "$WORKDIR/crashjobs.json")" \
  "c_abort1 ucrashed
c_abort2 ucrashed
c_abort3 ucrashed
c_pass passed
c_fail failed"

# ── fixtures x crashes/errors ────────────────────────────────────────────────
# A hardware crash (not a triaxi_user_error) in .init and in .fini, a
# sibling registered afterward in the same suite proving a fixture crash
# doesn't corrupt/hang the suite for later tests, and a timeout occurring in
# .init/.fini specifically (triaxi_test_interpret's state_hdr SETUP/INIT/
# FINI/CLEANUP branch already special-cases TIMEOUT, but nothing exercised
# that combination before — every other timeout test times out from body
# code, not a fixture). Timeout's JSON "phase" field was added alongside
# this test (previously only ERROR outcomes got one), so this also locks
# that reporting extension in.
"$COMBO" --no-color --json="$(native_path "$WORKDIR/fixcrash.json")" combo_fixture_crash >/dev/null 2>&1 || true
check_map "fixtures x crashes/timeouts: correctly classified, sibling still runs" \
  "$(json_phase_outcomes "$WORKDIR/fixcrash.json")" \
  "crash_in_init test_error init
crash_in_fini test_error fini
sibling_runs_fine passed -
timeout_in_init timeout init
timeout_in_fini timeout fini"

# ── debug x assertion types ──────────────────────────────────────────────────
# --debug forces isolation off (and implies --break) — checked against a
# representative spread of assertion kinds, each filtered individually so
# --debug's forced njobs=1 doesn't serialize them together.
check_debug_case() {
  local name="$1" expected_outcome="$2" expected_reason="$3"
  "$COMBO" --no-color --debug --json="$(native_path "$WORKDIR/debug_$name.json")" "combo_debug::$name" \
    >/dev/null 2>&1 || true
  local result
  # sys.argv[1], not string-interpolated into the source: a path baked into
  # a `-c "..."` script string is just a substring of one big argument to
  # MSYS/Git Bash, so its automatic POSIX->native path translation (which
  # only rewrites whole argv[] entries) never sees it — native Windows
  # python3 would get a literal, untranslated /tmp/... it can't open.
  result="$(python3 - "$(native_path "$WORKDIR/debug_$name.json")" <<'PY' | tr -d '\r'
import json
import sys

sys.stdout.reconfigure(newline="\n")
with open(sys.argv[1]) as f:
    d = json.load(f)

t = d["suites"][0]["tests"][0]
print(t["outcome"], t.get("termination", {}).get("reason", "-"))
PY
)"
  local outcome="${result%% *}"
  if [ "$outcome" = "$expected_outcome" ] && {
    [ "$expected_reason" = "-" ] || [ "${result#* }" = "$expected_reason" ]
  }; then
    ok "debug x assertion types: $name -> $result"
  else
    fail "debug x assertion types: $name" "expected outcome '$expected_outcome' (reason '$expected_reason'), got '$result'"
  fi
}
check_debug_case plain_pass passed -
# Every failing assertion under --debug also breaks (--debug implies
# --break); with no debugger attached that's a controlled "ucrashed", not a
# plain "failed" — see triaxi_log_res's state reset and the state==TEST
# exit.type switch in triaxi_test_interpret.
check_debug_case plain_fail ucrashed -
# assert_fault needs no isolation (the crash is caught mid-predicate via the
# in-process handler regardless), so it works the same with or without
# --debug.
check_debug_case fault_pass passed -
check_debug_case nfault_pass passed -
# assert_exit *does* need isolation — --debug forcing it off should still be
# reported as the same TRIAXI_ERROR_EXIT_ASSERT_WITHOUT_ISOLATION as a plain
# --isolation=off override (checked elsewhere in tests/c/errors.c), via the
# different code path --debug takes to force that setting.
check_debug_case exit_needs_isolation test_error exit_assert_without_isolation

# ── multi-TU x C/C++ ─────────────────────────────────────────────────────────
# One suite's tests registered from two different translation units —
# checks the runner aggregates cross-TU registrations correctly, for both
# languages.
for pair in "C:$MULTITU_C:multitu" "C++:$MULTITU_CPP:multitu_cpp"; do
  lang="${pair%%:*}"
  rest="${pair#*:}"
  bin="${rest%%:*}"
  suite="${rest#*:}"
  listing="$("$bin" --list | tr -d '\r' | sort | tr '\n' ',')"
  expected_listing="$(printf '%s::from_a_pass\n%s::from_b_fail\n%s::from_b_pass\n' \
    "$suite" "$suite" "$suite" | sort | tr '\n' ',')"
  if [ "$listing" = "$expected_listing" ]; then
    ok "multi-TU x $lang: cross-TU registrations merged into one suite"
  else
    fail "multi-TU x $lang: --list" "expected '$expected_listing', got '$listing'"
  fi

  "$bin" --no-color --json="$(native_path "$WORKDIR/multitu_$lang.json")" >/dev/null 2>&1 || true
  check_map "multi-TU x $lang: outcomes from both TUs correct" \
    "$(json_outcomes "$WORKDIR/multitu_$lang.json")" \
    "from_a_pass passed
from_b_pass passed
from_b_fail failed"
done

# ── all reporters x every outcome ───────────────────────────────────────────
# One test per outcome (7 of 8; uexception is C++-only, checked separately
# against combo_reporters_cpp below), run together so a single json/tap/
# junit invocation can be checked against all of them at once. This checks
# CORRECTNESS of each format's encoding, not just well-formedness (that's
# validate_formats.sh's job).

"$COMBO" --text=none --no-color --json="$(native_path "$WORKDIR/rep.json")" \
  --tap="$(native_path "$WORKDIR/rep.tap")" --junit="$(native_path "$WORKDIR/rep.xml")" \
  combo_reporters >/dev/null 2>&1 || true

check_map "reporters x outcome: JSON \"outcome\" correct for all 7" \
  "$(json_outcomes "$WORKDIR/rep.json")" \
  "r_passed passed
r_failed failed
r_skipped skipped
r_timeout timeout
r_ucrashed ucrashed
r_uexited uexited
r_test_error test_error"

tap_result="$(python3 - "$(native_path "$WORKDIR/rep.tap")" <<'PY' | tr -d '\r'
import re
import sys

sys.stdout.reconfigure(newline="\n")
lines = open(sys.argv[1]).read().splitlines()
out = []
for l in lines:
    m = re.match(r'(not )?ok \d+ - \S+ > (\S+)(.*)', l)
    if not m:
        continue
    status = "not_ok" if m.group(1) else "ok"
    skip = "SKIP" in m.group(3)
    out.append(f'{m.group(2)} {status} {skip}')
print("\n".join(sorted(out)))
PY
)"
expected_tap="$(printf '%s\n' \
  "r_passed ok False" \
  "r_failed not_ok False" \
  "r_skipped ok True" \
  "r_timeout not_ok False" \
  "r_ucrashed not_ok False" \
  "r_uexited not_ok False" \
  "r_test_error not_ok False" | sort)"
if [ "$tap_result" = "$expected_tap" ]; then
  ok "reporters x outcome: TAP ok/not-ok/SKIP correct for all 7"
else
  fail "reporters x outcome: TAP" "expected:
$expected_tap
got:
$tap_result"
fi

junit_result="$(python3 - "$(native_path "$WORKDIR/rep.xml")" <<'PY' | tr -d '\r'
import sys
import xml.etree.ElementTree as ET

sys.stdout.reconfigure(newline="\n")
root = ET.parse(sys.argv[1]).getroot()
out = []
for tc in root.iter("testcase"):
    name = tc.get("name")
    failure = tc.find("failure")
    error = tc.find("error")
    skipped = tc.find("skipped")
    if failure is not None:
        out.append(f'{name} failure {failure.get("type")}')
    elif error is not None:
        out.append(f'{name} error {error.get("type")}')
    elif skipped is not None:
        out.append(f'{name} skipped -')
    else:
        out.append(f'{name} none -')
print("\n".join(sorted(out)))
PY
)"
expected_junit="$(printf '%s\n' \
  "r_passed none -" \
  "r_failed failure failed" \
  "r_skipped skipped -" \
  "r_timeout error timeout" \
  "r_ucrashed error ucrashed" \
  "r_uexited error uexited" \
  "r_test_error error test_error" | sort)"
if [ "$junit_result" = "$expected_junit" ]; then
  ok "reporters x outcome: JUnit failure/error/skipped elements correct for all 7"
else
  fail "reporters x outcome: JUnit" "expected:
$expected_junit
got:
$junit_result"
fi

# uexception (C++-only) against combo_reporters_cpp, same treatment.
"$COMBO_CPP" --text=none --no-color --json="$(native_path "$WORKDIR/rep_cpp.json")" \
  --tap="$(native_path "$WORKDIR/rep_cpp.tap")" --junit="$(native_path "$WORKDIR/rep_cpp.xml")" \
  >/dev/null 2>&1 || true
check_map "reporters x outcome: uexception (C++) JSON correct" \
  "$(json_outcomes "$WORKDIR/rep_cpp.json")" \
  "r_uexception uexception"
if grep -q "^not ok 1 - combo_reporters_cpp > r_uexception" "$WORKDIR/rep_cpp.tap"; then
  ok "reporters x outcome: uexception (C++) TAP correct"
else
  fail "reporters x outcome: uexception (C++) TAP" "$(cat "$WORKDIR/rep_cpp.tap")"
fi
if python3 - "$(native_path "$WORKDIR/rep_cpp.xml")" <<'PY'
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
tc = next(root.iter("testcase"))
error = tc.find("error")
sys.exit(0 if error is not None and error.get("type") == "uexception" else 1)
PY
then
  ok "reporters x outcome: uexception (C++) JUnit correct"
else
  fail "reporters x outcome: uexception (C++) JUnit" "$(cat "$WORKDIR/rep_cpp.xml")"
fi

echo
echo "$check_num checks run"
exit "$status"
