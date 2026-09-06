# Triax

**Triax** is a single-header C/C++ test framework for systems code. It combines drop-in integration with process-isolated execution, crash/exit assertions, timeouts, output capture, parameterized tests, parallel workers, and CI-friendly reporting.

The name comes from **triaxial testing**: a specimen is placed under controlled conditions, stressed, and its failure behavior is observed. Triax applies the same idea to code.

## Why Triax?

Small C test frameworks are easy to embed but often deliberately minimal. Full-featured runners can isolate crashes, schedule tests, and emit CI reports, but add another build dependency. Triax targets the middle ground:

- one header to integrate;
- optional per-test process isolation;
- expected crash and `exit()` assertions;
- per-run, per-suite, and per-test timeouts/configuration;
- parallel isolated workers (`--jobs=N`);
- parameterized tests and fixtures;
- captured `stdout` / `stderr` access;
- assertion and non-fatal expectation APIs;
- text, JSON, TAP, and JUnit XML output;
- C and C++ APIs from the same header;
- automatic test registration without maintaining a manual test table.

## Quick start

```c
#include "triax.h"

static int add(int a, int b) { return a + b; }

triax_test(math, addition, .skip = false) {
    triax_assert_eq(add(2, 3), 5);
}

int main(int argc, char **argv) {
    Triax_RunConfig config = {0};
    return triax_run_argv(argc, argv, config);
}
```

Compile and run:

```sh
cc -std=c11 -Iinclude example.c -o example
./example
```

For C++, configuration uses the fluent builder syntax:

```cpp
triax_test(io, timeout, .timeout_ms(1000).isolation(TRIAX_ISOLATION_ON)) {
    triax_assert_true(true);
}
```

### Empty test configuration

`triax_test(suite, name)` is accepted by GCC/Clang/MSVC, but omitting a variadic macro argument is only standardized in newer language modes and can trigger `-Wpedantic` under C11/C++17. For warning-clean strict builds, pass an explicit no-op configuration (`.skip = false` in C or `.skip(false)` in C++), or use a newer language mode.

## Process-aware assertions

```c
triax_test(process, expected_abort, .isolation = TRIAX_ISOLATION_ON) {
    triax_assert_fault(TRIAX_FAULT_ABORT, abort());
}

triax_test(process, expected_exit, .isolation = TRIAX_ISOLATION_ON) {
    triax_assert_exit(7, exit(7));
}
```

Process isolation is recommended for code that can crash or hang. On POSIX, the process calling `triax_run()` must remain single-threaded while the run is active; isolated test children may create threads after they start.

## Parameterized tests

```c
struct Case { int a, b, expected; };
static const struct Case cases[] = {{1, 2, 3}, {2, 3, 5}};

triax_test(math, addition_cases, .params = triax_as_params(cases)) {
    const struct Case *c = triax_param(struct Case);
    triax_assert_eq(c->a + c->b, c->expected);
}
```

## Runner

Useful CLI options include:

```text
--isolation=on|off
--verbosity=always|on-fail|never
--timeout=MS
--jobs=N
--tags=list
--fail-fast
--text[=dst]
--json[=dst]
--tap[=dst]
--junit[=dst]
--list
```

Filters may be a suite (`math`) or a single test (`math::addition`).

## Integration modes

The default single-translation-unit mode requires only `#include "triax.h"`.

For multi-translation-unit test projects, define `TRIAX_MULTI_TU` in all test translation units and define `TRIAX_IMPL` in exactly one runner translation unit.

## Supported environments

The implementation contains dedicated paths for POSIX and Win32 and registration support for GNU/Clang, Apple/Mach-O, and MSVC-style toolchains.

Current release confidence:

- Linux + GCC: tested locally;
- Linux + Clang: tested locally;
- C11 and C++17: tested locally;
- AddressSanitizer + UndefinedBehaviorSanitizer: tested locally;
- macOS + AppleClang: tested locally (arm64);
- Windows: implementation is present, but remains **experimental until runtime CI has completed successfully**.

## Design trade-offs

Triax deliberately uses OS and toolchain facilities such as processes, signals/exceptions, memory mapping, and linker sections. That enables crash containment and automatic registration but makes it less suitable for freestanding or embedded targets than minimal test libraries. Process isolation also costs more than direct function calls.

The project is intended for desktop/server systems code where failure isolation and runner behavior are worth those trade-offs.

## Building the repository tests

With CMake:

```sh
cmake -S . -B build -DTRIAX_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Or directly:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude tests/selftest.c -o triax-selftest-c
c++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude tests/selftest.cpp -o triax-selftest-cpp
```

## License

MIT. See [LICENSE](LICENSE).
