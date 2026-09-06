// Ported from rk_lib/tests/test_rk_test.c (TEST_PRINT and its verbosity
// variants). Each test is wrapped in triaxi_validate/triaxi_validate_opts to
// verify its pass/fail/crash/timeout outcome, so ctest can gate on it like
// any other test. Scope note: the original suite's actual point was stdout/
// stderr print-VISIBILITY by verbosity level (SHALL PRINT vs SHANT PRINT) —
// that isn't something triaxi_validate can check (it only reads the JSON
// "outcome" field, which verbosity doesn't affect), and neither
// triaxi_validate nor triaxi_validate_opts currently has a way to attach a
// per-test .verbosity override to begin with. So per-test .verbosity has
// been dropped here; only suite-level verbosity (safe to share between the
// outer meta-check and the inner subject body) is preserved. Outcome
// verification is real coverage on its own, just narrower than the original.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

triax_suite(print, .verbosity = TRIAX_VERBOSITY_DEFAULT, .isolation = TRIAX_ISOLATION_ON);
triaxi_validate(print, test_pass_stdout_noprint, TRIAXI_VALIDATE_PASSED) {
  printf("SHANT PRINT"); // nolint
  triax_expect_true(1);
}
triaxi_validate(print, test_fail_stdout_print, TRIAXI_VALIDATE_FAILED) {
  printf("SHALL PRINT");
  triax_expect_true(0);
}
triaxi_validate(print, test_fault_stdout_print, TRIAXI_VALIDATE_UEXITED) {
  printf("SHALL PRINT"); // nolint
  triax_expect_true((exit(0), 1));
}
triaxi_validate_opts(print, test_timeout_stdout_print, TRIAXI_VALIDATE_TIMEOUT, 10,
                     TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  printf("SHALL PRINT"); // nolint
  sleep(100000);
}
triaxi_validate(print, test_crash_stdout_print, TRIAXI_VALIDATE_UCRASHED) {
  printf("SHALL PRINT"); // nolint
  raise(SIGABRT);
}
triaxi_validate(print, test_pass_stderr_noprint, TRIAXI_VALIDATE_PASSED) {
  fprintf(stderr, "SHANT PRINT"); // nolint
  triax_expect_true(1);
}
triaxi_validate(print, test_fail_stderr_print, TRIAXI_VALIDATE_FAILED) {
  fprintf(stderr, "SHALL PRINT");
  triax_expect_true(0);
}
triaxi_validate(print, test_fault_stderr_print, TRIAXI_VALIDATE_UEXITED) {
  fprintf(stderr, "SHALL PRINT"); // nolint
  triax_expect_true((exit(0), 1));
}
triaxi_validate_opts(print, test_timeout_stderr_print, TRIAXI_VALIDATE_TIMEOUT, 1,
                     TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  fprintf(stderr, "SHALL PRINT"); // nolint
  sleep(10);
}
triaxi_validate(print, test_crash_stderr_print, TRIAXI_VALIDATE_UCRASHED) {
  fprintf(stderr, "SHALL PRINT"); // nolint
  raise(SIGABRT);
}

// ── test-level verbosity: never ─────────────────────────────────────────────
triax_suite(print_verbosity_testlevel_never, .verbosity = TRIAX_VERBOSITY_ALWAYS,
            .isolation = TRIAX_ISOLATION_ON);
triaxi_validate(print_verbosity_testlevel_never, test_fail_stdout_print_never,
                TRIAXI_VALIDATE_FAILED) {
  printf("SHANT PRINT\n"); // nolint
  triax_assert_true(0);
}
triaxi_validate(print_verbosity_testlevel_never, test_fault_stdout_print_never,
                TRIAXI_VALIDATE_UEXITED) {
  printf("SHANT PRINT\n"); // nolint
  triax_expect_true((exit(0), 1));
}
triaxi_validate_opts(print_verbosity_testlevel_never, test_timeout_stdout_print_never,
                     TRIAXI_VALIDATE_TIMEOUT, 1, TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  printf("SHANT PRINT\n"); // nolint
  sleep(10);
}
triaxi_validate(print_verbosity_testlevel_never, test_print_to_out_e_never,
                TRIAXI_VALIDATE_UCRASHED) {
  printf("SHANT PRINT\n"); // nolint
  raise(SIGABRT);
}
triaxi_validate(print_verbosity_testlevel_never, test_print_to_err_p_never,
                TRIAXI_VALIDATE_PASSED) {
  fprintf(stderr, "SHANT PRINT\n"); // nolint
  triax_assert_true(1);
}
triaxi_validate(print_verbosity_testlevel_never, test_print_to_err_f_never,
                TRIAXI_VALIDATE_FAILED) {
  fprintf(stderr, "SHANT PRINT\n"); // nolint
  triax_assert_true(0);
}
triaxi_validate(print_verbosity_testlevel_never, test_print_to_err_c_never,
                TRIAXI_VALIDATE_UEXITED) {
  fprintf(stderr, "SHANT PRINT\n"); // nolint
  triax_expect_true((exit(0), 1));
}
triaxi_validate_opts(print_verbosity_testlevel_never, test_print_to_err_t_never,
                     TRIAXI_VALIDATE_TIMEOUT, 10, TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  fprintf(stderr, "SHANT PRINT\n"); // nolint
  sleep(10000);
}
triaxi_validate(print_verbosity_testlevel_never, test_print_to_err_e_never,
                TRIAXI_VALIDATE_UCRASHED) {
  fprintf(stderr, "SHANT PRINT\n"); // nolint
  raise(SIGABRT);
}

