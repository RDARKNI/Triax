// SPDX-License-Identifier: MIT
/// @file triax.h
/// @version 1.0.0
/// @brief Single-header process-isolated test framework for C and C++
///
/// Triax is a self-contained test runner for systems code. It combines drop-in
/// single-header integration with optional process isolation, crash/exit assertions,
/// timeouts, parallel execution, output capture, parameterization, and CI-friendly
/// text, JSON, TAP, and JUnit reporting.
///
/// @attention Windows support is experimental until covered by runtime CI.
/// @attention On Linux, include this header before system headers or compile
/// with `_DEFAULT_SOURCE` or `_GNU_SOURCE` enabled so required libc extensions
/// are visible.

#ifndef TRIAXI_TEST_H
# define TRIAXI_TEST_H

# ifdef __cplusplus
#  define TRIAXI_C 0L
#  ifdef _MSVC_LANG
#   define TRIAXI_CPP _MSVC_LANG
#  else
#   define TRIAXI_CPP __cplusplus
#  endif
# else
#  ifdef __STDC_VERSION__
#   define TRIAXI_C __STDC_VERSION__
#  else
#   define TRIAXI_C 199000L
#  endif
#  define TRIAXI_CPP 0L
# endif

# if defined(_MSC_VER) && !defined(__clang__)
#  define TRIAXI_MSVC _MSC_VER
# else
#  define TRIAXI_MSVC 0L
# endif

# if defined(__clang__)
#  define TRIAXI_CLANG (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
# else
#  define TRIAXI_CLANG 0L
# endif

# if defined(__GNUC__) && !defined(__clang__)
#  define TRIAXI_GCC (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
# else
#  define TRIAXI_GCC 0L
# endif

# ifdef _MSC_VER
#  define TRIAXI_MSVC_COMPAT 1L
# else
#  define TRIAXI_MSVC_COMPAT 0L
# endif

# ifdef __GNUC__
#  define TRIAXI_GNU_COMPAT 1L
# else
#  define TRIAXI_GNU_COMPAT 0L
# endif

// Registration backend is independent of the runtime platform.
// clang-cl defines _MSC_VER and uses the MSVC/COFF registration path;
// MinGW GCC/Clang use GNU section attributes even though _WIN32 is defined.
# if defined(_WIN32) && defined(_MSC_VER)
#  define TRIAXI_REG_MSVC_COFF 1L
# else
#  define TRIAXI_REG_MSVC_COFF 0L
# endif

# if defined(__APPLE__)
#  define TRIAXI_REG_MACHO 1L
# else
#  define TRIAXI_REG_MACHO 0L
# endif

# if !TRIAXI_REG_MSVC_COFF && !TRIAXI_REG_MACHO && (defined(__GNUC__) || defined(__clang__))
#  define TRIAXI_REG_GNU_SECTION 1L
# else
#  define TRIAXI_REG_GNU_SECTION 0L
# endif
// NOLINTBEGIN(performance-enum-size)

// ── Common API ──────────────────────────────────────────────────────────────
# ifndef _WIN32
#  if defined(__linux__) && !defined(_DEFAULT_SOURCE) && !defined(_GNU_SOURCE)
#   define _DEFAULT_SOURCE
#  endif
#  include <fcntl.h>
#  include <sys/mman.h>
#  ifdef MAP_ANONYMOUS
#   define TRIAXI_MAP_ANON MAP_ANONYMOUS
#  elif defined(MAP_ANON)
#   define TRIAXI_MAP_ANON MAP_ANON
#  else
#   error "triax.h requires anonymous mmap support"
#  endif

#  include <sys/stat.h>
#  include <sys/uio.h>
#  include <unistd.h>

# else // Win32
#  if TRIAXI_MSVC && TRIAXI_C && TRIAXI_MSVC < 1928
#   error "triax.h in C mode on MSVC requires MSVC 19.28+"
#  endif
#  if TRIAXI_MSVC && TRIAXI_C && TRIAXI_C < 201112L
#   error "triax.h in C mode on MSVC requires /std:c11 or /std:clatest"
#  endif
#  include <fcntl.h>
#  include <malloc.h> /* _resetstkoflw */
#  include <wchar.h>
#  include <windows.h>
# endif

# include <assert.h>
# include <errno.h>
# include <inttypes.h>
# include <limits.h>
# include <math.h>
# include <setjmp.h>
# include <signal.h>
# include <stdarg.h>
# include <stddef.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>

# if TRIAXI_CPP
#  include <array>
#  include <iostream>
#  include <sstream>
#  include <string>
#  include <type_traits>
#  include <utility>
// __has_include(<header>) only answers "does this file exist on disk" — on
// MSVC's STL specifically, the header exists as soon as any C++ mode is
// selected, but its *contents* hard-error (STL4038) if the active language
// standard predates what the header requires. Gating on the language
// version too (not just presence) avoids ever attempting the #include in a
// mode where it's certain to fail, e.g. this project's own advertised
// cxx_std_11 baseline.
#  if TRIAXI_CPP >= 201703L && defined(__has_include)
#   if __has_include(<string_view>)
#    include <string_view>
#   endif
#  endif
#  if TRIAXI_CPP >= 202002L && defined(__has_include)
#   if __has_include(<format>)
#    include <format>
#   endif
#  endif
# else
#  include <stdbool.h>
# endif

# if defined(TRIAX_IMPL) || !defined(TRIAX_MULTI_TU)
#  ifdef _WIN32
#   include <io.h>

#  else
#   include <sys/wait.h>
#   ifdef __linux__
#    include <sys/syscall.h>
#   elif defined(__APPLE__)
#    include <mach-o/getsect.h>
#    include <mach-o/ldsyms.h>
#   endif
#  endif
# endif

/// @defgroup triax_test_commons Common Test API
/// Shared configuration and fixtures for tests, suites, and runs.
/// @{

enum {
  TRIAX_VERBOSITY_INHERIT = 0, ///< Use the next level's setting.
  TRIAX_VERBOSITY_ON_FAIL,     ///< Print assertions only on failure.
  TRIAX_VERBOSITY_ALWAYS,      ///< Always print assertions.
  TRIAX_VERBOSITY_NEVER,       ///< Never print assertions.
  TRIAX_VERBOSITY_DEFAULT = TRIAX_VERBOSITY_ON_FAIL
};
typedef uint8_t Triax_Verbosity;

enum {
  TRIAX_ISOLATION_INHERIT = 0, ///< Use the next level's setting.
  TRIAX_ISOLATION_ON,          ///< Run tests in a child process.
  TRIAX_ISOLATION_OFF,         ///< Run tests in the main process.
  TRIAX_ISOLATION_DEFAULT = TRIAX_ISOLATION_ON
};
typedef uint8_t Triax_Isolation;

# define TRIAX_TIMEOUT_INHERIT 0          ///< Use the next level's setting.
# define TRIAX_TIMEOUT_NONE    UINT32_MAX ///< Disable Timeout
# define TRIAX_TIMEOUT_DEFAULT TRIAX_TIMEOUT_NONE

# if TRIAXI_CPP >= 201703L
#  define triax_noexcept noexcept
# else
#  define triax_noexcept
# endif

/// @brief Run/suite/test fixture function.
/// @note Assertion and test-control APIs must not be called from fixtures.
/// @note Run, suite, and test fixtures execute for every concrete test
/// invocation, including each parameterized invocation. Initialisation is
/// cumulative (run -> suite -> test) and finalisation runs in reverse order.
typedef void (*Triax_Fixture)(void) triax_noexcept;

/// @brief Shared attributes for tests, suites, and runs.
///
/// Cascading fields (`verbosity`, `isolation`, `timeout_ms`)
/// resolve in this order:
///
///     Test -> Suite -> Run -> Default
///
/// - All three use `*_INHERIT` and `*_DEFAULT`
///
/// Other fields:
/// - `skip`       — skips a test or suite (ignored on global level)
/// - `tags`       — category labels on a test/suite; a tag filter on the run
/// - `init/fini`  — additive fixtures at every level
typedef struct Triax_Attributes {
  char            TRIAXI_dummy; ///< Prevents empty designated-initialiser list
  bool            skip;         ///< Skip this test/suite; no-op at run level
  Triax_Verbosity verbosity;    ///< Text output verbosity (see @ref Triax_Verbosity)
  Triax_Isolation isolation;    ///< If fork per test (see @ref Triax_Isolation)
  uint32_t        timeout_ms;   ///< Per-test timeout ms (see @ref TRIAX_TIMEOUT_*)
  const char*     tags;         ///< Test/suite: comma-separated category labels, e.g.
                                ///< `"fast,unit"`. Run-level: only tests whose tags
                                ///< intersect this set are run; `NULL` runs all.
  Triax_Fixture   init;         ///< Run/suite/test init fixture
  Triax_Fixture   fini;         ///< Run/suite/test fini fixture
} Triax_Attributes;
/// @}

// ── Runner API ───────────────────────────────────────────────────────────────
# if defined(TRIAX_IMPL) || !defined(TRIAX_MULTI_TU)
/// @defgroup triax_runner Runner API
/// Types and functions for the test runner (the translation unit containing
/// `main`). Use these to configure output formats, parse CLI arguments, and
/// launch the test run.
/// @{

/// @brief Top-level configuration passed to @ref triax_run.
/// @note Zero-initialise for default attributes, flags and no test filters
typedef struct Triax_RunConfig {
  Triax_Attributes attrs; ///< Run-level @ref Triax_Attributes
  struct {
    const char* text;       ///< Formatted text output
    const char* json;       ///< JSON output
    const char* tap;        ///< Tap output
    const char* junit;      ///< JUnit XML
  } outpaths;               ///< Output paths / @ref TRIAX_OUTPATH_* sentinels for formats.
  int         kind;         ///< internal, for errors
  bool        no_color;     ///< Force-disable colour in text output
  bool        fail_fast;    ///< Exit the framework once a test is failed
  bool        debug_break;  ///< Break into an attached debugger on assertion failure
  bool        debug;        ///< Debug mode: one job, no isolation/timeout, break on failure
  uint8_t     njobs;        ///< parallel test processes (0 or 1 = sequential)
  uint8_t     nfilters;     ///< Number of filters (max: 255)
  const char* filters[255]; ///< Filters; owned by the caller
} Triax_RunConfig;

#  define TRIAX_OUTPATH_NONE    ((const char*)1) ///< Disable this output.
#  define TRIAX_OUTPATH_STDOUT  ((const char*)2) ///< Write this output to stdout.
#  define TRIAX_OUTPATH_DEFAULT ((const char*)0) ///< stdout for text, else none

/// @brief Runs all registered tests using the provided run configuration.
/// Initialises the framework, runs each matching test in registration order,
/// prints results to the configured outputs, then restores stdout/stderr.
///
/// @note On POSIX, process isolation uses `fork()`. The process calling
/// `triax_run()` must be single-threaded while the run is active. Isolated tests
/// may create threads after their child process has started.
/// @note In non-isolated mode, assertion and test-control APIs must only be
/// called from the thread executing the test function. Threads created by a
/// test must be joined before the test returns.
/// @note A crash or timeout in a non-isolated test may terminate or corrupt the
/// runner process. Process isolation is recommended for tests that may fault
/// or hang.
///
/// @param config  Run configuration produced by @ref triax_parse_argv or
///                assembled manually
/// @return Exit code:
///
/// - `0` success (all tests passed or skipped)
///
/// - `1` test failure, crash, timeout, or error
///
/// - `2` invalid user configuration
///
/// - Calls `abort()` on internal framework errors.
static inline int triax_run(Triax_RunConfig config);

#  define TRIAX_HELP                                                                               \
     "Usage: triax [options] [filters...]\n"                                                       \
     "\n"                                                                                          \
     "Filters (max 255):\n"                                                                        \
     "  suite                Run all tests in a suite\n"                                           \
     "  suite::test          Run one specific test\n"                                              \
     "\n"                                                                                          \
     "Output (dst: file path | stdout | none):\n"                                                  \
     "  --text[=dst]         Text output            (default: stdout)\n"                           \
     "  --json[=dst]         JSON output            (default: none)\n"                             \
     "  --tap[=dst]          TAP output             (default: none)\n"                             \
     "  --junit[=dst]        JUnit XML output       (default: none)\n"                             \
     "  At most one output format may use stdout.\n"                                               \
     "\n"                                                                                          \
     "Options:\n"                                                                                  \
     "  --isolation=on|off   Set global default process isolation\n"                               \
     "  --verbosity=VAL      Set global default verbosity: always, on-fail, never\n"               \
     "  --timeout=MS         Set global default timeout in milliseconds (0: no timeout)\n"         \
     "  --jobs=N             Set parallel test jobs (default: 1, max: 64; 0/1: sequential)\n"      \
     "  --tags=list          Run only tests matching comma-separated tags\n"                       \
     "  --no-color           Disable ANSI colours in text output\n"                                \
     "  --fail-fast          Stop launching tests after the first failure\n"                       \
     "  --break              Break into an attached debugger on assertion failure\n"               \
     "  --debug              Debug mode: --jobs=1, no isolation/timeout, --break\n"                \
     "\n"                                                                                          \
     "  --list, -l           List registered tests and exit\n"                                     \
     "  --help, -h           Show this help and exit\n"

/// @brief Parses command-line arguments into an @ref Triax_RunConfig. Use the
/// `--help` flag for a summary of usage or view @ref TRIAX_HELP.
/// @note
/// `--help` and `--list` print output and terminate the process with code `0`.
/// Unrecognised flags print help output and exit the process with code `2`.
///
/// Usage:
/// ```c
/// int main(int argc, char** argv) {
///   Triax_RunConfig defaults = {0};
///   defaults.attrs.isolation = TRIAX_ISOLATION_OFF;
///   return triax_run(triax_parse_argv(argc, argv, defaults));
/// }
/// ```
static inline Triax_RunConfig triax_parse_argv(int argc, char* argv[], Triax_RunConfig defaults);

/// @brief Null-terminated help string listing all recognised CLI flags.
/// Printed by `--help`; may also be embedded in a host program's own usage
/// output.

/// @brief Parses @p argv then immediately runs all matching tests.
/// Convenience wrapper equivalent to `triax_run(triax_parse_argv(argc, argv,
/// defaults))`.
/// @param argc      Argument count from `main`.
/// @param argv      Argument vector from `main`.
/// @param defaults  Base @ref Triax_RunConfig; overridden by any parsed flags.
/// @return Same exit codes as @ref triax_run.
static inline int             triax_run_argv(int argc, char* argv[], Triax_RunConfig defaults);

# else
#  define triax_run(...)        TRIAXI_WRONGMODULE_ERR(triax_run)
#  define triax_parse_argv(...) TRIAXI_WRONGMODULE_ERR(triax_parse_argv)
#  define triax_run_argv(...)   TRIAXI_WRONGMODULE_ERR(triax_run_argv)
# endif

/// @} // triax_runner

// ── Test Module API ──────────────────────────────────────────────────────────

/// @defgroup triax_test_module Test Module API
/// Macros and types for writing test files: declaring suites, registering
/// tests, and making assertions. A test module only needs these names.
/// @{

/// @brief Declares and registers a test function within a named suite.
/// The suite is created implicitly if it does not exist and may be configured
/// later via @ref triax_suite().
/// @param suite  Suite name.

/// @param name   Test name.
/// @param ...  Test configuration (attributes, parameters)
///
/// Usage:
/// ```c
/// // C (designated initialisers)
/// static const int cases[] = {1, 2, 3};
/// triax_test(math, add, .timeout_ms = 1000, .skip = true,
/// .params=triax_as_params(cases)) {
///     triax_assert_eq(add(1, 2), 3);
/// }
/// ```
/// ```c++
/// // C++ (fluent builder syntax)
/// static const int cases[] = {1, 2, 3};
/// triax_test(math, add, .timeout_ms(1000).skip().parameterize(cases)) {
///     triax_assert_eq(add(1, 2), 3);
/// }
/// ```
/// @note Early exit with `return` is valid. `exit()` is treated as failure.
/// @note Test identity is (suite, name).
# define triax_test(suite, name, ...) triaxi_test(suite, name, __VA_ARGS__)

/// @brief Registers a named test suite with optional attribute overrides.
/// If a suite with `name` has already been implicitly created via @ref
/// triax_test, its attributes are updated. May appear before or after @ref
/// triax_test calls for the same suite.
/// @param name       Bare identifier naming the suite
/// @param ...        Optional @ref Triax_Attributes fields as designated
///                   initialisers, e.g. `.timeout_ms = 5000`.
///
/// Usage:
/// ```c
/// triax_test(math, test1, .timeout_ms = 500) { ... }
/// triax_suite(math, .timeout_ms = 5000, .isolation = TRIAX_ISOLATION_ON);
/// // test1 has a timeout of 500ms and will run in isolated mode.
/// ```
# define triax_suite(name, ...)       triaxi_suite(name, __VA_ARGS__)

/// @brief Marks a static array as test parameters.
/// Runs the test once per element of a static array. Use @ref triax_param in the
/// test body to access the current element. On C++, this macro expands its
/// arguments as-is.
/// @param arr  Static array of parameters.
///
/// Usage:
/// ```c
/// static const int values[] = {10, 20, 30};
/// triax_test(math, test_!, .params=triax_as_params(values)) {
///     triax_assert_gt(0, *triax_param(int));
/// }
/// ```
# define triax_as_params(arr)         TRIAXI_as_params(arr)

/// @brief Casts the current parameter pointer to a typed pointer.
/// Only valid inside a parameterized test registered with @ref triax_as_params.
/// Expands to a `const T*` pointing at the current parameter element for this
/// invocation.
/// @param type The element type, e.g. `int` or `struct Point`.
///
/// Usage:
/// ```c
/// struct Case { int a, b, expected; };
/// static const struct Case cases[] = {{1, 2, 3}, {2, 3, 5}};
/// triax_test(math, test_add, .params = triax_as_params(cases)) {
///     const struct Case* c = triax_param(struct Case);
///     triax_assert_eq(add(c->a, c->b), c->expected);
/// }
/// ```
# define triax_param(type)                                                                         \
    (TRIAXI_exec.param ? ((const type*)TRIAXI_exec.param)                                          \
                       : (triaxi_user_error(TRIAXI_ERROR_PARAM_ACCESS), (const type*)0))

/// @defgroup triax_assertions Assertions and Expectations
/// @ingroup triax_test_module
///
/// Every check (except @ref triax_assert_fault and @ref triax_assert_exit) comes in
/// two flavours:
///   - `triax_assert_*` — records failure and immediately aborts the test
///   (longjmp).
///   - `triax_expect_*` — records failure and continues, reporting multiple
///   failures.
///
/// Both record the source location, the stringified expression, and actual
/// values.
/// @{

/// @brief Maximum Length of a stringified assertion
# ifndef _WIN32
#  define TRIAX_EXPR_MAX UINT8_MAX
# else
#  define TRIAX_EXPR_MAX 116
# endif

/// @brief Skips the current test immediately (recorded as skipped, not failed).
/// Typically guarded by a condition: `if (!feature_available()) triax_skip();`
/// @brief Still performs cleanup
# define triax_skip()                                                                              \
    (TRIAXI_exec.shared->state == TRIAXI_STATE_TEST                                                \
         ? TRIAXI_ABORT_TEST(TRIAXI_EXEC_SKIPPED)                                                  \
         : triaxi_user_error(TRIAXI_ERROR_SKIP_IN_FIXTURE))

// ── Generic
// ───────────────────────────────────────────────────────────────────
/// @brief Passes if @p cond is true; optional printf-style @p ... message on
/// failure.
// "" __VA_ARGS__ relies on adjacent string-literal concatenation (not token
// pasting) so a message-less call still supplies TRIAXI_AT_check's required
// fmt argument, instead of leaving a dangling trailing comma when the
// documented-optional message is omitted.
# define triax_expect(cond, ...)       triaxi_E1(check, 0, #cond, !(cond), "" __VA_ARGS__)
# define triax_assert(cond, ...)       triaxi_A1(check, 0, #cond, !(cond), "" __VA_ARGS__)

// ── Booleans / Pointers
// ───────────────────────────────────────────────────────
/// @brief Test whether a predicate is truthy, using implicit bool conversions
# define triax_expect_true(expr)       triaxi_E1(true, 0, #expr, expr)
# define triax_assert_true(expr)       triaxi_A1(true, 0, #expr, expr)
# define triax_expect_false(expr)      triaxi_E1(true, 1, #expr, expr)
# define triax_assert_false(expr)      triaxi_A1(true, 1, #expr, expr)

/// @brief Test whether a pointer is `NULL`
# define triax_expect_null(ptr)        triaxi_E1(null, 0, #ptr, ptr)
# define triax_assert_null(ptr)        triaxi_A1(null, 0, #ptr, ptr)
# define triax_expect_nonnull(ptr)     triaxi_E1(null, 1, #ptr, ptr)
# define triax_assert_nonnull(ptr)     triaxi_A1(null, 1, #ptr, ptr)

// ── Equality
// ────────────────────────────────────────────────────────────────── C:
// _Generic dispatch over arithmetic/pointer types; if two (const) char*
// arguments are passed, lexicographical string comparison is performed. C++:
// templates. In C, these do not accept
// @ref triax_str() length-based strings; use the specialised string assertions
// for this use case.
/// @brief Test whether two expressions evaluate to equal;
/// Note:
/// In C:
/// Floating Point types: Implicitly converts to long double and performs
/// comparison via `==` Integral Types: Works correctly and safely for mixed
/// signed/unsigned integral types (const) char*: Performs lexicographical
/// comparison of the strings, interpreting `NULL` as a unique string, different
/// from all nonnull strings other pointer types: Whenever a non-char* pointer
/// type is involved, it performs well-defined pointer comparisons Mixing
/// between these type classes is not allowed and results in a compiler error In
/// C++: Works equivalently to the C branch for the C builtin types and uses
/// operator overloads for comparisons to work with custom types
# define triax_expect_eq(exp, act)     triaxi_E1(eq, 0, #exp ", " #act, exp, act)
# define triax_assert_eq(exp, act)     triaxi_A1(eq, 0, #exp ", " #act, exp, act)
# define triax_expect_neq(exp, act)    triaxi_E1(eq, 1, #exp ", " #act, exp, act)
# define triax_assert_neq(exp, act)    triaxi_A1(eq, 1, #exp ", " #act, exp, act)

# define triax_expect_arreq(exp, act)  triaxi_ARREQ(0, #exp ", " #act, exp, act)
# define triax_assert_arreq(exp, act)  triaxi_ARREQ(1, #exp ", " #act, exp, act)
# define triax_expect_arrneq(exp, act) triaxi_ARRNEQ(0, #exp ", " #act, exp, act)
# define triax_assert_arrneq(exp, act) triaxi_ARRNEQ(1, #exp ", " #act, exp, act)

