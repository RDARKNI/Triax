#!/usr/bin/env bash
# Validates that a triax-based binary's JSON, TAP, and JUnit XML output are
# well-formed. JSON and XML are fed through real parsers, while TAP is
# structurally validated against the subset emitted by Triax. A nonzero exit
# from the binary under test is expected and ignored — only output shape is
# checked here, not outcome correctness.
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
# Fail if the script stops before its final exit, e.g. on a bash parse error.
completed=0
trap 'rm -rf "$WORKDIR"; [ "$completed" = 1 ] || { echo "FAIL: script ended early" >&2; exit 70; }' EXIT

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
if ! python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$(native_path "$WORKDIR/out.json")"; then
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
  BEGIN {
    plan_count = 0
    tests = 0
    expected_num = 1
    plan_start = -1
    plan_end = -1
  }

  /^[[:space:]]*$/ { next }
  /^#/ { next }

  /^[0-9]+\.\.[0-9]+([[:space:]]*#.*)?$/ {
    if (++plan_count > 1) {
      print "multiple TAP plan lines"
      bad = 1
      next
    }

    line = $0
    sub(/[[:space:]]*#.*/, "", line)
    split(line, p, /\.\./)

    plan_start = p[1] + 0
    plan_end   = p[2] + 0

    if (plan_start != 1) {
      print "plan must start at 1, got " plan_start
      bad = 1
    }

    if (plan_end < plan_start) {
      print "invalid TAP plan: " plan_start ".." plan_end
      bad = 1
    }

    next
  }

  /^(not )?ok[[:space:]]+[0-9]+[[:space:]]+-[[:space:]].+$/ {
    line = $0
    sub(/^(not )?ok[[:space:]]+/, "", line)

    split(line, f, /[[:space:]]+/)
    num = f[1] + 0

    if (num != expected_num) {
      print "expected test number " expected_num ", got " num
      bad = 1
    }

    expected_num++
    tests++
    next
  }

  {
    print "malformed TAP line " NR ": " $0
    bad = 1
  }

  END {
    if (plan_count != 1) {
      print "expected exactly one plan line, found " plan_count
      bad = 1
    }

    if (tests < 1) {
      print "no ok/not ok lines found"
      bad = 1
    }

    if (plan_count == 1) {
      expected_tests = plan_end - plan_start + 1
      if (tests != expected_tests) {
        print "plan says " expected_tests " tests, but found " tests
        bad = 1
      }
    }

    if (bad)
      exit 1
  }
' "$WORKDIR/out.tap"; then
  echo "FAIL: $WORKDIR/out.tap is not valid TAP" >&2
  status=1
else
  echo "OK"
fi

# Check operand semantics as well as output syntax, for both C and C++ runners.
echo "== Actual/expected operand order =="
if ! python3 - "$(native_path "$BIN")" "$(native_path "$WORKDIR/order.json")" <<'PY_ORDER'
import json, os, subprocess, sys
binary, report = sys.argv[1:]
run = subprocess.run([binary, '--no-color', '--text=stdout', '--json=' + report,
                      '--isolation=on', 'operand_order'],
                     env={**os.environ, 'TRIAXI_VALIDATE_CHILD': '1'},
                     capture_output=True, text=True, timeout=30)
assert run.returncode == 1, (run.returncode, run.stderr)
tests = {t['name']: t for s in json.load(open(report))['suites'] for t in s['tests']}
assert set(tests) == {'expectations', 'fatal'}, tests
failures = tests['expectations']['assert_failures']
assert tests['expectations']['outcome'] == 'failed'
assert tests['expectations']['assert_count'] == 6
assert [f['args'] for f in failures] == [
    ['7', '9'], ['actual', 'expected'], ['7', '9', '0.5'],
    ['7', '9', '0.125'], ['7', '9'], ['11', '22']
], failures
assert failures[4]['index'] == 0
assert failures[5]['offset'] == 0
assert tests['fatal']['assert_count'] == 1
assert tests['fatal']['assert_failures'][0]['args'] == ['7', '9']
assert '7 != 9' in run.stdout, run.stdout
assert '"actual" != "expected"' in run.stdout, run.stdout
print('OK')
PY_ORDER
then
  status=1
fi

# Skip reasons are reported, survive later log records (C++ destructors asserting during
# unwinding), and never hide a failed assertion.
echo "== Skip reasons =="
if ! python3 - "$(native_path "$BIN")" "$(native_path "$WORKDIR/skip.json")" "$(native_path "$WORKDIR/skip.tap")" <<'PY_SKIP'
import json, os, subprocess, sys
binary, report, tap = sys.argv[1:]
run = subprocess.run([binary, '--no-color', '--text=none', '--json=' + report, '--tap=' + tap,
                      '--isolation=on', 'skip_reasons'],
                     env={**os.environ, 'TRIAXI_VALIDATE_CHILD': '1'},
                     capture_output=True, text=True, timeout=30)
assert run.returncode == 1, (run.returncode, run.stderr)
tests = {t['name']: t for s in json.load(open(report))['suites'] for t in s['tests']}
expected = {'plain': ('skipped', None), 'formatted': ('skipped', 'needs gpu'),
            'failure_before_skip': ('failed', 'after a failure'),
            'passing_assert_while_unwinding': ('skipped', 'unwinding'),
            'failing_assert_while_unwinding': ('failed', 'unwinding')}
assert {'plain', 'formatted', 'failure_before_skip'} <= set(tests) <= set(expected), tests
lines = open(tap).read().splitlines()
for name, test in tests.items():
    outcome, reason = expected[name]
    assert test['outcome'] == outcome, test
    assert test.get('skip_reason') == reason, test
    if outcome == 'skipped':
        suffix = ' # SKIP' + (' ' + reason if reason else '')
        assert any(l.startswith('ok ') and l.endswith('skip_reasons > ' + name + suffix)
                   for l in lines), (name, lines)
    else:
        assert any(l.startswith('not ok ') and l.endswith('skip_reasons > ' + name)
                   for l in lines), (name, lines)
print('OK')
PY_SKIP
then
  status=1
fi

completed=1
exit "$status"