// ── test-level verbosity: always (implicit default suite) ─────────────────
triaxi_validate(print_always, test_pass_stdout_print_always, TRIAXI_VALIDATE_PASSED) {
  printf("SHALL PRINT\n"); // nolint
  triax_assert_true(1);
}
triaxi_validate(print_always, test_fail_stdout_print_always, TRIAXI_VALIDATE_FAILED) {
  printf("SHALL PRINT\n"); // nolint
  triax_assert_true(0);
}
triaxi_validate_opts(print_always, test_fault_stdout_print_always, TRIAXI_VALIDATE_UEXITED,
                     TRIAXI_VALIDATE_TIMEOUT_INHERIT, TRIAXI_VALIDATE_ISOLATION_ON) {
  printf("SHALL PRINT\n"); // nolint
  triax_expect_true((exit(0), 1));
}
triaxi_validate_opts(print_always, test_timeout_stdout_print_always, TRIAXI_VALIDATE_TIMEOUT, 10,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  printf("SHALL PRINT\n"); // nolint
  sleep(1000);
}
triaxi_validate_opts(print_always, test_print_to_out_e_always, TRIAXI_VALIDATE_UCRASHED,
                     TRIAXI_VALIDATE_TIMEOUT_INHERIT, TRIAXI_VALIDATE_ISOLATION_ON) {
  printf("SHALL PRINT\n"); // nolint
  raise(SIGABRT);
}
triaxi_validate(print_always, test_print_to_err_p_always, TRIAXI_VALIDATE_PASSED) {
  fprintf(stderr, "SHALL PRINT\n"); // nolint
  triax_assert_true(1);
}
triaxi_validate(print_always, test_print_to_err_f_always, TRIAXI_VALIDATE_FAILED) {
  fprintf(stderr, "SHALL PRINT\n"); // nolint
  triax_assert_true(0);
}
triaxi_validate_opts(print_always, test_print_to_err_c_always, TRIAXI_VALIDATE_UEXITED,
                     TRIAXI_VALIDATE_TIMEOUT_INHERIT, TRIAXI_VALIDATE_ISOLATION_ON) {
  fprintf(stderr, "SHALL PRINT\n"); // nolint
  triax_expect_true((exit(0), 1));
}
triaxi_validate_opts(print_always, test_print_to_err_t_always, TRIAXI_VALIDATE_TIMEOUT, 10,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  fprintf(stderr, "SHALL PRINT\n"); // nolint
  sleep(10);
}
triaxi_validate_opts(print_always, test_print_to_err_e_always, TRIAXI_VALIDATE_UCRASHED,
                     TRIAXI_VALIDATE_TIMEOUT_INHERIT, TRIAXI_VALIDATE_ISOLATION_ON) {
  fprintf(stderr, "SHALL PRINT\n"); // nolint
  raise(SIGABRT);
}

