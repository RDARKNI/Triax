// Coverage for TRIAXI_Error ("user_error"/"test_error" outcome): misuse of
// the framework API from within a test (parameter access outside a
// parameterized test, skip/assertions called from a fixture instead of the
// test body) and invalid per-test configuration (a timeout without
// isolation, an exit assertion without isolation, malformed .params). Each
// of these is reported as outcome "test_error" with a specific "reason" —
// verified via triaxi_validate_user_error(_opts/_attrs), not plain
// triaxi_validate, so a test can't pass by tripping the wrong error path.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

// Isolated: skip_in_fixture/assertion_in_fixture abort() the process (see
// triaxi_user_error_f — fixtures are noexcept in C++17+, so they can't use
// the normal longjmp/throw unwind), so this suite runs them the same way
// exits.c runs its crash tests: as their own child process, matching the
// safe/tested path rather than relying on the in-process SIGABRT handler.
triax_suite(errors, .isolation = TRIAX_ISOLATION_ON);

triaxi_validate_user_error(errors, param_access_without_params, "parameter_access") {
  // Not registered with .params — triax_param() has nothing to point at.
  (void)triax_param(int);
}

static void errors_skip_in_fixture_init(void) {
  if (triaxi_validate_is_child()) { triax_skip(); }
}
triaxi_validate_user_error_attrs(errors, skip_in_fixture, "skip_in_fixture",
                                 .init = errors_skip_in_fixture_init) {
  // Unreached: the fixture above aborts before this body ever runs.
}

static void errors_assertion_in_fixture_init(void) {
  if (triaxi_validate_is_child()) { triax_assert_true(1); }
}
triaxi_validate_user_error_attrs(errors, assertion_in_fixture, "assertion_in_fixture",
                                 .init = errors_assertion_in_fixture_init) {
  // Unreached: the fixture above aborts before this body ever runs.
}

triaxi_validate_user_error(errors, file_open_missing_path, "file_open") {
  (void)triax_read_file("/nonexistent/triax-selftest-path/does-not-exist");
}

// malformed_params is deliberately NOT covered here: it's rejected in
// triaxi_test_start before the body ever dispatches, for every invocation of
// that (suite, test) — outer meta-check included, since a test's .params is
// baked into its static registration and can't be made conditional on
// triaxi_validate_is_child() the way .init/.fini bodies above can. That
// means there's no way to keep the outer role's own dispatch of such a test
// "safe" the way triaxi_validate's if/else body-gating keeps every other
// case here safe. See tests/cli_fixture.c's fx_c::malformed_params +
// validate_cli.sh instead, where it's filtered from outside the process
// rather than gated from inside it.

// Not isolated on purpose: leaves .isolation/.timeout_ms unset at both suite
// and test level so the CLI --timeout/--isolation overrides passed by
// triaxi_validate_opts_user_error below are what actually reach the
// resolved settings triaxi_test_start checks. The `errors` suite above
// can't be reused here since its .isolation = ON would otherwise win.
triax_suite(errors_config, 0);

triaxi_validate_opts_user_error(errors_config, timeout_without_isolation,
                                "timeout_without_isolation", 500,
                                TRIAXI_VALIDATE_ISOLATION_OFF) {
  // Unreached: rejected before the body runs.
}

triaxi_validate_opts_user_error(errors_config, exit_assert_without_isolation,
                                "exit_assert_without_isolation", TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                                TRIAXI_VALIDATE_ISOLATION_OFF) {
  triax_assert_exit(0, (void)0);
}
