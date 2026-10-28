#!/usr/bin/env python3
"""Deterministic assertion-protocol fault injection for Triax self-tests.

Runs an isolated child against a temporary instrumented copy of the project's
real header. Does not change include/triax.h. Designed for CTest/CI.

Usage: python3 tests/validate_assert_protocol.py /path/to/include/triax.h --cc cc
       python3 tests/validate_assert_protocol.py /path/to/include/triax.h --cc cc --known-bugs

Strict mode requires recovery at every tested boundary. --known-bugs allows
only the exact publication-gap diagnostic loss, plus legacy logging loss for
headers without LOGGING. See tests/assert_protocol.md for coverage and limits.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

# Match exact source fragments; fail loudly after protocol refactors.
PUBLICATION_ANCHOR = ("  TRIAXI_exec.shared->nasserts = TRIAXI_exec.shared->nasserts + 1;\n"
                      "  TRIAXI_exec.shared->state    = TRIAXI_STATE_IN_ASSERT;")
FAILURE_ANCHOR = ("  triaxi_assert_write_header(TRIAXI_PACKETPREFIX_HDR_FAIL);\n"
                  "  triaxi_file_write(TRIAXI_exec.log, &TRIAXI_exec.pkg, record_size);")

# Most cases inject at assertion ten; boundary cases override that count.
# Earlier completed assertions are untouched.
CASES = {
    "evaluation_abort": {
        "anchor": "  TRIAXI_exec.shared->state    = TRIAXI_STATE_IN_ASSERT;\n}",
        "replacement": "  TRIAXI_exec.shared->state    = TRIAXI_STATE_IN_ASSERT;\n}",
        "test": "triax_expect_eq((abort(), 1234), 5678);",
        "expression": "expect_eq((abort(), 1234), 5678)",
        "kind": "unexpected_crash",
        "expected_count": 10,
    },
    "after_counter_before_state": {
        "anchor": PUBLICATION_ANCHOR,
        "replacement": ("  TRIAXI_exec.shared->nasserts = TRIAXI_exec.shared->nasserts + 1;\n"
                        "  if (TRIAXI_exec.shared->nasserts == 10) { abort(); }\n"
                        "  TRIAXI_exec.shared->state    = TRIAXI_STATE_IN_ASSERT;"),
        "test": "triax_expect_eq(1234, 5678);",
        "expression": "expect_eq(1234, 5678)",
        "kind": "unexpected_crash",
        "expected_count": 10,
        "known_bug": True,
    },
    "during_failure_record": {
        "anchor": FAILURE_ANCHOR,
        "replacement": ("  triaxi_assert_write_header(TRIAXI_PACKETPREFIX_HDR_FAIL);\n"
                        "  if (TRIAXI_exec.shared->nasserts == 10) { abort(); }\n"
                        "  triaxi_file_write(TRIAXI_exec.log, &TRIAXI_exec.pkg, record_size);"),
        "test": "triax_expect_eq(1234, 5678);",
        "expression": "expect_eq(1234, 5678)",
        "kind": "unexpected_crash",
        "expected_count": 10,
        "known_bug": True,
    },
    "partial_result_header": {
        "anchor": FAILURE_ANCHOR,
        "replacement": ("  triaxi_assert_write_header(TRIAXI_PACKETPREFIX_HDR_FAIL);\n"
                        "  if (TRIAXI_exec.shared->nasserts == 10) {\n"
                        "    triaxi_file_write(TRIAXI_exec.log, &TRIAXI_exec.pkg, sizeof(TRIAXI_AssertRes) / 2);\n"
                        "    abort();\n"
                        "  }\n"
                        "  triaxi_file_write(TRIAXI_exec.log, &TRIAXI_exec.pkg, record_size);"),
        "test": "triax_expect_eq(1234, 5678);",
        "expression": "expect_eq(1234, 5678)",
        "kind": "unexpected_crash",
        "expected_count": 10,
        "known_bug": True,
    },
    "completed_failure_then_abort": {
        "anchor": FAILURE_ANCHOR,
        "replacement": ("  triaxi_assert_write_header(TRIAXI_PACKETPREFIX_HDR_FAIL);\n"
                        "  triaxi_file_write(TRIAXI_exec.log, &TRIAXI_exec.pkg, record_size);\n"
                        "  if (TRIAXI_exec.shared->nasserts == 10) { abort(); }"),
        "test": "triax_expect_eq(1234, 5678);",
        "expression": "expect_eq(1234, 5678)",
        "kind": None,
        "expected_count": 10,
    },
    "successful_assertions": {
        "anchor": None,
        "replacement": None,
        "test": "triax_expect_eq(1234, 1234);",
        "expression": None,
        "kind": None,
        "expected_count": 10,
    },
}

# Real payload and alignment padding boundaries, separate from the result header.
for name, size in [("partial_operand_payload", "sizeof(TRIAXI_AssertRes) + TRIAXI_exec.pkg.len / 2"),
                   ("missing_record_padding", "sizeof(TRIAXI_AssertRes) + TRIAXI_exec.pkg.len")]:
    CASES[name] = dict(CASES["partial_result_header"])
    CASES[name]["replacement"] = CASES[name]["replacement"].replace(
        "sizeof(TRIAXI_AssertRes) / 2", size)
# Repeated same-site failures: preserve the earlier failure and recover the next.
CASES["same_site_interrupted"] = dict(CASES["during_failure_record"],
    prelude="", test="for (int i = 0; i < 2; ++i) { triax_expect_eq(1234, 5678); }",
    expected_count=2, expected_kinds=[None, "unexpected_crash"])
# In ALWAYS, 255 preceding success records exercise the modulo-256 boundary.
# ON_FAIL also checks that omitted successes do not affect the completion marker.
for name, base in [("wrap_interrupted", "during_failure_record"),
                   ("wrap_committed", "completed_failure_then_abort")]:
    CASES[name] = dict(CASES[base], expected_count=256,
        prelude="for (int i = 0; i < 255; ++i) { triax_expect_eq(i, i); }")

CASES["streaming_string_record"] = dict(CASES["during_failure_record"],
    anchor="    ssize_t n = writev(TRIAXI_exec.log, bufs + i, (int)(count - i));",
    replacement="    if (TRIAXI_exec.shared->nasserts == 10) { abort(); }\n"
                "    ssize_t n = writev(TRIAXI_exec.log, bufs + i, (int)(count - i));",
    test='triax_expect_streq("abc", "def");',
    expression='expect_streq("abc", "def")')

TEST_SOURCE = r'''
#include "triax.h"
#include <stdlib.h>
#if defined(__cplusplus)
# define PROTOCOL_ISOLATION .isolation(TRIAX_ISOLATION_ON)
#else
# define PROTOCOL_ISOLATION .isolation = TRIAX_ISOLATION_ON,
#endif
triax_test(protocol, injected, PROTOCOL_ISOLATION) {
  for (int i = 0; i < 9; ++i) { triax_expect_eq(i, i); }
  TEST_ASSERTION
}
TRIAX_MAIN()
'''


def run_case(source, compiler, workdir, name, spec, verbosity, known_bugs, language, compiler_args=()):
    header = source
    anchor = spec["anchor"]
    if anchor and header.count(anchor) != 1:
        raise RuntimeError(f"{name}: expected exactly one source anchor, found {header.count(anchor)}")
    if anchor:
        header = header.replace(anchor, spec["replacement"].replace("nasserts == 10", "nasserts == " + str(spec["expected_count"])), 1)
    build = workdir / f"{language}_{name}"

    build.mkdir(exist_ok=True)
    (build / "triax.h").write_text(header)
    extension = "cpp" if language == "cxx" else "c"
    source_path = build / ("test." + extension)
    source_path.write_text(TEST_SOURCE.replace("  for (int i = 0; i < 9; ++i) { triax_expect_eq(i, i); }", spec.get("prelude", "  for (int i = 0; i < 9; ++i) { triax_expect_eq(i, i); }")).replace("TEST_ASSERTION", spec["test"]))
    binary = build / ("test.exe" if os.name == "nt" else "test")
    if os.name == "nt" and Path(compiler).stem.lower() == "cl":
        cmd = [compiler, "/nologo", "/std:c++14" if language == "cxx" else "/std:c11",
               "/O2", "/I" + str(build), str(source_path), "/Fe:" + str(binary)]
    else:
        cmd = [compiler, "-std=c++11" if language == "cxx" else "-std=c11",
               "-O2", "-I", str(build), str(source_path), "-o", str(binary)]
    cmd[1:1] = compiler_args
    if not binary.exists():
        built = subprocess.run(cmd, capture_output=True, text=True)
        if built.returncode:
            raise RuntimeError(f"{name}: compile failed:\n{built.stdout}\n{built.stderr}")
    report = build / "report.json"
    args = [str(binary), "--no-color", "--text=none", "--isolation=on",
            "--verbosity=" + verbosity, "--json=" + str(report), "protocol::injected"]
    report.unlink(missing_ok=True)
    proc = subprocess.run(args, capture_output=True, text=True, timeout=30)
    if not report.is_file():
        raise RuntimeError(f"{name}: no report, exit={proc.returncode}\n{proc.stderr}")
    document = json.loads(report.read_text())
    suites = document["suites"]
    tests = [t for s in suites for t in s["tests"] if t["name"] == "injected"]
    if len(tests) != 1:
        raise RuntimeError(f"{name}: expected one test, got {len(tests)}")
    t = tests[0]
    failures = t.get("assert_failures", [])
    actual_count = t.get("assert_count")
    if actual_count != spec["expected_count"]:
        raise AssertionError(f"{name}: assertion count {actual_count}, expected {spec['expected_count']}")
    if name == "successful_assertions":
        if t["outcome"] != "passed" or failures or proc.returncode != 0:
            raise AssertionError(f"{name}: expected pass, got {t['outcome']}: {failures}")
        return "PASS"
    if proc.returncode == 0 or t["outcome"] != "ucrashed":
        raise AssertionError(f"{name}: expected unexpected crash, got {t['outcome']}, exit {proc.returncode}")
    expected_kinds = spec.get("expected_kinds", [spec["kind"]])
    if (len(failures) == len(expected_kinds)
            and all(f.get("expression") == spec["expression"]
                    and f.get("kind") == kind
                    for f, kind in zip(failures, expected_kinds))
            and proc.returncode == 1):
        return "PASS"
    # A fixed LOGGING implementation must never silently regress to losing records.
    known = spec.get("known_bug") and (name == "after_counter_before_state"
                                     or "TRIAXI_STATE_LOGGING" not in source)
    prior_kinds = expected_kinds[:-1]
    if (known and known_bugs and proc.returncode == 1
            and len(failures) == len(prior_kinds)
            and all(f.get("expression") == spec["expression"] and f.get("kind") == kind
                    for f, kind in zip(failures, prior_kinds))):
        return "KNOWN BUG (reproduced)"
    raise AssertionError(
        f"{name} ({verbosity}): expected expressions {spec['expression']!r} "
        f"with kinds {expected_kinds}; got {failures}, exit {proc.returncode}"
    )



def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("header", type=Path)
    p.add_argument("--cc", default=os.environ.get("CC", "cc"))
    p.add_argument("--language", choices=("c", "cxx"), default="c")
    p.add_argument("--known-bugs", action="store_true")
    p.add_argument("--compiler-arg", action="append", default=[])
    args = p.parse_args()
    source = args.header.read_text()
    failed = 0
    with tempfile.TemporaryDirectory(prefix="triax-protocol-") as temp:
        workdir = Path(temp)
        for name, spec in CASES.items():
            for verbosity in ("on-fail", "always", "never"):
                try:
                    result = run_case(source, args.cc, workdir, name, spec, verbosity, args.known_bugs, args.language, args.compiler_arg)
                    print(f"{name:34} {verbosity:8} {result}", flush=True)
                except (RuntimeError, AssertionError, OSError, ValueError, subprocess.TimeoutExpired) as exc:
                    print(f"{name:34} {verbosity:8} FAIL: {exc}", file=sys.stderr, flush=True)
                    failed += 1
    return int(bool(failed))


if __name__ == "__main__":
    sys.exit(main())
