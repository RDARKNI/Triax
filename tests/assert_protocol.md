# Assertion protocol fault injection

`validate_assert_protocol.py` copies the selected header to a temporary directory,
injects a deterministic worker abort, compiles a real test, and inspects its JSON.
It does not modify either repository header. Exact source anchors intentionally
fail after incompatible refactors. Unmodified control cases require no anchor.

The 12 cases run in on-fail, always, and never verbosity, in C11 and C++11:
evaluation abort; publication between count and IN_ASSERT; failure-header-only;
partial result header; partial operand payload; missing padding; completed failure;
success control; repeated same-site failures; interrupted and committed records
at the modulo-256 boundary; and interrupted streaming string output.

Checks require the exact expression, diagnostic kind, complete ordered failure
list, assertion count, outcome, and runner exit code. Extra diagnostics fail.
A completed record must appear once. ALWAYS exercises wraparound after 255
success records; on-fail/never verify that omitted success records do not confuse
recovery. These are deterministic interruption checks, not proof of memory
ordering or coverage of every instruction boundary.

Run against either header:

```
python3 tests/validate_assert_protocol.py include/triax.h --known-bugs
python3 tests/validate_assert_protocol.py experiments/shared-assert/triax.h --known-bugs
python3 tests/validate_assert_protocol.py experiments/shared-assert/triax.h --cc c++ --language cxx --known-bugs
```

`--known-bugs` accepts only the precise missing-current-diagnostic shape, while
requiring all preceding failures to remain correct. The publication gap remains
an acknowledged defect. Legacy headers without LOGGING may also reproduce lost
logging diagnostics. Headers with LOGGING must pass all logging cases even in
known-bug mode; a regression cannot be excused by that switch.

CTest registers C and C++ checks when Python and a UNIX platform are available.
CMake passes its macOS SDK selection to the compiler. The runner is intended for
native compiler invocations; cross-compilation/toolchain wrapper flags are not
fully modeled. Windows integration is disabled pending native validation.

For a strict gate that rejects the remaining publication gap:

```
cmake -S . -B build -DTRIAX_PROTOCOL_REQUIRE_RECOVERY=ON
ctest --test-dir build -R triax-assert-protocol --output-on-failure
```

The strict gate is currently expected to fail. This is visible diagnostic loss
before expression evaluation starts: the count has advanced while state is
still TEST. It does not demonstrate that a failing expression ran unnoticed.