# define triax_expect_arreq_n(exp, act, count)                                                     \
    triaxi_ARREQ_N(0, #exp ", " #act ", " #count, exp, act, count)
# define triax_assert_arreq_n(exp, act, count)                                                     \
    triaxi_ARREQ_N(1, #exp ", " #act ", " #count, exp, act, count)
# define triax_expect_arrneq_n(exp, act, count)                                                    \
    triaxi_ARRNEQ_N(0, #exp ", " #act ", " #count, exp, act, count)
# define triax_assert_arrneq_n(exp, act, count)                                                    \
    triaxi_ARRNEQ_N(1, #exp ", " #act ", " #count, exp, act, count)

// ── Ordering
// ──────────────────────────────────────────────────────────────────
# define triax_expect_gt(a, b)  triaxi_E1(gt, 0, #a ", " #b, a, b)
# define triax_assert_gt(a, b)  triaxi_A1(gt, 0, #a ", " #b, a, b)
# define triax_expect_geq(a, b) triaxi_E1(lt, 1, #a ", " #b, a, b)
# define triax_assert_geq(a, b) triaxi_A1(lt, 1, #a ", " #b, a, b)
# define triax_expect_lt(a, b)  triaxi_E1(lt, 0, #a ", " #b, a, b)
# define triax_assert_lt(a, b)  triaxi_A1(lt, 0, #a ", " #b, a, b)
# define triax_expect_leq(a, b) triaxi_E1(gt, 1, #a ", " #b, a, b)
# define triax_assert_leq(a, b) triaxi_A1(gt, 1, #a ", " #b, a, b)

// ── Floating-point
// ──────────────────────────────────────────────────────────── Absolute
// tolerance: |exp - act| <= tol
# define triax_expect_floateq_abstol(exp, act, tol)                                                \
    triaxi_E1(floateq_abstol, 0, #exp ", " #act ", " #tol, exp, act, tol)
# define triax_assert_floateq_abstol(exp, act, tol)                                                \
    triaxi_A1(floateq_abstol, 0, #exp ", " #act ", " #tol, exp, act, tol)
# define triax_expect_floatneq_abstol(exp, act, tol)                                               \
    triaxi_E1(floateq_abstol, 1, #exp ", " #act ", " #tol, exp, act, tol)
# define triax_assert_floatneq_abstol(exp, act, tol)                                               \
    triaxi_A1(floateq_abstol, 1, #exp ", " #act ", " #tol, exp, act, tol)
// Relative tolerance: |exp - act| / max(|exp|, |act|) <= tol
# define triax_expect_floateq_reltol(exp, act, tol)                                                \
    triaxi_E1(floateq_reltol, 0, #exp ", " #act ", " #tol, exp, act, tol)
# define triax_assert_floateq_reltol(exp, act, tol)                                                \
    triaxi_A1(floateq_reltol, 0, #exp ", " #act ", " #tol, exp, act, tol)
# define triax_expect_floatneq_reltol(exp, act, tol)                                               \
    triaxi_E1(floateq_reltol, 1, #exp ", " #act ", " #tol, exp, act, tol)
# define triax_assert_floatneq_reltol(exp, act, tol)                                               \
    triaxi_A1(floateq_reltol, 1, #exp ", " #act ", " #tol, exp, act, tol)

// ── Memory
// ────────────────────────────────────────────────────────────────────
# define triax_expect_memeq(ptr1, ptr2, siz)                                                       \
    triaxi_E1(memeq, 0, #ptr1 ", " #ptr2 ", " #siz, ptr1, ptr2, siz)
# define triax_assert_memeq(ptr1, ptr2, siz)                                                       \
    triaxi_A1(memeq, 0, #ptr1 ", " #ptr2 ", " #siz, ptr1, ptr2, siz)
# define triax_expect_memneq(ptr1, ptr2, siz)                                                      \
    triaxi_E1(memeq, 1, #ptr1 ", " #ptr2 ", " #siz, ptr1, ptr2, siz)
# define triax_assert_memneq(ptr1, ptr2, siz)                                                      \
    triaxi_A1(memeq, 1, #ptr1 ", " #ptr2 ", " #siz, ptr1, ptr2, siz)

/// @brief Test if ptr[0..siz-1] is all-zero / not all-zero
# define triax_expect_memzero(ptr, siz)  triaxi_E1(memzero, 0, #ptr ", " #siz, ptr, siz)
# define triax_assert_memzero(ptr, siz)  triaxi_A1(memzero, 0, #ptr ", " #siz, ptr, siz)
# define triax_expect_memnzero(ptr, siz) triaxi_E1(memzero, 1, #ptr ", " #siz, ptr, siz)
# define triax_assert_memnzero(ptr, siz) triaxi_A1(memzero, 1, #ptr ", " #siz, ptr, siz)

// ── Strings
// ───────────────────────────────────────────────────────────────────

/// @brief String-slice type used by string/stream assertions.
/// In C++, this Implicitly constructs from `const char*`, `std::string`,
/// `std::string_view` (if available). Use @ref triax_str(ptr, len) to pass a
/// non-null-terminated buffer.
typedef struct Triax_Str {
  const char* str;
  size_t      len;
# if TRIAXI_CPP
  Triax_Str() : str{nullptr}, len{0} {}
  Triax_Str(const char* s, size_t l) : str{s}, len{l} {}
  Triax_Str(const char* s) : str{s}, len{s ? strlen(s) : 0} {}
  Triax_Str(const std::string& s) : str{s.empty() ? "" : s.data()}, len{s.size()} {}
#  ifdef __cpp_lib_string_view
  Triax_Str(std::string_view s) : str{s.empty() ? "" : s.data()}, len{s.size()} {}
  operator std::string_view() const { return {str, len}; }
#  endif
# endif
} Triax_Str;

/// @brief Creates a @ref Triax_Str object from a pointer and a length
static inline Triax_Str triax_str(const char* ptr, size_t len);

/// @name Captured byte access
///
/// These functions allocate memory on the sender side. Returned strings remain
/// valid until the current test ends and are freed automatically.
///
/// @{

/// @brief Reads the complete contents of a file.
///
/// Opens the file at @p path, reads its contents into memory, and closes the
/// file before returning.
///
/// @param path NUL-terminated valid path to the file.
/// @return A string view over the file contents.
/// @note Fails the current test if the file cannot be opened or read.
static inline Triax_Str triax_read_file(const char* path);

/// @brief Returns the output captured from `stdout`.
/// @return A string view over all bytes written to `stdout` since the current
/// test started.
static inline Triax_Str triax_read_stdout(void);

/// @brief Returns the output captured from `stderr`.
/// @return A string view over all bytes written to `stderr` since the current
/// test started.
static inline Triax_Str triax_read_stderr(void);

/// @}

///@brief Accept (const) char*, std::string, std::string_view (C++17), or
/// Triax_Str (create via @ref triax_str(str))
/// null pointers are safe to use and compare equal to only each
/// other. to pass pointer-length pairs (which may contain embedded nulls), one
/// may pass a Triax_Str object (either directly or by using the @ref triax_str()
/// function) NULL strings are treated safely as strings different from all
/// nonnull strings.
# define triax_expect_streq(str1, str2)                                                            \
    triaxi_E1(streq, 0, #str1 ", " #str2, TRIAXI_STR(str1), TRIAXI_STR(str2))
# define triax_assert_streq(str1, str2)                                                            \
    triaxi_A1(streq, 0, #str1 ", " #str2, TRIAXI_STR(str1), TRIAXI_STR(str2))
# define triax_expect_strneq(str1, str2)                                                           \
    triaxi_E1(streq, 1, #str1 ", " #str2, TRIAXI_STR(str1), TRIAXI_STR(str2))
# define triax_assert_strneq(str1, str2)                                                           \
    triaxi_A1(streq, 1, #str1 ", " #str2, TRIAXI_STR(str1), TRIAXI_STR(str2))

# define triax_expect_str_startswith(str, pref)                                                    \
    triaxi_E1(str_startswith, 0, #str ", " #pref, TRIAXI_STR(str), TRIAXI_STR(pref))
# define triax_assert_str_startswith(str, pref)                                                    \
    triaxi_A1(str_startswith, 0, #str ", " #pref, TRIAXI_STR(str), TRIAXI_STR(pref))
# define triax_expect_str_nstartswith(str, pref)                                                   \
    triaxi_E1(str_startswith, 1, #str ", " #pref, TRIAXI_STR(str), TRIAXI_STR(pref))
# define triax_assert_str_nstartswith(str, pref)                                                   \
    triaxi_A1(str_startswith, 1, #str ", " #pref, TRIAXI_STR(str), TRIAXI_STR(pref))

# define triax_expect_str_endswith(str, suf)                                                       \
    triaxi_E1(str_endswith, 0, #str ", " #suf, TRIAXI_STR(str), TRIAXI_STR(suf))
# define triax_assert_str_endswith(str, suf)                                                       \
    triaxi_A1(str_endswith, 0, #str ", " #suf, TRIAXI_STR(str), TRIAXI_STR(suf))
# define triax_expect_str_nendswith(str, suf)                                                      \
    triaxi_E1(str_endswith, 1, #str ", " #suf, TRIAXI_STR(str), TRIAXI_STR(suf))
# define triax_assert_str_nendswith(str, suf)                                                      \
    triaxi_A1(str_endswith, 1, #str ", " #suf, TRIAXI_STR(str), TRIAXI_STR(suf))

# define triax_expect_str_contains(str, needle)                                                    \
    triaxi_E1(str_contains, 0, #str ", " #needle, TRIAXI_STR(str), TRIAXI_STR(needle))
# define triax_assert_str_contains(str, needle)                                                    \
    triaxi_A1(str_contains, 0, #str ", " #needle, TRIAXI_STR(str), TRIAXI_STR(needle))
# define triax_expect_str_ncontains(str, needle)                                                   \
    triaxi_E1(str_contains, 1, #str ", " #needle, TRIAXI_STR(str), TRIAXI_STR(needle))
# define triax_assert_str_ncontains(str, needle)                                                   \
    triaxi_A1(str_contains, 1, #str ", " #needle, TRIAXI_STR(str), TRIAXI_STR(needle))

// ── Process
// ───────────────────────────────────────────────────────────────────
/// @brief Platform-agnostic fault wrappers: These abstract over types of errors
typedef uint32_t Triax_Fault;
# define TRIAX_FAULT_ANY ((Triax_Fault)(-1))
# ifndef _WIN32
#  define TRIAX_FAULT_MEMORY     ((Triax_Fault)SIGSEGV)
#  define TRIAX_FAULT_ABORT      ((Triax_Fault)SIGABRT)
#  define TRIAX_FAULT_ILLEGAL_OP ((Triax_Fault)SIGILL)
#  define TRIAX_FAULT_ARITHMETIC ((Triax_Fault)SIGFPE)
#  define TRIAX_FAULT_BREAKPOINT ((Triax_Fault)SIGTRAP)
# else
#  define TRIAX_FAULT_ABORT      ((Triax_Fault)0x40000015UL)
#  define TRIAX_FAULT_MEMORY     ((Triax_Fault)EXCEPTION_ACCESS_VIOLATION)
#  define TRIAX_FAULT_ILLEGAL_OP ((Triax_Fault)EXCEPTION_ILLEGAL_INSTRUCTION)
#  define TRIAX_FAULT_ARITHMETIC ((Triax_Fault)EXCEPTION_INT_DIVIDE_BY_ZERO)
#  define TRIAX_FAULT_BREAKPOINT ((Triax_Fault)EXCEPTION_BREAKPOINT)
# endif

/// @brief Asserts that @p ... terminates the process with a fault matching @p
/// fault.
/// @param fault  Expected crash reason (@ref Triax_Fault). `TRIAX_FAULT_ANY`
/// accepts any crash.
/// @param ...    Code block to execute, evaluated as a statement.
# define triax_assert_fault(fault, ...)                                                            \
    triaxi_assert_fault(0, 1, fault, #fault ", " #__VA_ARGS__, __VA_ARGS__)
# define triax_expect_fault(fault, ...)                                                            \
    triaxi_assert_fault(0, 0, fault, #fault ", " #__VA_ARGS__, __VA_ARGS__)
# define triax_assert_nfault(...) triaxi_assert_fault(1, 1, 0, #__VA_ARGS__ " ", __VA_ARGS__)
# define triax_expect_nfault(...) triaxi_assert_fault(1, 0, 0, #__VA_ARGS__ " ", __VA_ARGS__)

# define TRIAX_EXIT_ANY           UINT32_MAX

/// @brief Test that @p ... calls `exit()` with a code matching @p exit_code.
/// @param code       Expected exit code; pass `TRIAX_EXIT_ANY` to accept any.
/// @param ...        Code block to execute, evaluated as a statement.
/// @note Requires process isolation. Using this assertion without isolation
/// reports a test error because exit() cannot be safely intercepted in-process.
# define triax_assert_exit(code, ...)                                                              \
    triaxi_assert_exit(0, 1, code, #code ", " #__VA_ARGS__, __VA_ARGS__)
# define triax_expect_exit(code, ...)                                                              \
    triaxi_assert_exit(0, 0, code, #code ", " #__VA_ARGS__, __VA_ARGS__)
# define triax_assert_nexit(...) triaxi_assert_exit(1, 1, 0, #__VA_ARGS__ " ", __VA_ARGS__)
# define triax_expect_nexit(...) triaxi_assert_exit(1, 0, 0, #__VA_ARGS__ " ", __VA_ARGS__)
/// @}
/// @} // triax_test_module

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////  Implementation   /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

# define TRIAXI_DO_PRAGMA(a)     _Pragma(#a)
# if TRIAXI_GNU_COMPAT
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpragmas"
#  pragma GCC diagnostic ignored "-Wgnu-anonymous-struct"
#  pragma GCC diagnostic ignored "-Wnested-anon-types"
#  pragma GCC diagnostic ignored "-Wc99-extensions"
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#  pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#  pragma GCC diagnostic ignored "-Wc2x-extensions"
#  pragma GCC diagnostic ignored "-Wunused-function"
#  define TRIAXI_IGNWARN_GNU_BEG(warn)                                                             \
     TRIAXI_DO_PRAGMA(GCC diagnostic push)                                                         \
     TRIAXI_DO_PRAGMA(GCC diagnostic ignored warn)
#  define TRIAXI_IGNWARN_GNU_END TRIAXI_DO_PRAGMA(GCC diagnostic pop)
# else
#  pragma warning(push)
#  pragma warning(disable : 4996)

#  define TRIAXI_IGNWARN_GNU_BEG(warn)
#  define TRIAXI_IGNWARN_GNU_END
# endif
# pragma region implementation
# pragma region common_utilities
# define TRIAXI_WRONGMODULE_ERR(FNAME)                                                             \
    static_assert(0,                                                                               \
                  #FNAME " cannot be used in a non-runner translation unit — define "              \
                         "TRIAX_IMPL in exactly one translation unit or disable TRIAX_MULTI_TU.")

# if TRIAXI_MSVC_COMPAT
#  define triaxi_restrict __restrict
# elif TRIAXI_GNU_COMPAT
#  define triaxi_restrict __restrict__
# elif TRIAXI_C >= 199901L
#  define triaxi_restrict restrict
# else
#  define triaxi_restrict
# endif

# if TRIAXI_CPP
#  define triaxi_alignas(x)       alignas(x)
#  define TRIAXI_EXTERN_FOR_CONST extern
#  define TRIAXI_EXTERN_C_BEG     extern "C" {
#  define TRIAXI_EXTERN_C_END     }
#  define TRIAXI_ZINIT
#  define TRIAXI_T(T) T
# else
#  define triaxi_alignas(x) _Alignas(x)
#  define TRIAXI_EXTERN_FOR_CONST
#  define TRIAXI_EXTERN_C_BEG
#  define TRIAXI_EXTERN_C_END
#  define TRIAXI_ZINIT 0
#  define TRIAXI_T(T)  (T)
# endif

# ifdef unreachable
#  define triaxi_unreachable() unreachable()
# elif TRIAXI_CPP >= 202302L
#  define triaxi_unreachable() std::unreachable()
# elif TRIAXI_GNU_COMPAT
#  define triaxi_unreachable() __builtin_unreachable()
# elif TRIAXI_MSVC_COMPAT
#  define triaxi_unreachable() __assume(0)
# else
#  define triaxi_unreachable() (assert(0 && "unreachable code reached"), triaxi_fatal())
# endif

# if TRIAXI_CPP >= 201103L
#  define triaxi_noreturn [[noreturn]]
# elif TRIAXI_C >= 201112L
#  define triaxi_noreturn _Noreturn
# elif TRIAXI_MSVC_COMPAT
#  define triaxi_noreturn __declspec(noreturn)
# elif TRIAXI_GNU_COMPAT
#  define triaxi_noreturn __attribute__((__noreturn__))
# else
#  define triaxi_noreturn
# endif

# define triaxi_countof(...)     (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))
# define triaxi_lenof(lit)       (sizeof("" lit "") - 1)

# define TRIAXI_STRINGIFY_(x)    #x
# define TRIAXI_STRINGIFY(x)     TRIAXI_STRINGIFY_(x)
# define TRIAXI_CONCAT_(a, b)    a##b
# define TRIAXI_CONCAT(a, b)     TRIAXI_CONCAT_(a, b)
# define TRIAXI_COMMA_ONE(x)     x,

# define TRIAXI_COUNT_ONE(x)     +1
# define TRIAXI_COUNT_ALL(X)     (0 + X(TRIAXI_COUNT_ONE, ))

# define TRIAXI_STRINGIFY_ONE(x) #x,
# define TRIAXI_STRINGIFY_ALL(X) X(TRIAXI_STRINGIFY_ONE, )

# define TRIAXI_MIN(x, y)        ((x) <= (y) ? (x) : (y))
# define TRIAXI_MAX(x, y)        ((x) >= (y) ? (x) : (y))

static inline size_t triaxi_catstr(void* dst, const void* src, size_t len) {
  return memcpy(dst, src, len), len;
}
# define triaxi_catlit(dst, lit)     (memcpy(dst, lit, triaxi_lenof(lit)), triaxi_lenof(lit))
# define triaxi_fputlit(str, stream) fwrite(str, 1, triaxi_lenof(str), stream)

TRIAXI_EXTERN_C_BEG triaxi_noreturn void triaxi_fatal_f(const char* file, int line);
TRIAXI_EXTERN_C_END
# define triaxi_fatal() triaxi_fatal_f(__FILE__, __LINE__)

triaxi_noreturn static inline void triaxi_user_error_f(uint32_t error, uint32_t syserr);
# define triaxi_user_error(error) triaxi_user_error_f((error), 0)
# ifndef _WIN32
#  define triaxi_user_syserror(error) triaxi_user_error_f((error), (uint32_t)errno)
# else
#  define triaxi_user_syserror(error) triaxi_user_error_f((error), (uint32_t)GetLastError())
# endif

static inline Triax_Str triax_str(const char* ptr, size_t len) {
  return TRIAXI_T(Triax_Str){ptr, len};
}
static inline Triax_Str triaxi_tostr(const char* cstr) {
  return triax_str(cstr, cstr ? strlen(cstr) : 0);
}
# if !TRIAXI_CPP
#  define TRIAXI_CONTRAV(T, val) _Generic(val, T: val, default: (T){0})
#  define TRIAXI_STR(str)                                                                          \
     _Generic(str,                                                                                 \
         Triax_Str: TRIAXI_CONTRAV(Triax_Str, str),                                                \
         char*: triaxi_tostr(TRIAXI_CONTRAV(char*, str)),                                          \
         const char*: triaxi_tostr(TRIAXI_CONTRAV(const char*, str)))
# else
#  define TRIAXI_STR(str) Triax_Str(str)
# endif

static inline void triaxi_flush_all(void) {
  fflush(NULL);
# if TRIAXI_CPP
  std::cout.flush(), std::cerr.flush();
# endif
}

static inline int64_t triaxi_now_ms(void) {
# ifndef _WIN32
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) { triaxi_fatal(); };
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
# else
  static LARGE_INTEGER freq;
  LARGE_INTEGER        counter;
  if (!freq.QuadPart && !QueryPerformanceFrequency(&freq)) { triaxi_fatal(); }
  if (!QueryPerformanceCounter(&counter)) { triaxi_fatal(); }
  return (counter.QuadPart * 1000LL) / freq.QuadPart;
# endif
}
static inline uint32_t triaxi_elapsed_ms(int64_t t0) { return (uint32_t)(triaxi_now_ms() - t0); }

# ifndef _WIN32
typedef int TRIAXI_File;
#  define triaxi_file_close(f) close(f)
#  define triaxi_file_valid(f) ((f) >= 0)
# else
typedef HANDLE TRIAXI_File;
#  define triaxi_file_close(f) CloseHandle(f)
#  define triaxi_file_valid(f) ((f) != INVALID_HANDLE_VALUE)
# endif

static inline TRIAXI_File triaxi_file_open_r(const char* path) {
# ifndef _WIN32
  return open(path, O_RDONLY);
# else
  return CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL, NULL);
# endif
}

static inline size_t triaxi_file_size(TRIAXI_File f) {
# ifndef _WIN32
  struct stat st;
  if (fstat(f, &st)) { triaxi_fatal(); }
  if (st.st_size < 0) { triaxi_fatal(); }
  if ((uintmax_t)st.st_size > SIZE_MAX) { triaxi_fatal(); }
  return (size_t)st.st_size;
# else
  LARGE_INTEGER size64;
  if (!GetFileSizeEx(f, &size64)) { triaxi_fatal(); }
  if (size64.QuadPart < 0) { triaxi_fatal(); }
  if ((uintmax_t)size64.QuadPart > SIZE_MAX) { triaxi_fatal(); }
  return (size_t)size64.QuadPart;
# endif
}
static inline void triaxi_file_read_into(TRIAXI_File f, char* str, size_t len) {
# ifndef _WIN32
  const size_t max_chunk = (size_t)(SIZE_MAX >> 1);
  for (size_t pos = 0; pos < len;) {
    ssize_t r = pread(f, str + pos, TRIAXI_MIN(len - pos, max_chunk), (off_t)pos);
    if (r == -1) {
      if (errno == EINTR) { continue; }
      triaxi_fatal();
    } else if (r == 0) {
      triaxi_fatal();
    }
    pos += (size_t)r;
  }
# else
  LARGE_INTEGER z = {TRIAXI_ZINIT}, orig;
  if (!SetFilePointerEx(f, z, &orig, FILE_CURRENT) || !SetFilePointerEx(f, z, NULL, FILE_BEGIN)) {
    triaxi_fatal();
  }
  for (uint64_t pos = 0; pos < len;) {
    uint64_t nbytes = len - pos;
    DWORD    chunk  = (DWORD)((nbytes > 0xFFFFFFFFUL) ? 0xFFFFFFFFUL : nbytes);
    DWORD    r      = 0;
    if (!ReadFile(f, str + pos, chunk, &r, NULL) || !r) { triaxi_fatal(); }
    pos += r;
  }
  if (!SetFilePointerEx(f, orig, NULL, FILE_BEGIN)) { triaxi_fatal(); }
# endif
}

static inline const char* triaxi_fault_tostr(Triax_Fault reason) {
# define TRIAXI_FAULT_NONE 0
  if (reason == TRIAXI_FAULT_NONE) { return "No fault"; }
  if (reason == TRIAX_FAULT_ANY) { return "Any fault"; }
# ifndef _WIN32
  const char* s = strsignal((int)reason);
  return s ? s : "unknown signal";
# else
  char        buf[512];
  static char resbuf[sizeof(buf) + sizeof("NTSTATUS: ")];
  DWORD       code = (DWORD)reason;
  DWORD       len;
  // Fault codes are NTSTATUS values — try ntdll first, Win32 as fallback.
  HMODULE     ntdll = GetModuleHandleA("ntdll.dll");
  if (ntdll) {
    len = FormatMessageA(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS, ntdll, code,
                         0, buf, sizeof(buf), NULL);
    if (len) {
      while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n')) { buf[--len] = '\0'; }
      snprintf(resbuf, sizeof(resbuf), "NTSTATUS: %s", buf);
      return resbuf;
    }
  }
  len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, 0,
                       buf, sizeof(buf), NULL);
  if (len) {
    while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n')) { buf[--len] = '\0'; }
    snprintf(resbuf, sizeof(resbuf), "Win32: %s", buf);
    return resbuf;
  }
  snprintf(resbuf, sizeof(resbuf), "Unknown fault code: 0x%08" PRIX32, (uint32_t)code);
  return resbuf;
# endif
}

# pragma endregion common_utilities

# pragma region common_types

typedef struct TRIAXI_Params {
  size_t      elsize, elcount;
  const void* ptr;
} TRIAXI_Params;

typedef struct TRIAXI_TestConfig {
  char            TRIAXI_dummy;
  bool            skip;
  Triax_Verbosity verbosity;
  Triax_Isolation isolation;
  uint32_t        timeout_ms;
  const char*     tags;
  Triax_Fixture   init, fini;
  TRIAXI_Params   params;
} TRIAXI_TestConfig;
static_assert(offsetof(TRIAXI_TestConfig, params) == sizeof(Triax_Attributes), "layout drift");

# if TRIAXI_GNU_COMPAT
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpedantic"
# else
#  pragma warning(push)
#  pragma warning(disable : 4201)
#  pragma warning(disable : 5274)
# endif

typedef struct TRIAXI_Test {
  const char* suitename;
  const char* name;
  const char* file;
  void (*func)(void);
  union {
    TRIAXI_TestConfig config; // user-facing initialisation
    struct {
      Triax_Attributes attrs;
      TRIAXI_Params    params;
    };
  };
# if TRIAXI_CPP
  constexpr TRIAXI_Test(const char* suitename_, const char* name_, const char* file_,
                        void (*const func_)(void), TRIAXI_TestConfig config_)
      : suitename{suitename_}, name{name_}, file{file_}, func{func_},
        attrs{config_.TRIAXI_dummy, config_.skip, config_.verbosity, config_.isolation,
              config_.timeout_ms,   config_.tags, config_.init,      config_.fini},
        params{config_.params} {}
  constexpr TRIAXI_Test() : suitename{}, name{}, file{}, func{}, attrs{}, params{} {}
# endif
} TRIAXI_Test;

# if TRIAXI_GNU_COMPAT
#  pragma GCC diagnostic pop
# else
#  pragma warning(pop)
# endif

typedef struct TRIAXI_SuiteReg {
  const char*      name;
  Triax_Attributes attrs; // user-facing initialisation
} TRIAXI_SuiteReg;

# pragma endregion common_types

# pragma region registration_macros

# if !TRIAXI_CPP /* clang-format off */
#  define TRIAXI_Test_attrs_init(...) {{0, __VA_ARGS__}}
#  define TRIAXI_Suite_attrs_init(...) {0, __VA_ARGS__}
#  define TRIAXI_as_params(arr) {sizeof(*(arr)), triaxi_countof(arr), (arr)}
#   else
#  define TRIAXI_Test_attrs_init(...) TRIAXI_TestConfigBuilder{TRIAXI_TestConfigBuilder{} __VA_ARGS__}
#  define TRIAXI_Suite_attrs_init(...) TRIAXI_AttributeBuilder{} __VA_ARGS__
#  define TRIAXI_as_params(arr) arr /* clang-format on */
template <typename D>
struct TRIAXI_AttributeBuilderBase {
  constexpr D skip(bool v = true) const {
    return static_cast<const D*>(this)->make(
        {0, v, _.verbosity, _.isolation, _.timeout_ms, _.tags, _.init, _.fini});
  }
  constexpr D verbosity(Triax_Verbosity v) const {
    return static_cast<const D*>(this)->make(
        {0, _.skip, v, _.isolation, _.timeout_ms, _.tags, _.init, _.fini});
  }
  constexpr D isolation(Triax_Isolation v) const {
    return static_cast<const D*>(this)->make(
        {0, _.skip, _.verbosity, v, _.timeout_ms, _.tags, _.init, _.fini});
  }
  constexpr D timeout_ms(uint32_t v) const {
    return static_cast<const D*>(this)->make(
        {0, _.skip, _.verbosity, _.isolation, v, _.tags, _.init, _.fini});
  }
  constexpr D tags(const char* v) const {
    return static_cast<const D*>(this)->make(
        {0, _.skip, _.verbosity, _.isolation, _.timeout_ms, v, _.init, _.fini});
  }
  constexpr D init(Triax_Fixture v) const {
    return static_cast<const D*>(this)->make(
        {0, _.skip, _.verbosity, _.isolation, _.timeout_ms, _.tags, v, _.fini});
  }
  constexpr D fini(Triax_Fixture v) const {
    return static_cast<const D*>(this)->make(
        {0, _.skip, _.verbosity, _.isolation, _.timeout_ms, _.tags, _.init, v});
  }

protected:
  Triax_Attributes _{};
  constexpr TRIAXI_AttributeBuilderBase() {}
  constexpr TRIAXI_AttributeBuilderBase(Triax_Attributes a) : _{a} {}
};

struct TRIAXI_AttributeBuilder : TRIAXI_AttributeBuilderBase<TRIAXI_AttributeBuilder> {
  constexpr TRIAXI_AttributeBuilder() {}
  constexpr TRIAXI_AttributeBuilder make(Triax_Attributes a) const {
    return TRIAXI_AttributeBuilder{a};
  }
  constexpr operator Triax_Attributes() const { return _; }

private:
  constexpr TRIAXI_AttributeBuilder(Triax_Attributes a) : TRIAXI_AttributeBuilderBase{a} {}
};

struct TRIAXI_TestConfigBuilder : TRIAXI_AttributeBuilderBase<TRIAXI_TestConfigBuilder> {
  constexpr TRIAXI_TestConfigBuilder() {}
  constexpr TRIAXI_TestConfigBuilder make(Triax_Attributes a) const {
    return TRIAXI_TestConfigBuilder{a, params};
  }
  template <typename T, size_t N>
  constexpr TRIAXI_TestConfigBuilder parameterize(const T (&arr)[N]) const {
    return TRIAXI_TestConfigBuilder{_, {sizeof(T), N, arr}};
  }
  constexpr operator TRIAXI_TestConfig() const {
    return TRIAXI_TestConfig{0,      _.skip, _.verbosity, _.isolation, _.timeout_ms,
                             _.tags, _.init, _.fini,      params};
  }

private:
  TRIAXI_Params params{};
  constexpr TRIAXI_TestConfigBuilder(Triax_Attributes a, TRIAXI_Params p)
      : TRIAXI_AttributeBuilderBase{a}, params{p} {}
};
# endif
// no_sanitize_address prevents ASan from inserting 32-byte redzones between
// entries in the section — without it, getsectiondata / __start_/__stop_
// iteration reads into the redzones and triggers global-buffer-overflow.

// Keep PE/COFF section names short so GNU/MinGW and MSVC linkers can both
// represent them without relying on long-name extensions.
# if TRIAXI_REG_MSVC_COFF
#  pragma section("TRXTST$m", read)
#  pragma section("TRXSUT$m", read)
#  ifdef _M_IX86
#   define TRIAXI_LINKER_USE(x) __pragma(comment(linker, "/include:_" TRIAXI_STRINGIFY(x)))
#  else
#   define TRIAXI_LINKER_USE(x) __pragma(comment(linker, "/include:" TRIAXI_STRINGIFY(x)))
#  endif
#  define TRIAXI_TEST_SECTION  __declspec(allocate("TRXTST$m")) __declspec(no_sanitize_address)
#  define TRIAXI_SUITE_SECTION __declspec(allocate("TRXSUT$m")) __declspec(no_sanitize_address)
# elif TRIAXI_REG_MACHO || TRIAXI_REG_GNU_SECTION
#  define TRIAXI_LINKER_USE(x)
#  if TRIAXI_CLANG
#   define TRIAXI_NOSANITIZE no_sanitize_address
#  else
#   define TRIAXI_NOSANITIZE
#  endif
#  if TRIAXI_REG_MACHO
#   define TRIAXI_TEST_SECTION                                                                     \
      __attribute__((used, section("__DATA,TRXTST"), aligned(__alignof__(TRIAXI_Test)),            \
                     TRIAXI_NOSANITIZE))
#   define TRIAXI_SUITE_SECTION                                                                    \
      __attribute__((used, section("__DATA,TRXSUT"), aligned(__alignof__(TRIAXI_SuiteReg)),        \
                     TRIAXI_NOSANITIZE))
#  elif defined(_WIN32)
// GNU PE/COFF: omit `retain` because that attribute is ELF-oriented and is not
// consistently supported by MinGW toolchains. References to __start/__stop
// keep the merged registration sections reachable from the executable.
#   define TRIAXI_TEST_SECTION                                                                     \
      __attribute__((used, section("TRXTST"), aligned(__alignof__(TRIAXI_Test)), TRIAXI_NOSANITIZE))
#   define TRIAXI_SUITE_SECTION                                                                    \
      __attribute__((used, section("TRXSUT"), aligned(__alignof__(TRIAXI_SuiteReg)),               \
                     TRIAXI_NOSANITIZE))
#  else
#   define TRIAXI_TEST_SECTION                                                                     \
      __attribute__((used, section("TRXTST"), retain, aligned(__alignof__(TRIAXI_Test)),           \
                     TRIAXI_NOSANITIZE))
#   define TRIAXI_SUITE_SECTION                                                                    \
      __attribute__((used, section("TRXSUT"), retain, aligned(__alignof__(TRIAXI_SuiteReg)),       \
                     TRIAXI_NOSANITIZE))
#  endif
# else
#  error "Triax: unsupported compiler/linker registration backend"
# endif

# if TRIAXI_REG_MSVC_COFF
// MSVC COFF $a/$m/$z subsections are concatenated in that order, but nothing
// guarantees tight packing of an arbitrary struct's size/alignment across
// that concatenation the way triaxi_sections_init()'s sentinel-plus-one
// pointer arithmetic would need — a linker that aligns the next subsection's
// contribution more strongly than TRIAXI_Test's own alignment could leave
// that arithmetic landing in padding rather than the first real entry, with
// every field after that misread. Registering a *pointer* to each object
// instead of the object itself avoids this: every section entry is then a
// plain pointer — same size and natural alignment as every other entry, so
// there's no padding for the arithmetic to land in — matching the standard
// .CRT$XCU-style registration idiom. The actual test/suite data lives in an
// ordinary internal-linkage object declared alongside it; only its address
// goes in the section.
#  define TRIAXI_MAKE_TEST(x, ...)                                                                 \
     static const TRIAXI_Test                         x##_obj = __VA_ARGS__;                       \
     TRIAXI_TEST_SECTION                              TRIAXI_LINKER_USE(x)                         \
     TRIAXI_EXTERN_FOR_CONST const TRIAXI_Test* const x = &x##_obj
#  define TRIAXI_MAKE_SUITEREG(x, ...)                                                             \
     static const TRIAXI_SuiteReg                         x##_obj = __VA_ARGS__;                   \
     TRIAXI_SUITE_SECTION                                 TRIAXI_LINKER_USE(x)                     \
     TRIAXI_EXTERN_FOR_CONST const TRIAXI_SuiteReg* const x = &x##_obj
# else
#  define TRIAXI_MAKE_TEST(x, ...)                                                                 \
     TRIAXI_TEST_SECTION                       TRIAXI_LINKER_USE(x)                                \
     TRIAXI_EXTERN_FOR_CONST const TRIAXI_Test x = __VA_ARGS__
#  define TRIAXI_MAKE_SUITEREG(x, ...)                                                             \
     TRIAXI_SUITE_SECTION                          TRIAXI_LINKER_USE(x)                            \
     TRIAXI_EXTERN_FOR_CONST const TRIAXI_SuiteReg x = __VA_ARGS__
# endif

# if defined(TRIAX_IMPL) || !defined(TRIAX_MULTI_TU)
#  if TRIAXI_REG_MSVC_COFF
#   pragma section("TRXTST$a", read)
#   pragma section("TRXTST$z", read)
#   pragma section("TRXSUT$a", read)
#   pragma section("TRXSUT$z", read)
// Bounds sentinels: plain null pointers, not object values — see the
// TRIAXI_MAKE_TEST comment above for why pointers are what makes the $a/$m/$z
// concatenation safe to walk.
__declspec(allocate("TRXTST$a")) const TRIAXI_Test* const     TRIAXI_Test_a     = NULL;
__declspec(allocate("TRXTST$z")) const TRIAXI_Test* const     TRIAXI_Test_z     = NULL;
__declspec(allocate("TRXSUT$a")) const TRIAXI_SuiteReg* const TRIAXI_SuiteReg_a = NULL;
__declspec(allocate("TRXSUT$z")) const TRIAXI_SuiteReg* const TRIAXI_SuiteReg_z = NULL;
#  elif TRIAXI_REG_GNU_SECTION
// __start_SECNAME/__stop_SECNAME are only auto-defined by the linker if the
// named section actually exists in the link; a program with no triax_test (or
// no triax_suite) would otherwise leave them undefined and fail to link. Force
// both sections to always exist. triaxi_register_tests/triaxi_register_suites
// already skip zero-initialised (name == NULL) entries.
TRIAXI_EXTERN_C_BEG
TRIAXI_MAKE_TEST(TRIAXI_Test_sentinel, {TRIAXI_ZINIT});
TRIAXI_MAKE_SUITEREG(TRIAXI_SuiteReg_sentinel, {TRIAXI_ZINIT});
TRIAXI_EXTERN_C_END
#  endif
# endif

# if (defined(TRIAX_IMPL) || !defined(TRIAX_MULTI_TU)) && TRIAXI_REG_GNU_SECTION
TRIAXI_EXTERN_C_BEG
extern const TRIAXI_Test     __start_TRXTST, __stop_TRXTST;
extern const TRIAXI_SuiteReg __start_TRXSUT, __stop_TRXSUT;
TRIAXI_EXTERN_C_END
# endif

# if !TRIAXI_CPP
#  define triaxi_test(suitename, name, ...)                                                        \
     static void triaxf_##suitename##_##name(void);                                                \
     TRIAXI_IGNWARN_GNU_BEG("-Wmissing-field-initializers")                                        \
     TRIAXI_MAKE_TEST(TRIAXI_test_##suitename##_##name,                                            \
                      {#suitename, #name, __FILE__, triaxf_##suitename##_##name,                   \
                       TRIAXI_Test_attrs_init(__VA_ARGS__)});                                      \
     TRIAXI_IGNWARN_GNU_END                                                                        \
     static void triaxf_##suitename##_##name(void)
#  define triaxi_suite(name, ...)                                                                  \
     TRIAXI_IGNWARN_GNU_BEG("-Wmissing-field-initializers")                                        \
     TRIAXI_MAKE_SUITEREG(TRIAXI_scfg_##name, {#name, TRIAXI_Suite_attrs_init(__VA_ARGS__)});      \
     TRIAXI_IGNWARN_GNU_END typedef char TRIAXI_DUMMY

# else
#  define triaxi_test(suitename, name, ...)                                                        \
     static void triaxf_##suitename##_##name(void);                                                \
     extern "C" {                                                                                  \
       static void triaxfwrapped_##suitename##_##name(void) {                                      \
         TRIAXI_ExecOutcome outcome;                                                               \
         try {                                                                                     \
           triaxf_##suitename##_##name();                                                          \
           return;                                                                                 \
         } catch (TRIAXI_TestException abort) { outcome = abort.outcome; } catch (...) {           \
           outcome = TRIAXI_EXEC_EXCEPTION;                                                        \
         }                                                                                         \
         longjmp(TRIAXI_exec.jmp, (int)outcome);                                                   \
       }                                                                                           \
       TRIAXI_IGNWARN_GNU_BEG("-Wmissing-field-initializers")                                      \
       TRIAXI_MAKE_TEST(TRIAXI_test_##suitename##_##name,                                          \
                        TRIAXI_Test(#suitename, #name, __FILE__,                                   \
                                    triaxfwrapped_##suitename##_##name,                            \
                                    TRIAXI_Test_attrs_init(__VA_ARGS__)));                         \
       TRIAXI_IGNWARN_GNU_END                                                                      \
     }                                                                                             \
     static void triaxf_##suitename##_##name(void)
#  define triaxi_suite(name, ...)                                                                  \
     extern "C" {                                                                                  \
       TRIAXI_IGNWARN_GNU_BEG("-Wmissing-field-initializers")                                      \
       TRIAXI_MAKE_SUITEREG(TRIAXI_scfg_##name, {#name, TRIAXI_Suite_attrs_init(__VA_ARGS__)});    \
       TRIAXI_IGNWARN_GNU_END                                                                      \
     }                                                                                             \
     typedef char TRIAXI_DUMMY

# endif

# pragma endregion registration_macros

# pragma region assert_protocol

# define TRIAXI_ASSERTTYPE_ITEMS(X, PRE)                                                           \
    X(PRE##exit)                                                                                   \
    X(PRE##fault)                                                                                  \
    X(PRE##true)                                                                                   \
    X(PRE##null)                                                                                   \
    X(PRE##eq)                                                                                     \
    X(PRE##gt)                                                                                     \
    X(PRE##lt)                                                                                     \
    X(PRE##floateq_abstol)                                                                         \
    X(PRE##floateq_reltol)                                                                         \
    X(PRE##memeq)                                                                                  \
    X(PRE##memzero)                                                                                \
    X(PRE##streq)                                                                                  \
    X(PRE##str_startswith)                                                                         \
    X(PRE##str_endswith)                                                                           \
    X(PRE##str_contains)                                                                           \
    X(PRE##check) X(PRE##arreq)

typedef enum TRIAXI_AssertType {
  TRIAXI_ASSERTTYPE_ITEMS(TRIAXI_COMMA_ONE, TRIAXI_AT_)
} TRIAXI_AssertType;
enum { TRIAXI_AT_COUNT = TRIAXI_COUNT_ALL(TRIAXI_ASSERTTYPE_ITEMS) };

# if TRIAXI_GNU_COMPAT
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wflexible-array-extensions"
#  pragma GCC diagnostic ignored "-Wpedantic"
# else
#  pragma warning(push)
#  pragma warning(disable : 5274)
#  pragma warning(disable : 4200)
# endif

typedef uint8_t TRIAXI_PacketPrefix;
enum { TRIAXI_RES_PREFIX = 0 };

typedef struct TRIAXI_AssertHdr {
  union {
    TRIAXI_PacketPrefix is_hdr;
    uint8_t             expr_len;
  };
  uint8_t  type;
  uint8_t  ae;
  uint8_t  neg;
  uint32_t cr; // code/signal
  uint32_t line;
# ifndef _WIN32
  uint8_t     reserved[4];
  const char* expr_str;
# else
  char expr_str[TRIAX_EXPR_MAX];
# endif
} TRIAXI_AssertHdr;

# define TRIAXI_ENCODING_MASK      0x0F
# define TRIAXI_ENCODING_DEFAULT   0x00
# define TRIAXI_ENCODING_STRING    0x01

# define TRIAXI_ENCODING_NULL_ARG1 0x40
# define TRIAXI_ENCODING_NULL_ARG2 0x80

typedef enum TRIAXI_AR {
  TRIAXI_AR_FAIL1 = 1,
  TRIAXI_AR_FAIL2,
  TRIAXI_AR_FAIL3,
  TRIAXI_AR_TIMEOUT,
  TRIAXI_AR_UCRASHED,
  TRIAXI_AR_UEXITED,
  TRIAXI_AR_UEXCEPT
} TRIAXI_AR;

typedef struct TRIAXI_AssertRes {
  TRIAXI_PacketPrefix is_hdr; // must be zero
  uint8_t             val;
  uint8_t             code; // encoding
  uint8_t             _pad[1];
  uint32_t            len;
  char                args[];
} TRIAXI_AssertRes;

/// @brief State of the Test
typedef enum TRIAXI_State {
  TRIAXI_STATE_SETUP,
  TRIAXI_STATE_INIT,
  TRIAXI_STATE_TEST,
  TRIAXI_STATE_IN_ASSERT,
  TRIAXI_STATE_FINI,
  TRIAXI_STATE_CLEANUP,
  TRIAXI_STATE_DONE,
  TRIAXI_STATE_SKIPPED
} TRIAXI_State;

typedef enum TRIAXI_Error {
  TRIAXI_ERROR_NONE,

  TRIAXI_ERROR_TIMEOUT_WITHOUT_ISOLATION,
  TRIAXI_ERROR_MALFORMED_PARAMS,

  TRIAXI_ERROR_PARAM_ACCESS,
  TRIAXI_ERROR_SKIP_IN_FIXTURE,
  TRIAXI_ERROR_ASSERT_IN_FIXTURE,
  TRIAXI_ERROR_EXIT_ASSERT_WITHOUT_ISOLATION,

  TRIAXI_ERROR_FILE_OPEN,
  TRIAXI_ERROR_FILE_READ,
} TRIAXI_Error;

enum {
  TRIAXI_ERROR_KIND_SHIFT = 24,
  TRIAXI_ERROR_SYS_MASK   = 0x00FFFFFFu,
};

static inline uint32_t triaxi_error_pack(TRIAXI_Error error, uint32_t syserr) {
  if ((uint32_t)error > UINT8_MAX) { triaxi_fatal(); }
  return ((uint32_t)error << TRIAXI_ERROR_KIND_SHIFT) | (syserr & TRIAXI_ERROR_SYS_MASK);
}
static inline TRIAXI_Error triaxi_error_kind(uint32_t code) {
  return (TRIAXI_Error)(code >> TRIAXI_ERROR_KIND_SHIFT);
}
static inline uint32_t triaxi_error_syserr(uint32_t code) { return code & TRIAXI_ERROR_SYS_MASK; }

/// @brief Reason the Process terminated
typedef enum TRIAXI_ExitType {
  TRIAXI_EXIT_NONE,
  TRIAXI_EXIT_TIMEOUT,
  TRIAXI_EXIT_EXIT,
  TRIAXI_EXIT_FAULT,
  TRIAXI_EXIT_EXCEPTION,
  TRIAXI_EXIT_ERROR
} TRIAXI_ExitType;

typedef struct TRIAXI_Exit {
  TRIAXI_ExitType type;
  union {
    uint32_t    code; // on posix 0-255, on windows < 0x80000000u
    Triax_Fault reason;
  };
} TRIAXI_Exit;

# ifdef _WIN32
#  define TRIAXI_INDEX_NONE SIZE_MAX
typedef struct TRIAXI_WinLaunch {
  TRIAXI_File log, out, err;
  size_t      suite_idx;
  size_t      test_idx;
  size_t      invocation_idx;
} TRIAXI_WinLaunch;
# endif

typedef struct TRIAXI_Shared {
  triaxi_alignas(64) TRIAXI_State state;
  uint32_t    duration_ms;
  TRIAXI_Exit exit;
# ifdef _WIN32
  TRIAXI_WinLaunch launch;
# endif
} TRIAXI_Shared;

# pragma endregion assert_protocol

# pragma region exec_state

# if !defined(TRIAX_MULTI_TU)
#  define TRIAXI_SHARED_LINKAGE static
# elif defined(TRIAX_IMPL)
#  define TRIAXI_SHARED_LINKAGE
# else
#  define TRIAXI_SHARED_LINKAGE extern
# endif

TRIAXI_EXTERN_C_BEG

TRIAXI_SHARED_LINKAGE FILE* TRIAXI_true_stderr; // todo broken on windows
TRIAXI_SHARED_LINKAGE struct TRIAXI_ExecState { // per-test execution state
  TRIAXI_File             log, out, err;        // files where logs, stdout, stderr are written to
  bool                    isolated, in_test, debug_break;
  const void*             param;
  jmp_buf                 jmp;
  volatile TRIAXI_Shared* shared; // volatile to ensure write before _exit()
  struct {
    void** ptrs;
    size_t cap, count;
  } allocations;
  union {
    TRIAXI_AssertRes pkg;
    char             storage[65536L];
  };
} TRIAXI_exec;

TRIAXI_EXTERN_C_END
# if TRIAXI_GNU_COMPAT
#  pragma GCC diagnostic pop
# else
#  pragma warning(pop)
# endif

static inline void* triaxi_allocate(size_t nbytes) {
  if (!nbytes) { return NULL; }
  void* ptr = malloc(nbytes);
  if (!ptr) { triaxi_fatal(); }
  if (TRIAXI_exec.allocations.count == TRIAXI_exec.allocations.cap) {
    TRIAXI_exec.allocations.cap
        = TRIAXI_exec.allocations.cap ? TRIAXI_exec.allocations.cap * 2 : 16;
    TRIAXI_exec.allocations.ptrs = (void**)realloc(TRIAXI_exec.allocations.ptrs,
                                                   sizeof(void*) * TRIAXI_exec.allocations.cap);
    if (!TRIAXI_exec.allocations.ptrs) { triaxi_fatal(); }
  }
  TRIAXI_exec.allocations.ptrs[TRIAXI_exec.allocations.count++] = ptr;
  return ptr;
}

static inline void triaxi_allocations_free_all(void) {
  for (size_t i = 0; i < TRIAXI_exec.allocations.count; ++i) {
    free(TRIAXI_exec.allocations.ptrs[i]);
  }
  free(TRIAXI_exec.allocations.ptrs);
  TRIAXI_exec.allocations.ptrs = NULL;
  TRIAXI_exec.allocations.cap = TRIAXI_exec.allocations.count = 0;
}

static inline Triax_Str triaxi_file_read(TRIAXI_File f) {
  size_t len = triaxi_file_size(f);
  if (!len) { return TRIAXI_T(Triax_Str){"", 0}; };
  char* str = (char*)triaxi_allocate(len);
  triaxi_file_read_into(f, str, len);
  return TRIAXI_T(Triax_Str){str, len};
}

static inline Triax_Str triaxi_file_read_user(TRIAXI_File f) {
# ifndef _WIN32
  struct stat st;
  if (fstat(f, &st)) { triaxi_user_syserror(TRIAXI_ERROR_FILE_READ); }
  if (st.st_size < 0 || (uintmax_t)st.st_size > SIZE_MAX) {
    triaxi_user_error(TRIAXI_ERROR_FILE_READ);
  }
  size_t len = (size_t)st.st_size;
# else
  LARGE_INTEGER size64;
  if (!GetFileSizeEx(f, &size64)) { triaxi_user_syserror(TRIAXI_ERROR_FILE_READ); }
  if (size64.QuadPart < 0 || (uintmax_t)size64.QuadPart > SIZE_MAX) {
    triaxi_user_error(TRIAXI_ERROR_FILE_READ);
  }
  size_t len = (size_t)size64.QuadPart;
# endif
  if (!len) { return TRIAXI_T(Triax_Str){"", 0}; }

  char* str = (char*)triaxi_allocate(len);
# ifndef _WIN32
  const size_t max_chunk = (size_t)(SIZE_MAX >> 1);
  for (size_t pos = 0; pos < len;) {
    ssize_t r = pread(f, str + pos, TRIAXI_MIN(len - pos, max_chunk), (off_t)pos);
    if (r == -1) {
      if (errno == EINTR) { continue; }
      triaxi_user_syserror(TRIAXI_ERROR_FILE_READ);
    }
    if (r == 0) { triaxi_user_error(TRIAXI_ERROR_FILE_READ); }
    pos += (size_t)r;
  }
# else
  LARGE_INTEGER z = {TRIAXI_ZINIT}, orig;
  if (!SetFilePointerEx(f, z, &orig, FILE_CURRENT) || !SetFilePointerEx(f, z, NULL, FILE_BEGIN)) {
    triaxi_user_syserror(TRIAXI_ERROR_FILE_READ);
  }
  for (uint64_t pos = 0; pos < len;) {
    uint64_t nbytes = len - pos;
    DWORD    chunk  = (DWORD)((nbytes > 0xFFFFFFFFUL) ? 0xFFFFFFFFUL : nbytes);
    DWORD    r      = 0;
    if (!ReadFile(f, str + pos, chunk, &r, NULL)) { triaxi_user_syserror(TRIAXI_ERROR_FILE_READ); }
    if (!r) { triaxi_user_error(TRIAXI_ERROR_FILE_READ); }
    pos += r;
  }
  if (!SetFilePointerEx(f, orig, NULL, FILE_BEGIN)) {
    triaxi_user_syserror(TRIAXI_ERROR_FILE_READ);
  }
# endif
  return TRIAXI_T(Triax_Str){str, len};
}

static inline Triax_Str triax_read_file(const char* path) {
  if (!path) {
    errno = EINVAL;
    triaxi_user_syserror(TRIAXI_ERROR_FILE_OPEN);
  }

  TRIAXI_File f = triaxi_file_open_r(path);
  if (!triaxi_file_valid(f)) { triaxi_user_syserror(TRIAXI_ERROR_FILE_OPEN); }

  Triax_Str res = triaxi_file_read_user(f);
  triaxi_file_close(f);
  return res;
}

static inline Triax_Str triax_read_stdout(void) {
# if TRIAXI_CPP
  std::cout.flush();
# endif
  if (fflush(stdout)) { triaxi_fatal(); }
  return triaxi_file_read_user(TRIAXI_exec.out);
}
static inline Triax_Str triax_read_stderr(void) {
# if TRIAXI_CPP
  std::cerr.flush();
# endif
  fflush(stderr);
  return triaxi_file_read_user(TRIAXI_exec.err);
}
# pragma endregion exec_state

# pragma region log_protocol

static inline void triaxi_log_write(const void* buf, size_t len) {
  const char* ptr = (const char*)buf;
# ifndef _WIN32
  while (len) {
    ssize_t w = write(TRIAXI_exec.log, ptr, len);
    if (w == -1) {
      if (errno == EINTR) { continue; }
      triaxi_fatal();
    } else if (w == 0) {
      triaxi_fatal();
    }
    len -= (size_t)w, ptr += w;
  }
# else
  for (DWORD w; len > 0; ptr += (size_t)w, len -= (size_t)w) {
    DWORD chunk = (len > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (DWORD)len;
    if (!WriteFile(TRIAXI_exec.log, ptr, chunk, &w, 0) || !w) { triaxi_fatal(); }
  }
# endif
}

static inline void triaxi_log_hdr(uint8_t len, uint8_t type, uint8_t ae, uint8_t neg, uint32_t cr,
                                  uint32_t line, const char* str) {
# ifndef _WIN32
  TRIAXI_AssertHdr hdr = {{len}, type, ae, neg, cr, line, {0}, str};
# else
  TRIAXI_AssertHdr hdr = {{len}, type, ae, neg, cr, line, {0}};
  memcpy(hdr.expr_str, str, hdr.expr_len);
# endif
  if (TRIAXI_exec.shared->state != TRIAXI_STATE_TEST) {
    triaxi_user_error(TRIAXI_ERROR_ASSERT_IN_FIXTURE);
  }
  triaxi_log_write(&hdr, sizeof(hdr));
  TRIAXI_exec.shared->state = TRIAXI_STATE_IN_ASSERT;
}

# define TRIAXI_LOG_HDR(type, ae, neg, cr, lit)                                                    \
    triaxi_log_hdr(                                                                                \
        (uint8_t)(sizeof(lit) - 1 <= TRIAX_EXPR_MAX ? sizeof(lit) - 1 : TRIAX_EXPR_MAX),           \
        (uint8_t)(type), (uint8_t)(ae), (uint8_t)(neg), (cr), (uint32_t)__LINE__, (lit))

static inline void triaxi_debug_break(void) {
# ifdef _WIN32
  DebugBreak();
# elif TRIAXI_CLANG && defined(__has_builtin)
#  if __has_builtin(__builtin_debugtrap)
  __builtin_debugtrap();
#  else
  raise(SIGTRAP);
#  endif
# else
  raise(SIGTRAP);
# endif
}

static inline void triaxi_log_res(TRIAXI_AR val) {
  TRIAXI_exec.pkg.val = (uint8_t)val;
  size_t pad          = (sizeof(TRIAXI_exec.pkg) + TRIAXI_exec.pkg.len + 7) & ~(size_t)7;
  memset(TRIAXI_exec.pkg.args + TRIAXI_exec.pkg.len, 0,
         pad - sizeof(TRIAXI_exec.pkg) - TRIAXI_exec.pkg.len);
  triaxi_log_write(&TRIAXI_exec.pkg, pad);
  /*
   * The result is now fully logged, so this assertion is resolved — every
   * caller (the plain expect/assert ternary, TRIAXI_ABORT_TEST) would reset
   * state back to TEST right after this returns anyway. Doing it here
   * instead, before triaxi_debug_break() below, matters because a signal
   * landing inside triaxi_debug_break() (the intended case: SIGTRAP with no
   * debugger attached, so Triax's own crash handler runs) must not observe
   * state still IN_ASSERT — triaxi_test_interpret only expects IN_ASSERT
   * paired with an EXIT_ERROR exit (e.g. assert-in-fixture); pairing it with
   * EXIT_FAULT here as if it were a mid-predicate crash previously tripped
   * its consistency check (triaxi_fatal()) instead of reporting a normal
   * ucrashed/fault outcome, since — unlike a real mid-predicate crash — this
   * assertion's result had already been completely logged.
   */
  TRIAXI_exec.shared->state = TRIAXI_STATE_TEST;
  if (TRIAXI_exec.debug_break) { triaxi_debug_break(); }
}
# pragma endregion log_protocol

# pragma region assertion_dispatch

/// @brief Reason we jumped out of the test function?
typedef enum TRIAXI_ExecOutcome {
  TRIAXI_EXEC_RETURNED,
  TRIAXI_EXEC_ASSERTED,
  TRIAXI_EXEC_SKIPPED,
  TRIAXI_EXEC_EXCEPTION,
  TRIAXI_EXEC_CRASHED,
  TRIAXI_EXEC_ERROR
} TRIAXI_ExecOutcome;

# if !TRIAXI_CPP
#  define TRIAXI_ABORT_TEST(outcome)                                                               \
     (TRIAXI_exec.shared->state = TRIAXI_STATE_TEST, longjmp(TRIAXI_exec.jmp, (int)(outcome)))
# else
struct TRIAXI_TestException {
  TRIAXI_ExecOutcome outcome;
};
#  define TRIAXI_ABORT_TEST(outcome)                                                               \
     TRIAXI_exec.shared->state = TRIAXI_STATE_TEST, throw TRIAXI_TestException { (outcome) }
# endif

triaxi_noreturn static inline void triaxi_user_error_f(uint32_t error, uint32_t syserr) {
  TRIAXI_exec.shared->exit.type = TRIAXI_EXIT_ERROR;
  TRIAXI_exec.shared->exit.code = triaxi_error_pack((TRIAXI_Error)error, syserr);

  /*
   * Errors raised while evaluating the test body/assertion can use the normal
   * test-abort path. Fixture functions are noexcept in C++17+, so throwing
   * through them would terminate; abort instead. In isolated mode the parent
   * preserves TRIAXI_EXIT_ERROR from shared memory. POSIX non-isolated mode
   * already catches SIGABRT, and its crash path preserves a pre-recorded error.
   */
  if (TRIAXI_exec.shared->state == TRIAXI_STATE_TEST
      || TRIAXI_exec.shared->state == TRIAXI_STATE_IN_ASSERT) {
# if !TRIAXI_CPP
    longjmp(TRIAXI_exec.jmp, (int)TRIAXI_EXEC_ERROR);
# else
    throw TRIAXI_TestException{TRIAXI_EXEC_ERROR};
# endif
  }
  abort();
}

# define triaxi_A1(type, neg, lit, ...)                                                            \
    (TRIAXI_LOG_HDR(TRIAXI_AT_##type, 1, neg, 0, lit),                                             \
     (triaxi_AF_##type(neg, __VA_ARGS__)                                                           \
          ? TRIAXI_ABORT_TEST(TRIAXI_EXEC_ASSERTED)                                                \
          : (void)(TRIAXI_exec.shared->state = TRIAXI_STATE_TEST, 0)))

# define triaxi_E1(type, neg, lit, ...)                                                            \
    (TRIAXI_LOG_HDR(TRIAXI_AT_##type, 0, neg, 0, lit), triaxi_AF_##type(neg, __VA_ARGS__),         \
     (void)(TRIAXI_exec.shared->state = TRIAXI_STATE_TEST, 0))

# define triaxi_assert_fault(v, ae, fault, lit, ...)                                               \
    do {                                                                                           \
      const Triax_Fault TRIAXI_expected_v = (Triax_Fault)(fault);                                  \
      TRIAXI_LOG_HDR(TRIAXI_AT_fault, ae, v, TRIAXI_expected_v, lit);                              \
      __VA_ARGS__;                                                                                 \
      {                                                                                            \
        if (triaxi_AF_fault(v, TRIAXI_expected_v) && ae) {                                         \
          TRIAXI_ABORT_TEST(TRIAXI_EXEC_ASSERTED);                                                 \
        }                                                                                          \
        TRIAXI_exec.shared->state = TRIAXI_STATE_TEST;                                             \
      }                                                                                            \
    } while (0)

# define triaxi_assert_exit(v, ae, code, lit, ...)                                                 \
    do {                                                                                           \
      const uint32_t TRIAXI_expected_v = (uint32_t)(code);                                         \
      if (!TRIAXI_exec.isolated) {                                                                 \
        triaxi_user_error(TRIAXI_ERROR_EXIT_ASSERT_WITHOUT_ISOLATION);                             \
      }                                                                                            \
      TRIAXI_LOG_HDR(TRIAXI_AT_exit, ae, v, TRIAXI_expected_v, lit);                               \
      __VA_ARGS__;                                                                                 \
      {                                                                                            \
        if (triaxi_AF_exit(v, (uint32_t)TRIAXI_expected_v) && ae) {                                \
          TRIAXI_ABORT_TEST(TRIAXI_EXEC_ASSERTED);                                                 \
        }                                                                                          \
        TRIAXI_exec.shared->state = TRIAXI_STATE_TEST;                                             \
      }                                                                                            \
    } while (0)

# define triaxi_sprintargs1(RES, a1, f1) ((RES).len = (uint32_t)(sprintf((RES).args, f1, a1) + 1))
# define triaxi_sprintargs2(RES, a1, f1, a2, f2)                                                   \
    ((RES).len = (uint32_t)(sprintf((RES).args, f1 "%c" f2, a1, '\0', a2) + 1))
# define triaxi_sprintargs3(RES, a1, f1, a2, f2, a3, f3)                                           \
    ((RES).len = (uint32_t)(sprintf((RES).args, f1 "%c" f2 "%c" f3, a1, '\0', a2, '\0', a3) + 1))

# define triaxi_log_RES0(v)                                                                        \
    (((v) == 0 ? 0 : ((TRIAXI_exec.pkg.len = 0), triaxi_log_res((TRIAXI_AR)(v)), 1)))
# define triaxi_log_RES1(v, e1, f1)                                                                \
    (((v) == 0                                                                                     \
          ? 0                                                                                      \
          : (triaxi_sprintargs1(TRIAXI_exec.pkg, e1, f1), triaxi_log_res((TRIAXI_AR)(v)), 1)))
# define triaxi_log_RES2(v, e1, f1, e2, f2)                                                        \
    (((v) == 0 ? 0                                                                                 \
               : (triaxi_sprintargs2(TRIAXI_exec.pkg, e1, f1, e2, f2),                             \
                  triaxi_log_res((TRIAXI_AR)(v)), 1)))
# define triaxi_log_RES3(v, e1, f1, e2, f2, e3, f3)                                                \
    (((v) == 0 ? 0                                                                                 \
               : (triaxi_sprintargs3(TRIAXI_exec.pkg, e1, f1, e2, f2, e3, f3),                     \
                  triaxi_log_res((TRIAXI_AR)(v)), 1)))

# define TRIAXI_eq(v, e1, e2) ((uint8_t)((v) == ((e1) == (e2))))
# define TRIAXI_gt(v, e1, e2) ((uint8_t)(!(v) ? (((e1) <= (e2)) + ((e1) < (e2))) : !((e1) <= (e2))))
# define TRIAXI_lt(v, e1, e2) ((uint8_t)(!(v) ? (((e1) >= (e2)) + ((e1) > (e2))) : !((e1) >= (e2))))

static inline int triaxi_strcmp(Triax_Str e1, Triax_Str e2) {
  if (!e1.str || !e2.str) { return (!e1.str && !e2.str) ? 0 : !e1.str ? -1 : 1; }
  size_t min = TRIAXI_MIN(e1.len, e2.len);
  int    cr  = min ? memcmp(e1.str, e2.str, min) : 0;
  return cr ? cr : (e1.len > e2.len) - (e1.len < e2.len);
}

static inline bool triaxi_AF_fault(bool v, uint32_t expected) {
  if (v) { return 0; }
  triaxi_sprintargs2(TRIAXI_exec.pkg, triaxi_fault_tostr(expected), "%s", "no fault", "%s");
  triaxi_log_res(TRIAXI_AR_FAIL1);
  return 1;
}

static inline bool triaxi_AF_exit(bool v, uint32_t expected) {
  if (v) { return 0; }
  if (expected == TRIAX_EXIT_ANY) {
    triaxi_sprintargs2(TRIAXI_exec.pkg, "any exit", "%s", "no exit", "%s");
  } else {
    triaxi_sprintargs2(TRIAXI_exec.pkg, expected, "%" PRIu32, "no exit", "%s");
  }
  triaxi_log_res(TRIAXI_AR_FAIL1);
  return 1;
}

static inline bool triaxi_strn_compare_impl_send(uint8_t val, Triax_Str e1, Triax_Str e2) {
  if (!val) { return 0; }
  TRIAXI_exec.pkg.val = val;
  const bool null1 = !e1.str, null2 = !e2.str;

  TRIAXI_exec.pkg.code = TRIAXI_ENCODING_STRING | (null1 ? TRIAXI_ENCODING_NULL_ARG1 : 0)
                       | (null2 ? TRIAXI_ENCODING_NULL_ARG2 : 0);
  if (null1) { e1 = TRIAXI_T(Triax_Str){"", 0}; }
  if (null2) { e2 = TRIAXI_T(Triax_Str){"", 0}; }
  /*
   * Result payload lengths are uint32_t on the wire. Keep both string
   * diagnostics when truncation is necessary: reserve roughly half for each,
   * then give any space unused by a short string to the other.
   */
  const size_t max_data = UINT32_MAX - sizeof(e1.len);
  size_t       e1_len   = e1.len;
  size_t       e2_len   = e2.len;
  if (e1_len > max_data || e2_len > max_data - e1_len) {
    size_t half = max_data / 2;
    e1_len      = TRIAXI_MIN(e1.len, half);
    e2_len      = TRIAXI_MIN(e2.len, max_data - e1_len);
    if (e2_len == e2.len) { e1_len = TRIAXI_MIN(e1.len, max_data - e2_len); }
  }
  TRIAXI_exec.pkg.len        = (uint32_t)(sizeof(e1.len) + e1_len + e2_len);
  static const char zeros[8] = {TRIAXI_ZINIT};
  size_t            pad      = (8 - (TRIAXI_exec.pkg.len & 7)) & 7;
# ifndef _WIN32
  TRIAXI_IGNWARN_GNU_BEG("-Wcast-qual")
  struct iovec bufs[] = {{&TRIAXI_exec.pkg, sizeof(TRIAXI_exec.pkg)},
                         {(void*)&e1_len, sizeof(e1_len)},
                         {(void*)e1.str, e1_len},
                         {(void*)e2.str, e2_len},
                         {(void*)zeros, pad}};
  TRIAXI_IGNWARN_GNU_END
  for (size_t count = triaxi_countof(bufs), i = 0; i < count;) {
    while (i < count && bufs[i].iov_len == 0) { ++i; }
    if (i >= count) { break; }
    ssize_t ns = writev(TRIAXI_exec.log, &bufs[i], (int)(count - i));
    if (ns < 0) {
      if (errno == EINTR) { continue; }
      triaxi_fatal();
    } else if (ns == 0) {
      triaxi_fatal();
    }
    for (size_t n = (size_t)ns; i < count; ++i) {
      struct iovec* v = &bufs[i];
      if (n >= v->iov_len) {
        n -= v->iov_len;
      } else {
        v->iov_base = (char*)v->iov_base + n, v->iov_len -= n;
        break;
      }
    }
  }
  TRIAXI_exec.pkg.code = TRIAXI_ENCODING_DEFAULT;
  return 1;
# else
  if (TRIAXI_exec.pkg.len + pad <= sizeof(TRIAXI_exec.storage) - sizeof(TRIAXI_exec.pkg)) {
    char* p = TRIAXI_exec.pkg.args;

    memcpy(p, &e1_len, sizeof(e1_len)), p += sizeof(e1_len);
    memcpy(p, e1.str, e1_len), p          += e1_len;
    memcpy(p, e2.str, e2_len), p          += e2_len;
    memcpy(p, zeros, pad), p              += pad;

    triaxi_log_write(&TRIAXI_exec.pkg, (size_t)(p - (char*)&TRIAXI_exec.pkg));
    TRIAXI_exec.pkg.code = TRIAXI_ENCODING_DEFAULT;
    return 1;
  }
  /* Slow path: batch buffers into storage and write in chunks. */
  Triax_Str bufs[] = {
      {(const char*)&e1_len, sizeof(e1_len)},
      {e1.str, e1_len},
      {e2.str, e2_len},
      {zeros, pad},
  };
  char *const beg = (char*)TRIAXI_exec.storage, *const end = beg + sizeof(TRIAXI_exec.storage),
              *cur = beg + sizeof(TRIAXI_exec.pkg);

  for (size_t i = 0; i < triaxi_countof(bufs); ++i) {
    while (bufs[i].len) {
      size_t to_copy = TRIAXI_MIN(bufs[i].len, (size_t)(end - cur));
      memcpy(cur, bufs[i].str, to_copy);
      cur         += to_copy;
      bufs[i].str += to_copy;
      bufs[i].len -= to_copy;
      if (cur == end) {
        triaxi_log_write(beg, (size_t)(cur - beg));
        cur = beg;
      }
    }
  }
  if (cur > beg) { triaxi_log_write(beg, (size_t)(cur - beg)); }
  TRIAXI_exec.pkg.code = TRIAXI_ENCODING_DEFAULT;
  return 1;
# endif
}

/// @brief Marks a function as printf-like so the compiler validates format
/// strings against variadic arguments at every call site (1-indexed,
/// counting from the start of the parameter list).
# if TRIAXI_GNU_COMPAT
__attribute__((format(printf, 3, 4)))
# endif
static inline bool
    triaxi_AF_check(bool v, bool failed, const char* fmt, ...) {
  (void)v;
  if (!failed) { return 0; }

  va_list ap;
  va_start(ap, fmt);
  size_t cap = sizeof(TRIAXI_exec.storage) - sizeof(TRIAXI_exec.pkg) - 7;
  int    n   = vsnprintf(TRIAXI_exec.pkg.args, cap, fmt, ap);
  va_end(ap);
  if (n < 0) {
    static const char msg[] = "[format error]";
    memcpy(TRIAXI_exec.pkg.args, msg, sizeof(msg));
    TRIAXI_exec.pkg.len = sizeof(msg);
  } else {
    bool   trunc        = n >= (int)cap;
    size_t len          = trunc ? cap : (size_t)n + 1;
    TRIAXI_exec.pkg.len = (uint32_t)len;
    if (trunc) {
      static const char marker[] = "...[truncated]";
      memcpy(TRIAXI_exec.pkg.args + len - sizeof(marker), marker, sizeof(marker));
    }
  }
  triaxi_log_res(TRIAXI_AR_FAIL1);
  return 1;
}

static inline bool triaxi_AF_true(bool v, bool e1) { return triaxi_log_RES0(v ^ !e1); }
static inline bool triaxi_AF_null(bool v, const void* e1) {
  return triaxi_log_RES1((e1 != NULL) != v, e1, "%p");
}
static inline bool triaxi_AF_memeq(bool v, const void* e1, const void* e2, size_t e3) {
  const unsigned char *_e1 = (const unsigned char*)e1, *_e2 = (const unsigned char*)e2;

  for (size_t i = 0; i < e3; ++i) {
    if (_e1[i] != _e2[i]) {
      if (v) { return 0; }
      size_t len = TRIAXI_MIN(e3 - i, 16);
      char   ibuf[sizeof(size_t) * 3 + 1];
      int    n = snprintf(ibuf, sizeof(ibuf), "%zu", i);
      if (n < 0 || (size_t)n >= sizeof(ibuf)) { triaxi_fatal(); }
      size_t ilen = (size_t)n + 1;
      char*  p    = TRIAXI_exec.pkg.args;
      memcpy(p, ibuf, ilen), p   += ilen;
      memcpy(p, _e1 + i, len), p += len;
      memcpy(p, _e2 + i, len), p += len;
      TRIAXI_exec.pkg.len = (uint32_t)(p - TRIAXI_exec.pkg.args);
      triaxi_log_res(TRIAXI_AR_FAIL1);
      return 1;
    }
  }

  /* Equal. */
  if (!v) { return 0; }
  TRIAXI_exec.pkg.len = 0;
  triaxi_log_res(TRIAXI_AR_FAIL1);
  return 1;
}

static inline bool triaxi_AF_memzero(bool v, const void* e1, size_t e2) {
  const unsigned char* _e1 = (const unsigned char*)e1;
  for (size_t i = 0; i < e2; ++i) {
    if (_e1[i]) {
      if (v) { return 0; }
      size_t len = TRIAXI_MIN(e2 - i, 16);
      char   ibuf[sizeof(size_t) * 3 + 1];
      int    n = snprintf(ibuf, sizeof(ibuf), "%zu", i);
      if (n < 0 || (size_t)n >= sizeof(ibuf)) { triaxi_fatal(); }
      size_t ilen = (size_t)n + 1;
      char*  p    = TRIAXI_exec.pkg.args;
      memcpy(p, ibuf, ilen), p   += ilen;
      memcpy(p, _e1 + i, len), p += len;
      TRIAXI_exec.pkg.len = (uint32_t)(p - TRIAXI_exec.pkg.args);
      triaxi_log_res(TRIAXI_AR_FAIL1);
      return 1;
    }
  }
  /* All zero. */
  if (!v) { return 0; }
  TRIAXI_exec.pkg.len = 0;
  triaxi_log_res(TRIAXI_AR_FAIL1);
  return 1;
}

static inline bool triaxi_AF_eq_SS(bool v, Triax_Str e1, Triax_Str e2) {
  return triaxi_strn_compare_impl_send(TRIAXI_eq(v, triaxi_strcmp(e1, e2), 0), e1, e2);
}
static inline bool triaxi_AF_gt_SS(bool v, Triax_Str e1, Triax_Str e2) {
  int val = triaxi_strcmp(e1, e2);
  return triaxi_strn_compare_impl_send(TRIAXI_gt(v, val, 0), e1, e2);
}
static inline bool triaxi_AF_lt_SS(bool v, Triax_Str e1, Triax_Str e2) {
  int val = triaxi_strcmp(e1, e2);
  return triaxi_strn_compare_impl_send(TRIAXI_lt(v, val, 0), e1, e2);
}
# if !TRIAXI_CPP
static inline bool triaxi_AF_eq_ss(bool v, const char* e1, const char* e2) {
  return triaxi_AF_eq_SS(v, triaxi_tostr(e1), triaxi_tostr(e2));
}
static inline bool triaxi_AF_gt_ss(bool v, const char* e1, const char* e2) {
  return triaxi_AF_gt_SS(v, triaxi_tostr(e1), triaxi_tostr(e2));
}
static inline bool triaxi_AF_lt_ss(bool v, const char* e1, const char* e2) {
  return triaxi_AF_lt_SS(v, triaxi_tostr(e1), triaxi_tostr(e2));
}
static inline bool triaxi_AF_eq_Ss(bool v, Triax_Str e1, const char* e2) {
  return triaxi_AF_eq_SS(v, e1, triaxi_tostr(e2));
}
static inline bool triaxi_AF_gt_Ss(bool v, Triax_Str e1, const char* e2) {
  return triaxi_AF_gt_SS(v, e1, triaxi_tostr(e2));
}
static inline bool triaxi_AF_lt_Ss(bool v, Triax_Str e1, const char* e2) {
  return triaxi_AF_lt_SS(v, e1, triaxi_tostr(e2));
}
static inline bool triaxi_AF_eq_sS(bool v, const char* e1, Triax_Str e2) {
  return triaxi_AF_eq_SS(v, triaxi_tostr(e1), e2);
}
static inline bool triaxi_AF_gt_sS(bool v, const char* e1, Triax_Str e2) {
  return triaxi_AF_gt_SS(v, triaxi_tostr(e1), e2);
}
static inline bool triaxi_AF_lt_sS(bool v, const char* e1, Triax_Str e2) {
  return triaxi_AF_lt_SS(v, triaxi_tostr(e1), e2);
}
# endif
static inline bool triaxi_AF_streq(bool v, Triax_Str e1, Triax_Str e2) {
  return triaxi_AF_eq_SS(v, e1, e2);
}
static inline bool triaxi_AF_str_startswith(bool v, Triax_Str str, Triax_Str pref) {
  bool ok = str.str && pref.str && str.len >= pref.len
         && (!pref.len || !memcmp(str.str, pref.str, pref.len));
  return triaxi_strn_compare_impl_send(!v ? !ok : ok, str, pref);
}
static inline bool triaxi_AF_str_endswith(bool v, Triax_Str str, Triax_Str suf) {
  bool ok = str.str && suf.str && str.len >= suf.len
         && (!suf.len || !memcmp(str.str + str.len - suf.len, suf.str, suf.len));
  return triaxi_strn_compare_impl_send(!v ? !ok : ok, str, suf);
}
static inline bool triaxi_AF_str_contains(bool v, Triax_Str str, Triax_Str needle) {
  bool found = str.str && needle.str && !needle.len;
  if (!found && str.str && needle.str && str.len >= needle.len) {
    const char *beg = str.str, *end = str.str + str.len - needle.len + 1;
    char        c = needle.str[0];
    for (; (beg = (const char*)memchr(beg, c, (size_t)(end - beg))); ++beg) {
      if (!memcmp(beg, needle.str, needle.len)) {
        found = true;
        break;
      }
    }
  }
  return triaxi_strn_compare_impl_send(!v ? !found : found, str, needle);
}

static inline bool triaxi_AF_eq_u(bool v, unsigned long long e1, unsigned long long e2) {
  return triaxi_log_RES2(TRIAXI_eq(v, e1, e2), e1, "%llu", e2, "%llu");
}
static inline bool triaxi_AF_gt_u(bool v, unsigned long long e1, unsigned long long e2) {
  return triaxi_log_RES2(TRIAXI_gt(v, e1, e2), e1, "%llu", e2, "%llu");
}
static inline bool triaxi_AF_lt_u(bool v, unsigned long long e1, unsigned long long e2) {
  return triaxi_log_RES2(TRIAXI_lt(v, e1, e2), e1, "%llu", e2, "%llu");
}
static inline bool triaxi_AF_eq_i(bool v, long long e1, long long e2) {
  return triaxi_log_RES2(TRIAXI_eq(v, e1, e2), e1, "%lld", e2, "%lld");
}
static inline bool triaxi_AF_gt_i(bool v, long long e1, long long e2) {
  return triaxi_log_RES2(TRIAXI_gt(v, e1, e2), e1, "%lld", e2, "%lld");
}
static inline bool triaxi_AF_lt_i(bool v, long long e1, long long e2) {
  return triaxi_log_RES2(TRIAXI_lt(v, e1, e2), e1, "%lld", e2, "%lld");
}

static inline bool triaxi_AF_eq_ui(bool v, unsigned long long e1, long long e2) {
  if (e2 < 0) { return triaxi_log_RES2(!v ? 1 : 0, e1, "%llu", e2, "%lld"); }
  return triaxi_AF_eq_u(v, e1, (unsigned long long)e2);
}
static inline bool triaxi_AF_gt_ui(bool v, unsigned long long e1, long long e2) {
  if (e2 < 0) { return triaxi_log_RES2(v ? 1 : 0, e1, "%llu", e2, "%lld"); }
  return triaxi_AF_gt_u(v, e1, (unsigned long long)e2);
}
static inline bool triaxi_AF_lt_ui(bool v, unsigned long long e1, long long e2) {
  if (e2 < 0) { return triaxi_log_RES2(!v ? 2 : 0, e1, "%llu", e2, "%lld"); }
  return triaxi_AF_lt_u(v, e1, (unsigned long long)e2);
}
static inline bool triaxi_AF_eq_iu(bool v, long long e1, unsigned long long e2) {
  if (e1 < 0) { return triaxi_log_RES2(!v ? 1 : 0, e1, "%lld", e2, "%llu"); }
  return triaxi_AF_eq_u(v, (unsigned long long)e1, e2);
}
static inline bool triaxi_AF_gt_iu(bool v, long long e1, unsigned long long e2) {
  if (e1 < 0) { return triaxi_log_RES2(!v ? 2 : 0, e1, "%lld", e2, "%llu"); }
  return triaxi_AF_gt_u(v, (unsigned long long)e1, e2);
}
static inline bool triaxi_AF_lt_iu(bool v, long long e1, unsigned long long e2) {
  if (e1 < 0) { return triaxi_log_RES2(v ? 1 : 0, e1, "%lld", e2, "%llu"); }
  return triaxi_AF_lt_u(v, (unsigned long long)e1, e2);
}

static inline bool triaxi_AF_eq_vp(bool v, const void* e1, const void* e2) {
  return triaxi_log_RES2(TRIAXI_eq(v, e1, e2), e1, "%p", e2, "%p");
}
static inline bool triaxi_AF_gt_vp(bool v, const void* e1, const void* e2) {
  uintptr_t u1 = (uintptr_t)e1, u2 = (uintptr_t)e2;
  return triaxi_log_RES2(TRIAXI_gt(v, u1, u2), e1, "%p", e2, "%p");
}
static inline bool triaxi_AF_lt_vp(bool v, const void* e1, const void* e2) {
  uintptr_t u1 = (uintptr_t)e1, u2 = (uintptr_t)e2;
  return triaxi_log_RES2(TRIAXI_lt(v, u1, u2), e1, "%p", e2, "%p");
}
static inline bool triaxi_AF_floateq_abstol(bool v, long double e1, long double e2,
                                            long double tol) {
  bool close = tol >= 0 && (e1 == e2 || (isfinite(e1) && isfinite(e2) && fabsl(e1 - e2) <= tol));

  return triaxi_log_RES3(v == close, e1, "%.21Lg", e2, "%.21Lg", tol, "%.21Lg");
}

static inline bool triaxi_AF_floateq_reltol(bool v, long double e1, long double e2,
                                            long double tol) {
  bool close
      = tol >= 0
     && (e1 == e2
         || (isfinite(e1) && isfinite(e2) && fabsl(e1 - e2) <= tol * fmaxl(fabsl(e1), fabsl(e2))));

  return triaxi_log_RES3(v == close, e1, "%.21Lg", e2, "%.21Lg", tol, "%.21Lg");
}

static inline bool triaxi_AF_eq_ld(bool v, long double e1, long double e2) {
  return triaxi_log_RES2(TRIAXI_eq(v, e1, e2), e1, "%.21Lg", e2, "%.21Lg");
}
static inline bool triaxi_AF_gt_ld(bool v, long double e1, long double e2) {
  if (isnan(e1) || isnan(e2)) {
    return triaxi_log_RES2(TRIAXI_AR_FAIL3, e1, "%.21Lg", e2, "%.21Lg");
  }
  return triaxi_log_RES2(TRIAXI_gt(v, e1, e2), e1, "%.21Lg", e2, "%.21Lg");
}
static inline bool triaxi_AF_lt_ld(bool v, long double e1, long double e2) {
  if (isnan(e1) || isnan(e2)) {
    return triaxi_log_RES2(TRIAXI_AR_FAIL3, e1, "%.21Lg", e2, "%.21Lg");
  }
  return triaxi_log_RES2(TRIAXI_lt(v, e1, e2), e1, "%.21Lg", e2, "%.21Lg");
}
static inline bool triaxi_AF_arreq_u(bool v, size_t index, unsigned long long e1,
                                     unsigned long long e2) {
  bool r = !(e1 == e2);
  if (!v && r) { (void)triaxi_log_RES3(TRIAXI_AR_FAIL1, index, "%zu", e1, "%llu", e2, "%llu"); }
  return r;
}
static inline bool triaxi_AF_arreq_i(bool v, size_t index, long long e1, long long e2) {
  bool r = !(e1 == e2);
  if (!v && r) { (void)triaxi_log_RES3(TRIAXI_AR_FAIL1, index, "%zu", e1, "%lld", e2, "%lld"); }
  return r;
}
static inline bool triaxi_AF_arreq_ui(bool v, size_t index, unsigned long long e1, long long e2) {
  bool r = !(e2 >= 0 && e1 == (unsigned long long)e2);
  if (!v && r) { (void)triaxi_log_RES3(TRIAXI_AR_FAIL1, index, "%zu", e1, "%llu", e2, "%lld"); }
  return r;
}
static inline bool triaxi_AF_arreq_iu(bool v, size_t idx, long long e1, unsigned long long e2) {
  bool r = !(e1 >= 0 && (unsigned long long)e1 == e2);
  if (!v && r) { (void)triaxi_log_RES3(TRIAXI_AR_FAIL1, idx, "%zu", e1, "%lld", e2, "%llu"); }
  return r;
}

static inline bool triaxi_AF_arreq_ld(bool v, size_t idx, long double e1, long double e2) {
  bool r = !(e1 == e2);
  if (!v && r) { (void)triaxi_log_RES3(TRIAXI_AR_FAIL1, idx, "%zu", e1, "%.21Lg", e2, "%.21Lg"); }
  return r;
}

static inline bool triaxi_AF_arreq_vp(bool v, size_t index, const void* e1, const void* e2) {
  bool r = !(e1 == e2);
  if (!v && r) { (void)triaxi_log_RES3(TRIAXI_AR_FAIL1, index, "%zu", e1, "%p", e2, "%p"); }
  return r;
}
static inline bool triaxi_AF_arreq_SS(bool v, size_t index, Triax_Str e1, Triax_Str e2) {
  const bool r = !(triaxi_strcmp(e1, e2) == 0);
  if (v || !r) { return r; }

  TRIAXI_exec.pkg.val = TRIAXI_AR_FAIL1;
  const bool null1 = !e1.str, null2 = !e2.str;
  TRIAXI_exec.pkg.code = TRIAXI_ENCODING_STRING | (null1 ? TRIAXI_ENCODING_NULL_ARG1 : 0)
                       | (null2 ? TRIAXI_ENCODING_NULL_ARG2 : 0);
  if (null1) { e1 = TRIAXI_T(Triax_Str){"", 0}; }
  if (null2) { e2 = TRIAXI_T(Triax_Str){"", 0}; }

  char index_buf[sizeof(size_t) * 3 + 1];
  int  index_n = snprintf(index_buf, sizeof(index_buf), "%zu", index);
  if (index_n < 0 || (size_t)index_n >= sizeof(index_buf)) { triaxi_fatal(); }

  /* Include the terminating NUL in the wire representation. */
  const size_t index_len = (size_t)index_n + 1;
  const size_t metadata  = index_len + sizeof(size_t);
  const size_t max_data  = (size_t)UINT32_MAX - metadata;

  size_t       e1_len = e1.len, e2_len = e2.len;

  if (e1_len > max_data || e2_len > max_data - e1_len) {
    size_t half = max_data / 2;

    e1_len      = TRIAXI_MIN(e1.len, half);
    e2_len      = TRIAXI_MIN(e2.len, max_data - e1_len);

    if (e2_len == e2.len) { e1_len = TRIAXI_MIN(e1.len, max_data - e2_len); }
  }

  TRIAXI_exec.pkg.len        = (uint32_t)(metadata + e1_len + e2_len);

  static const char zeros[8] = {TRIAXI_ZINIT};
  size_t            pad      = (8 - (TRIAXI_exec.pkg.len & 7)) & 7;

# ifndef _WIN32
  TRIAXI_IGNWARN_GNU_BEG("-Wcast-qual")

  struct iovec bufs[] = {
      {&TRIAXI_exec.pkg, sizeof(TRIAXI_exec.pkg)},
      {index_buf, index_len},
      {&e1_len, sizeof(e1_len)},
      {(void*)e1.str, e1_len},
      {(void*)e2.str, e2_len},
      {(void*)zeros, pad},
  };
  TRIAXI_IGNWARN_GNU_END

  for (size_t nbufs = triaxi_countof(bufs), i = 0; i < nbufs;) {
    while (i < nbufs && bufs[i].iov_len == 0) { ++i; }
    if (i >= nbufs) { break; }

    ssize_t ns = writev(TRIAXI_exec.log, &bufs[i], (int)(nbufs - i));

    if (ns < 0) {
      if (errno == EINTR) { continue; }
      triaxi_fatal();
    }
    if (ns == 0) { triaxi_fatal(); }

    for (size_t n = (size_t)ns; i < nbufs; ++i) {
      struct iovec* buf = &bufs[i];

      if (n >= buf->iov_len) {
        n -= buf->iov_len;
      } else {
        buf->iov_base  = (char*)buf->iov_base + n;
        buf->iov_len  -= n;
        break;
      }
    }
  }

# else

  if (TRIAXI_exec.pkg.len + pad <= sizeof(TRIAXI_exec.storage) - sizeof(TRIAXI_exec.pkg)) {
    char* p = TRIAXI_exec.pkg.args;

    memcpy(p, index_buf, index_len), p    += index_len;
    memcpy(p, &e1_len, sizeof(e1_len)), p += sizeof(e1_len);
    memcpy(p, e1.str, e1_len), p          += e1_len;
    memcpy(p, e2.str, e2_len), p          += e2_len;
    memcpy(p, zeros, pad), p              += pad;

    triaxi_log_write(&TRIAXI_exec.pkg, (size_t)(p - (char*)&TRIAXI_exec.pkg));

  } else {
    Triax_Str bufs[] = {
        {index_buf, index_len}, {(const char*)&e1_len, sizeof(e1_len)},
        {e1.str, e1_len},       {e2.str, e2_len},
        {zeros, pad},
    };

    char* const beg = TRIAXI_exec.storage;
    char* const end = beg + sizeof(TRIAXI_exec.storage);
    char*       cur = beg + sizeof(TRIAXI_exec.pkg);

    for (size_t i = 0; i < triaxi_countof(bufs); ++i) {
      while (bufs[i].len) {
        size_t to_copy = TRIAXI_MIN(bufs[i].len, (size_t)(end - cur));

        memcpy(cur, bufs[i].str, to_copy);

        cur         += to_copy;
        bufs[i].str += to_copy;
        bufs[i].len -= to_copy;

        if (cur == end) {
          triaxi_log_write(beg, (size_t)(cur - beg));
          cur = beg;
        }
      }
    }
    if (cur > beg) { triaxi_log_write(beg, (size_t)(cur - beg)); }
  }
# endif
  TRIAXI_exec.pkg.code = TRIAXI_ENCODING_DEFAULT;
  return r;
}

static inline bool triaxi_AF_arreq_ss(bool v, size_t index, const char* e1, const char* e2) {
  return triaxi_AF_arreq_SS(v, index, triaxi_tostr(e1), triaxi_tostr(e2));
}
static inline bool triaxi_AF_arreq_sS(bool v, size_t index, const char* e1, Triax_Str e2) {
  return triaxi_AF_arreq_SS(v, index, triaxi_tostr(e1), e2);
}
static inline bool triaxi_AF_arreq_Ss(bool v, size_t index, Triax_Str e1, const char* e2) {
  return triaxi_AF_arreq_SS(v, index, e1, triaxi_tostr(e2));
}

# define triaxi_ARREQ_N_(ae, arr1, arr2, N)                                                        \
    do {                                                                                           \
      const size_t TRIAXI_count = (N);                                                             \
      TRIAXI_exec.pkg.val       = (uint8_t)TRIAXI_AR_FAIL1;                                        \
      for (size_t TRIAXI_i = 0; TRIAXI_i < TRIAXI_count; ++TRIAXI_i) {                             \
        if (triaxi_AF_arreq(0, TRIAXI_i, (arr1)[TRIAXI_i], (arr2)[TRIAXI_i])) {                    \
          if (ae) { TRIAXI_ABORT_TEST(TRIAXI_EXEC_ASSERTED); }                                     \
          break;                                                                                   \
        }                                                                                          \
      }                                                                                            \
      TRIAXI_exec.shared->state = TRIAXI_STATE_TEST;                                               \
    } while (0)

# define triaxi_ARRNEQ_N_(ae, arr1, arr2, N)                                                       \
    do {                                                                                           \
      const size_t TRIAXI_count = (N);                                                             \
      TRIAXI_exec.pkg.val       = (uint8_t)TRIAXI_AR_FAIL1;                                        \
      size_t TRIAXI_i           = 0;                                                               \
      for (; TRIAXI_i < TRIAXI_count; ++TRIAXI_i) {                                                \
        if (triaxi_AF_arreq(1, TRIAXI_i, (arr1)[TRIAXI_i], (arr2)[TRIAXI_i])) { break; }           \
      }                                                                                            \
      if (TRIAXI_i == TRIAXI_count) {                                                              \
        TRIAXI_IGNWARN_GNU_BEG("-Wdeprecated-declarations")                                        \
        (void)triaxi_log_RES1(TRIAXI_AR_FAIL1, TRIAXI_count, "%zu");                               \
        TRIAXI_IGNWARN_GNU_END                                                                     \
        if (ae) { TRIAXI_ABORT_TEST(TRIAXI_EXEC_ASSERTED); }                                       \
      }                                                                                            \
      TRIAXI_exec.shared->state = TRIAXI_STATE_TEST;                                               \
    } while (0)

# define triaxi_ARREQ_N(ae, lit, arr1, arr2, N)                                                    \
    do {                                                                                           \
      TRIAXI_LOG_HDR(TRIAXI_AT_arreq, ae, 0, 0, lit);                                              \
      triaxi_ARREQ_N_(ae, arr1, arr2, N);                                                          \
    } while (0)

# define triaxi_ARRNEQ_N(ae, lit, arr1, arr2, N)                                                   \
    do {                                                                                           \
      TRIAXI_LOG_HDR(TRIAXI_AT_arreq, ae, 1, 0, lit);                                              \
      triaxi_ARRNEQ_N_(ae, arr1, arr2, N);                                                         \
    } while (0)

# if !TRIAXI_CPP
#  define triaxi_ARREQ(ae, lit, arr1, arr2)                                                        \
     do {                                                                                          \
       static_assert(triaxi_countof(arr1) == triaxi_countof(arr2),                                 \
                     "Arrays must have the same size");                                            \
       TRIAXI_LOG_HDR(TRIAXI_AT_arreq, ae, 0, 0, lit);                                             \
       triaxi_ARREQ_N_(ae, arr1, arr2, triaxi_countof(arr1));                                      \
     } while (0)

#  define triaxi_ARRNEQ(ae, lit, arr1, arr2)                                                       \
     do {                                                                                          \
       static_assert(triaxi_countof(arr1) == triaxi_countof(arr2),                                 \
                     "Arrays must have the same size");                                            \
       TRIAXI_LOG_HDR(TRIAXI_AT_arreq, ae, 1, 0, lit);                                             \
       triaxi_ARRNEQ_N_(ae, arr1, arr2, triaxi_countof(arr1));                                     \
     } while (0)

# else
#  define triaxi_ARREQ(ae, lit, arr1, arr2)                                                        \
     do {                                                                                          \
       TRIAXI_LOG_HDR(TRIAXI_AT_arreq, ae, 0, 0, lit);                                             \
       const size_t TRIAXI_nn = triaxi_cpp_size_check(0, (arr1), (arr2));                          \
       if (TRIAXI_nn == SIZE_MAX) {                                                                \
         TRIAXI_exec.shared->state = TRIAXI_STATE_TEST;                                            \
         if (ae) { TRIAXI_ABORT_TEST(TRIAXI_EXEC_ASSERTED); }                                      \
       } else {                                                                                    \
         triaxi_ARREQ_N_(ae, arr1, arr2, TRIAXI_nn);                                               \
       }                                                                                           \
     } while (0)
#  define triaxi_ARRNEQ(ae, lit, arr1, arr2)                                                       \
     do {                                                                                          \
       TRIAXI_LOG_HDR(TRIAXI_AT_arreq, ae, 1, 0, lit);                                             \
       const size_t TRIAXI_nn = triaxi_cpp_size_check(1, (arr1), (arr2));                          \
       if (TRIAXI_nn == SIZE_MAX) {                                                                \
         TRIAXI_exec.shared->state = TRIAXI_STATE_TEST;                                            \
       } else {                                                                                    \
         triaxi_ARRNEQ_N_(ae, arr1, arr2, TRIAXI_nn);                                              \
       }                                                                                           \
     } while (0)
# endif

# if !TRIAXI_CPP
#  if TRIAXI_MSVC && _MSC_VER < 1944
#   define TRIAXI_CHAR_SIGNED(...)
#   define TRIAXI_CHAR_UNSIGNED(...)
#  elif CHAR_MIN < 0
#   define TRIAXI_CHAR_SIGNED(...) __VA_ARGS__
#   define TRIAXI_CHAR_UNSIGNED(...)
#  else
#   define TRIAXI_CHAR_SIGNED(...)
#   define TRIAXI_CHAR_UNSIGNED(...) __VA_ARGS__
#  endif

#  if TRIAXI_GNU_COMPAT
__attribute__((error("Cannot compare integer and float types")))
#  endif
void triaxi_int_float_error(int, ...); // linker error on MSVC

#  define triaxi_SELFUN_I(F, E)                                                                    \
     _Generic(E, unsigned long long: F##_iu, unsigned long: F##_iu, default: F##_i)
#  define triaxi_SELFUN_F(F, E)                                                                    \
     _Generic(E,                                                                                   \
         float: F##_ld,                                                                            \
         double: F##_ld,                                                                           \
         long double: F##_ld,                                                                      \
         default: triaxi_int_float_error)
#  define triaxi_SELFUN_s(F, E)                                                                    \
     _Generic(E, char*: F##_ss, const char*: F##_ss, Triax_Str: F##_Ss, default: F##_vp)
#  define triaxi_SELFUN_S(F, E)                                                                    \
     _Generic(E, char*: F##_sS, const char*: F##_sS, Triax_Str: F##_SS, default: F##_vp)

// clang-format off
#  define triaxi_SELFUN(F, E1, E2)                                                                 \
     _Generic(E2,                                                                                  \
         char*: triaxi_SELFUN_s(F, E1),                                                            \
         const char*: triaxi_SELFUN_s(F, E1),                                                      \
         Triax_Str: triaxi_SELFUN_S(F, E1),                                                        \
         float: triaxi_SELFUN_F(F, E1),                                                            \
         double: triaxi_SELFUN_F(F, E1),                                                           \
         long double: triaxi_SELFUN_F(F, E1),                                                      \
         default: _Generic(E1,                                                                     \
             float: triaxi_int_float_error,                                                        \
             double: triaxi_int_float_error,                                                       \
             long double: triaxi_int_float_error,                                                  \
             TRIAXI_CHAR_SIGNED(char : triaxi_SELFUN_I(F, E2), )                                   \
             signed char: triaxi_SELFUN_I(F, E2),                                                  \
             short: triaxi_SELFUN_I(F, E2),                                                        \
             int: triaxi_SELFUN_I(F, E2),                                                          \
             long: triaxi_SELFUN_I(F, E2),                                                         \
             long long: triaxi_SELFUN_I(F, E2),                                                    \
             default: _Generic(E2,                                                                 \
                 TRIAXI_CHAR_SIGNED(char : F##_ui, )                                               \
                 signed char: F##_ui,                                                              \
                 short: F##_ui,                                                                    \
                 int: F##_ui,                                                                      \
                 long: F##_ui,                                                                     \
                 long long: F##_ui,                                                                \
                 TRIAXI_CHAR_UNSIGNED(char : F##_u, )                                              \
                 unsigned char: F##_u,                                                             \
                 unsigned short: F##_u,                                                            \
                 unsigned: F##_u,                                                                  \
                 unsigned long: F##_u,                                                             \
                 unsigned long long: F##_u,                                                        \
                 bool: F##_i,                                                                      \
                 default: F##_vp)))
// clang-format on

#  define triaxi_AF_eq(v, exp, act) triaxi_SELFUN(triaxi_AF_eq, exp, act)(v, exp, act)
#  define triaxi_AF_lt(v, exp, act) triaxi_SELFUN(triaxi_AF_lt, exp, act)(v, exp, act)
#  define triaxi_AF_gt(v, exp, act) triaxi_SELFUN(triaxi_AF_gt, exp, act)(v, exp, act)
#  define triaxi_AF_arreq(v, index, exp, act)                                                      \
     triaxi_SELFUN(triaxi_AF_arreq, exp, act)(v, index, exp, act)

# else
template <typename T>
struct TRIAXI_RemoveCvRef {
  typedef typename std::remove_cv<typename std::remove_reference<T>::type>::type type;
};
template <typename T>
struct TRIAXI_StaticExtentImpl
    : std::integral_constant<size_t, std::extent<T>::value ? std::extent<T>::value : SIZE_MAX> {};

template <typename T, size_t N>
struct TRIAXI_StaticExtentImpl<std::array<T, N>> : std::integral_constant<size_t, N> {};

template <typename T>
struct TRIAXI_StaticExtent : TRIAXI_StaticExtentImpl<typename TRIAXI_RemoveCvRef<T>::type> {};

template <typename T>
static constexpr typename std::enable_if<TRIAXI_StaticExtent<T>::value != SIZE_MAX, size_t>::type
    triaxi_cpp_size(const T&) noexcept {
  return TRIAXI_StaticExtent<T>::value;
}

template <typename T>
static inline typename std::enable_if<TRIAXI_StaticExtent<T>::value == SIZE_MAX, size_t>::type
    triaxi_cpp_size(const T& c) {
  return c.size();
}

template <typename A, typename B> // At least one runtime-sized operand.
static inline typename std::enable_if<TRIAXI_StaticExtent<A>::value == SIZE_MAX
                                          || TRIAXI_StaticExtent<B>::value == SIZE_MAX,
                                      size_t>::type
    triaxi_cpp_size_check(bool neq, const A& a, const B& b) {
  const size_t s1 = triaxi_cpp_size(a), s2 = triaxi_cpp_size(b);
  if (s1 == s2) { return s1; } // Different lengths already establish inequality.
  if (!neq) { (void)triaxi_log_RES2(TRIAXI_AR_FAIL2, s1, "%zu", s2, "%zu"); }
  return SIZE_MAX;
}

// Both operands have statically-known extents. A size mismatch is considered programmer misuse for
// both eq and neq.
template <typename A, typename B>
static inline typename std::enable_if<TRIAXI_StaticExtent<A>::value != SIZE_MAX
                                          && TRIAXI_StaticExtent<B>::value != SIZE_MAX,
                                      size_t>::type
    triaxi_cpp_size_check(bool, const A&, const B&) {
  static_assert(TRIAXI_StaticExtent<A>::value == TRIAXI_StaticExtent<B>::value,
                "Arrays must have the same size");
  return TRIAXI_StaticExtent<A>::value;
}

class TRIAXI_TestStream : public std::ostream {
public:
  TRIAXI_TestStream() : std::ostream(&streambuf) {}
  template <class T>
  void add_arg(const T& val) {
#  if defined(__cpp_lib_format)
    add_arg_impl<T>(val, formattable<T>{}, streamable<T>{});
#  else
    add_arg_impl<T>(val, streamable<T>{});
#  endif
  }
  void add_arg(std::nullptr_t) { this->write("null", 4), this->put('\0'); }
  void add_arg(float val) {
    auto p = this->precision(9);
    *this << val, this->precision(p), this->put('\0');
  }
  void add_arg(double val) {
    auto p = this->precision(17);
    *this << val, this->precision(p), this->put('\0');
  }
  void add_arg(long double val) {
    auto p = this->precision(21);
    *this << val, this->precision(p), this->put('\0');
  }
  bool send(TRIAXI_AR val) {
    if (streambuf.trunc) {
      static constexpr char tc[] = "...[truncated]";
      memcpy(TRIAXI_exec.pkg.args + streambuf.len - sizeof(tc), tc, sizeof(tc));
    }
    TRIAXI_exec.pkg.len = (uint32_t)streambuf.len;
    triaxi_log_res((TRIAXI_AR)val);
    return 1;
  }

private:
  template <typename...>
  using void_t = void;
  template <typename T, typename U = void>
  struct streamable : std::false_type {};
  template <typename T>
  struct streamable<T, void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
      : std::true_type {};

#  if defined(__cpp_lib_format)
  template <typename T, typename U = void>
  struct formattable : std::false_type {};

  template <typename T>
  struct formattable<T,
                     void_t<decltype(std::formatter<typename TRIAXI_RemoveCvRef<T>::type, char>{})>>
      : std::true_type {};

  template <class T>
  void add_arg_impl(const T& val, std::true_type /*formattable*/, ...) {
    const size_t avail = streambuf.cap - streambuf.len;
    /* Need at least one byte for this argument's terminating NUL. */
    if (avail <= 1) {
      streambuf.trunc = true, streambuf.len = streambuf.cap;
      return;
    }
    char* const  dst = TRIAXI_exec.pkg.args + streambuf.len;
    /* reserve one byte for '\0' */
    const auto   res     = std::format_to_n(dst, (ptrdiff_t)(avail - 1), "{}", val);

    const size_t written = (size_t)(res.out - dst);
    if ((size_t)res.size > avail - 1) {
      /* res.size is how many characters formatting wanted to produce. */
      streambuf.trunc = true;
      streambuf.len   = streambuf.cap;
      return;
    }
    streambuf.len                         += written;
    TRIAXI_exec.pkg.args[streambuf.len++]  = '\0';
  }

  template <class T> // Fallback: operator<< streamable
  void add_arg_impl(const T& val, std::false_type, std::true_type /*streamable*/) {
    *this << val, this->put('\0');
  }
  template <class T> // Last resort
  void add_arg_impl(const T&, std::false_type, std::false_type) {
    this->write("<?>", 3), this->put('\0');
  }
#  else
  template <class T>
  void add_arg_impl(const T& val, std::true_type) {
    *this << val, this->put('\0');
  }
  template <class T>
  void add_arg_impl(const T&, std::false_type) {
    this->write("<?>", 3), this->put('\0');
  }
#  endif
  class TRIAX_Sbuf : public std::streambuf {
    friend TRIAXI_TestStream;

  protected:
    static constexpr size_t cap{sizeof(TRIAXI_exec.storage) - sizeof(TRIAXI_exec.pkg) - 7};
    size_t                  len{0};
    bool                    trunc{false};
    virtual std::streamsize xsputn(const char* s, std::streamsize n) override {
      size_t avail = cap - len;
      if ((size_t)n > avail) { trunc = true, n = (std::streamsize)avail; }
      memcpy(TRIAXI_exec.pkg.args + len, s, (size_t)n), len += (size_t)n;
      return n;
    }
    virtual int_type overflow(int_type ch) override {
      if (ch != traits_type::eof()) {
        if (len == cap) { return trunc = true, traits_type::eof(); }
        TRIAXI_exec.pkg.args[len++] = (char)ch;
      }
      return ch;
    }
  } streambuf;
};
template <class T>
struct TRIAXI_IsStr
    : std::integral_constant<bool, std::is_same<typename std::decay<T>::type, const char*>::value
                                       || std::is_same<typename std::decay<T>::type, char*>::value
                                       || std::is_same<T, Triax_Str>::value
                                       || std::is_same<T, std::string>::value
#  ifdef __cpp_lib_string_view
                                       || std::is_same<T, std::string_view>::value
#  endif
                             > {
};

template <class T>
struct TRIAXI_IsHandled
    : std::integral_constant<bool, std::is_arithmetic<T>::value
                                       || std::is_pointer<typename std::decay<T>::type>::value
                                       || TRIAXI_IsStr<T>::value> {};
#  define TRIAXI_FOR_i                                                                             \
     typename std::enable_if<(std::is_integral<T>::value && std::is_signed<T>::value               \
                              && std::is_integral<U>::value && std::is_signed<U>::value),          \
                             int>::type                                                            \
         = 0
#  define TRIAXI_FOR_u                                                                             \
     typename std::enable_if<(std::is_unsigned<T>::value && std::is_unsigned<U>::value),           \
                             int>::type                                                            \
         = 0
#  define TRIAXI_FOR_iu                                                                            \
     typename std::enable_if<(std::is_integral<T>::value && std::is_signed<T>::value               \
                              && std::is_unsigned<U>::value),                                      \
                             int>::type                                                            \
         = 0
#  define TRIAXI_FOR_ui                                                                            \
     typename std::enable_if<(std::is_unsigned<T>::value && std::is_integral<U>::value             \
                              && std::is_signed<U>::value),                                        \
                             int>::type                                                            \
         = 0
#  define TRIAXI_FOR_ld                                                                            \
     typename std::enable_if<                                                                      \
         (std::is_floating_point<T>::value && std::is_floating_point<U>::value), int>::type        \
         = 0
#  define TRIAXI_FOR_vp                                                                            \
     typename std::enable_if<(std::is_pointer<T>::value && std::is_pointer<U>::value               \
                              && !(TRIAXI_IsStr<T>::value && TRIAXI_IsStr<U>::value)),             \
                             int>::type                                                            \
         = 0
#  define TRIAXI_FOR_SS                                                                            \
     typename std::enable_if<TRIAXI_IsStr<T>::value && TRIAXI_IsStr<U>::value, int>::type = 0
#  define TRIAXI_FOR_unhandled                                                                     \
     typename std::enable_if<!(TRIAXI_IsHandled<T>::value && TRIAXI_IsHandled<U>::value),          \
                             int>::type                                                            \
         = 0

#  define TRIAXI_AF_FORWARD(OP, SUF)                                                               \
     template <class T, class U, TRIAXI_FOR_##SUF>                                                 \
     static inline bool triaxi_AF_##OP(bool v, T e1, U e2) {                                       \
       return triaxi_AF_##OP##_##SUF(v, e1, e2);                                                   \
     }
#  define TRIAXI_AF_FORWARD_SS(OP)                                                                 \
     template <class T, class U, TRIAXI_FOR_SS>                                                    \
     static inline bool triaxi_AF_##OP(bool v, const T& e1, const U& e2) {                         \
       return triaxi_AF_##OP##_SS(v, Triax_Str(e1), Triax_Str(e2));                                \
     }
#  define TRIAXI_AF_FORWARD_XX(OP)                                                                 \
     template <class T, class U, TRIAXI_FOR_unhandled>                                             \
     static inline bool triaxi_AF_##OP(bool v, const T& e1, const U& e2) {                         \
       uint8_t val = TRIAXI_##OP(v, e1, e2);                                                       \
       if (!val) { return 0; }                                                                     \
       TRIAXI_TestStream stream;                                                                   \
       stream.add_arg(e1), stream.add_arg(e2);                                                     \
       return stream.send((TRIAXI_AR)val);                                                         \
     }

#  define TRIAXI_AF_OVERLOADS(X, Y, Z, OP)                                                         \
     X(OP, i) X(OP, u) X(OP, iu) X(OP, ui) X(OP, ld) X(OP, vp) Y(OP) Z(OP)
TRIAXI_AF_OVERLOADS(TRIAXI_AF_FORWARD, TRIAXI_AF_FORWARD_SS, TRIAXI_AF_FORWARD_XX, eq)
TRIAXI_AF_OVERLOADS(TRIAXI_AF_FORWARD, TRIAXI_AF_FORWARD_SS, TRIAXI_AF_FORWARD_XX, gt)
TRIAXI_AF_OVERLOADS(TRIAXI_AF_FORWARD, TRIAXI_AF_FORWARD_SS, TRIAXI_AF_FORWARD_XX, lt)

#  define TRIAXI_AF_FORWARD_ARREQ(OP, SUF)                                                         \
     template <class T, class U, TRIAXI_FOR_##SUF>                                                 \
     static inline bool triaxi_AF_##OP(bool v, size_t index, T e1, U e2) {                         \
       return triaxi_AF_##OP##_##SUF(v, index, e1, e2);                                            \
     }

#  define TRIAXI_AF_FORWARD_ARREQ_SS(OP)                                                           \
     template <class T, class U, TRIAXI_FOR_SS>                                                    \
     static inline bool triaxi_AF_##OP(bool v, size_t index, const T& e1, const U& e2) {           \
       return triaxi_AF_##OP##_SS(v, index, Triax_Str(e1), Triax_Str(e2));                         \
     }

#  define TRIAXI_AF_FORWARD_ARREQ_XX(OP)                                                           \
     template <class T, class U, TRIAXI_FOR_unhandled>                                             \
     static inline bool triaxi_AF_##OP(bool v, size_t index, const T& e1, const U& e2) {           \
       bool r = !(e1 == e2);                                                                       \
       if (!v && r) {                                                                              \
         TRIAXI_TestStream stream;                                                                 \
         stream.add_arg(index), stream.add_arg(e1), stream.add_arg(e2);                            \
         stream.send(TRIAXI_AR_FAIL1);                                                             \
       }                                                                                           \
       return r;                                                                                   \
     }

TRIAXI_AF_OVERLOADS(TRIAXI_AF_FORWARD_ARREQ, TRIAXI_AF_FORWARD_ARREQ_SS, TRIAXI_AF_FORWARD_ARREQ_XX,
                    arreq)

# endif /*TRIAXI_CPP*/

# pragma endregion assertion_dispatch

# if defined(TRIAX_IMPL) || !defined(TRIAX_MULTI_TU) /* TRIAXI_REGION_RUNNER_ONLY */

#  pragma region runner_types

typedef struct TRIAXI_Suite {
  const char*         name;
  Triax_Attributes    attrs;
  const TRIAXI_Test** tests;
  size_t              ntests;
} TRIAXI_Suite;

/// @brief The logical outcome of a test
typedef enum TRIAXI_Outcome {
  TRIAXI_OUTCOME_SKIP,
  TRIAXI_OUTCOME_PASSED,
  TRIAXI_OUTCOME_FAIL,
  TRIAXI_OUTCOME_UCRASH,  // unexpected crash while result was expected
  TRIAXI_OUTCOME_UEXIT,   // unexpected exit while result was expected
  TRIAXI_OUTCOME_UEXCEPT, // unexpected exception while result was expected
  TRIAXI_OUTCOME_TIMEOUT, // timeout
  TRIAXI_OUTCOME_ERROR,   // framework-detected user/test error
} TRIAXI_Outcome;
enum { TRIAXI_OUTCOME_COUNT = TRIAXI_OUTCOME_ERROR + 1 };

typedef struct TRIAXI_TestResult {
  struct {
    Triax_Str out, err;
  } capt;
  unsigned       nasserts;
  unsigned       nfails;
  TRIAXI_Outcome outcome;
  TRIAXI_State   state;
  TRIAXI_Exit    exit;
  uint32_t       duration_ms;
} TRIAXI_TestResult;

typedef struct TRIAXI_SuiteResult {
  size_t   outcomes[TRIAXI_OUTCOME_COUNT];
  uint32_t duration_ms;
} TRIAXI_SuiteResult;

typedef struct TRIAXI_RunResult {
  size_t suites_run;
  struct {
    size_t selected; // definitions surviving suite/test/tag filters
  } tests;
  struct {
    size_t selected; // concrete invocations surviving filters
    size_t cnts[TRIAXI_OUTCOME_COUNT];
  } invocations;
  uint32_t duration_ms;
} TRIAXI_RunResult;

#  ifndef _WIN32
#   define TRIAXI_PROC_NONE ((pid_t)0)
#  else
#   define TRIAXI_PROC_NONE NULL
#  endif

typedef struct TRIAXI_Fixtures {
  Triax_Fixture init, fini;
} TRIAXI_Fixtures;

typedef struct TRIAXI_AnsiColours {
  char reset[8], bold[8], dim[8], red[8], green[8], yellow[8], blue[8], magenta[8], cyan[8];
} TRIAXI_AnsiColours;

typedef struct TRIAXI_OutputCtx {
  struct {
    FILE *text, *json, *tap, *junit;
  } streams;
  TRIAXI_AnsiColours ansi;
} TRIAXI_OutputCtx;

typedef struct TRIAXI_StdBackup {
  struct {
    FILE* file;
  } out, err;
} TRIAXI_StdBackup;

typedef struct TRIAXI_Settings {
  uint32_t        timeout_ms;
  Triax_Verbosity verbosity;
  Triax_Isolation isolation;
} TRIAXI_Settings;

typedef struct TRIAXI_RunCtx {
  TRIAXI_Settings    settings;
  TRIAXI_Fixtures    fixtures;
  const char*        tags;
  size_t             nfilters;
  const char* const* filters;
  size_t             njobs;
  bool               fail_fast;
  bool               debug;
  TRIAXI_OutputCtx   out;
  TRIAXI_StdBackup   saved;
} TRIAXI_RunCtx;

typedef struct TRIAXI_SuiteCtx {
  TRIAXI_Settings settings;
  struct {
    TRIAXI_Fixtures run, suite;
  } fixtures;
} TRIAXI_SuiteCtx;

typedef struct TRIAXI_TestInvocation {
  const TRIAXI_Test* test;
  size_t             idx;
  TRIAXI_Settings    settings;
  struct {
    TRIAXI_Fixtures run, suite, test;
  } fixtures;
} TRIAXI_TestInvocation;

typedef struct TRIAXI_TestSlot {
  // Persistent slot resources.
  TRIAXI_Shared* shared;
  TRIAXI_File    log, out, err;
#  ifdef _WIN32
  WCHAR* environment;
#  endif
  // Current invocation state.
#  ifndef _WIN32
  pid_t process;
#  else
  HANDLE process;
  HANDLE job;
#  endif
  TRIAXI_TestInvocation invocation;
  int64_t               start_ms;
} TRIAXI_TestSlot;

typedef struct TRIAXI_DynBuf {
  char*  str;
  size_t cap;
} TRIAXI_DynBuf;

enum { TRIAXI_JOBS_MAX = 64 };
static struct {
  struct {
    const TRIAXI_Test *beg, *end;
  } tests;
  size_t ntests;
  size_t ninvocations;
  struct {
    const TRIAXI_SuiteReg *beg, *end;
  } suiteregs;
  size_t          nsuites;
  TRIAXI_Suite*   suites;
  size_t          max_prev_slots;
  TRIAXI_TestSlot slots[TRIAXI_JOBS_MAX];
  TRIAXI_Shared*  shared;
  struct { // reusable buffers
    TRIAXI_DynBuf log, out, err;
  } sbufs;
  struct {
    FILE *suite_buf, *test_buf;
  } junit;
#  ifdef _WIN32
  void (*old_sigabrt)(int);
  HANDLE shared_mapping;
  WCHAR  executable_path[32768];
  WCHAR  command_line[32768]; /* mutable CreateProcessW scratch */
#  endif
} TRIAXI_global;
#  define triaxi_global_initialised() (!!TRIAXI_global.max_prev_slots)

static const TRIAXI_Settings TRIAXI_default_settings = {
    TRIAX_TIMEOUT_DEFAULT,
    (Triax_Verbosity)TRIAX_VERBOSITY_DEFAULT,
    (Triax_Isolation)TRIAX_ISOLATION_DEFAULT,
};

#  pragma endregion runner_types

#  pragma region runner_utilities

#  ifdef _WIN32
#   define triaxi_dup       _dup
#   define triaxi_dup2      _dup2
#   define triaxi_fdopen    _fdopen
#   define triaxi_fileno    _fileno
#   define triaxi_isatty    _isatty
#   define triaxi_ftruncate _chsize_s
#   define triaxi_lseek     _lseeki64
#  else
#   define triaxi_dup       dup
#   define triaxi_dup2      dup2
#   define triaxi_fdopen    fdopen
#   define triaxi_fileno    fileno
#   define triaxi_isatty    isatty
#   define triaxi_ftruncate ftruncate
#   define triaxi_lseek     lseek
#  endif

static inline uintmax_t triaxi_parse_u(const char* s) {
  if (!s || s[0] < '0' || s[0] > '9') { return (uintmax_t)-1; }
  errno = 0;
  char*     ep;
  uintmax_t r = strtoumax(s, &ep, 10);
  if (ep == s || *ep || errno == ERANGE) { return (uintmax_t)-1; }
  return r;
}

static inline void triaxi_mkdirp(const char* path) {
  char   buf[4096];
  size_t n = strlen(path);
  if (n >= sizeof(buf)) { return; }
  memcpy(buf, path, n + 1);
  for (char* p = buf + 1; *p; ++p) {
#  ifndef _WIN32
    if (*p != '/') { continue; }
    char c = *p;
    *p     = '\0';
    mkdir(buf, 0755); // error handling via fopen in caller
#  else
    if (*p != '/' && *p != '\\') { continue; }
    char c = *p;
    *p     = '\0';
    CreateDirectoryA(buf, NULL);
#  endif
    *p = c;
  }
}

static inline TRIAXI_File triaxi_tmpfile_create(const char* base) {
  TRIAXI_File res;
#  ifndef _WIN32
#   if defined(__linux__) && defined(__NR_memfd_create)
  if ((res = (int)syscall(__NR_memfd_create, base, 1)) >= 0) { return res; }
#   endif // fallback
  char      buf[4096 + 256], *s = buf;
  Triax_Str tmp = {getenv("TMPDIR")};
  if (!tmp.str || !*tmp.str) { tmp.str = getenv("TMP"); }
  if (!tmp.str || !*tmp.str) { tmp.str = getenv("TEMP"); }
  if (!tmp.str || !*tmp.str) { tmp.str = "/tmp"; }
  if ((tmp.len = strlen(tmp.str)) >= sizeof(buf) - 100) { triaxi_fatal(); }
  s += triaxi_catstr(s, tmp.str, tmp.len);
  if (tmp.str[tmp.len - 1] != '/') { *s++ = '/'; }
  tmp.str = base, tmp.len = strlen(base);
  s += triaxi_catstr(s, tmp.str, tmp.len);
  s += triaxi_catlit(s, "XXXXXX"), *s = '\0';
  if ((res = mkstemp(buf)) < 0) { triaxi_fatal(); }
  if (fcntl(res, F_SETFD, FD_CLOEXEC) == -1) { triaxi_fatal(); };
  if (unlink(buf)) { triaxi_fatal(); }
  return res;
#  else
  char  temp_path[MAX_PATH];
  DWORD temp_path_len = GetTempPathA((DWORD)sizeof(temp_path), temp_path);
  if (!temp_path_len || temp_path_len >= sizeof(temp_path)) { triaxi_fatal(); }
  SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
  char                temp_name[MAX_PATH];
  if (!GetTempFileNameA(temp_path, base, 0, temp_name)) { triaxi_fatal(); }
  res = CreateFileA(temp_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
  if (res == INVALID_HANDLE_VALUE) { triaxi_fatal(); }
  return res;
#  endif
}
static inline void triaxi_tmpfiles_create(size_t njobs) {
  for (size_t i = TRIAXI_global.max_prev_slots; i < njobs; ++i) {
    TRIAXI_TestSlot* h = &TRIAXI_global.slots[i];
    h->log             = triaxi_tmpfile_create("triax_log");
    h->out             = triaxi_tmpfile_create("triax_out");
    h->err             = triaxi_tmpfile_create("triax_err");
#  ifdef _WIN32
    h->shared->launch.log = h->log;
    h->shared->launch.out = h->out;
    h->shared->launch.err = h->err;
#  endif
  }
}

static inline Triax_Str triaxi_file_read_buf(TRIAXI_File f, TRIAXI_DynBuf* buf) {
  size_t len = triaxi_file_size(f);
  if (!len) { return TRIAXI_T(Triax_Str){TRIAXI_ZINIT}; };
  if (len > buf->cap) {
    if (!(buf->str = (char*)realloc(buf->str, buf->cap = len))) { triaxi_fatal(); };
  }
  triaxi_file_read_into(f, buf->str, len);
  return TRIAXI_T(Triax_Str){buf->str, len};
}

static inline void triaxi_file_clear_stream(FILE* file) {
  if (triaxi_ftruncate(triaxi_fileno(file), 0)) { triaxi_fatal(); }
  if (fseek(file, 0, SEEK_SET)) { triaxi_fatal(); }
}
static inline void triaxi_file_clear_fd(int fd) {
  if (triaxi_lseek(fd, 0, SEEK_SET) < 0) { triaxi_fatal(); }
  if (triaxi_ftruncate(fd, 0)) { triaxi_fatal(); }
}

#  ifndef _WIN32
#   define triaxi_file_clear triaxi_file_clear_fd
#  else
static inline void triaxi_file_clear_handle(HANDLE handle) {
  LARGE_INTEGER liDistance = {TRIAXI_ZINIT};
  if (!SetFilePointerEx(handle, liDistance, NULL, FILE_BEGIN)) { triaxi_fatal(); }
  if (!SetEndOfFile(handle)) { triaxi_fatal(); }
}
#   define triaxi_file_clear triaxi_file_clear_handle
#  endif

static inline TRIAXI_StdBackup triaxi_stdfds_backup_create(void) {
  TRIAXI_StdBackup r;
  int              fd;
  if ((fd = triaxi_dup(1)) < 0) { triaxi_fatal(); }
  if (!(r.out.file = triaxi_fdopen(fd, "w"))) { triaxi_fatal(); }
  if ((fd = triaxi_dup(2)) < 0) { triaxi_fatal(); }
  if (!(r.err.file = triaxi_fdopen(fd, "w"))) { triaxi_fatal(); }
  TRIAXI_true_stderr = r.err.file;
  return r;
}

static inline void triaxi_stdfds_backup_close(const TRIAXI_StdBackup* backup) {
  FILE *const out = backup->out.file, *const err = backup->err.file;
  if (fclose(out)) { triaxi_fatal(); }
  TRIAXI_true_stderr = NULL;
  if (fclose(err)) { triaxi_fatal(); }
}

static inline void triaxi_stdfds_redirect(TRIAXI_TestSlot* h) {
  TRIAXI_exec.out = h->out, TRIAXI_exec.log = h->log, TRIAXI_exec.err = h->err;
  for (int i = 0; i < 2; ++i) {
    int         stdidx = !i ? 1 : 2;
    TRIAXI_File tmp    = !i ? TRIAXI_exec.out : TRIAXI_exec.err;
#  ifndef _WIN32
    if (triaxi_dup2(tmp, stdidx) < 0) { triaxi_fatal(); }
#  else
    DWORD  stdhandle = !i ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
    HANDLE dup;
    if (!DuplicateHandle(GetCurrentProcess(), tmp, GetCurrentProcess(), &dup, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
      triaxi_fatal();
    }
    int fd = _open_osfhandle((intptr_t)dup, _O_WRONLY | _O_BINARY);
    if (fd < 0) { triaxi_fatal(); }
    if (triaxi_dup2(fd, stdidx) < 0) { triaxi_fatal(); }
    _close(fd);
    intptr_t osfh = _get_osfhandle(stdidx);
    if (osfh == -1 || osfh == -2) { triaxi_fatal(); }
    if (!SetStdHandle(stdhandle, (HANDLE)osfh)) { triaxi_fatal(); }
#  endif
  }
}

static inline void triaxi_stdfds_restore(const TRIAXI_StdBackup* saved) {
  if (triaxi_dup2(triaxi_fileno(saved->out.file), 1) < 0) { triaxi_fatal(); }
  if (triaxi_dup2(triaxi_fileno(saved->err.file), 2) < 0) { triaxi_fatal(); }
#  ifdef _WIN32
  intptr_t out = _get_osfhandle(1), err = _get_osfhandle(2);
  if (out == -1 || out == -2) { triaxi_fatal(); }
  if (err == -1 || err == -2) { triaxi_fatal(); }
  if (!SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)out)) { triaxi_fatal(); }
  if (!SetStdHandle(STD_ERROR_HANDLE, (HANDLE)err)) { triaxi_fatal(); }
#  endif
}

#  pragma endregion runner_utilities

#  pragma region runner_registration

#  if TRIAXI_CPP
#   define TRIAXI_ALLOC_ARRAY(T, n) ((n) ? new T[n] : NULL)
#  else
#   define TRIAXI_ALLOC_ARRAY(T, n) ((n) ? (T*)malloc((n) * sizeof(T)) : NULL)
#  endif

static inline void triaxi_sections_init(void) {
#  if TRIAXI_REG_MSVC_COFF

#   if TRIAXI_CPP
#    define TRIAXI_ALLOC_ARRAY(T, n) ((n) ? new T[n] : NULL)
#   else
#    define TRIAXI_ALLOC_ARRAY(T, n) ((n) ? (T*)malloc((n) * sizeof(T)) : NULL)
#   endif

  /*
   * TRIAXI_Test_a/_z (and the SuiteReg equivalents) are now pointers, not
   * objects — see TRIAXI_MAKE_TEST's comment for why. &T##_a + 1 / &T##_z
   * therefore walks a pointer array (uniform size/alignment, so the $a/$m/$z
   * subsection concatenation can't insert padding inside it), landing
   * exactly on the first and one-past-the-last real entry. Each entry then
   * gets copied by value into a freshly allocated, genuinely-packed array —
   * so every other place in this file that treats TRIAXI_global.tests/
   * suiteregs as a plain contiguous C array of objects keeps working
   * unmodified, on this backend as on every other.
   */
#   define TRIAXI_SECTION_INIT_MSVC(T, field)                                                      \
      do {                                                                                         \
        extern const T* const T##_a;                                                               \
        extern const T* const T##_z;                                                               \
        const T* const*       triaxi_pbeg = &T##_a + 1;                                            \
        const T* const*       triaxi_pend = &T##_z;                                                \
        size_t                triaxi_n    = 0;                                                     \
        for (const T* const* p = triaxi_pbeg; p != triaxi_pend; ++p) {                             \
          if (*p) { ++triaxi_n; }                                                                  \
        }                                                                                          \
        T* triaxi_arr = TRIAXI_ALLOC_ARRAY(T, triaxi_n);                                           \
        if (triaxi_n && !triaxi_arr) { triaxi_fatal(); }                                           \
        size_t triaxi_i = 0;                                                                       \
        for (const T* const* p = triaxi_pbeg; p != triaxi_pend; ++p) {                             \
          if (*p) { triaxi_arr[triaxi_i++] = **p; }                                                \
        }                                                                                          \
        (field).beg = triaxi_arr;                                                                  \
        (field).end = triaxi_arr + triaxi_n;                                                       \
      } while (0)
  TRIAXI_SECTION_INIT_MSVC(TRIAXI_Test, TRIAXI_global.tests);
  TRIAXI_SECTION_INIT_MSVC(TRIAXI_SuiteReg, TRIAXI_global.suiteregs);
#   undef TRIAXI_SECTION_INIT_MSVC
#  elif TRIAXI_REG_MACHO
  unsigned long triaxi_test_section_size = 0, triaxi_suite_section_size = 0;
  TRIAXI_global.tests.beg = (const TRIAXI_Test*)(const void*)getsectiondata(
      &_mh_execute_header, "__DATA", "TRXTST", &triaxi_test_section_size);
  TRIAXI_global.tests.end
      = TRIAXI_global.tests.beg
          ? (const TRIAXI_Test*)(const void*)((const char*)TRIAXI_global.tests.beg
                                              + triaxi_test_section_size)
          : (const TRIAXI_Test*)0;
  TRIAXI_global.suiteregs.beg = (const TRIAXI_SuiteReg*)(const void*)getsectiondata(
      &_mh_execute_header, "__DATA", "TRXSUT", &triaxi_suite_section_size);
  TRIAXI_global.suiteregs.end
      = TRIAXI_global.suiteregs.beg
          ? (const TRIAXI_SuiteReg*)(const void*)((const char*)TRIAXI_global.suiteregs.beg
                                                  + triaxi_suite_section_size)
          : (const TRIAXI_SuiteReg*)0;
#  elif TRIAXI_REG_GNU_SECTION
  TRIAXI_global.tests.beg     = &__start_TRXTST;
  TRIAXI_global.tests.end     = &__stop_TRXTST;
  TRIAXI_global.suiteregs.beg = &__start_TRXSUT;
  TRIAXI_global.suiteregs.end = &__stop_TRXSUT;
#  else
#   error "Triax: unsupported compiler/linker registration backend"
#  endif
}

static inline TRIAXI_Suite* triaxi_find_suite(const char* name) {
  for (size_t i = 0; i < TRIAXI_global.nsuites; ++i) {
    TRIAXI_Suite* s = &TRIAXI_global.suites[i];
    if (name == s->name || !strcmp(name, s->name)) { return s; }
  }
  return NULL;
}
static inline void triaxi_register_tests(void) {
  const TRIAXI_Test *beg = TRIAXI_global.tests.beg, *end = TRIAXI_global.tests.end;
  size_t             cap = 16;
  TRIAXI_global.nsuites  = 0;
  TRIAXI_global.suites   = (TRIAXI_Suite*)malloc(sizeof(TRIAXI_Suite) * cap);
  if (!TRIAXI_global.suites) { triaxi_fatal(); }
  for (const TRIAXI_Test* t = beg; t != end; ++t) {
    if (!t->name) { continue; }
    ++TRIAXI_global.ntests;
    TRIAXI_global.ninvocations += TRIAXI_MAX(t->params.elcount, (size_t)1);
    TRIAXI_Suite* s             = triaxi_find_suite(t->suitename);
    if (s) {
      ++s->ntests;
      continue;
    }
    if (TRIAXI_global.nsuites == cap) {
      size_t nb            = sizeof(TRIAXI_Suite) * (cap *= 2);
      TRIAXI_global.suites = (TRIAXI_Suite*)realloc(TRIAXI_global.suites, nb);
      if (!TRIAXI_global.suites) { triaxi_fatal(); }
    }
    TRIAXI_global.suites[TRIAXI_global.nsuites++]
        = TRIAXI_T(TRIAXI_Suite){t->suitename, {TRIAXI_ZINIT}, NULL, 1};
  }
  for (size_t i = 0; i < TRIAXI_global.nsuites; ++i) {
    TRIAXI_Suite* s = &TRIAXI_global.suites[i];
    s->tests        = (const TRIAXI_Test**)malloc(sizeof(*s->tests) * s->ntests);
    if (!s->tests) { triaxi_fatal(); }
    s->ntests = 0;
  }
  for (const TRIAXI_Test* t = beg; t != end; ++t) {
    if (!t->name) { continue; }
    TRIAXI_Suite* s = triaxi_find_suite(t->suitename);
    if (!s) { triaxi_unreachable(); }
    s->tests[s->ntests++] = t;
  }
}

static inline void triaxi_register_suites(void) {
  const TRIAXI_SuiteReg *beg = TRIAXI_global.suiteregs.beg, *end = TRIAXI_global.suiteregs.end;
  for (const TRIAXI_SuiteReg* sr = beg; sr != end; ++sr) {
    if (!sr->name) { continue; } // zero-padding?
    TRIAXI_Suite* s = triaxi_find_suite(sr->name);
    if (s) { s->attrs = sr->attrs; }
  }
}
#  pragma endregion runner_registration

#  pragma region runner_process_control

/* POSIX signal/process-control state exists only on POSIX. All Windows
 * compilers (MSVC, clang-cl, MinGW GCC/Clang) use the Win32 process path. */
#  ifndef _WIN32

static struct {
  const int ign[3];
  const int term[3];
  const int crash[6];
  struct {
    struct sigaction ign, dfl, crash, term;
  } sas;
  struct {
    struct sigaction ign[3];
    struct sigaction term[3];
    struct sigaction crash[6];
    stack_t          altstack;
  } old;
} TRIAXI_signals = {{SIGPIPE, SIGUSR1, SIGUSR2},
                    {SIGTERM, SIGINT, SIGHUP},
                    {SIGSEGV, SIGBUS, SIGILL, SIGABRT, SIGFPE, SIGTRAP}};

#   define triaxi_sigaction(_sig, _act, _oldact)                                                   \
      (sigaction((_sig), (_act), (_oldact)) ? triaxi_fatal() : (void)0)

static inline void triaxi_sighandler_termination(int sig) {
  if (TRIAXI_exec.isolated) {
    /*
     * Isolated test is its own process-group leader. Kill the test and
     * descendants together. The current signal is blocked while this handler
     * executes, so resetting to default before kill() makes the re-raised
     * signal terminate us when the handler returns.
     */
    sigaction(sig, &TRIAXI_signals.sas.dfl, NULL);
    kill(0, sig);
    return;
  }
  /*
   * Runner: terminate only process groups belonging to active isolated tests.
   * Do not signal the runner's inherited process group.
   */
  for (size_t i = 0; i < triaxi_countof(TRIAXI_global.slots); ++i) {
    if (TRIAXI_global.slots[i].process > 0) { kill(-TRIAXI_global.slots[i].process, sig); }
  }
  sigaction(sig, &TRIAXI_signals.sas.dfl, NULL);
  kill(getpid(), sig);
  // only problem with this: children forked during non-isolated test runs
  // remain
}
static volatile sig_atomic_t TRIAXI_crash_signal;

static inline void           triaxi_sighandler_crash(int sig) {
  TRIAXI_crash_signal = sig;
  // exceptions out of a signal handler are UB
  longjmp(TRIAXI_exec.jmp, TRIAXI_EXEC_CRASHED); /*sig must be in TRIAXI_signals_crashes*/
}

#  else
/* Isolation child: makes abort() exit with a detectable crash code
   on all compilers, including MinGW and pre-2015 MSVC. */
static inline void triaxi_sighandler_abort(int sig) {
  (void)sig, RaiseException(TRIAX_FAULT_ABORT, EXCEPTION_NONCONTINUABLE, 0, NULL);
}
#  endif

static inline void triaxi_signals_init(void) {
#  ifndef _WIN32
  sigemptyset(&TRIAXI_signals.sas.ign.sa_mask);
  sigemptyset(&TRIAXI_signals.sas.dfl.sa_mask);
  TRIAXI_signals.sas.ign.sa_handler   = SIG_IGN;
  TRIAXI_signals.sas.dfl.sa_handler   = SIG_DFL;

  TRIAXI_signals.sas.crash.sa_handler = triaxi_sighandler_crash;
  sigemptyset(&TRIAXI_signals.sas.crash.sa_mask);
  TRIAXI_signals.sas.crash.sa_flags  = (int)(SA_ONSTACK | SA_RESETHAND | SA_NODEFER);
  TRIAXI_signals.sas.term.sa_handler = triaxi_sighandler_termination;
  sigfillset(&TRIAXI_signals.sas.term.sa_mask);
#  endif
}

#  ifdef _WIN32

/*
 * Environment blocks are sorted case-insensitively by variable name.
 * Special drive-current-directory entries have names such as "=C:", so
 * their separator is the second '=' rather than the first.
 */
static inline int triaxi_windows_env_name_cmp(const WCHAR* entry, const WCHAR* name) {
  const WCHAR* eq = wcschr(entry + (*entry == L'='), L'=');
  if (!eq || eq - entry > INT_MAX) { triaxi_fatal(); }

  int r = CompareStringOrdinal(entry, (int)(eq - entry), name, -1, TRUE);
  if (!r) { triaxi_fatal(); }

  return r - CSTR_EQUAL;
}
static inline void triaxi_windows_environments_init(size_t njobs) {
  LPWCH penv = GetEnvironmentStringsW();
  if (!penv) { triaxi_fatal(); }

  /* Upper bound for sanitized parent environment. */
  size_t cap = 1;
  for (const WCHAR* p = penv; *p;) {
    size_t len = wcslen(p) + 1;
    if (len > SIZE_MAX - cap) { triaxi_fatal(); }
    cap += len, p += len;
  }
  if (cap > SIZE_MAX / sizeof(WCHAR)) { triaxi_fatal(); }

  WCHAR* base = (WCHAR*)malloc(cap * sizeof(*base));
  if (!base) { triaxi_fatal(); }
  WCHAR* dst    = base;

  size_t shm_at = SIZE_MAX, slot_at = SIZE_MAX;
  for (const WCHAR* src = penv; *src;) {
    size_t len   = wcslen(src) + 1;
    int    cshm  = triaxi_windows_env_name_cmp(src, L"TRIAX_SHM");
    int    cslot = triaxi_windows_env_name_cmp(src, L"TRIAX_SLOT");

    size_t pos   = (size_t)(dst - base);

    if (shm_at == SIZE_MAX && cshm >= 0) { shm_at = pos; }
    if (slot_at == SIZE_MAX && cslot >= 0) { slot_at = pos; }

    /* Drop inherited/stale framework variables. */
    if (cshm && cslot) { memcpy(dst, src, len * sizeof(*dst)), dst += len; }
    src += len;
  }

  const size_t parent_chars = (size_t)(dst - base);
  if (shm_at == SIZE_MAX) { shm_at = parent_chars; }
  if (slot_at == SIZE_MAX) { slot_at = parent_chars; }
  if (shm_at > slot_at) { triaxi_fatal(); }

  WCHAR shm_var[64];
  int   n = swprintf(shm_var, triaxi_countof(shm_var), L"TRIAX_SHM=%llu",
                     (unsigned long long)(uintptr_t)TRIAXI_global.shared_mapping);

  if (n < 0 || (size_t)n >= triaxi_countof(shm_var)) { triaxi_fatal(); }

  const size_t shm_len = (size_t)n + 1;

  for (size_t i = 0; i < njobs; ++i) {
    WCHAR slot_var[64];

    n = swprintf(slot_var, triaxi_countof(slot_var), L"TRIAX_SLOT=%llu", (unsigned long long)i);
    if (n < 0 || (size_t)n >= triaxi_countof(slot_var)) { triaxi_fatal(); }
    const size_t slot_len = (size_t)n + 1;

    if (parent_chars > SIZE_MAX - shm_len || parent_chars + shm_len > SIZE_MAX - slot_len
        || parent_chars + shm_len + slot_len == SIZE_MAX) {
      triaxi_fatal();
    }

    const size_t nchars = parent_chars + shm_len + slot_len + 1;
    if (nchars > SIZE_MAX / sizeof(WCHAR)) { triaxi_fatal(); }

    WCHAR* env = (WCHAR*)malloc(nchars * sizeof(*env));
    if (!env) { triaxi_fatal(); }

    WCHAR* out = env;

    /* Before TRIAX_SHM. */
    memcpy(out, base, shm_at * sizeof(*out)), out     += shm_at;
    memcpy(out, shm_var, shm_len * sizeof(*out)), out += shm_len;

    /* Between TRIAX_SHM and TRIAX_SLOT. */
    size_t middle = slot_at - shm_at;
    memcpy(out, base + shm_at, middle * sizeof(*out)), out += middle;

    memcpy(out, slot_var, slot_len * sizeof(*out)), out    += slot_len;

    /* After TRIAX_SLOT. */
    size_t tail = parent_chars - slot_at;
    memcpy(out, base + slot_at, tail * sizeof(*out)), out += tail;
    *out                               = L'\0';
    TRIAXI_global.slots[i].environment = env;
  }
  free(base);
  if (!FreeEnvironmentStringsW(penv)) { triaxi_fatal(); }
}

/*
 * The root process is known to have exited before this is called.
 *
 * If terminated is false, terminate the job now to remove descendants.
 * If true, the timeout path already called TerminateJobObject.
 */
static inline void triaxi_windows_slot_process_finish(TRIAXI_TestSlot* h, bool terminated) {
  if (!terminated) {
    if (!TerminateJobObject(h->job, 1)) { triaxi_fatal(); }
  }
  if (!CloseHandle(h->process)) { triaxi_fatal(); }
  h->process = TRIAXI_PROC_NONE;
  for (;;) { // triaxi_windows_job_wait_empty
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = {TRIAXI_ZINIT};
    if (!QueryInformationJobObject(h->job, JobObjectBasicAccountingInformation, &info, sizeof(info),
                                   NULL)) {
      triaxi_fatal();
    }
    if (!info.ActiveProcesses) { break; }
    Sleep(1);
  }
  if (!CloseHandle(h->job)) { triaxi_fatal(); }
  h->job = NULL;
}

#  endif

static inline void triaxi_process_control_init(size_t njobs) {
#  ifndef _WIN32
  (void)njobs;
  static char altstack_mem[65536L]; // sized for ASAN/instrumented builds
  stack_t     ss = {0};
  ss.ss_sp = altstack_mem, ss.ss_size = sizeof(altstack_mem), ss.ss_flags = 0;
  if (sigaltstack(&ss, &TRIAXI_signals.old.altstack)) { triaxi_fatal(); }
  for (size_t i = 0; i < triaxi_countof(TRIAXI_signals.term); ++i) {
    triaxi_sigaction(TRIAXI_signals.term[i], &TRIAXI_signals.sas.term, &TRIAXI_signals.old.term[i]);
  }
  for (size_t i = 0; i < triaxi_countof(TRIAXI_signals.ign); ++i) {
    triaxi_sigaction(TRIAXI_signals.ign[i], &TRIAXI_signals.sas.ign, &TRIAXI_signals.old.ign[i]);
  }

  // Crash handlers aren't installed globally, but record what existed
  // before the run so non-isolated tests can restore them.
  for (size_t i = 0; i < triaxi_countof(TRIAXI_signals.crash); ++i) {
    triaxi_sigaction(TRIAXI_signals.crash[i], NULL, &TRIAXI_signals.old.crash[i]);
  }
#  else
  if ((TRIAXI_global.old_sigabrt = signal(SIGABRT, triaxi_sighandler_abort)) == SIG_ERR) {
    triaxi_fatal();
  }
  triaxi_windows_environments_init(njobs);
#  endif
}

static inline void triaxi_process_control_reset(void) {
#  ifndef _WIN32
  for (size_t i = 0; i < triaxi_countof(TRIAXI_signals.term); ++i) {
    triaxi_sigaction(TRIAXI_signals.term[i], &TRIAXI_signals.old.term[i], NULL);
  }
  for (size_t i = 0; i < triaxi_countof(TRIAXI_signals.ign); ++i) {
    triaxi_sigaction(TRIAXI_signals.ign[i], &TRIAXI_signals.old.ign[i], NULL);
  }

  for (size_t i = 0; i < triaxi_countof(TRIAXI_signals.crash); ++i) {
    triaxi_sigaction(TRIAXI_signals.crash[i], &TRIAXI_signals.old.crash[i], NULL);
  }
  stack_t ss = TRIAXI_signals.old.altstack;
  if (ss.ss_flags & SS_DISABLE) {
    // Darwin bug: zero size isn't ignored with SS_DISABLE
    ss.ss_flags = SS_DISABLE, ss.ss_size = (size_t)SIGSTKSZ;
  } else {
    ss.ss_flags = 0;
  }
  if (sigaltstack(&ss, NULL)) { triaxi_fatal(); }
#  else
  for (size_t i = 0; i < TRIAXI_global.max_prev_slots; ++i) {
    free(TRIAXI_global.slots[i].environment);
    TRIAXI_global.slots[i].environment = NULL;
  }
  if (signal(SIGABRT, TRIAXI_global.old_sigabrt) == SIG_ERR) { triaxi_fatal(); }
#  endif
}
TRIAXI_EXTERN_C_BEG
triaxi_noreturn void triaxi_fatal_f(const char* file, int line) {
  FILE* err = TRIAXI_true_stderr ? TRIAXI_true_stderr : stderr;
#  ifndef _WIN32
  fprintf(err, "%s:%d - triax framework error: %s\n", file, line,
          errno ? strerror(errno) : "(not provided)");

  (void)fflush(err);
  if (TRIAXI_exec.in_test) {
    if (TRIAXI_exec.isolated) {
      sigaction(SIGTERM, &TRIAXI_signals.sas.ign, NULL);
      kill(0, SIGTERM);
    }
    abort(); /* non-isolated: SIGABRT handler -> longjmp -> test error */
  } else {
    if (!TRIAXI_exec.isolated) {
      for (size_t i = 0; i < triaxi_countof(TRIAXI_global.slots); ++i) {
        if (TRIAXI_global.slots[i].process > 0) { kill(-TRIAXI_global.slots[i].process, SIGTERM); }
      }
    }
    abort();
  }
#  else

  DWORD error = GetLastError();
  if (error) {
    char* buf = NULL;
    DWORD len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                   | FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL, error, 0, (LPSTR)&buf, 0, NULL);
    if (len && buf) {
      while (len && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) { --len; }
      fprintf(err, "%s:%d - triax framework error: %.*s\n", file, line, (int)len, buf);
      LocalFree(buf);
    } else {
      fprintf(err, "%s:%d - triax framework error: Windows error %lu\n", file, line,
              (unsigned long)error);
    }
  } else {
    fprintf(err, "%s:%d - triax framework error\n", file, line);
  }
  (void)fflush(err);

  if (!TRIAXI_exec.isolated && !TRIAXI_exec.in_test) {
    for (size_t i = 0; i < triaxi_countof(TRIAXI_global.slots); ++i) {
      TRIAXI_TestSlot* h = &TRIAXI_global.slots[i];
      if (h->process != TRIAXI_PROC_NONE) { TerminateJobObject(h->job, 1); }
    }
  }
  abort();
#  endif
}
TRIAXI_EXTERN_C_END
#  pragma endregion runner_process_control

#  pragma region runner_printing

static const char* TRIAXI_assertnames[2][TRIAXI_AT_COUNT]
    = {{TRIAXI_STRINGIFY_ALL(TRIAXI_ASSERTTYPE_ITEMS)},
       {"nexit", "nfault", "false", "nonnull", "neq", "leq", "geq", "floatneq_abstol",
        "floatneq_reltol", "memneq", "memnzero", "strneq", "str_nstartswith", "str_nendswith",
        "str_ncontains", "check", "arrneq"}};

typedef enum TRIAXI_PrintMeta {
  TRIAXI_PRINT_META_NONE,
  TRIAXI_PRINT_META_INDEX,
  TRIAXI_PRINT_META_OFFSET
} TRIAXI_PrintMeta;

typedef struct TRIAXI_PrintArgs {
  Triax_Str        args[5];
  unsigned         nargs;
  TRIAXI_PrintMeta meta;
  union {
    Triax_Str index;
    Triax_Str mem_offset;
  };
} TRIAXI_PrintArgs;

static inline TRIAXI_PrintArgs triaxi_print_unpack_args(TRIAXI_AssertType       type,
                                                        const TRIAXI_AssertRes* res) {
  TRIAXI_PrintArgs r = {TRIAXI_ZINIT};
  if (!res->len) { return r; }
  const char *beg = res->args, *end = beg + res->len;
  switch (type) {
  default             : triaxi_unreachable();

  case TRIAXI_AT_memeq: {
    const char* nul = (const char*)memchr(beg, '\0', (size_t)(end - beg));
    if (!nul) { triaxi_fatal(); }
    r.mem_offset       = TRIAXI_T(Triax_Str){beg, (size_t)(nul - beg)};
    r.meta             = TRIAXI_PRINT_META_OFFSET;
    beg                = nul + 1;
    const size_t bytes = (size_t)(end - beg);
    if (bytes & 1) { triaxi_fatal(); }
    const size_t n = bytes / 2;
    r.args[0]      = TRIAXI_T(Triax_Str){beg, n};
    r.args[1]      = TRIAXI_T(Triax_Str){beg + n, n};
    r.nargs        = 2;
    return r;
  }
  case TRIAXI_AT_memzero: {
    const char* nul = (const char*)memchr(beg, '\0', (size_t)(end - beg));
    if (!nul) { triaxi_fatal(); }
    r.mem_offset = TRIAXI_T(Triax_Str){beg, (size_t)(nul - beg)};
    r.meta       = TRIAXI_PRINT_META_OFFSET;
    beg          = nul + 1;
    r.args[0]    = TRIAXI_T(Triax_Str){beg, (size_t)(end - beg)};
    r.nargs      = 1;
    return r;
  }
  case TRIAXI_AT_arreq:
    if (res->val == TRIAXI_AR_FAIL1) {
      const char* nul = (const char*)memchr(beg, '\0', (size_t)(end - beg));
      if (!nul) { triaxi_fatal(); }
      r.index = TRIAXI_T(Triax_Str){beg, (size_t)(nul - beg)};
      r.meta  = TRIAXI_PRINT_META_INDEX;
      beg     = nul + 1;
    }
    break;
  case TRIAXI_AT_check:
  case TRIAXI_AT_eq:
  case TRIAXI_AT_exit:
  case TRIAXI_AT_fault:
  case TRIAXI_AT_floateq_abstol:
  case TRIAXI_AT_floateq_reltol:
  case TRIAXI_AT_gt:
  case TRIAXI_AT_lt:
  case TRIAXI_AT_null:
  case TRIAXI_AT_str_contains:
  case TRIAXI_AT_str_endswith:
  case TRIAXI_AT_str_startswith:
  case TRIAXI_AT_streq:
  case TRIAXI_AT_true          : break;
  }
  switch (res->code & TRIAXI_ENCODING_MASK) {
  case TRIAXI_ENCODING_STRING: {
    size_t e1_len;
    if ((size_t)(end - beg) < sizeof(e1_len)) { triaxi_fatal(); }
    memcpy(&e1_len, beg, sizeof(e1_len)), beg += sizeof(e1_len);
    const size_t bytes = (size_t)(end - beg);
    if (e1_len > bytes) { triaxi_fatal(); }
    const size_t e2_len = bytes - e1_len;
    const bool   null1  = res->code & TRIAXI_ENCODING_NULL_ARG1;
    const bool   null2  = res->code & TRIAXI_ENCODING_NULL_ARG2;
    if ((null1 && e1_len) || (null2 && e2_len)) { triaxi_fatal(); }
    r.args[0] = null1 ? TRIAXI_T(Triax_Str){NULL, 0} : TRIAXI_T(Triax_Str){beg, e1_len};
    r.args[1] = null2 ? TRIAXI_T(Triax_Str){NULL, 0} : TRIAXI_T(Triax_Str){beg + e1_len, e2_len};
    r.nargs   = 2;
    return r;
  }

  case TRIAXI_ENCODING_DEFAULT:
    while (beg < end) {
      if (r.nargs == triaxi_countof(r.args)) { triaxi_fatal(); }

      const char* nul = (const char*)memchr(beg, '\0', (size_t)(end - beg));
      if (!nul) { triaxi_fatal(); }

      r.args[r.nargs++] = TRIAXI_T(Triax_Str){beg, (size_t)(nul - beg)};
      beg               = nul + 1;
    }
    return r;
  default: triaxi_fatal();
  }
}

static inline const char* triaxi_print_outcome_tostr(TRIAXI_Outcome et) {
  switch (et) {
  case TRIAXI_OUTCOME_SKIP   : return "skipped";
  case TRIAXI_OUTCOME_PASSED : return "passed";
  case TRIAXI_OUTCOME_FAIL   : return "failed";
  case TRIAXI_OUTCOME_UCRASH : return "ucrashed";
  case TRIAXI_OUTCOME_UEXIT  : return "uexited";
  case TRIAXI_OUTCOME_UEXCEPT: return "uexception";
  case TRIAXI_OUTCOME_TIMEOUT: return "timeout";
  case TRIAXI_OUTCOME_ERROR  : return "test_error";
  default                    : triaxi_unreachable();
  }
}

static inline const char* triaxi_error_name(TRIAXI_Error error) {
  switch (error) {
  case TRIAXI_ERROR_NONE                         : return "none";
  case TRIAXI_ERROR_TIMEOUT_WITHOUT_ISOLATION    : return "timeout_without_isolation";
  case TRIAXI_ERROR_MALFORMED_PARAMS             : return "malformed_params";
  case TRIAXI_ERROR_PARAM_ACCESS                 : return "parameter_access";
  case TRIAXI_ERROR_SKIP_IN_FIXTURE              : return "skip_in_fixture";
  case TRIAXI_ERROR_ASSERT_IN_FIXTURE            : return "assertion_in_fixture";
  case TRIAXI_ERROR_EXIT_ASSERT_WITHOUT_ISOLATION: return "exit_assert_without_isolation";
  case TRIAXI_ERROR_FILE_OPEN                    : return "file_open";
  case TRIAXI_ERROR_FILE_READ                    : return "file_read";
  default                                        : triaxi_unreachable();
  }
}
static inline const char* triaxi_error_tostr(TRIAXI_Error error) {
  switch (error) {
  case TRIAXI_ERROR_NONE                     : return "no error";
  case TRIAXI_ERROR_TIMEOUT_WITHOUT_ISOLATION: return "timeouts require process isolation";
  case TRIAXI_ERROR_MALFORMED_PARAMS         : return "malformed test parameters";
  case TRIAXI_ERROR_PARAM_ACCESS             : return "parameter access in non-parameterized test";
  case TRIAXI_ERROR_SKIP_IN_FIXTURE          : return "skip is not valid in fixtures";
  case TRIAXI_ERROR_ASSERT_IN_FIXTURE        : return "assertions are only valid in test functions";
  case TRIAXI_ERROR_EXIT_ASSERT_WITHOUT_ISOLATION:
    return "exit assertions require process isolation";
  case TRIAXI_ERROR_FILE_OPEN: return "could not open file";
  case TRIAXI_ERROR_FILE_READ: return "could not read file";
  default                    : triaxi_unreachable();
  }
}
static inline const char* triaxi_syserror_tostr(uint32_t syserr, char* buf, size_t cap) {
  if (!syserr) { return NULL; }
#  ifndef _WIN32
  (void)buf, (void)cap;
  return strerror((int)syserr);
#  else
  DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                           (DWORD)syserr, 0, buf, (DWORD)cap, NULL);
  if (!n) {
    snprintf(buf, cap, "Windows error %" PRIu32, syserr);
  } else {
    while (n && (buf[n - 1] == '\r' || buf[n - 1] == '\n')) { buf[--n] = '\0'; }
  }
  return buf;
#  endif
}

static inline size_t triaxi_utf8_seq_len(const char* str, size_t len) {
  if (!len) { return 0; }
  const unsigned char* s = (const unsigned char*)str;
  unsigned char        c = s[0];
  if (c < 0x80) { return 1; }
  if (c >= 0xC2 && c <= 0xDF) { return len >= 2 && (s[1] & 0xC0) == 0x80 ? 2 : 0; }
  if (c == 0xE0) {
    return len >= 3 && s[1] >= 0xA0 && s[1] <= 0xBF && (s[2] & 0xC0) == 0x80 ? 3 : 0;
  }
  if ((c >= 0xE1 && c <= 0xEC) || (c >= 0xEE && c <= 0xEF)) {
    return len >= 3 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 ? 3 : 0;
  }
  if (c == 0xED) {
    return len >= 3 && s[1] >= 0x80 && s[1] <= 0x9F && (s[2] & 0xC0) == 0x80 ? 3 : 0;
  }
  if (c == 0xF0) {
    return len >= 4 && s[1] >= 0x90 && s[1] <= 0xBF && (s[2] & 0xC0) == 0x80
                && (s[3] & 0xC0) == 0x80
             ? 4
             : 0;
  }
  if (c >= 0xF1 && c <= 0xF3) {
    return len >= 4 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80 ? 4
                                                                                               : 0;
  }
  if (c == 0xF4) {
    return len >= 4 && s[1] >= 0x80 && s[1] <= 0x8F && (s[2] & 0xC0) == 0x80
                && (s[3] & 0xC0) == 0x80
             ? 4
             : 0;
  }
  return 0;
}

static inline bool triaxi_utf8_valid(const char* str, size_t len) {
  if (!str) { return len == 0; }
  for (size_t i = 0; i < len;) {
    size_t n = triaxi_utf8_seq_len(str + i, len - i);
    if (!n) { return false; }
    i += n;
  }
  return true;
}

static inline void triaxi_print_hex(FILE* out, const char* str, size_t len) {
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < len; ++i) {
    unsigned char c   = (unsigned char)str[i];
    char          b[] = {hex[c >> 4], hex[c & 15]};
    fwrite(b, 1, sizeof(b), out);
  }
}

static inline void triaxi_print_json_escape(FILE* out, const char* str, size_t len) {
  if (!str) { return; }
  const char* beg = str;
  size_t      i   = 0;
  while (i < len) {
    unsigned char c = (unsigned char)str[i];
    if (c >= 0x80) {
      size_t n = triaxi_utf8_seq_len(str + i, len - i);
      if (n) {
        i += n;
        continue;
      }
    } else if (c >= 0x20 && c != '"' && c != '\\') {
      ++i;
      continue;
    }

    if (str + i > beg) { fwrite(beg, 1, (size_t)(str + i - beg), out); }
    static const char hex[]  = "0123456789abcdef";
    char              buf[6] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
    size_t            nbuf   = 6;
    switch (c) {
    case '\b': buf[1] = 'b', nbuf = 2; break;
    case '\t': buf[1] = 't', nbuf = 2; break;
    case '\n': buf[1] = 'n', nbuf = 2; break;
    case '\f': buf[1] = 'f', nbuf = 2; break;
    case '\r': buf[1] = 'r', nbuf = 2; break;
    case '"' : buf[1] = '"', nbuf = 2; break;
    case '\\': buf[1] = '\\', nbuf = 2; break;
    default  : break;
    }
    fwrite(buf, 1, nbuf, out);
    ++i;
    beg = str + i;
  }
  if (str + len > beg) { fwrite(beg, 1, (size_t)(str + len - beg), out); }
}

static inline void triaxi_print_json_bytes(FILE* out, Triax_Str str) {
  if (triaxi_utf8_valid(str.str, str.len)) {
    putc('"', out);
    triaxi_print_json_escape(out, str.str, str.len);
    putc('"', out);
    return;
  }
  if (!str.str) { triaxi_fatal(); }
  triaxi_fputlit("{\"encoding\":\"hex\",\"data\":\"", out);
  triaxi_print_hex(out, str.str, str.len);
  triaxi_fputlit("\"}", out);
}

static inline bool triaxi_xml_valid(const char* str, size_t len) {
  if (!triaxi_utf8_valid(str, len)) { return false; }

  for (size_t i = 0; i < len;) {
    unsigned char c = (unsigned char)str[i];

    if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') { return false; }

    size_t n = triaxi_utf8_seq_len(str + i, len - i);

    /* U+FFFE / U+FFFF are not XML 1.0 characters. */
    if (n == 3 && (unsigned char)str[i] == 0xEF && (unsigned char)str[i + 1] == 0xBF
        && ((unsigned char)str[i + 2] == 0xBE || (unsigned char)str[i + 2] == 0xBF)) {
      return false;
    }

    i += n;
  }

  return true;
}

static inline void triaxi_print_xml_escape(FILE* out, const char* str, size_t len) {
  if (!str) { return; }
  const char *beg = str, *cur = beg, *end = beg + len;
  for (; cur < end; ++cur) {
    unsigned char c       = (unsigned char)*cur;
    bool          illegal = c < 0x20 && c != '\t' && c != '\n' && c != '\r';
    if (!illegal && c != '"' && c != '&' && c != '\'' && c != '<' && c != '>') { continue; }
    fwrite(beg, 1, (size_t)(cur - beg), out);
    beg = cur + 1;
    if (illegal) { continue; }
    switch (c) {
    default  : triaxi_unreachable();
    case '"' : triaxi_fputlit("&quot;", out); break;
    case '&' : triaxi_fputlit("&amp;", out); break;
    case '\'': triaxi_fputlit("&apos;", out); break;
    case '<' : triaxi_fputlit("&lt;", out); break;
    case '>' : triaxi_fputlit("&gt;", out); break;
    }
  }
  fwrite(beg, 1, (size_t)(end - beg), out);
}
static inline void triaxi_print_xml_bytes(FILE* out, Triax_Str str) {
  if (triaxi_xml_valid(str.str, str.len)) {
    triaxi_print_xml_escape(out, str.str, str.len);
    return;
  }

  if (!str.str) { triaxi_fatal(); }

  triaxi_fputlit("hex:", out);
  triaxi_print_hex(out, str.str, str.len);
}
static inline void triaxi_print_run_beg_text(const TRIAXI_RunCtx* run) { (void)run; }

static inline void triaxi_print_suite_beg_text(const TRIAXI_RunCtx* run, const TRIAXI_Suite* s) {
  if (!run->out.streams.text) { return; }
  fprintf(run->out.streams.text, "\n%s%s%s\n--------------------------------------------------\n",
          run->out.ansi.bold, s->name, run->out.ansi.reset);
}
static inline Triax_Str triaxi_sprint_hex(char* buf, Triax_Str s) {
  char*             out   = buf;
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < s.len; ++i) {
    unsigned char c = (unsigned char)s.str[i];
    if (i) { *out++ = ' '; }
    *out++ = hex[c >> 4];
    *out++ = hex[c & 0xf];
  }
  return TRIAXI_T(Triax_Str){buf, (size_t)(out - buf)};
}
static inline void triaxi_print_assert_text(const TRIAXI_RunCtx* run, const TRIAXI_Test* t,
                                            const TRIAXI_AssertHdr* hdr,
                                            const TRIAXI_AssertRes* res) {
  FILE* out = run->out.streams.text;
  if (!out) { return; }
  bool              neg   = hdr->neg;
  TRIAXI_AssertType atype = (TRIAXI_AssertType)hdr->type;
  {
    const char *c    = run->out.ansi.red, *l;
    bool        more = false;
    if (!res) {
      more = 0, l = "[P]", c = run->out.ansi.green;
    } else {
      switch ((TRIAXI_AR)res->val) {
      default                : triaxi_unreachable();
      case TRIAXI_AR_FAIL1   :
      case TRIAXI_AR_FAIL2   :
      case TRIAXI_AR_FAIL3   : more = 1, l = "[F]"; break;
      case TRIAXI_AR_TIMEOUT : more = 0, l = "[T]"; break;
      case TRIAXI_AR_UCRASHED: more = 0, l = "[C]"; break;
      case TRIAXI_AR_UEXITED : more = 0, l = "[E]"; break;
      case TRIAXI_AR_UEXCEPT : more = 0, l = "[X]"; break;
      }
    }
    fprintf(out, "%s%s%s %s:%" PRIu32 " %s_%s(%.*s)%c", c, l, run->out.ansi.reset, t->file,
            hdr->line, hdr->ae ? "assert" : "expect", TRIAXI_assertnames[neg][atype],
            (int)hdr->expr_len, hdr->expr_str, more ? ' ' : '\n');
    if (!more) { return; }
  }
  TRIAXI_PrintArgs pa = triaxi_print_unpack_args(atype, res);
  char             prebuf[512], postbuf[512];
  char             obuf[512];
  prebuf[0] = '\0', postbuf[0] = '\0';
  char *                 pre = prebuf, *post = postbuf;
  Triax_Str              exp = {TRIAXI_ZINIT}, got = {TRIAXI_ZINIT};
  const char *           op = "", *q1 = "", *q2 = "";
  static const Triax_Str nullstr = {"NULL", triaxi_lenof("NULL")};
  if ((res->code & TRIAXI_ENCODING_MASK) == TRIAXI_ENCODING_STRING) {
    if (res->code & TRIAXI_ENCODING_NULL_ARG1) {
      pa.args[0] = nullstr;
    } else {
      q1 = "\"";
    }
    if (res->code & TRIAXI_ENCODING_NULL_ARG2) {
      pa.args[1] = nullstr;
    } else {
      q2 = "\"";
    }
  }

  switch (atype) {
  default: triaxi_unreachable();
  // --- crash / exit ---
  case TRIAXI_AT_exit:
  case TRIAXI_AT_fault:
    exp = pa.args[0], got = pa.args[1]; // fault/exit already formatted
    pre += triaxi_catlit(pre, "Expected "), *pre = '\0';
    op = " got ";
    break;

  // --- unary ---
  case TRIAXI_AT_true: op = !neg ? "was false" : "was true"; break;
  case TRIAXI_AT_null:
    if (neg) {
      op = "was NULL";
      break;
    }
    exp = pa.args[0], got = nullstr;
    op = " != ";
    break;

  // --- numeric equality ---
  case TRIAXI_AT_eq:
    exp = pa.args[0], got = pa.args[1];
    op = !neg ? " != " : " == ";
    break;

  case TRIAXI_AT_gt:
    exp = pa.args[0], got = pa.args[1];
    if (res->val == TRIAXI_AR_FAIL3) {
      op = " unordered with ";
    } else {
      op = !neg ? (res->val == TRIAXI_AR_FAIL1 ? " == " : " < ") : " > ";
    }
    break;

  case TRIAXI_AT_lt:
    exp = pa.args[0], got = pa.args[1];
    if (res->val == TRIAXI_AR_FAIL3) {
      op = " unordered with ";
    } else {
      op = !neg ? (res->val == TRIAXI_AR_FAIL1 ? " == " : " > ") : " < ";
    }
    break;
  // --- floats ---
  case TRIAXI_AT_floateq_abstol:
  case TRIAXI_AT_floateq_reltol:
    exp = pa.args[0], got = pa.args[1];
    op     = !neg ? " != " : " == ";
    post  += triaxi_catlit(post, " (tol: ");
    post  += triaxi_catstr(post, pa.args[2].str, pa.args[2].len);
    post  += triaxi_catlit(post, ") ");
    *post  = '\0';
    break;

  case TRIAXI_AT_memeq:
    if (!neg) {
      exp   = triaxi_sprint_hex(obuf, pa.args[0]);
      got   = triaxi_sprint_hex(obuf + sizeof(obuf) / 2, pa.args[1]);
      pre  += triaxi_catlit(pre, " (byte ");
      pre  += triaxi_catstr(pre, pa.mem_offset.str, pa.mem_offset.len);
      pre  += triaxi_catlit(pre, ") ");
      *pre  = '\0';
      op    = " != ";
    } else {
      op = "equal";
    }
    break;

  case TRIAXI_AT_memzero:
    if (!neg) {
      exp   = triaxi_sprint_hex(obuf, pa.args[0]);
      pre  += triaxi_catlit(pre, " (byte ");
      pre  += triaxi_catstr(pre, pa.mem_offset.str, pa.mem_offset.len);
      pre  += triaxi_catlit(pre, ") ");
      *pre  = '\0';
      op    = " nonzero ";
    } else {
      op = " all zero ";
    }
    break;
  // --- string ---
  case TRIAXI_AT_streq:
    exp = pa.args[0], got = pa.args[1];
    op = !neg ? " != " : " == ";
    break;
  case TRIAXI_AT_str_startswith:
    exp = pa.args[0], got = pa.args[1];
    op = !neg ? " does not start with " : " starts with ";
    break;
  case TRIAXI_AT_str_endswith:
    exp = pa.args[0], got = pa.args[1];
    op = !neg ? " does not end with " : " ends with ";
    break;
  case TRIAXI_AT_str_contains:
    exp = pa.args[0], got = pa.args[1];
    op = !neg ? " does not contain " : " contains ";
    break;
  // --- format ---
  case TRIAXI_AT_check: op = pa.args[0].str; break;
  case TRIAXI_AT_arreq:
    if (res->val == TRIAXI_AR_FAIL2) {
      // Automatic-size arreq: runtime lengths differ.
      exp = pa.args[0], got = pa.args[1];
      op    = " != ";
      pre  += triaxi_catlit(pre, "[size] ");
      *pre  = '\0';
    } else if (neg) {
      pre  += triaxi_catlit(pre, "[all ");
      pre  += triaxi_catstr(pre, pa.index.str, pa.index.len);
      pre  += triaxi_catlit(pre, " elements equal]");
      *pre  = '\0';
      q1 = q2 = "";
    } else {
      exp = pa.args[0], got = pa.args[1];
      op    = " != ";
      pre  += triaxi_catlit(pre, "[index ");
      pre  += triaxi_catstr(pre, pa.index.str, pa.index.len);
      pre  += triaxi_catlit(pre, "] ");
      *pre  = '\0';
    }
    break;
  }
  fprintf(out, "%s%s", prebuf, q1);
  if (exp.len) { fwrite(exp.str, 1, exp.len, out); }
  fprintf(out, "%s%s%s%s%s", q1, run->out.ansi.red, op, run->out.ansi.reset, q2);
  if (got.len) { fwrite(got.str, 1, got.len, out); }
  fprintf(out, "%s%s\n", q2, postbuf);
}

/* Stable machine-readable execution phase; NULL when no phase is meaningful. */
static inline const char* triaxi_state_phase_name(TRIAXI_State state) {
  switch (state) {
  default                    : triaxi_unreachable();
  case TRIAXI_STATE_SETUP    : return "setup";
  case TRIAXI_STATE_INIT     : return "init";
  case TRIAXI_STATE_TEST     :
  case TRIAXI_STATE_IN_ASSERT: return "body";
  case TRIAXI_STATE_FINI     : return "fini";
  case TRIAXI_STATE_CLEANUP  : return "cleanup";
  case TRIAXI_STATE_DONE     :
  case TRIAXI_STATE_SKIPPED  : return NULL;
  }
}

/* Human-readable phase for diagnostics; omit the ordinary test-body phase. */
static inline const char* triaxi_state_phase_phrase(TRIAXI_State state) {
  switch (state) {
  default                    : triaxi_unreachable();
  case TRIAXI_STATE_INIT     : return "fixture init";
  case TRIAXI_STATE_FINI     : return "fixture fini";
  case TRIAXI_STATE_TEST     :
  case TRIAXI_STATE_IN_ASSERT: return NULL;
  case TRIAXI_STATE_SETUP    :
  case TRIAXI_STATE_CLEANUP  :
  case TRIAXI_STATE_DONE     :
  case TRIAXI_STATE_SKIPPED  : return triaxi_state_phase_name(state); // "setup"/"cleanup"/NULL
  }
}

static inline void triaxi_print_test_beg_text(const TRIAXI_RunCtx*         run,
                                              const TRIAXI_TestInvocation* inv) {
  if (!run->out.streams.text) { return; }
  const TRIAXI_Test* t = inv->test;
  char               buf[32];
  if (t->params.elcount) {
    sprintf(buf, "[%zu]", inv->idx);
  } else {
    buf[0] = '\0';
  }
  fprintf(run->out.streams.text, "%s%s::%s%s%s%s%s\n", run->out.ansi.dim, t->suitename,
          run->out.ansi.reset, run->out.ansi.yellow, t->name, buf, run->out.ansi.reset);
}
static inline void triaxi_print_test_end_text(const TRIAXI_RunCtx*         run,
                                              const TRIAXI_TestInvocation* inv,
                                              const TRIAXI_TestResult*     r) {
  FILE* out = run->out.streams.text;
  if (!out) { return; }
  Triax_Verbosity v = inv->settings.verbosity;
  if (v != TRIAX_VERBOSITY_NEVER
      && (r->outcome != TRIAXI_OUTCOME_PASSED || v == TRIAX_VERBOSITY_ALWAYS)) {
    if (r->capt.out.len > 0) {
      fprintf(out, "%s[stdout]%s", run->out.ansi.cyan, run->out.ansi.reset);
      fwrite(r->capt.out.str, 1, r->capt.out.len, out);
      fprintf(out, "%s[/stdout]%s\n", run->out.ansi.cyan, run->out.ansi.reset);
    }
    if (r->capt.err.len > 0) {
      fprintf(out, "%s[stderr]%s", run->out.ansi.cyan, run->out.ansi.reset);
      fwrite(r->capt.err.str, 1, r->capt.err.len, out);
      fprintf(out, "%s[/stderr]%s\n", run->out.ansi.cyan, run->out.ansi.reset);
    }
  }
  unsigned    total = r->nasserts, failed = r->nfails, passed = total - failed;
  const char *label, *color;
  char        detail[1028];
  switch (r->outcome) {
  default: triaxi_unreachable();
  case TRIAXI_OUTCOME_SKIP:
    label = "> SKIP", color = run->out.ansi.yellow;
    detail[0] = '\0';
    break;
  case TRIAXI_OUTCOME_PASSED:
    label = "> PASS", color = run->out.ansi.green;
    sprintf(detail, ": %u assertions", r->nasserts);
    break;
  case TRIAXI_OUTCOME_FAIL:
    label = "> FAIL", color = run->out.ansi.red;
    sprintf(detail, ": %u failed, %u passed (total %u)", failed, passed, total);
    break;
  case TRIAXI_OUTCOME_UCRASH:
    label = "> UCRASH", color = run->out.ansi.red;
    sprintf(detail, ": %u failed, %u passed (total %u), crashed with: %s", failed, passed, total,
            triaxi_fault_tostr(r->exit.reason));
    break;
  case TRIAXI_OUTCOME_UEXIT:
    label = "> UEXIT", color = run->out.ansi.red;
    sprintf(detail, ": %u failed, %u passed (total %u), exited with code %u", failed, passed, total,
            r->exit.code);
    break;
  case TRIAXI_OUTCOME_UEXCEPT:
    label = "> UEXCEPTION", color = run->out.ansi.red;
    sprintf(detail, ": %u failed, %u passed (total %u), unhandled C++ exception", failed, passed,
            total);
    break;
  case TRIAXI_OUTCOME_TIMEOUT: {
    label = "TIMEOUT", color = run->out.ansi.red;
    const char* phase = triaxi_state_phase_phrase(r->state);
    if (phase) {
      sprintf(detail, ": %u failed, %u passed (total %u), %s exceeded %" PRIu32 "ms", failed,
              passed, total, phase, inv->settings.timeout_ms);
    } else {
      sprintf(detail, ": %u failed, %u passed (total %u), exceeded %" PRIu32 "ms", failed, passed,
              total, inv->settings.timeout_ms);
    }
    break;
  }
  case TRIAXI_OUTCOME_ERROR:
    color = run->out.ansi.red;
    switch (r->state) {
    default                    : triaxi_unreachable();
    case TRIAXI_STATE_SETUP    : label = "> TEST SETUP ERROR"; break;
    case TRIAXI_STATE_INIT     : label = "> FIXTURE INIT ERROR"; break;
    case TRIAXI_STATE_TEST     : label = "> TEST FUNCTION ERROR"; break;
    case TRIAXI_STATE_FINI     : label = "> FIXTURE FINI ERROR"; break;
    case TRIAXI_STATE_CLEANUP  : label = "> CLEANUP ERROR"; break;
    case TRIAXI_STATE_IN_ASSERT: label = "> TEST FUNCTION ERROR"; break;
    case TRIAXI_STATE_DONE     : triaxi_unreachable();
    case TRIAXI_STATE_SKIPPED  : triaxi_unreachable();
    }
    switch (r->exit.type) {
    default                   : triaxi_unreachable();
    case TRIAXI_EXIT_TIMEOUT  : triaxi_unreachable();
    case TRIAXI_EXIT_EXIT     : sprintf(detail, ": exited with code %u", r->exit.code); break;
    case TRIAXI_EXIT_FAULT    : sprintf(detail, ": %s", triaxi_fault_tostr(r->exit.reason)); break;
    case TRIAXI_EXIT_EXCEPTION: sprintf(detail, ": unhandled C++ exception"); break;
    case TRIAXI_EXIT_ERROR    : {
      TRIAXI_Error error  = triaxi_error_kind(r->exit.code);
      uint32_t     syserr = triaxi_error_syserr(r->exit.code);
      char         sysbuf[256];
      const char*  sysmsg = triaxi_syserror_tostr(syserr, sysbuf, sizeof(sysbuf));
      if (sysmsg) {
        snprintf(detail, sizeof(detail), ": %s: %s", triaxi_error_tostr(error), sysmsg);
      } else {
        snprintf(detail, sizeof(detail), ": %s", triaxi_error_tostr(error));
      }
      break;
    }
    case TRIAXI_EXIT_NONE: triaxi_unreachable();
    }
    break;
  }
  fprintf(out, "%s%s%s%s", color, label, run->out.ansi.reset, detail);
  if (r->outcome != TRIAXI_OUTCOME_SKIP) { fprintf(out, " (%" PRIu32 "ms)", r->duration_ms); }
  triaxi_fputlit("\n\n", out);
}
static inline void triaxi_print_suite_end_text(const TRIAXI_RunCtx*      run,
                                               const TRIAXI_SuiteResult* sr) {
  FILE* out = run->out.streams.text;
  if (!out) { return; }
  size_t        total = 0;
  const size_t* r     = sr->outcomes;
  const char*   sep   = "";
  for (size_t i = 0; i < triaxi_countof(sr->outcomes); ++i) {
    total += r[i];
    if (r[i]) {
      fprintf(out, " %s %zu %s", sep, r[i], triaxi_print_outcome_tostr((TRIAXI_Outcome)i));
      sep = "|";
    }
  }
  fprintf(out, "\n/ %zu invocations", total);
  triaxi_fputlit("\n--------------------------------------------------\n\n", out);
}
static inline size_t triaxi_outcomes_total(const size_t cnts[TRIAXI_OUTCOME_COUNT]) {
  size_t n = 0;
  for (size_t i = 0; i < TRIAXI_OUTCOME_COUNT; ++i) { n += cnts[i]; }
  return n;
}
static inline void triaxi_print_run_end_text(const TRIAXI_RunCtx* run, const TRIAXI_RunResult* r) {
  FILE* out = run->out.streams.text;
  if (!out) { return; }
  const size_t* cr    = r->invocations.cnts;
  size_t        total = r->invocations.selected;
  bool          fail  = cr[TRIAXI_OUTCOME_PASSED] + cr[TRIAXI_OUTCOME_SKIP] != total;
  fprintf(out, "== %s%s%s ======================================== \n",
          fail ? run->out.ansi.red : run->out.ansi.green, fail ? "FAILED" : "PASSED",
          run->out.ansi.reset);

  fprintf(out, "%zu Passed", cr[TRIAXI_OUTCOME_PASSED]);

  if (cr[TRIAXI_OUTCOME_SKIP]) { fprintf(out, " | %zu skipped", cr[TRIAXI_OUTCOME_SKIP]); }
  if (cr[TRIAXI_OUTCOME_FAIL]) { fprintf(out, " | %zu failed", cr[TRIAXI_OUTCOME_FAIL]); }
  if (cr[TRIAXI_OUTCOME_ERROR]) { fprintf(out, " | %zu errored", cr[TRIAXI_OUTCOME_ERROR]); }
  if (cr[TRIAXI_OUTCOME_UCRASH]) { fprintf(out, " | %zu ucrashed", cr[TRIAXI_OUTCOME_UCRASH]); }
  if (cr[TRIAXI_OUTCOME_UEXIT]) { fprintf(out, " | %zu uexited", cr[TRIAXI_OUTCOME_UEXIT]); }
  if (cr[TRIAXI_OUTCOME_UEXCEPT]) { fprintf(out, " | %zu uexception", cr[TRIAXI_OUTCOME_UEXCEPT]); }
  if (cr[TRIAXI_OUTCOME_TIMEOUT]) { fprintf(out, " | %zu timed out", cr[TRIAXI_OUTCOME_TIMEOUT]); }
  fprintf(out, "\nTests: %zu selected / %zu total\n", r->tests.selected, TRIAXI_global.ntests);
  fprintf(out, "Suites run: %zu\n", r->suites_run);
  if (TRIAXI_global.ntests != r->tests.selected) {
    fprintf(out, "Filtered: %zu tests\n", TRIAXI_global.ntests - r->tests.selected);
  }
  const size_t completed = triaxi_outcomes_total(r->invocations.cnts);

  fprintf(out, "Invocations: %zu run / %zu selected / %zu total\n", completed,
          r->invocations.selected, TRIAXI_global.ninvocations);
  if (r->invocations.selected != TRIAXI_global.ninvocations) {
    fprintf(out, "Filtered: %zu invocations\n",
            TRIAXI_global.ninvocations - r->invocations.selected);
  }
  if (completed != r->invocations.selected) {
    fprintf(out, "Not run: %zu invocations\n", r->invocations.selected - completed);
  }

  fprintf(out, "Total time: %" PRIu32 "ms\n", r->duration_ms);
}

static inline void triaxi_print_run_beg_json(const TRIAXI_RunCtx* run) {
  if (!run->out.streams.json) { return; }
  triaxi_fputlit("{\n  \"suites\": [", run->out.streams.json);
}

static inline void triaxi_print_suite_beg_json(const TRIAXI_RunCtx* run, const TRIAXI_Suite* s,
                                               const TRIAXI_RunResult* r) {
  FILE* out = run->out.streams.json;
  if (!out) { return; }
  if (r->suites_run) { putc(',', out); }
  triaxi_fputlit("\n    {"
                 "\n      \"name\": \"",
                 out);
  triaxi_print_json_escape(out, s->name, strlen(s->name));
  triaxi_fputlit("\",\n      \"tests\": [", out);
}
static inline void triaxi_print_assert_json(const TRIAXI_RunCtx* run, const TRIAXI_TestResult* tr,
                                            const TRIAXI_AssertHdr* hdr,
                                            const TRIAXI_AssertRes* res) {
  FILE* out = run->out.streams.json;
  if (!out) { return; }
  if (!res) { return; }
  TRIAXI_AssertType atype = (TRIAXI_AssertType)hdr->type;
  const char*       pref  = tr->nfails ? "," : ",\n          \"assert_failures\": [";
  fprintf(out,
          "%s\n            {"
          "\n              \"line\": %" PRIu32 ","
          "\n              \"expression\": \"%s_%s(",
          pref, hdr->line, hdr->ae ? "assert" : "expect", TRIAXI_assertnames[hdr->neg][atype]);
  triaxi_print_json_escape(out, hdr->expr_str, hdr->expr_len);
  triaxi_fputlit(")\"", out);
  switch (res->val) {
  case TRIAXI_AR_TIMEOUT: triaxi_fputlit(",\n              \"kind\": \"timeout\"", out); break;
  case TRIAXI_AR_UCRASHED:
    triaxi_fputlit(",\n              \"kind\": \"unexpected_crash\"", out);
    break;
  case TRIAXI_AR_UEXITED:
    triaxi_fputlit(",\n              \"kind\": \"unexpected_exit\"", out);
    break;
  case TRIAXI_AR_UEXCEPT:
    triaxi_fputlit(",\n              \"kind\": \"unhandled_exception\"", out);
    break;
  default:
    if (res->len) {
      TRIAXI_PrintArgs pa = triaxi_print_unpack_args(atype, res);
      if (pa.nargs) {
        char obuf[512];
        triaxi_fputlit(",\n              \"args\": [", out);
        for (unsigned i = 0; i < pa.nargs; ++i) {
          if (i) { triaxi_fputlit(", ", out); }
          Triax_Str arg = pa.args[i];
          if (atype == TRIAXI_AT_memeq || atype == TRIAXI_AT_memzero) {
            arg = triaxi_sprint_hex(obuf + i * sizeof(obuf) / 2, arg);
          }
          if (!arg.str) {
            triaxi_fputlit("null", out);
          } else {
            triaxi_print_json_bytes(out, arg);
          }
        }
        putc(']', out);
      }
      if (pa.meta == TRIAXI_PRINT_META_INDEX) {
        if (atype == TRIAXI_AT_arreq && hdr->neg) {
          fprintf(out, ",\n              \"count\": %.*s", (int)pa.index.len, pa.index.str);
        } else {
          fprintf(out, ",\n              \"index\": %.*s", (int)pa.index.len, pa.index.str);
        }
      } else if (pa.meta == TRIAXI_PRINT_META_OFFSET) {
        fprintf(out, ",\n              \"offset\": %.*s", (int)pa.mem_offset.len,
                pa.mem_offset.str);
      }
    }
    break;
  }
  triaxi_fputlit("\n            }", out);
}

static inline void triaxi_print_test_beg_json(const TRIAXI_RunCtx*         run,
                                              const TRIAXI_TestInvocation* inv, bool first) {
  FILE* out = run->out.streams.json;
  if (!out) { return; }
  if (!first) { putc(',', out); }
  const TRIAXI_Test* const t = inv->test;
  fprintf(out,
          "\n        {"
          "\n          \"name\": \"%s\","
          "\n          \"file\": \"",
          t->name);
  triaxi_print_json_escape(out, t->file, strlen(t->file)), putc('\"', out);
  if (t->params.elcount) { fprintf(out, ",\n          \"invocation\": %zu", inv->idx); }
  if (t->attrs.tags) {
    triaxi_fputlit(",\n          \"tags\": \"", out);
    triaxi_print_json_escape(out, t->attrs.tags, strlen(t->attrs.tags));
    putc('\"', out);
  }
}
static inline void triaxi_print_test_end_json(const TRIAXI_RunCtx*     run,
                                              const TRIAXI_TestResult* r) {
  FILE* out = run->out.streams.json;
  if (!out) { return; }
  TRIAXI_Outcome outcome = r->outcome;
  if (r->nfails > 0) { triaxi_fputlit("\n          ]", out); }
  if (outcome != TRIAXI_OUTCOME_SKIP) {
    fprintf(out, ",\n          \"assert_count\": %u", r->nasserts);
  }
  fprintf(out, ",\n          \"outcome\": \"%s\"", triaxi_print_outcome_tostr(outcome));
  if (outcome == TRIAXI_OUTCOME_ERROR || outcome == TRIAXI_OUTCOME_TIMEOUT) {
    const char* phase = triaxi_state_phase_name(r->state);
    if (phase) { fprintf(out, ",\n          \"phase\": \"%s\"", phase); }
  }

  switch (outcome) {
  default                   : triaxi_unreachable();
  case TRIAXI_OUTCOME_SKIP  :
  case TRIAXI_OUTCOME_PASSED:
  case TRIAXI_OUTCOME_FAIL  : break;
  case TRIAXI_OUTCOME_UCRASH:
  case TRIAXI_OUTCOME_UEXIT:
  case TRIAXI_OUTCOME_UEXCEPT:
  case TRIAXI_OUTCOME_TIMEOUT:
  case TRIAXI_OUTCOME_ERROR:
    triaxi_fputlit(",\n          \"termination\": { \"type\": ", out);
    switch (r->exit.type) {
    default                 : triaxi_unreachable();
    case TRIAXI_EXIT_TIMEOUT: triaxi_fputlit("\"timeout\"}", out); break;
    case TRIAXI_EXIT_EXIT   : fprintf(out, "\"exit\", \"code\": %u}", r->exit.code); break;
    case TRIAXI_EXIT_FAULT  : {
      const char* reason = triaxi_fault_tostr(r->exit.reason);
      triaxi_fputlit("\"fault\", \"reason\": \"", out);
      triaxi_print_json_escape(out, reason, strlen(reason));
      triaxi_fputlit("\"}", out);
      break;
    }
    case TRIAXI_EXIT_EXCEPTION: triaxi_fputlit("\"exception\"}", out); break;
    case TRIAXI_EXIT_ERROR    : {
      TRIAXI_Error error  = triaxi_error_kind(r->exit.code);
      uint32_t     syserr = triaxi_error_syserr(r->exit.code);
      fprintf(out, "\"user_error\", \"reason\": \"%s\"", triaxi_error_name(error));
      if (syserr) {
        char        sysbuf[256];
        const char* sysmsg = triaxi_syserror_tostr(syserr, sysbuf, sizeof(sysbuf));
        fprintf(out, ", \"system_error\": %" PRIu32, syserr);
        if (sysmsg) {
          triaxi_fputlit(", \"system_message\": \"", out);
          triaxi_print_json_escape(out, sysmsg, strlen(sysmsg));
          triaxi_fputlit("\"", out);
        }
      }
      triaxi_fputlit("}", out);
      break;
    }
    case TRIAXI_EXIT_NONE: triaxi_unreachable();
    }
    break;
  }
  if (outcome != TRIAXI_OUTCOME_SKIP) {
    fprintf(out, ",\n          \"duration_ms\": %" PRIu32, r->duration_ms);
    if (r->capt.out.len) {
      triaxi_fputlit(",\n          \"stdout\": ", out);
      triaxi_print_json_bytes(out, r->capt.out);
    }
    if (r->capt.err.len) {
      triaxi_fputlit(",\n          \"stderr\": ", out);
      triaxi_print_json_bytes(out, r->capt.err);
    }
  }

  triaxi_fputlit("\n        }", out);
}
static inline void triaxi_print_suite_end_json(const TRIAXI_RunCtx* run) {
  if (!run->out.streams.json) { return; }
  triaxi_fputlit("\n      ]"
                 "\n    }",
                 run->out.streams.json);
}

static inline void triaxi_print_run_end_json(const TRIAXI_RunCtx* run) {
  if (!run->out.streams.json) { return; }
  triaxi_fputlit("\n  ]"
                 "\n}"
                 "\n",
                 run->out.streams.json);
}
static inline void triaxi_print_test_end_tap(const TRIAXI_RunCtx*         run,
                                             const TRIAXI_TestInvocation* inv,
                                             const TRIAXI_RunResult* r, const TRIAXI_TestResult* tr,
                                             size_t report_index) {
  if (!run->out.streams.tap) { return; }
  const TRIAXI_Test* t = inv->test;
  char               buf[128];
  buf[0] = '\0';
  if (t->params.elcount) { sprintf(buf, "[%zu]", inv->idx); }
  const size_t n = triaxi_outcomes_total(r->invocations.cnts) + report_index;
  fprintf(run->out.streams.tap, "%sok %zu - %s > %s%s%s\n",
          (tr->outcome == TRIAXI_OUTCOME_PASSED || tr->outcome == TRIAXI_OUTCOME_SKIP) ? ""
                                                                                       : "not ",
          n, t->suitename, t->name, buf, tr->outcome == TRIAXI_OUTCOME_SKIP ? " # SKIP" : "");
}

static inline void triaxi_print_run_end_tap(const TRIAXI_RunCtx* run, const TRIAXI_RunResult* r) {
  if (!run->out.streams.tap) { return; }
  fprintf(run->out.streams.tap, "1..%zu\n", triaxi_outcomes_total(r->invocations.cnts));
}

static inline void triaxi_print_run_beg_junit(const TRIAXI_RunCtx* run) {
  if (!run->out.streams.junit) { return; }
  triaxi_fputlit("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuites>\n",
                 run->out.streams.junit);
}

static inline void triaxi_print_suite_beg_junit(const TRIAXI_RunCtx* run) {
  if (!run->out.streams.junit) { return; }
  triaxi_file_clear_stream(TRIAXI_global.junit.suite_buf);
}

static inline void triaxi_print_test_beg_junit(const TRIAXI_RunCtx* run) {
  if (!run->out.streams.junit) { return; }
  triaxi_file_clear_stream(TRIAXI_global.junit.test_buf);
}

static inline void triaxi_print_assert_junit(const TRIAXI_RunCtx* run, const TRIAXI_AssertHdr* hdr,
                                             const TRIAXI_AssertRes* res) {
  (void)run;
  FILE* out = TRIAXI_global.junit.test_buf;
  if (!out) { return; }
  if (!res) { return; }
  fprintf(out, "        %s_%s(", hdr->ae ? "assert" : "expect",
          TRIAXI_assertnames[hdr->neg][(TRIAXI_AssertType)hdr->type]);
  triaxi_print_xml_escape(out, hdr->expr_str, hdr->expr_len);
  triaxi_fputlit(")\n", out);
}
static inline void triaxi_print_copy_file_range(FILE* dst, FILE* src) {
  fflush(src), fflush(dst);
  if (fseek(src, 0, SEEK_SET)) { triaxi_fatal(); }
  char* chunk = TRIAXI_exec.storage;
  for (size_t n; (n = fread(chunk, 1, sizeof(TRIAXI_exec.storage), src)) > 0;) {
    fwrite(chunk, 1, n, dst);
  }
}

static inline void triaxi_print_test_end_junit(const TRIAXI_RunCtx*         run,
                                               const TRIAXI_TestInvocation* inv,
                                               const TRIAXI_TestResult*     r) {
  if (!run->out.streams.junit) { return; }
  const TRIAXI_Test* t   = inv->test;
  FILE*              out = TRIAXI_global.junit.suite_buf;
  triaxi_fputlit("    <testcase classname=\"", out);
  triaxi_print_xml_escape(out, t->suitename, strlen(t->suitename));
  triaxi_fputlit("\" name=\"", out);
  triaxi_print_xml_escape(out, t->name, strlen(t->name));
  if (t->params.elcount) { fprintf(out, "[%zu]", inv->idx); }

  fprintf(out, "\" assertions=\"%u\"  time=\"%" PRIu32 ".%03" PRIu32 "\"", r->nasserts,
          r->duration_ms / 1000,
          r->duration_ms % 1000); // never skip time
  const char* fltstr;
  switch (r->outcome) {
  default                 : triaxi_unreachable();
  case TRIAXI_OUTCOME_SKIP: triaxi_fputlit(">\n      <skipped/>\n    </testcase>\n", out); return;
  case TRIAXI_OUTCOME_PASSED:
    if (!r->capt.out.len && !r->capt.err.len) {
      triaxi_fputlit("/>\n", out);
      return;
    }
    triaxi_fputlit(">\n", out);
    break;
  case TRIAXI_OUTCOME_FAIL:
    fprintf(out, ">\n      <failure type=\"failed\" message=\"assertion failure\">\n");
    triaxi_print_copy_file_range(out, TRIAXI_global.junit.test_buf);
    triaxi_fputlit("      </failure>\n", out);
    break;
  case TRIAXI_OUTCOME_UCRASH:
    fprintf(out, ">\n      <error type=\"ucrashed\" message=\"crashed "
                 "unexpectedly with: ");
    fltstr = triaxi_fault_tostr(r->exit.reason);
    triaxi_print_xml_escape(out, fltstr, strlen(fltstr)), triaxi_fputlit("\"/>\n", out);
    break;
  case TRIAXI_OUTCOME_UEXIT:
    fprintf(out,
            ">\n      <error type=\"uexited\" message=\"exited unexpectedly "
            "with code %u\"/>\n",
            r->exit.code);
    break;
  case TRIAXI_OUTCOME_UEXCEPT:
    fprintf(out, ">\n      <error type=\"uexception\" message=\"unhandled C++ "
                 "exception\"/>\n");
    break;
  case TRIAXI_OUTCOME_TIMEOUT: {
    const char* phase = triaxi_state_phase_phrase(r->state);
    if (phase) {
      fprintf(out, ">\n      <error type=\"timeout\" message=\"%s: exceeded %" PRIu32 "ms\"/>\n",
              phase, inv->settings.timeout_ms);
    } else {
      fprintf(out, ">\n      <error type=\"timeout\" message=\"exceeded %" PRIu32 "ms\"/>\n",
              inv->settings.timeout_ms);
    }
    break;
  }
  case TRIAXI_OUTCOME_ERROR: {
    const char *type, *phase;
    switch (r->state) {
    default                    : triaxi_unreachable();
    case TRIAXI_STATE_SETUP    : type = "test_setup_error", phase = "setup: "; break;
    case TRIAXI_STATE_INIT     : type = "fixture_init_error", phase = "fixture init: "; break;
    case TRIAXI_STATE_TEST     : type = "test_error", phase = "test function: "; break;
    case TRIAXI_STATE_FINI     : type = "fixture_fini_error", phase = "fixture fini: "; break;
    case TRIAXI_STATE_CLEANUP  : type = "cleanup_error", phase = "cleanup: "; break;
    case TRIAXI_STATE_IN_ASSERT: type = "test_error", phase = "test function: "; break;
    case TRIAXI_STATE_DONE     : triaxi_unreachable();
    case TRIAXI_STATE_SKIPPED  : triaxi_unreachable();
    }
    fprintf(out, ">\n      <error type=\"%s\" message=\"%s", type, phase);
    switch (r->exit.type) {
    default                 : triaxi_unreachable();
    case TRIAXI_EXIT_TIMEOUT: triaxi_unreachable();
    case TRIAXI_EXIT_EXIT   : fprintf(out, "exited with code %u", r->exit.code); break;
    case TRIAXI_EXIT_FAULT:
      fltstr = triaxi_fault_tostr(r->exit.reason);
      triaxi_print_xml_escape(out, fltstr, strlen(fltstr));
      break;
    case TRIAXI_EXIT_EXCEPTION: triaxi_fputlit("unhandled C++ exception", out); break;
    case TRIAXI_EXIT_ERROR    : {
      TRIAXI_Error error  = triaxi_error_kind(r->exit.code);
      uint32_t     syserr = triaxi_error_syserr(r->exit.code);
      const char*  msg    = triaxi_error_tostr(error);
      triaxi_print_xml_escape(out, msg, strlen(msg));
      if (syserr) {
        char        sysbuf[256];
        const char* sysmsg = triaxi_syserror_tostr(syserr, sysbuf, sizeof(sysbuf));
        if (sysmsg) {
          triaxi_fputlit(": ", out);
          triaxi_print_xml_escape(out, sysmsg, strlen(sysmsg));
        }
      }
      break;
    }
    case TRIAXI_EXIT_NONE: triaxi_unreachable(); break;
    }
    triaxi_fputlit("\"/>\n", out);
    break;
  }
  }

  if (r->capt.out.len) {
    triaxi_fputlit("      <system-out>", out);
    triaxi_print_xml_bytes(out, r->capt.out);
    triaxi_fputlit("</system-out>\n", out);
  }
  if (r->capt.err.len) {
    triaxi_fputlit("      <system-err>", out);
    triaxi_print_xml_bytes(out, r->capt.err);
    triaxi_fputlit("</system-err>\n", out);
  }
  triaxi_fputlit("    </testcase>\n", out);
}
static inline void triaxi_print_suite_end_junit(const TRIAXI_RunCtx* run, const TRIAXI_Suite* s,
                                                const TRIAXI_SuiteResult* sr) {
  FILE* out = run->out.streams.junit;
  if (!out) { return; }
  size_t tests = 0;
  for (size_t i = 0; i < triaxi_countof(sr->outcomes); ++i) { tests += sr->outcomes[i]; }
  triaxi_fputlit("  <testsuite name=\"", out);
  triaxi_print_xml_escape(out, s->name, strlen(s->name));
  fprintf(out,
          "\" tests=\"%zu\" failures=\"%zu\" errors=\"%zu\" skipped=\"%zu\" "
          "time=\"%" PRIu32 ".%03" PRIu32 "\">\n",
          tests, sr->outcomes[TRIAXI_OUTCOME_FAIL],
          sr->outcomes[TRIAXI_OUTCOME_UCRASH] + sr->outcomes[TRIAXI_OUTCOME_UEXIT]
              + sr->outcomes[TRIAXI_OUTCOME_UEXCEPT] + sr->outcomes[TRIAXI_OUTCOME_ERROR]
              + sr->outcomes[TRIAXI_OUTCOME_TIMEOUT],
          sr->outcomes[TRIAXI_OUTCOME_SKIP], sr->duration_ms / 1000, sr->duration_ms % 1000);
  triaxi_print_copy_file_range(out, TRIAXI_global.junit.suite_buf);
  triaxi_fputlit("  </testsuite>\n", out);
}

static inline void triaxi_print_run_end_junit(const TRIAXI_RunCtx* run) {
  FILE* out = run->out.streams.junit;
  if (!out) { return; }
  triaxi_fputlit("</testsuites>\n", out);
}

static inline void triaxi_print_run_beg(const TRIAXI_RunCtx* run) {
  triaxi_print_run_beg_text(run);
  triaxi_print_run_beg_json(run);
  triaxi_print_run_beg_junit(run);
}
static inline void triaxi_print_suite_beg(const TRIAXI_RunCtx* run, const TRIAXI_Suite* s,
                                          const TRIAXI_RunResult* r) {
  triaxi_print_suite_beg_text(run, s);
  triaxi_print_suite_beg_json(run, s, r);
  triaxi_print_suite_beg_junit(run);
}

static inline void triaxi_print_test_beg(const TRIAXI_RunCtx* run, const TRIAXI_TestInvocation* inv,
                                         bool first) {
  triaxi_print_test_beg_text(run, inv);
  triaxi_print_test_beg_json(run, inv, first);
  triaxi_print_test_beg_junit(run);
}

static inline void triaxi_print_assert(const TRIAXI_RunCtx* run, const TRIAXI_TestInvocation* inv,
                                       const TRIAXI_TestResult* tr, const TRIAXI_AssertHdr* hdr,
                                       const TRIAXI_AssertRes* res) {
  Triax_Verbosity v = inv->settings.verbosity;
  if (v != TRIAX_VERBOSITY_NEVER && !(v != TRIAX_VERBOSITY_ALWAYS && !res)) {
    triaxi_print_assert_text(run, inv->test, hdr, res);
  }
  triaxi_print_assert_json(run, tr, hdr, res);
  triaxi_print_assert_junit(run, hdr, res);
}

static inline void triaxi_print_test_end(const TRIAXI_RunCtx* run, const TRIAXI_TestInvocation* inv,
                                         const TRIAXI_RunResult* r, const TRIAXI_TestResult* tr,
                                         size_t report_index) {
  triaxi_print_test_end_text(run, inv, tr);
  triaxi_print_test_end_json(run, tr);
  triaxi_print_test_end_tap(run, inv, r, tr, report_index);
  triaxi_print_test_end_junit(run, inv, tr);
}
static inline void triaxi_print_suite_end(const TRIAXI_RunCtx* run, const TRIAXI_Suite* s,
                                          const TRIAXI_SuiteResult* sr) {
  triaxi_print_suite_end_text(run, sr);
  triaxi_print_suite_end_json(run);
  triaxi_print_suite_end_junit(run, s, sr);
}
static inline void triaxi_print_run_end(const TRIAXI_RunCtx* run, const TRIAXI_RunResult* r) {
  triaxi_print_run_end_text(run, r);
  triaxi_print_run_end_json(run);
  triaxi_print_run_end_tap(run, r);
  triaxi_print_run_end_junit(run);
}
#  pragma endregion runner_printing

#  pragma region runner_initialisation
#  define triaxi_config_error_f(fmt, ...)                                                          \
     (fprintf((TRIAXI_true_stderr ? TRIAXI_true_stderr : stderr), "Error: " fmt "\n",              \
              __VA_ARGS__),                                                                        \
      exit(2))
#  define triaxi_config_error(lit) triaxi_config_error_f("%s", (lit))

static inline TRIAXI_OutputCtx triaxi_output_create(const Triax_RunConfig*  config,
                                                    const TRIAXI_StdBackup* saved) {
  static const char* const defaults[]
      = {TRIAX_OUTPATH_STDOUT, TRIAX_OUTPATH_NONE, TRIAX_OUTPATH_NONE, TRIAX_OUTPATH_NONE};
  TRIAXI_OutputCtx out = {TRIAXI_ZINIT};
  FILE** const     outfiles[]
      = {&out.streams.text, &out.streams.json, &out.streams.tap, &out.streams.junit};
  static const char names[][6] = {"text", "json", "tap", "junit"};
  const char* const outpaths[] = {config->outpaths.text, config->outpaths.json,
                                  config->outpaths.tap, config->outpaths.junit};
  for (size_t nstdout = 0, i = 0; i < triaxi_countof(outfiles); ++i) {
    const char* path = outpaths[i];
    if (path == TRIAX_OUTPATH_DEFAULT) { path = defaults[i]; }
    if (path == TRIAX_OUTPATH_NONE) {
      *outfiles[i] = NULL;
    } else if (path == TRIAX_OUTPATH_STDOUT) {
      if (++nstdout > 1) {
        triaxi_config_error_f("More than one output stream set to stdout. %s", TRIAX_HELP);
      };
      *outfiles[i] = saved->out.file;
    } else {
      if (!(triaxi_mkdirp(path), *outfiles[i] = fopen(path, "w"))) {
        triaxi_config_error_f("Cannot open %s output '%s': %s", names[i], path, strerror(errno));
      }
    }
  }
  static const TRIAXI_AnsiColours color
      = {"\x1b[0m",  "\x1b[1m",  "\x1b[2m",  "\x1b[31m", "\x1b[32m",
         "\x1b[33m", "\x1b[34m", "\x1b[35m", "\x1b[36m"},
      no_color   = {TRIAXI_ZINIT};
  bool use_color = !config->no_color && !getenv("NO_COLOR") && out.streams.text
                && triaxi_isatty(triaxi_fileno(out.streams.text));
  out.ansi = use_color ? color : no_color;
  return out;
}
static inline void triaxi_output_close(const TRIAXI_RunCtx* run) {
  FILE* const outfiles[] = {run->out.streams.text, run->out.streams.json, run->out.streams.tap,
                            run->out.streams.junit};
  for (size_t i = 0; i < triaxi_countof(outfiles); ++i) {
    if (outfiles[i] && outfiles[i] != run->saved.out.file) {
      if (fclose(outfiles[i])) { triaxi_fatal(); }
    }
  }
}

static inline void triaxi_junit_buffers_init(void) {
  TRIAXI_File su = triaxi_tmpfile_create("triax_ju_su");
  TRIAXI_File te = triaxi_tmpfile_create("triax_ju_te");
  int         su_fd, te_fd;
#  ifndef _WIN32
  su_fd = su, te_fd = te;
#  else
  if (!SetHandleInformation(su, HANDLE_FLAG_INHERIT, 0)) { triaxi_fatal(); }
  if (!SetHandleInformation(te, HANDLE_FLAG_INHERIT, 0)) { triaxi_fatal(); }
  if ((su_fd = _open_osfhandle((intptr_t)su, _O_RDWR | _O_BINARY)) < 0) { triaxi_fatal(); }
  if ((te_fd = _open_osfhandle((intptr_t)te, _O_RDWR | _O_BINARY)) < 0) { triaxi_fatal(); }
#  endif
  if (!(TRIAXI_global.junit.suite_buf = triaxi_fdopen(su_fd, "w+b"))) { triaxi_fatal(); }
  if (!(TRIAXI_global.junit.test_buf = triaxi_fdopen(te_fd, "w+b"))) { triaxi_fatal(); }
}

static inline void triaxi_shared_init(void) {
  enum { size = sizeof(TRIAXI_Shared) * TRIAXI_JOBS_MAX };
#  ifndef _WIN32
  if ((TRIAXI_global.shared = (TRIAXI_Shared*)mmap(NULL, size, PROT_READ | PROT_WRITE,
                                                   MAP_SHARED | TRIAXI_MAP_ANON, -1, 0))
      == MAP_FAILED) {
    triaxi_fatal();
  }
#  else
  SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
  if (!(TRIAXI_global.shared_mapping
        = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, (DWORD)size, NULL))) {
    triaxi_fatal();
  }
  if (!(TRIAXI_global.shared = (TRIAXI_Shared*)MapViewOfFile(TRIAXI_global.shared_mapping,
                                                             FILE_MAP_ALL_ACCESS, 0, 0, size))) {
    triaxi_fatal();
  }
  DWORD n = GetModuleFileNameW(NULL, TRIAXI_global.executable_path,
                               (DWORD)triaxi_countof(TRIAXI_global.executable_path));

  if (!n || n >= (DWORD)triaxi_countof(TRIAXI_global.executable_path)) { triaxi_fatal(); }
#  endif
  for (size_t i = 0; i < TRIAXI_JOBS_MAX; ++i) {
    TRIAXI_global.slots[i].shared = &TRIAXI_global.shared[i];
  }
}

static inline TRIAXI_Settings triaxi_settings_resolve(const Triax_Attributes* attrs,
                                                      TRIAXI_Settings         parent) {
  if (attrs->timeout_ms != TRIAX_TIMEOUT_INHERIT) { parent.timeout_ms = attrs->timeout_ms; }
  if (attrs->verbosity != TRIAX_VERBOSITY_INHERIT) { parent.verbosity = attrs->verbosity; }
  if (attrs->isolation != TRIAX_ISOLATION_INHERIT) { parent.isolation = attrs->isolation; }
  return parent;
}
static inline TRIAXI_RunCtx triaxi_run_ctx_make(const Triax_RunConfig* config) {
  const TRIAXI_StdBackup saved = triaxi_stdfds_backup_create(); // needs to be here
  TRIAXI_Settings settings     = triaxi_settings_resolve(&config->attrs, TRIAXI_default_settings);
  size_t          njobs        = (size_t)TRIAXI_MAX(config->njobs, 1);
  if (config->debug) {
    settings.isolation  = TRIAX_ISOLATION_OFF;
    settings.timeout_ms = TRIAX_TIMEOUT_NONE;
    njobs               = 1;
  }
  return TRIAXI_T(TRIAXI_RunCtx){
      settings,
      {config->attrs.init, config->attrs.fini},
      config->attrs.tags,
      config->nfilters,
      config->filters,
      njobs,
      config->fail_fast,
      config->debug,
      triaxi_output_create(config, &saved),
      saved,
  };
}

enum {
  TRIAXI_PARSE_RUN,
  TRIAXI_PARSE_HELP,
  TRIAXI_PARSE_LIST,
  TRIAXI_PARSE_ERR_INVALID_FLAG,
  TRIAXI_PARSE_ERR_INVALID_OPTION,
  TRIAXI_PARSE_ERR_TOO_MANY_FILTERS,
  TRIAXI_PARSE_ERR_DUP_OUTPUT,
};
#  define TRIAXI_ERRORDATA_FLAGTYPE      0
#  define TRIAXI_ERRORDATA_SEL_OPTION    1
#  define TRIAXI_ERRORDATA_AVAIL_OPTIONS 2
static inline void triaxi_config_validate(const Triax_RunConfig* config) {
  switch (config->kind) {
  default               : triaxi_unreachable();
  case TRIAXI_PARSE_RUN : break;
  case TRIAXI_PARSE_HELP: triaxi_fputlit(TRIAX_HELP, stdout), exit(0);
  case TRIAXI_PARSE_LIST:
    for (const TRIAXI_Test* t = TRIAXI_global.tests.beg; t != TRIAXI_global.tests.end; ++t) {
      if (!t->name) { continue; }
      printf("%s::%s\n", t->suitename, t->name);
    }
    exit(0);
  case TRIAXI_PARSE_ERR_INVALID_FLAG:
    triaxi_config_error_f("unrecognized flag: '%s'\n%s", config->filters[TRIAXI_ERRORDATA_FLAGTYPE],
                          TRIAX_HELP);
  case TRIAXI_PARSE_ERR_INVALID_OPTION:
    triaxi_config_error_f("%s: invalid value '%s', expected %s\n%s",
                          config->filters[TRIAXI_ERRORDATA_FLAGTYPE],
                          config->filters[TRIAXI_ERRORDATA_SEL_OPTION],
                          config->filters[TRIAXI_ERRORDATA_AVAIL_OPTIONS], TRIAX_HELP);
  case TRIAXI_PARSE_ERR_TOO_MANY_FILTERS:
    triaxi_config_error_f("Too many filter arguments (max 255)\n%s", TRIAX_HELP);
  case TRIAXI_PARSE_ERR_DUP_OUTPUT:
    triaxi_config_error_f("Output stream '%s' set more than once.\n%s",
                          config->filters[TRIAXI_ERRORDATA_FLAGTYPE], TRIAX_HELP);
  }
  for (size_t i = 0; i < config->nfilters; ++i) {
    if (!config->filters[i]) { triaxi_config_error("filter strings must not be NULL"); }
  }
  switch (config->attrs.isolation) {
  case TRIAX_ISOLATION_INHERIT:
  case TRIAX_ISOLATION_ON:
  case TRIAX_ISOLATION_OFF    : break;
  default:
    triaxi_config_error("Invalid Isolation Value Selected: must be TRIAX_ISOLATION_INHERIT, "
                        "TRIAX_ISOLATION_ON or TRIAX_ISOLATION_OFF.");
  }
  switch (config->attrs.verbosity) {
  case TRIAX_VERBOSITY_INHERIT:
  case TRIAX_VERBOSITY_ON_FAIL:
  case TRIAX_VERBOSITY_ALWAYS:
  case TRIAX_VERBOSITY_NEVER  : break;
  default:
    triaxi_config_error(
        "Invalid Verbosity Value Selected: must be TRIAX_VERBOSITY_INHERIT, "
        "TRIAX_VERBOSITY_ON_FAIL, TRIAX_VERBOSITY_ALWAYS or TRIAX_VERBOSITY_NEVER.");
  }

  if (config->njobs > TRIAXI_JOBS_MAX) {
    triaxi_config_error_f("--jobs: value %u exceeds maximum of %u.", (unsigned)config->njobs,
                          (unsigned)TRIAXI_JOBS_MAX);
  }
}

static inline void          triaxi_windows_childentry(const Triax_RunConfig* config);
static inline TRIAXI_RunCtx triaxi_runner_init(const Triax_RunConfig* config) {
  TRIAXI_exec.debug_break = config->debug_break || config->debug;
  if (!TRIAXI_global.tests.beg) { triaxi_sections_init(); };
  triaxi_windows_childentry(config);
  triaxi_config_validate(config);
  if (!triaxi_global_initialised()) {
    triaxi_shared_init();
    triaxi_register_tests();
    triaxi_register_suites();
    triaxi_signals_init();
    //  setvbuf(stdout, NULL, _IONBF, 0), setvbuf(stderr, NULL, _IONBF, 0);
    triaxi_junit_buffers_init();
  }
  size_t njobs = config->debug ? 1 : TRIAXI_MAX(1, config->njobs);
  if (njobs > TRIAXI_global.max_prev_slots) {
    triaxi_tmpfiles_create(njobs);
    TRIAXI_global.max_prev_slots = njobs;
  }
  triaxi_process_control_init(njobs);
  return triaxi_run_ctx_make(config);
}

static inline void triaxi_runner_cleanup(const TRIAXI_RunCtx* run) {
  triaxi_flush_all();
  triaxi_output_close(run);
  triaxi_stdfds_restore(&run->saved);
  triaxi_stdfds_backup_close(&run->saved);
  triaxi_process_control_reset();
}

#  pragma endregion runner_initialisation

#  pragma region runner_execution
#  ifndef _WIN32
#   define triaxi_crash_handlers_impl(_h)                                                          \
      do {                                                                                         \
        for (size_t i = 0; i < triaxi_countof(TRIAXI_signals.crash); ++i) {                        \
          triaxi_sigaction(TRIAXI_signals.crash[i], &_h, NULL);                                    \
        }                                                                                          \
      } while (0)
#   define triaxi_win_try {
#   define triaxi_win_except(...)                                                                  \
      }                                                                                            \
      if (0)
#  elif TRIAXI_MSVC_COMPAT
#   define triaxi_crash_handlers_impl(_h)
#   define triaxi_win_try __try {
#   define triaxi_win_except(...)                                                                  \
      }                                                                                            \
      __except (__VA_ARGS__)

static inline int triaxi_windows_exception_filter(DWORD fault) {
  if (!TRIAXI_exec.isolated) { return EXCEPTION_EXECUTE_HANDLER; }
  TRIAXI_exec.shared->exit.reason = (Triax_Fault)fault;
  TRIAXI_exec.shared->exit.type   = TRIAXI_EXIT_FAULT;
  return EXCEPTION_CONTINUE_SEARCH;
}
#  else
/* MinGW does not implement MSVC's __try/__except syntax. Isolated tests still
 * get full crash detection from the child-process exit status; a hardware fault
 * in a non-isolated test may terminate the runner, which is already documented. */
#   define triaxi_crash_handlers_impl(_h)
#   define triaxi_win_try {
#   define triaxi_win_except(...)                                                                  \
      }                                                                                            \
      if (0)
#  endif
#  define triaxi_crash_handlers_setup()    triaxi_crash_handlers_impl(TRIAXI_signals.sas.crash)
#  define triaxi_crash_handlers_teardown() triaxi_crash_handlers_impl(TRIAXI_signals.old.crash[i])

static inline void triaxi_run_func(const TRIAXI_TestInvocation* inv, TRIAXI_Shared* shared) {
  const TRIAXI_Test* const t = inv->test;
  const TRIAXI_Fixtures*   fixtures[]
      = {&inv->fixtures.run, &inv->fixtures.suite, &inv->fixtures.test};
  TRIAXI_exec.in_test = true;
  TRIAXI_exec.param
      = t->params.ptr ? (const char*)t->params.ptr + t->params.elsize * inv->idx : NULL;
  TRIAXI_exec.shared            = shared;
  TRIAXI_exec.shared->exit.type = TRIAXI_EXIT_NONE;
  TRIAXI_exec.shared->exit.code = 0;
  int64_t            t0         = triaxi_now_ms();
  // Hardware crash -> NEVER FINI
  // Crashes only caught in non-isolation
  TRIAXI_ExecOutcome res = TRIAXI_EXEC_RETURNED;
  triaxi_win_try switch (setjmp(TRIAXI_exec.jmp)) {
  default: triaxi_unreachable();
  case TRIAXI_EXEC_RETURNED:
    if (!TRIAXI_exec.isolated) { triaxi_crash_handlers_setup(); }
    for (size_t fi = 0; fi < triaxi_countof(fixtures); ++fi) {
      if (!fixtures[fi]->init) { continue; }
      TRIAXI_exec.shared->state = TRIAXI_STATE_INIT;
      fixtures[fi]->init();
    }
    TRIAXI_exec.shared->state = TRIAXI_STATE_TEST;
    t->func();
do_fini:
    for (size_t fi = triaxi_countof(fixtures); fi-- > 0;) {
      if (!fixtures[fi]->fini) { continue; }
      TRIAXI_exec.shared->state = TRIAXI_STATE_FINI;
      fixtures[fi]->fini();
    }
    TRIAXI_exec.shared->state = TRIAXI_STATE_CLEANUP;
    triaxi_flush_all();
    TRIAXI_exec.shared->duration_ms = triaxi_elapsed_ms(t0);
    switch (res) {
    case TRIAXI_EXEC_ERROR: triaxi_unreachable();
    default               : triaxi_unreachable();
    case TRIAXI_EXEC_RETURNED:
      TRIAXI_exec.shared->state     = TRIAXI_STATE_DONE;
      TRIAXI_exec.shared->exit.type = TRIAXI_EXIT_EXIT;
      break;

    case TRIAXI_EXEC_ASSERTED:
      TRIAXI_exec.shared->state     = TRIAXI_STATE_DONE;
      TRIAXI_exec.shared->exit.type = TRIAXI_EXIT_EXIT;
      break;

    case TRIAXI_EXEC_SKIPPED:
      TRIAXI_exec.shared->state     = TRIAXI_STATE_SKIPPED;
      TRIAXI_exec.shared->exit.type = TRIAXI_EXIT_EXIT;
      break;

    case TRIAXI_EXEC_EXCEPTION:
      TRIAXI_exec.shared->state     = TRIAXI_STATE_TEST; // correct until c++ init/fini
      TRIAXI_exec.shared->exit.type = TRIAXI_EXIT_EXCEPTION;
      break;

    case TRIAXI_EXEC_CRASHED: triaxi_unreachable();
    }
    break;
  case TRIAXI_EXEC_ASSERTED : res = TRIAXI_EXEC_ASSERTED; goto do_fini;
  case TRIAXI_EXEC_SKIPPED  : res = TRIAXI_EXEC_SKIPPED; goto do_fini;
  case TRIAXI_EXEC_EXCEPTION: res = TRIAXI_EXEC_EXCEPTION; goto do_fini;
  case TRIAXI_EXEC_ERROR:
    TRIAXI_exec.shared->duration_ms = triaxi_elapsed_ms(t0);
    triaxi_flush_all();
    break;
  case TRIAXI_EXEC_CRASHED:
#  ifdef _WIN32
    triaxi_unreachable();
#  else
    goto crashed;
#  endif
  }
  // Windows SEH handling is kept local to the execution wrapper.
  triaxi_win_except(TRIAXI_exec.isolated ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER) {
#  if defined(_WIN32) && TRIAXI_MSVC_COMPAT
    const DWORD fault = GetExceptionCode();
    if (fault == EXCEPTION_STACK_OVERFLOW) { _resetstkoflw(); }
    TRIAXI_exec.shared->duration_ms = triaxi_elapsed_ms(t0);
    TRIAXI_exec.shared->exit.reason = (Triax_Fault)fault;
    TRIAXI_exec.shared->exit.type   = TRIAXI_EXIT_FAULT;
#  elif !defined(_WIN32)
crashed:
    if (TRIAXI_exec.isolated) { triaxi_unreachable(); }
    TRIAXI_exec.shared->duration_ms = triaxi_elapsed_ms(t0);
    if (TRIAXI_exec.shared->exit.type != TRIAXI_EXIT_ERROR) {
      const int fault                 = TRIAXI_crash_signal;
      TRIAXI_exec.shared->exit.reason = (Triax_Fault)fault;
      TRIAXI_exec.shared->exit.type   = TRIAXI_EXIT_FAULT;
    }
#  else
    triaxi_unreachable();
#  endif
  }
  if (TRIAXI_exec.isolated) { return; }
  triaxi_crash_handlers_teardown();
  TRIAXI_exec.in_test = false;
  triaxi_allocations_free_all();
}
/*
 * Classifies a finished slot from its final (state, exit.type) pair, plus
 * whatever assertions its log recorded. Every triaxi_fatal()/
 * triaxi_unreachable() below encodes a claim that some (state, exit.type)
 * cell in that matrix cannot occur; two of those claims were wrong (see the
 * state_hdr TEST-state fix and the state_res SETUP/INIT/FINI/CLEANUP fix
 * below) and only surfaced once --break/--debug and fixture-crash
 * combinations were actually tested, not from reading the code. Whenever a
 * new cell (a new way to reach some state, or a new TRIAXI_ExitType/
 * TRIAXI_State value) is added here, re-examine every remaining unreachable/
 * fatal call, since each one is a claim, not a proof.
 *
 * One claim shared by several of them: r.exit.type is never
 * TRIAXI_EXIT_NONE by the time this runs. That's not a property of this
 * function — TRIAXI_EXIT_NONE is only ever written once, at the very start
 * of triaxi_run_func — it's a property every caller of
 * triaxi_collect_active_at (hence triaxi_test_collect_slot, hence here) is
 * responsible for upholding: each call site either has
 * h->process == TRIAXI_PROC_NONE (a non-isolated result, always fully
 * resolved by triaxi_run_func's own exhaustive completion switch before
 * being "parked" this way) or is preceded by a triaxi_fix_testres /
 * triaxi_fix_testres_timeout call. triaxi_unreachable() compiles to a bare
 * optimizer hint (__builtin_unreachable()/__assume(0)) on the compilers
 * this project actually ships warnings-clean on — undefined behavior, not a
 * diagnostic, if a future change to the collection loop ever violates this
 * without also updating every place that depends on it.
 */
static inline TRIAXI_TestResult triaxi_test_interpret(const TRIAXI_RunCtx*   run,
                                                      const TRIAXI_TestSlot* h) {
  TRIAXI_TestResult r = {TRIAXI_ZINIT};
  r.exit              = h->shared->exit;
  r.state             = h->shared->state;
  r.duration_ms       = h->shared->duration_ms;
  if (r.state == TRIAXI_STATE_SKIPPED) { return r; }
  r.outcome                        = TRIAXI_OUTCOME_PASSED;
  Triax_Str log                    = triaxi_file_read_buf(h->log, &TRIAXI_global.sbufs.log);
  r.capt.out                       = triaxi_file_read_buf(h->out, &TRIAXI_global.sbufs.out),
  r.capt.err                       = triaxi_file_read_buf(h->err, &TRIAXI_global.sbufs.err);
  const TRIAXI_TestInvocation* inv = &h->invocation;
  const TRIAXI_AssertHdr*      hdr = NULL;
  if (!log.str) { goto state_hdr; }
  for (const char *b = log.str, *e = b + log.len;;) {
    if ((size_t)(e - b) < sizeof(*hdr)) { goto state_hdr; }
    hdr = (const TRIAXI_AssertHdr*)(const void*)b, b += sizeof(*hdr);
    ++r.nasserts;
    const TRIAXI_AssertRes* res = NULL;
    if ((size_t)(e - b) < sizeof(TRIAXI_PacketPrefix)) { goto state_res; }
    if (*(const TRIAXI_PacketPrefix*)b == TRIAXI_RES_PREFIX) {
      if ((size_t)(e - b) < sizeof(TRIAXI_AssertRes)) { goto state_res; }
      res = (const TRIAXI_AssertRes*)(const void*)b, b += sizeof(TRIAXI_AssertRes);
      size_t args_padded = ((size_t)res->len + 7) & ~(size_t)7;
      if ((size_t)(e - b) < args_padded) { goto state_res; }
      b += args_padded;
    }
    triaxi_print_assert(run, inv, &r, hdr, res);
    if (res) { ++r.nfails, r.outcome = TRIAXI_OUTCOME_FAIL; }
  }
state_hdr:
  switch (h->shared->state) {
  default: triaxi_unreachable();
  case TRIAXI_STATE_SETUP:
  case TRIAXI_STATE_INIT:
  case TRIAXI_STATE_FINI:
  case TRIAXI_STATE_CLEANUP:
    /*
     * Kept as a generic (phase-tagged) error rather than reclassified to
     * ucrashed/uexited/uexception below: triaxi_print_test_end_json only
     * emits "phase" for TRIAXI_OUTCOME_ERROR, and phase (setup/init/fini/
     * cleanup) is exactly the useful information here — "fixture init
     * error: Aborted" tells the user where it happened, which a bare
     * "ucrashed" wouldn't.
     */
    r.outcome = r.exit.type == TRIAXI_EXIT_TIMEOUT ? TRIAXI_OUTCOME_TIMEOUT : TRIAXI_OUTCOME_ERROR;
    break;
  case TRIAXI_STATE_TEST:
    /*
     * Not mid-assertion, so unlike the equivalent switch in state_res below
     * there's no pending expected fault/exit code to compare against —
     * whatever happened here is always unexpected. Previously every
     * non-timeout exit type here was lumped into a generic "test_error",
     * which misclassified a plain crash/uncaught-exception/unexpected-exit()
     * in ordinary test-body code (i.e. not inside an assert_fault/
     * assert_exit expression) as framework/user misuse instead of the
     * ucrashed/uexception/uexited outcome it actually is.
     */
    switch (r.exit.type) {
    case TRIAXI_EXIT_NONE     : triaxi_unreachable();
    default                   : triaxi_unreachable();
    case TRIAXI_EXIT_TIMEOUT  : r.outcome = TRIAXI_OUTCOME_TIMEOUT; break;
    case TRIAXI_EXIT_ERROR    : r.outcome = TRIAXI_OUTCOME_ERROR; break;
    case TRIAXI_EXIT_FAULT    : r.outcome = TRIAXI_OUTCOME_UCRASH; break;
    case TRIAXI_EXIT_EXIT     : r.outcome = TRIAXI_OUTCOME_UEXIT; break;
    case TRIAXI_EXIT_EXCEPTION: r.outcome = TRIAXI_OUTCOME_UEXCEPT; break;
    }
    break;
  case TRIAXI_STATE_IN_ASSERT:
    /*
     * Reaching state_hdr at all means the log, fully parsed, held only
     * complete header(+result) records — nothing dangling. So if state is
     * still IN_ASSERT here, whatever stopped execution did so strictly
     * between "the current assertion's outcome was fully logged" and "the
     * state gets reset to TEST" — a gap that's now closed on the normal
     * path (triaxi_log_res resets state to TEST itself, right after
     * writing the result, before triaxi_debug_break() can fire) and on the
     * error path (triaxi_user_error_f only reaches this state==IN_ASSERT
     * check by having already set exit.type = ERROR, and setting it is
     * unconditional before the branch that decides whether to longjmp).
     * Anything else that could stop execution mid-predicate (a real crash,
     * a hang killed by timeout) does so *before* a result is ever logged,
     * which is a state_res short-read on the result, not a state_hdr
     * short-read on the next header — see state_res's own IN_ASSERT
     * handling below for that case. This used to fire on quite literally
     * every failing assertion under --break (see triaxi_log_res).
     */
    if (r.exit.type != TRIAXI_EXIT_ERROR) { triaxi_fatal(); }
    r.outcome = TRIAXI_OUTCOME_ERROR;
    break;
  case TRIAXI_STATE_DONE   : break;
  case TRIAXI_STATE_SKIPPED: triaxi_unreachable();
  }
  return r;

state_res: // expected res
  switch (h->shared->state) {
  default: triaxi_unreachable();
  case TRIAXI_STATE_TEST:
  case TRIAXI_STATE_DONE:
  /*
   * A header was logged with no result to follow (headers are always
   * logged, even for a passing assertion; only a failure also logs a
   * result) — the assertion that produced it passed, and execution has
   * since moved on. That's true regardless of which phase things ended up
   * in by the time we ran out of log data: state == SETUP/INIT/FINI/
   * CLEANUP here means the last assertion in the test body passed and
   * something abnormal (e.g. a crash) then happened in a fixture, which is
   * a real, reachable case (see state_hdr's own SETUP/INIT/FINI/CLEANUP
   * case for the phase-tagged classification of exactly that), not the
   * impossible/corrupted-log situation a triaxi_fatal() here used to
   * assume.
   */
  case TRIAXI_STATE_SETUP:
  case TRIAXI_STATE_INIT:
  case TRIAXI_STATE_FINI:
  case TRIAXI_STATE_CLEANUP:
    triaxi_print_assert(run, inv, &r, hdr, NULL); // Last assertion passed.
    goto state_hdr;
  case TRIAXI_STATE_SKIPPED  : triaxi_unreachable();
  case TRIAXI_STATE_IN_ASSERT: break;
  }
  TRIAXI_AssertRes* d = &TRIAXI_exec.pkg;
  memset(d, 0, sizeof(*d)); // old msvc on c++ won't accept the other one
  switch (r.exit.type) {
  case TRIAXI_EXIT_NONE   : triaxi_unreachable();
  default                 : triaxi_unreachable();
  case TRIAXI_EXIT_TIMEOUT: d->val = TRIAXI_AR_TIMEOUT, r.outcome = TRIAXI_OUTCOME_TIMEOUT; break;
  case TRIAXI_EXIT_ERROR  : r.outcome = TRIAXI_OUTCOME_ERROR; return r;
  case TRIAXI_EXIT_EXIT:
    if ((TRIAXI_AssertType)hdr->type != TRIAXI_AT_exit) {
      d->val = TRIAXI_AR_UEXITED, r.outcome = TRIAXI_OUTCOME_UEXIT;
    } else if (hdr->neg) {
      d->val = TRIAXI_AR_FAIL1, r.outcome = TRIAXI_OUTCOME_FAIL;
      triaxi_sprintargs2(*d, "no exit", "%s", r.exit.code, "%" PRIu32);
    } else if (hdr->cr != UINT32_MAX && hdr->cr != r.exit.code) {
      d->val = TRIAXI_AR_FAIL1, r.outcome = TRIAXI_OUTCOME_FAIL;
      triaxi_sprintargs2(*d, hdr->cr, "%" PRIu32, r.exit.code, "%" PRIu32);
    } else {
      d = NULL;
    }
    break;
  case TRIAXI_EXIT_FAULT:
    if ((TRIAXI_AssertType)hdr->type != TRIAXI_AT_fault) {
      d->val = TRIAXI_AR_UCRASHED, r.outcome = TRIAXI_OUTCOME_UCRASH;
    } else if (hdr->neg) {
      d->val = TRIAXI_AR_FAIL1, r.outcome = TRIAXI_OUTCOME_FAIL;
      triaxi_sprintargs2(*d, "no fault", "%s", triaxi_fault_tostr(r.exit.reason), "%s");
    } else if (hdr->cr != UINT32_MAX && hdr->cr != r.exit.reason) {
      d->val = TRIAXI_AR_FAIL1, r.outcome = TRIAXI_OUTCOME_FAIL;
      const char* ss  = triaxi_fault_tostr(hdr->cr);
      size_t      len = strlen(ss) + 1;
      memcpy(d->args, ss, d->len = (uint32_t)len);
      ss = triaxi_fault_tostr(r.exit.reason), len  = strlen(ss) + 1;
      memcpy(d->args + d->len, ss, len), d->len   += (uint32_t)len;
    } else {
      d = NULL;
    }
    break;
  case TRIAXI_EXIT_EXCEPTION: d->val = TRIAXI_AR_UEXCEPT, r.outcome = TRIAXI_OUTCOME_UEXCEPT; break;
  }
  triaxi_print_assert(run, inv, &r, hdr, d);
  if (d) { ++r.nfails; }
  return r;
}

static inline TRIAXI_SuiteCtx triaxi_suite_ctx_make(TRIAXI_Settings         run_settings,
                                                    TRIAXI_Fixtures         run_fixtures,
                                                    const Triax_Attributes* attrs) {
  return TRIAXI_T(TRIAXI_SuiteCtx){
      triaxi_settings_resolve(attrs, run_settings),
      {run_fixtures, {attrs->init, attrs->fini}},
  };
}

static inline TRIAXI_TestInvocation
    triaxi_test_invocation_make(const TRIAXI_SuiteCtx* suite, const TRIAXI_Test* test, size_t idx) {
  return TRIAXI_T(TRIAXI_TestInvocation){
      test,
      idx,
      triaxi_settings_resolve(&test->attrs, suite->settings),
      {suite->fixtures.run, suite->fixtures.suite, {test->attrs.init, test->attrs.fini}},
  };
}
static inline void triaxi_windows_childentry(const Triax_RunConfig* config) {
#  ifndef _WIN32
  (void)config;
#  else
  const char *shm_val = getenv("TRIAX_SHM"), *slot_val = getenv("TRIAX_SLOT");
  if (!shm_val || !slot_val) { return; } // not child
  TRIAXI_exec.isolated = true;
  /* Suppress crash dialogs and WER in the child. */
  SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
  if (signal(SIGABRT, triaxi_sighandler_abort) == SIG_ERR) { triaxi_fatal(); }
  setvbuf(stdout, NULL, _IONBF, 0), setvbuf(stderr, NULL, _IONBF, 0);

  uintmax_t mapping_value = triaxi_parse_u(shm_val);
  if (mapping_value == (uintmax_t)-1 || mapping_value > UINTPTR_MAX) { triaxi_fatal(); }
  HANDLE         shared_mapping = (HANDLE)(uintptr_t)mapping_value;
  TRIAXI_Shared* shared = (TRIAXI_Shared*)MapViewOfFile(shared_mapping, FILE_MAP_ALL_ACCESS, 0, 0,
                                                        sizeof(*shared) * TRIAXI_JOBS_MAX);
  if (!shared) { triaxi_fatal(); }
  if (!CloseHandle(shared_mapping)) { triaxi_fatal(); }
  uintmax_t slot_idx = triaxi_parse_u(slot_val);
  if (slot_idx >= TRIAXI_JOBS_MAX) { triaxi_fatal(); }
  if (!SetEnvironmentVariableW(L"TRIAX_SHM", NULL)) { triaxi_fatal(); }
  if (!SetEnvironmentVariableW(L"TRIAX_SLOT", NULL)) { triaxi_fatal(); }

  TRIAXI_Shared* const          child_shared = &shared[slot_idx];
  const TRIAXI_WinLaunch* const launch       = &child_shared->launch;

  if (launch->test_idx >= (size_t)(TRIAXI_global.tests.end - TRIAXI_global.tests.beg)) {
    triaxi_fatal();
  }
  const TRIAXI_Test* const t           = TRIAXI_global.tests.beg + launch->test_idx;

  Triax_Attributes         suite_attrs = {TRIAXI_ZINIT};

  if (launch->suite_idx != TRIAXI_INDEX_NONE) {
    const TRIAXI_SuiteReg *beg = TRIAXI_global.suiteregs.beg, *end = TRIAXI_global.suiteregs.end;

    if (launch->suite_idx >= (size_t)(end - beg)) { triaxi_fatal(); }

    const TRIAXI_SuiteReg* sr = beg + launch->suite_idx;
    if (!sr->name || strcmp(sr->name, t->suitename)) { triaxi_fatal(); }

    suite_attrs = sr->attrs;
  }

  const TRIAXI_SuiteCtx suite = triaxi_suite_ctx_make(
      triaxi_settings_resolve(&config->attrs, TRIAXI_default_settings),
      TRIAXI_T(TRIAXI_Fixtures){config->attrs.init, config->attrs.fini}, &suite_attrs);

  size_t pi = 0;
  if (launch->invocation_idx != TRIAXI_INDEX_NONE) {
    if (launch->invocation_idx >= t->params.elcount) { triaxi_fatal(); }
    pi = launch->invocation_idx;
  }

  TRIAXI_TestSlot h = {TRIAXI_ZINIT};
  h.shared          = child_shared;
  h.log             = launch->log;
  h.out             = launch->out;
  h.err             = launch->err;
  h.invocation      = triaxi_test_invocation_make(&suite, t, pi);
  triaxi_flush_all();
  triaxi_stdfds_redirect(&h);
  triaxi_run_func(&h.invocation, h.shared);
  _exit(0);
#  endif
}

static inline bool triaxi_suite_filter(const TRIAXI_RunCtx* run, Triax_Str suite) {
  if (!run->nfilters) { return true; }
  for (size_t i = 0; i < run->nfilters; ++i) {
    const char *f = run->filters[i], *sep = strstr(f, "::");
    if (!sep) {
      if (!strcmp(f, suite.str)) { return true; }
    } else if ((size_t)(sep - f) == suite.len && !strncmp(f, suite.str, suite.len)) {
      return true;
    }
  }
  return false;
}

static inline bool triaxi_test_filter(const TRIAXI_RunCtx* run, Triax_Str suite, const char* test) {
  if (!run->nfilters) { return true; }
  for (size_t nfilters = run->nfilters, i = 0; i < nfilters; ++i) {
    const char *f = run->filters[i], *sep = strstr(f, "::");
    if (!sep) {
      if (!strcmp(f, suite.str)) { return true; }
    } else if ((size_t)(sep - f) == suite.len && !strncmp(f, suite.str, suite.len)
               && !strcmp(sep + 2, test)) {
      return true;
    }
  }
  return false;
}

static inline bool triaxi_tags_match(const TRIAXI_RunCtx* run, const TRIAXI_Suite* suite,
                                     const TRIAXI_Test* test) {
  const char* tags[] = {run->tags, suite->attrs.tags, test->attrs.tags};
  if (!tags[0]) { return true; }
  for (const char *s1 = tags[0], *e1; *s1; s1 = *e1 ? e1 + 1 : e1) {
    for (e1 = s1; *e1 && *e1 != ','; ++e1);
    for (int i = 1; i < 3; ++i) {
      if (!tags[i]) { continue; }
      for (const char *s2 = tags[i], *e2; *s2; s2 = *e2 ? e2 + 1 : e2) {
        for (e2 = s2; *e2 && *e2 != ','; ++e2);
        if (e1 - s1 == e2 - s2 && !strncmp(s2, s1, (size_t)(e1 - s1))) { return true; }
      }
    }
  }
  return false;
}
#  ifndef _WIN32
typedef int TRIAXI_RawStatus;
#  else
typedef DWORD TRIAXI_RawStatus;
#  endif

static inline TRIAXI_Exit triaxi_exit_from_status(TRIAXI_RawStatus status) {
#  ifndef _WIN32
  if (WIFSIGNALED(status)) {
    return TRIAXI_T(TRIAXI_Exit){TRIAXI_EXIT_FAULT, {(Triax_Fault)WTERMSIG(status)}};
  } else if (WIFEXITED(status)) {
    return TRIAXI_T(TRIAXI_Exit){TRIAXI_EXIT_EXIT, {(uint32_t)WEXITSTATUS(status)}};
  }
  triaxi_fatal();
#  else
  // Windows maps exception-like statuses to faults; ordinary codes are exits.
  if (status >= 0x80000000u || status == TRIAX_FAULT_ABORT) {
    return TRIAXI_T(TRIAXI_Exit){TRIAXI_EXIT_FAULT, {status}};
  } else {
    return TRIAXI_T(TRIAXI_Exit){TRIAXI_EXIT_EXIT, {status}};
  }
#  endif
}

static inline TRIAXI_Outcome triaxi_test_collect_slot(const TRIAXI_RunCtx*    run,
                                                      const TRIAXI_RunResult* rres,
                                                      TRIAXI_TestSlot* h, size_t* nreported) {
  triaxi_print_test_beg(run, &h->invocation, *nreported == 0);
  TRIAXI_TestResult res = triaxi_test_interpret(run, h);
  triaxi_print_test_end(run, &h->invocation, rres, &res, ++*nreported);
  triaxi_file_clear(h->log), triaxi_file_clear(h->out), triaxi_file_clear(h->err);
  return res.outcome;
}
#  ifndef _WIN32
/*
 * Terminate every process remaining in an isolated test's process group.
 *
 * The root test process is the group leader, so -pgid reaches it and all
 * descendants that remain in the group. POSIX provides no portable way for
 * the runner to reap arbitrary grandchildren or wait for an entire process
 * group to disappear: dead descendants may remain visible as zombies until
 * their eventual parent reaps them. Do not poll group existence here.
 *
 * Output produced by descendants after the root test process terminates is
 * not guaranteed to be captured; the containment guarantee is that remaining
 * group members are sent SIGKILL so they cannot continue running indefinitely.
 */
static inline void triaxi_terminate_group(pid_t pgid) {
  if (kill(-pgid, SIGKILL) && errno != ESRCH) { triaxi_fatal(); }
}
#  endif

static inline TRIAXI_Outcome triaxi_collect_active_at(const TRIAXI_RunCtx*    run,
                                                      const TRIAXI_RunResult* rres,
                                                      TRIAXI_TestSlot** active, size_t* inflight,
                                                      size_t i, size_t* nreported) {
  if (i >= *inflight) { triaxi_unreachable(); }
  TRIAXI_TestSlot* h = active[i];
#  ifdef _WIN32
  if (h->process != TRIAXI_PROC_NONE) { triaxi_windows_slot_process_finish(h, false); }
#  else
  if (h->process != TRIAXI_PROC_NONE) { triaxi_terminate_group(h->process); }
#  endif
  TRIAXI_Outcome o  = triaxi_test_collect_slot(run, rres, h, nreported);
  h->process        = TRIAXI_PROC_NONE;
  active[i]         = active[--*inflight];
  active[*inflight] = h;
  return o;
}
#  ifndef _WIN32
static inline pid_t triaxi_waitpid(pid_t pid, int* status, int flags) {
  pid_t res;
  do { res = waitpid(pid, status, flags); } while (res == -1 && errno == EINTR);
  return res;
}
#  endif

static inline bool triaxi_test_finished(const TRIAXI_TestSlot* h) {
  return h->shared->exit.type != TRIAXI_EXIT_NONE;
}
static inline void triaxi_fix_testres(TRIAXI_TestSlot* h, TRIAXI_RawStatus status,
                                      uint32_t duration_ms) {
  if (!triaxi_test_finished(h)) {
    h->shared->duration_ms = duration_ms;
    h->shared->exit        = triaxi_exit_from_status(status);
  }
}
static inline void triaxi_fix_testres_timeout(TRIAXI_TestSlot* h, uint32_t duration_ms) {
  if (!triaxi_test_finished(h)) {
    h->shared->duration_ms = duration_ms;
    h->shared->exit        = TRIAXI_T(TRIAXI_Exit){TRIAXI_EXIT_TIMEOUT, {0}};
  }
}
static inline TRIAXI_Outcome triaxi_collect_from_active(const TRIAXI_RunCtx*    run,
                                                        const TRIAXI_RunResult* rres,
                                                        TRIAXI_TestSlot** active, size_t* inflight,
                                                        size_t* nreported) {
  size_t n = *inflight;
  if (!n) { triaxi_unreachable(); }
  TRIAXI_RawStatus status;
#  ifndef _WIN32
  if (n == 1) {
    TRIAXI_TestSlot* h = active[0];
    if (h->process == TRIAXI_PROC_NONE) {
      return triaxi_collect_active_at(run, rres, active, inflight, 0, nreported);
    }
    if (h->invocation.settings.timeout_ms == TRIAX_TIMEOUT_NONE) {
      if (triaxi_waitpid(h->process, &status, 0) == -1) { triaxi_fatal(); }
      triaxi_fix_testres(h, status, triaxi_elapsed_ms(h->start_ms));
      return triaxi_collect_active_at(run, rres, active, inflight, 0, nreported);
    }
  }
  for (;;) {
    int64_t now = triaxi_now_ms();
    for (size_t i = 0; i < n; ++i) {
      TRIAXI_TestSlot* h = active[i];
      if (h->process == TRIAXI_PROC_NONE) {
        return triaxi_collect_active_at(run, rres, active, inflight, i, nreported);
      }
      uint32_t timeout_ms = h->invocation.settings.timeout_ms;
      switch (triaxi_waitpid(h->process, &status, WNOHANG)) {
      case -1: triaxi_fatal();
      case 0 : break;
      default:
        triaxi_fix_testres(h, status, triaxi_elapsed_ms(h->start_ms));
        return triaxi_collect_active_at(run, rres, active, inflight, i, nreported);
      }
      if (timeout_ms == TRIAX_TIMEOUT_NONE || now - h->start_ms < (int64_t)timeout_ms) { continue; }
      // The child may have exited naturally after the probe above but before we reached the timeout
      switch (triaxi_waitpid(h->process, &status, WNOHANG)) {
      case -1: triaxi_fatal();
      case 0 : break;
      default:
        triaxi_fix_testres(h, status, triaxi_elapsed_ms(h->start_ms));
        return triaxi_collect_active_at(run, rres, active, inflight, i, nreported);
      }
      uint32_t elapsed = triaxi_elapsed_ms(h->start_ms);
      bool     killed  = false;
      if (!kill(-h->process, SIGKILL)) {
        killed = true;
      } else {
        if (errno != EPERM && errno != ESRCH) { triaxi_fatal(); }
        switch (triaxi_waitpid(h->process, &status, WNOHANG)) {
        case -1: triaxi_fatal();
        case 0:
          /*
           * It genuinely hasn't become waitable.
           * Group kill failed for some other reason, so guarantee
           * termination of our direct child.
           */
          if (kill(h->process, SIGKILL) && errno != ESRCH) { triaxi_fatal(); }
          killed = true;
          break;
        default:
          // It exited naturally in the race window, not actually a timeout kill.
          triaxi_fix_testres(h, status, triaxi_elapsed_ms(h->start_ms));
          return triaxi_collect_active_at(run, rres, active, inflight, i, nreported);
        }
      }
      if (triaxi_waitpid(h->process, &status, 0) == -1) { triaxi_fatal(); }
      if (killed && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
        triaxi_fix_testres_timeout(h, elapsed);
      } else {
        triaxi_fix_testres(h, status, triaxi_elapsed_ms(h->start_ms));
      }
      /*
       * The timeout path already signalled the process group above. Repeat
       * termination after reaping the root to cover descendants that may still
       * remain in the group; this is containment only, not a wait-for-group
       * operation.
       */
      triaxi_terminate_group(h->process);
      h->process = TRIAXI_PROC_NONE;
      return triaxi_collect_active_at(run, rres, active, inflight, i, nreported);
    }
    static const struct timespec ts = {0, 1000000}; // 1 ms
    nanosleep(&ts, NULL);
  }

#  else
  // WaitForMultipleObjects blocks on every in-flight isolated child and wakes
  // when one exits. The nearest test deadline bounds the wait. Job-object
  // termination also kills descendants.
  HANDLE waits[TRIAXI_JOBS_MAX];
  for (size_t i = 0; i < n; ++i) {
    TRIAXI_TestSlot* h = active[i];
    // Parked non-isolated result: execution has already committed the result.
    if (h->process == TRIAXI_PROC_NONE) {
      return triaxi_collect_active_at(run, rres, active, inflight, i, nreported);
    }
    waits[i] = h->process;
  }
  for (;;) {
    int64_t now       = triaxi_now_ms();
    int64_t nearest   = -1;
    size_t  expired_i = n;
    for (size_t i = 0; i < n; ++i) {
      TRIAXI_TestSlot* h          = active[i];
      uint32_t         timeout_ms = h->invocation.settings.timeout_ms;
      if (timeout_ms == TRIAX_TIMEOUT_NONE) { continue; }
      int64_t remaining = h->start_ms + (int64_t)timeout_ms - now;
      if (remaining <= 0) {
        expired_i = i;
        break;
      }
      if (nearest < 0 || remaining < nearest) { nearest = remaining; }
    }
    if (expired_i != n) {
      TRIAXI_TestSlot* expired = active[expired_i];
      // process may have completed naturally in between
      switch (WaitForSingleObject(expired->process, 0)) {
      default: triaxi_fatal();
      case WAIT_OBJECT_0:
        if (!GetExitCodeProcess(expired->process, &status)) { triaxi_fatal(); }
        triaxi_fix_testres(expired, status, triaxi_elapsed_ms(expired->start_ms));
        return triaxi_collect_active_at(run, rres, active, inflight, expired_i, nreported);
      case WAIT_TIMEOUT: break;
      }
      uint32_t elapsed = triaxi_elapsed_ms(expired->start_ms);

      if (!TerminateJobObject(expired->job, 1)) { triaxi_fatal(); }
      if (WaitForSingleObject(expired->process, INFINITE) != WAIT_OBJECT_0) { triaxi_fatal(); }
      triaxi_fix_testres_timeout(expired, elapsed);

      /*
       * Release the root and wait until all job descendants are gone before
       * interpreting the capture files.
       */
      triaxi_windows_slot_process_finish(expired, true);
      return triaxi_collect_active_at(run, rres, active, inflight, expired_i, nreported);
    }
    DWORD timeout = nearest < 0 ? INFINITE : (DWORD)nearest;
    DWORD rr      = WaitForMultipleObjects((DWORD)n, waits, FALSE, timeout);
    if (rr < WAIT_OBJECT_0 + (DWORD)n) {
      size_t           i = (size_t)(rr - WAIT_OBJECT_0);
      TRIAXI_TestSlot* h = active[i];
      if (!GetExitCodeProcess(h->process, &status)) { triaxi_fatal(); }
      triaxi_fix_testres(h, status, triaxi_elapsed_ms(h->start_ms));
      return triaxi_collect_active_at(run, rres, active, inflight, i, nreported);
    }
    if (rr == WAIT_TIMEOUT) { continue; }
    triaxi_fatal();
  }
#  endif
}

static inline void triaxi_test_exec_isolation(TRIAXI_TestSlot* h) {
#  ifndef _WIN32
  switch ((h->process = fork())) {
  case -1: triaxi_fatal();
  case 0: // child
    if (setpgid(0, 0)) { triaxi_fatal(); }
    /*
     * Restore the pre-Triax signal dispositions (SIGTERM/SIGINT/SIGHUP,
     * SIGPIPE/SIGUSR1/SIGUSR2, crash signals) and alternate stack before
     * fixtures/user code run. Isolated tests are observed from the parent
     * via exit status, not in-process signal handling (that's only wired up
     * for non-isolated tests, see triaxi_crash_handlers_setup below), so the
     * child needs none of the runner's own signal machinery — leaving it in
     * place would mean a test of SIGPIPE, alarms, or signal inheritance
     * isn't running in the environment the program would normally see.
     */
    TRIAXI_exec.isolated = true;
    triaxi_process_control_reset();
    triaxi_stdfds_redirect(h);
    triaxi_run_func(&h->invocation, h->shared);
    _exit(0);
  default: // parent
    if (setpgid(h->process, h->process) && errno != EACCES && errno != ESRCH) { triaxi_fatal(); }
    return;
  }
#  else
  const TRIAXI_Test* const t      = h->invocation.test;
  TRIAXI_WinLaunch* const  launch = &h->shared->launch;
  launch->invocation_idx          = t->params.elcount ? h->invocation.idx : TRIAXI_INDEX_NONE;
  launch->test_idx                = (size_t)(t - TRIAXI_global.tests.beg);
  launch->suite_idx               = TRIAXI_INDEX_NONE;
  for (const TRIAXI_SuiteReg* sr = TRIAXI_global.suiteregs.beg; sr != TRIAXI_global.suiteregs.end;
       ++sr) {
    if (sr->name && !strcmp(sr->name, t->suitename)) {
      launch->suite_idx = (size_t)(sr - TRIAXI_global.suiteregs.beg);
      break;
    }
  }

  const WCHAR* const cmd = GetCommandLineW(); // CreateProcessW may modify its command-line buffer
  if (!cmd) { triaxi_fatal(); }
  const size_t nchars = wcslen(cmd) + 1;
  if (nchars > triaxi_countof(TRIAXI_global.command_line)) { triaxi_fatal(); }
  memcpy(TRIAXI_global.command_line, cmd, nchars * sizeof(*cmd));

  triaxi_alignas(void*) char attr_buf[512];
  STARTUPINFOEXW             sia = {TRIAXI_ZINIT};
  sia.StartupInfo.cb             = sizeof(sia);
  sia.StartupInfo.dwFlags        = STARTF_USESTDHANDLES;
  sia.StartupInfo.hStdOutput     = h->out;
  sia.StartupInfo.hStdError      = h->err;
  SECURITY_ATTRIBUTES sa         = {sizeof(sa), NULL, TRUE};
  sia.StartupInfo.hStdInput
      = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, NULL);
  if (sia.StartupInfo.hStdInput == INVALID_HANDLE_VALUE) { triaxi_fatal(); }

  SIZE_T attr_size = 0;
  if (!InitializeProcThreadAttributeList(NULL, 1, 0, &attr_size)
      && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    triaxi_fatal();
  }
  if (attr_size > sizeof(attr_buf)) { triaxi_fatal(); }
  sia.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)(void*)attr_buf;

  if (!InitializeProcThreadAttributeList(sia.lpAttributeList, 1, 0, &attr_size)) { triaxi_fatal(); }
  HANDLE inherit_handles[] = {
      h->log, h->out, h->err, sia.StartupInfo.hStdInput, TRIAXI_global.shared_mapping,
  };
  if (!UpdateProcThreadAttribute(sia.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                 inherit_handles, sizeof(inherit_handles), NULL, NULL)) {
    triaxi_fatal();
  }

  // if (h->process || h->job) { triaxi_fatal(); }
  if (!(h->job = CreateJobObjectW(NULL, NULL))) { triaxi_fatal(); }
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION li = {TRIAXI_ZINIT};
  li.BasicLimitInformation.LimitFlags     = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(h->job, JobObjectExtendedLimitInformation, &li, sizeof(li))) {
    triaxi_fatal();
  }

  PROCESS_INFORMATION pi = {TRIAXI_ZINIT};
  if (!CreateProcessW(TRIAXI_global.executable_path, TRIAXI_global.command_line, NULL, NULL, TRUE,
                      EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                      h->environment, NULL, &sia.StartupInfo, &pi)) {
    triaxi_fatal();
  }
  if (!AssignProcessToJobObject(h->job, pi.hProcess)) {
    DWORD error = GetLastError();
    (void)TerminateProcess(pi.hProcess, 1); // not yet contained by job
    SetLastError(error);
    triaxi_fatal();
  }
  h->process = pi.hProcess;

  if (ResumeThread(pi.hThread) == (DWORD)-1) { triaxi_fatal(); }
  if (!CloseHandle(pi.hThread)) { triaxi_fatal(); }

  DeleteProcThreadAttributeList(sia.lpAttributeList);
  if (!CloseHandle(sia.StartupInfo.hStdInput)) { triaxi_fatal(); }
#  endif
}

static inline void triaxi_test_exec_noisolation(TRIAXI_TestSlot* h) {
  h->process = TRIAXI_PROC_NONE; // park: result collected via collect_any
  triaxi_run_func(&h->invocation, h->shared);
}
static inline void triaxi_test_start(TRIAXI_TestSlot* h, TRIAXI_TestInvocation inv,
                                     const TRIAXI_StdBackup* saved) {
  triaxi_flush_all(); // required before fork/redirection
  h->invocation          = inv;
  h->shared->state       = TRIAXI_STATE_SETUP;
  h->shared->duration_ms = 0;
  h->shared->exit        = TRIAXI_T(TRIAXI_Exit){TRIAXI_ZINIT};
  h->start_ms            = triaxi_now_ms();
  if (((inv.test->params.ptr != NULL) ^ (inv.test->params.elsize > 0))
      || (inv.test->params.ptr && !inv.test->params.elcount)) {
    h->process           = TRIAXI_PROC_NONE;
    h->shared->exit.type = TRIAXI_EXIT_ERROR;
    h->shared->exit.code = triaxi_error_pack(TRIAXI_ERROR_MALFORMED_PARAMS, 0);
    return;
  }
  if (inv.settings.isolation != TRIAX_ISOLATION_OFF) {
    triaxi_test_exec_isolation(h);
  } else {
    if (inv.settings.timeout_ms != TRIAX_TIMEOUT_NONE) {
      h->process           = TRIAXI_PROC_NONE;
      h->shared->exit.type = TRIAXI_EXIT_ERROR;
      h->shared->exit.code = triaxi_error_pack(TRIAXI_ERROR_TIMEOUT_WITHOUT_ISOLATION, 0);
      return;
    }
    triaxi_stdfds_redirect(h);
    triaxi_test_exec_noisolation(h);
    triaxi_stdfds_restore(saved);
  }
}

static inline bool triaxi_suite_run(const TRIAXI_RunCtx* run, TRIAXI_RunResult* r,
                                    const TRIAXI_Suite* s) {
  const TRIAXI_SuiteCtx suite = triaxi_suite_ctx_make(run->settings, run->fixtures, &s->attrs);
  TRIAXI_SuiteResult    res   = {TRIAXI_ZINIT};
  bool                  has_launched = false; // gates suite beg/end printing
  bool                  failed_fast  = false;
  size_t                nreported    = 0;
  TRIAXI_TestSlot*      active[TRIAXI_JOBS_MAX];
  size_t                nactive    = 0;
  const size_t          max_active = run->njobs;
  for (size_t i = 0; i < max_active; ++i) { active[i] = &TRIAXI_global.slots[i]; }
  Triax_Str suitestr = {s->name, strlen(s->name)};
  int64_t   t0       = triaxi_now_ms();
  for (size_t s_idx = 0; s_idx < s->ntests; ++s_idx) {
    const TRIAXI_Test* const t = s->tests[s_idx];
    if (!triaxi_test_filter(run, suitestr, t->name) || !triaxi_tags_match(run, s, t)) { continue; }
    ++r->tests.selected;
    size_t pc                = TRIAXI_MAX(t->params.elcount, (size_t)1);
    r->invocations.selected += pc;
    if (failed_fast) { continue; }
    if (!has_launched) { // print suite boundaries at most once
      has_launched = true;
      triaxi_print_suite_beg(run, s, r);
      ++r->suites_run;
    }
    for (size_t pi = 0; pi < pc; ++pi) {
      TRIAXI_TestInvocation inv = triaxi_test_invocation_make(&suite, t, pi);
      if (run->debug) {
        inv.settings.isolation  = TRIAX_ISOLATION_OFF;
        inv.settings.timeout_ms = TRIAX_TIMEOUT_NONE;
      }
      if (s->attrs.skip || t->attrs.skip) {
        TRIAXI_TestResult tres = {TRIAXI_ZINIT};
        triaxi_print_test_beg(run, &inv, nreported == 0);
        triaxi_print_test_end(run, &inv, r, &tres, ++nreported);
        ++res.outcomes[TRIAXI_OUTCOME_SKIP];
        continue;
      }
      TRIAXI_TestSlot* h; // if queue is full, reap first
      if (nactive == max_active) {
        TRIAXI_Outcome o = triaxi_collect_from_active(run, r, active, &nactive, &nreported);
        ++res.outcomes[o];
        if (run->fail_fast && o != TRIAXI_OUTCOME_PASSED && o != TRIAXI_OUTCOME_SKIP) {
          failed_fast = true;
          break;
        }
      }
      h = active[nactive];
      triaxi_test_start(h, inv, &run->saved);
      active[nactive++] = h;
    }
  }
  while (nactive) {
    ++res.outcomes[triaxi_collect_from_active(run, r, active, &nactive, &nreported)];
  }
  if (has_launched) {
    res.duration_ms = triaxi_elapsed_ms(t0);
    triaxi_print_suite_end(run, s, &res);
  }
  for (size_t i = 0; i < triaxi_countof(res.outcomes); ++i) {
    r->invocations.cnts[i] += res.outcomes[i];
  }
  return res.outcomes[TRIAXI_OUTCOME_FAIL] || res.outcomes[TRIAXI_OUTCOME_UCRASH]
      || res.outcomes[TRIAXI_OUTCOME_UEXIT] || res.outcomes[TRIAXI_OUTCOME_UEXCEPT]
      || res.outcomes[TRIAXI_OUTCOME_TIMEOUT] || res.outcomes[TRIAXI_OUTCOME_ERROR];
}

static inline int triax_run(Triax_RunConfig config) {
  const TRIAXI_RunCtx run         = triaxi_runner_init(&config);
  TRIAXI_RunResult    res         = {TRIAXI_ZINIT};
  bool                failed_fast = false;
  int64_t             t0          = triaxi_now_ms();
  triaxi_print_run_beg(&run);
  for (size_t idx = 0; idx < TRIAXI_global.nsuites; ++idx) {
    const TRIAXI_Suite* s        = &TRIAXI_global.suites[idx];
    Triax_Str           suitestr = {s->name, strlen(s->name)};
    if (!triaxi_suite_filter(&run, suitestr)) { continue; }
    if (!failed_fast) {
      if (triaxi_suite_run(&run, &res, s) && run.fail_fast) { failed_fast = 1; }
      continue;
    }
    for (size_t i = 0; i < s->ntests; ++i) {
      const TRIAXI_Test* t = s->tests[i];
      if (!triaxi_test_filter(&run, suitestr, t->name) || !triaxi_tags_match(&run, s, t)) {
        continue;
      }
      ++res.tests.selected;
      res.invocations.selected += TRIAXI_MAX(t->params.elcount, (size_t)1);
    }
    continue;
  }
  res.duration_ms = triaxi_elapsed_ms(t0);
  triaxi_print_run_end(&run, &res);
  triaxi_runner_cleanup(&run);
  return res.invocations.cnts[TRIAXI_OUTCOME_PASSED] + res.invocations.cnts[TRIAXI_OUTCOME_SKIP]
      != res.invocations.selected;
}

#  pragma endregion runner_execution

#  pragma region runner_cli
static inline const char* triaxi_parse_flag_val(const char* arg, const char* flag) {
  size_t n = strlen(flag);
  if (strncmp(arg, flag, n)) { return NULL; }
  if (arg[n] == '=') { return arg + n + 1; }
  if (!arg[n]) { return ""; }
  return NULL;
}

static inline Triax_RunConfig triaxi_parse_argv(int argc, const char* const* argv,
                                                Triax_RunConfig defaults) {
  Triax_RunConfig res = defaults;
  const char*     val;
  bool            out_set[4]       = {0};
  bool            filters_override = false;
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (arg[0] != '-') {
      if (!filters_override) {
        res.nfilters = 0, filters_override = 1;
      } else {
        if (res.nfilters >= triaxi_countof(res.filters)) {
          res.kind = TRIAXI_PARSE_ERR_TOO_MANY_FILTERS;
          return res;
        }
      }
      res.filters[res.nfilters++] = arg;
      continue;
    }
    if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
      res.kind = TRIAXI_PARSE_HELP;
      return res;
    } else if (!strcmp(arg, "--list") || !strcmp(arg, "-l")) {
      if (res.kind == TRIAXI_PARSE_RUN) { res.kind = TRIAXI_PARSE_LIST; }
    } else if (!strcmp(arg, "--no-color")) {
      res.no_color = true;
    } else if (!strcmp(arg, "--fail-fast")) {
      res.fail_fast = true;
    } else if (!strcmp(arg, "--break")) {
      res.debug_break = true;
    } else if (!strcmp(arg, "--debug")) {
      res.debug = true;
    } else if ((val = triaxi_parse_flag_val(arg, "--tags"))) {
      res.attrs.tags = *val ? val : NULL;
    } else if ((val = triaxi_parse_flag_val(arg, "--isolation"))) {
      if (!strcmp(val, "on")) {
        res.attrs.isolation = TRIAX_ISOLATION_ON;
      } else if (!strcmp(val, "off")) {
        res.attrs.isolation = TRIAX_ISOLATION_OFF;
      } else {
        res.kind                                    = TRIAXI_PARSE_ERR_INVALID_OPTION;
        res.filters[TRIAXI_ERRORDATA_FLAGTYPE]      = "--isolation";
        res.filters[TRIAXI_ERRORDATA_SEL_OPTION]    = val;
        res.filters[TRIAXI_ERRORDATA_AVAIL_OPTIONS] = "'on' or 'off";
        return res;
      }
    } else if ((val = triaxi_parse_flag_val(arg, "--verbosity"))) {
      if (!strcmp(val, "always")) {
        res.attrs.verbosity = TRIAX_VERBOSITY_ALWAYS;
      } else if (!strcmp(val, "on-fail")) {
        res.attrs.verbosity = TRIAX_VERBOSITY_ON_FAIL;
      } else if (!strcmp(val, "never")) {
        res.attrs.verbosity = TRIAX_VERBOSITY_NEVER;
      } else {
        res.kind                                    = TRIAXI_PARSE_ERR_INVALID_OPTION;
        res.filters[TRIAXI_ERRORDATA_FLAGTYPE]      = "--verbosity";
        res.filters[TRIAXI_ERRORDATA_SEL_OPTION]    = val;
        res.filters[TRIAXI_ERRORDATA_AVAIL_OPTIONS] = "'always', 'on-fail', or 'never";
        return res;
      }
    } else if ((val = triaxi_parse_flag_val(arg, "--timeout"))) {
      uintmax_t ms = triaxi_parse_u(val);
      if (ms == (uintmax_t)-1 || ms >= UINT32_MAX) {
        res.kind                                    = TRIAXI_PARSE_ERR_INVALID_OPTION;
        res.filters[TRIAXI_ERRORDATA_FLAGTYPE]      = "--timeout";
        res.filters[TRIAXI_ERRORDATA_SEL_OPTION]    = val;
        res.filters[TRIAXI_ERRORDATA_AVAIL_OPTIONS] = "integer 0..4294967294";
        return res;
      }
      res.attrs.timeout_ms = (uint32_t)ms;
    } else if ((val = triaxi_parse_flag_val(arg, "--jobs"))) {
      uintmax_t j = triaxi_parse_u(val);
      if (j == (uintmax_t)-1 || j > TRIAXI_JOBS_MAX) {
        res.kind                                    = TRIAXI_PARSE_ERR_INVALID_OPTION;
        res.filters[TRIAXI_ERRORDATA_FLAGTYPE]      = "--jobs";
        res.filters[TRIAXI_ERRORDATA_SEL_OPTION]    = val;
        res.filters[TRIAXI_ERRORDATA_AVAIL_OPTIONS] = "integer 0..64";
        return res;
      }
      res.njobs = (uint8_t)j;
    } else {
      const char** outtypes[]
          = {&res.outpaths.text, &res.outpaths.json, &res.outpaths.tap, &res.outpaths.junit};
      static const char* outt[] = {"--text", "--json", "--tap", "--junit"};
      int                type   = -1;
      for (int j = 0; j < 4; ++j) {
        if ((val = triaxi_parse_flag_val(arg, outt[j]))) {
          type = j;
          break;
        }
      }
      if (type == -1) {
        res.kind                               = TRIAXI_PARSE_ERR_INVALID_FLAG;
        res.filters[TRIAXI_ERRORDATA_FLAGTYPE] = arg;
        return res;
      }
      if (out_set[type]) {
        res.kind                               = TRIAXI_PARSE_ERR_DUP_OUTPUT;
        res.filters[TRIAXI_ERRORDATA_FLAGTYPE] = outt[type] + 2;
        return res;
      }
      if (!*val || !strcmp(val, "stdout")) {
        *outtypes[type] = TRIAX_OUTPATH_STDOUT;
      } else if (!strcmp(val, "none") || !strcmp(val, "off")) {
        *outtypes[type] = TRIAX_OUTPATH_NONE;
      } else {
        *outtypes[type] = val;
      }
      // text is stdout by default, turns off when other is set to stdout
      if (*outtypes[type] == TRIAX_OUTPATH_STDOUT) {
        if (type != 0 && *outtypes[0] == TRIAX_OUTPATH_DEFAULT) {
          *outtypes[0] = TRIAX_OUTPATH_NONE;
        }
      }
      out_set[type] = 1;
    }
  }
  return res;
}

static inline Triax_RunConfig triax_parse_argv(int argc, char* argv[], Triax_RunConfig defaults) {
  return triaxi_parse_argv(argc, (const char* const*)argv, defaults);
}
static inline int triax_run_argv(int argc, char* argv[], Triax_RunConfig defaults) {
  return triax_run(triax_parse_argv(argc, argv, defaults));
}
#  pragma endregion runner_cli

# endif /* TRIAXI_REGION_RUNNER_ONLY */

# undef triaxi_catlit
# undef triaxi_noreturn
# undef triaxi_unreachable
# undef triaxi_restrict
# undef TRIAXI_MIN
# undef TRIAXI_MAX
# undef triaxi_lenof
# undef TRIAXI_COMMA_ONE

# ifdef __GNUC__
#  pragma GCC diagnostic pop
# else
#  pragma warning(pop)
# endif

// NOLINTEND(performance-enum-size)
# pragma endregion implementation

#endif /* TRIAXI_TEST_H */

// MIT License
//
// Copyright (c) 2026 Dariusch Knigge
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
