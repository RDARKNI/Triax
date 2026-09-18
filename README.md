# Triax

**Triax** is a single-header C/C++ test framework for systems code.

Copy `triax.h` into your project, include it, and compile normally. There is no library to build, no package to install, and no test runner to link separately.

Despite the single-header integration, Triax provides automatic test registration, a rich type-generic set of assertion macros, process-isolated tests, crash and `exit()` assertions, timeouts, output capture, parameterized tests, parallel execution, and CI-friendly reporting.

## Quick start

Put `triax.h` next to your test file:

```text
triax.h
example.c
```

Then write your tests:

```c
#include "triax.h"

static int add(int a, int b) {
    return a + b;
}

triax_test(math, addition, .skip = false) {
    triax_assert_eq(add(2, 3), 5);
}

TRIAX_MAIN()
```

Compile and run:

```sh
cc -std=c11 example.c -o example
./example
```

That is the complete integration.

No CMake configuration, generated files, external runner, or additional library is required.

## Why Triax?

Minimal C test frameworks are easy to embed, but often stop at assertions and test registration. More capable test runners provide isolation, scheduling, crash handling, and machine-readable reports, but usually require additional build dependencies.

Triax aims to provide those runner features while remaining a single file:

* single-header integration;
* C and C++ APIs from the same header;
* automatic test registration;
* optional per-test process isolation;
* expected crash and `exit()` assertions;
* per-run, per-suite, and per-test configuration;
* test timeouts;
* parallel isolated workers with `--jobs=N`;
* captured `stdout` and `stderr`;
* parameterized tests and fixtures;
* fatal assertions and non-fatal expectations;
* text, JSON, TAP, and JUnit XML output.

The name comes from **triaxial testing**: a specimen is placed under controlled conditions, stressed, and its failure behavior is observed. Triax applies the same idea to code.

## C++ usage

The same header works in C++. Configuration uses a fluent builder syntax:

```cpp
#include "triax.h"

triax_test(io, timeout,
           .timeout_ms(1000)
           .isolation(TRIAX_ISOLATION_ON)) {
    triax_assert_true(true);
}

TRIAX_MAIN()
```

## Process-aware assertions

Tests that intentionally crash or terminate can be checked directly:

```c
triax_test(process, expected_abort,
           .isolation = TRIAX_ISOLATION_ON) {
    triax_assert_fault(TRIAX_FAULT_ABORT, abort());
}

triax_test(process, expected_exit,
           .isolation = TRIAX_ISOLATION_ON) {
    triax_assert_exit(7, exit(7));
}
```

Process isolation is recommended for code that may crash or hang.

On POSIX, the process calling `triax_run()` must remain single-threaded while a run is active. Isolated test children may create threads after they start.

## Parameterized tests

```c
struct Case {
    int a;
    int b;
    int expected;
};

static const struct Case cases[] = {
    {1, 2, 3},
    {2, 3, 5}
};

triax_test(math, addition_cases,
           .params = triax_as_params(cases)) {
    const struct Case *c = triax_param(struct Case);
    triax_assert_eq(c->a + c->b, c->expected);
}
```

## Runner

`TRIAX_MAIN()` defines the default test runner:

```c
TRIAX_MAIN()
```

For custom configuration, define `main()` yourself:

```c
int main(int argc, char **argv) {
    Triax_RunConfig config = {0};
    config.njobs = 4;

    return triax_run_argv(argc, argv, config);
}
```

Useful command-line options include:

```text
--isolation=on|off
--verbosity=always|on-fail|never
--timeout=MS
--jobs=N
--tags=list
--no-color
--fail-fast
--break
--debug
--text[=dst]
--json[=dst]
--tap[=dst]
--junit[=dst]
--list
```

`--break` raises a breakpoint trap when an assertion fails, allowing an attached debugger to stop at the failure.

`--debug` implies `--break`, `--jobs=1`, and disables isolation and timeouts so that a failing test remains in the runner process.

Filters may name a suite:

```sh
./tests math
```

or an individual test:

```sh
./tests math::addition
```

## Integration

### Single translation unit

For a simple test executable, there is nothing to configure:

```c
#include "triax.h"

/* tests */

TRIAX_MAIN()
```

Compile the source file normally.

### Multiple translation units

For test programs split across multiple source files, define `TRIAX_MULTI_TU` in all test translation units and define `TRIAX_IMPL` in exactly one runner translation unit.

Only that translation unit should define the runner, for example with:

```c
#define TRIAX_MULTI_TU
#define TRIAX_IMPL
#include "triax.h"

TRIAX_MAIN()
```

## Empty test configuration

`triax_test(suite, name)` is accepted by GCC, Clang, and MSVC, but omitting a variadic macro argument is only standardized in newer language modes and may trigger `-Wpedantic` under C11 or C++17.

For warning-clean strict builds, provide an explicit no-op configuration:

```c
triax_test(math, addition, .skip = false) {
    /* ... */
}
```

or in C++:

```cpp
triax_test(math, addition, .skip(false)) {
    /* ... */
}
```

Alternatively, use a newer language mode where empty variadic arguments are standardized.

## Supported environments

Triax contains dedicated POSIX and Win32 execution paths, along with automatic-registration support for GNU/Clang, Apple/Mach-O, and MSVC-style toolchains.

The project's CI currently covers:

* Linux x86_64 and ARM64 with GCC and Clang;
* AddressSanitizer and UndefinedBehaviorSanitizer on Linux;
* macOS Apple Silicon with AppleClang;
* Windows x86_64 with MSVC;
* AddressSanitizer and dedicated Release builds on Windows;
* warnings-as-errors configurations where applicable.

## Design trade-offs

Triax uses operating-system and toolchain facilities including processes, signals or exceptions, memory mapping, and linker sections.

These mechanisms enable crash containment, automatic test registration, output capture, and isolated execution, but make Triax more appropriate for desktop and server systems software than for freestanding or deeply embedded targets.

Process-isolated tests also carry more overhead than direct in-process function calls.

## Developing Triax

CMake is used to build and validate **Triax itself**. It is not required to use the framework.

To run the repository's own test suite:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

`BUILD_TESTING`, provided by CTest, defaults to enabled when Triax is built as the top-level project. Pass:

```sh
-DBUILD_TESTING=OFF
```

to skip configuring the repository tests.

## License

MIT. See [LICENSE](LICENSE).