// ── always, suite-level ─────────────────────────────────────────────────────
triax_suite(print_always_suitelevel, .verbosity = TRIAX_VERBOSITY_ALWAYS);
triaxi_validate(print_always_suitelevel, test_pass_stdout_print_always_suitelevel,
                TRIAXI_VALIDATE_PASSED) {
  printf("SHALL PRINT\n"); // nolint
  triax_assert_true(1);
}
triaxi_validate(print_always_suitelevel, test_fail_stdout_print_always_suitelevel,
                TRIAXI_VALIDATE_FAILED) {
  printf("SHALL PRINT\n"); // nolint
  triax_assert_true(0);
}
triaxi_validate_opts(print_always_suitelevel, test_fault_stdout_print_always_suitelevel,
                     TRIAXI_VALIDATE_UEXITED, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  printf("SHALL PRINT\n"); // nolint
  triax_expect_true((exit(0), 1));
}
triaxi_validate_opts(print_always_suitelevel, test_timeout_stdout_print_always_suitelevel,
                     TRIAXI_VALIDATE_TIMEOUT, 10, TRIAXI_VALIDATE_ISOLATION_ON) {
  printf("SHALL PRINT\n"); // nolint
  sleep(1000);
}
triaxi_validate_opts(print_always_suitelevel, test_print_to_out_e_always_suitelevel,
                     TRIAXI_VALIDATE_UCRASHED, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  printf("SHALL PRINT\n"); // nolint
  raise(SIGABRT);
}
triaxi_validate(print_always_suitelevel, test_print_to_err_p_always_suitelevel,
                TRIAXI_VALIDATE_PASSED) {
  fprintf(stderr, "SHALL PRINT\n"); // nolint
  triax_assert_true(1);
}
triaxi_validate(print_always_suitelevel, test_print_to_err_f_always_suitelevel,
                TRIAXI_VALIDATE_FAILED) {
  fprintf(stderr, "SHALL PRINT\n"); // nolint
  triax_assert_true(0);
}
triaxi_validate_opts(print_always_suitelevel, test_print_to_err_c_always_suitelevel,
                     TRIAXI_VALIDATE_UEXITED, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  fprintf(stderr, "SHALL PRINT\n"); // nolint
  triax_expect_true((exit(0), 1));
}
triaxi_validate_opts(print_always_suitelevel, test_print_to_err_t_always_suitelevel,
                     TRIAXI_VALIDATE_TIMEOUT, 10, TRIAXI_VALIDATE_ISOLATION_ON) {
  fprintf(stderr, "SHALL PRINT\n"); // nolint
  sleep(1000);
}
triaxi_validate_opts(print_always_suitelevel, test_print_to_err_e_always_suitelevel,
                     TRIAXI_VALIDATE_UCRASHED, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  fprintf(stderr, "SHALL PRINT\n"); // nolint
  raise(SIGABRT);
}

// ── never, suite-level ──────────────────────────────────────────────────────
triax_suite(print_never_suitelevel, .verbosity = TRIAX_VERBOSITY_NEVER);
triaxi_validate(print_never_suitelevel, test_pass_stdout_noprint2_suitelevel,
                TRIAXI_VALIDATE_PASSED) {
  printf("SHANT PRINT\n"); // nolint
  triax_assert_true(1);
}
triaxi_validate(print_never_suitelevel, test_fail_stdout_print_never_suitelevel,
                TRIAXI_VALIDATE_FAILED) {
  printf("SHANT PRINT\n"); // nolint
  triax_assert_true(0);
}
triaxi_validate_opts(print_never_suitelevel, test_fault_stdout_print_never_suitelevel,
                     TRIAXI_VALIDATE_UEXITED, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  printf("SHANT PRINT\n"); // nolint
  triax_expect_true((exit(0), 1));
}
triaxi_validate_opts(print_never_suitelevel, test_timeout_stdout_print_never_suitelevel,
                     TRIAXI_VALIDATE_TIMEOUT, 10, TRIAXI_VALIDATE_ISOLATION_ON) {
  printf("SHANT PRINT\n"); // nolint
  sleep(1000);
}
triaxi_validate_opts(print_never_suitelevel, test_print_to_out_e_never_suitelevel,
                     TRIAXI_VALIDATE_UCRASHED, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  printf("SHANT PRINT\n"); // nolint
  raise(SIGABRT);
}
triaxi_validate(print_never_suitelevel, test_print_to_err_p_never_suitelevel,
                TRIAXI_VALIDATE_PASSED) {
  fprintf(stderr, "SHANT PRINT\n"); // nolint
  triax_assert_true(1);
}
triaxi_validate(print_never_suitelevel, test_print_to_err_f_never_suitelevel,
                TRIAXI_VALIDATE_FAILED) {
  fprintf(stderr, "SHANT PRINT\n"); // nolint
  triax_assert_true(0);
}
triaxi_validate_opts(print_never_suitelevel, test_print_to_err_c_never_suitelevel,
                     TRIAXI_VALIDATE_UEXITED, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  fprintf(stderr, "SHANT PRINT\n"); // nolint
  triax_expect_true((exit(0), 1));
}
triaxi_validate_opts(print_never_suitelevel, test_print_to_err_t_never_suitelevel,
                     TRIAXI_VALIDATE_TIMEOUT, 10, TRIAXI_VALIDATE_ISOLATION_ON) {
  fprintf(stderr, "SHANT PRINT\n"); // nolint
  sleep(10);
}
triaxi_validate_opts(print_never_suitelevel, test_print_to_err_e_never_suitelevel,
                     TRIAXI_VALIDATE_UCRASHED, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  fprintf(stderr, "SHANT PRINT\n"); // nolint
  raise(SIGABRT);
}
