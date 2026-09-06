// Ported from rk_lib/tests/test_rk_test.c (TEST_EXITS, TEST_TIMEOUT). Each
// test is wrapped in triaxi_validate/triaxi_validate_opts: the outer run
// spawns a filtered child copy of this test and asserts it produces the
// designed outcome (failed/crashed/timed out/etc.), so tests that are
// deliberately designed to fail, crash, or time out still report an overall
// PASS in ctest instead of needing a separate, unverified demo binary.
// Per-test .timeout_ms is passed via triaxi_validate_opts as a CLI override
// applied only to the child invocation — see triax_selfverify.h's comment on
// why it can't just be attached to the registered test's own attrs (that
// would also bound the outer meta-check's own fork+exec+wait+read work).
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

triax_suite(exits, .isolation = TRIAX_ISOLATION_ON);

triaxi_validate(exits, skip, TRIAXI_VALIDATE_SKIPPED) { triax_skip(); }
triaxi_validate(exits, pass, TRIAXI_VALIDATE_PASSED) { triax_assert_true(1); }
triaxi_validate(exits, fail, TRIAXI_VALIDATE_FAILED) { triax_assert_true(0); }
triaxi_validate_opts(exits, timeout, TRIAXI_VALIDATE_TIMEOUT, 1, TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  sleep(100);
}
triaxi_validate(exits, uexit, TRIAXI_VALIDATE_UEXITED) { triax_assert_true((exit(1), 1)); }
triaxi_validate(exits, ufault, TRIAXI_VALIDATE_UCRASHED) { triax_assert_true(raise(SIGABRT)); }
// Distinct from uexit/ufault above: those crash/exit from inside an
// assertion's own expression (state still IN_ASSERT); these do it from plain
// body code, with no assertion in flight at all (state == TEST). Both are
// classified the same way (ucrashed/uexited) — this covers the
// TRIAXI_STATE_TEST branch of triaxi_test_interpret independently of the
// TRIAXI_STATE_IN_ASSERT one. Previously misclassified as "test_error"
// (hence names ending in _testerr, now stale) until that branch was fixed to
// key off exit.type instead of lumping every non-timeout exit together.
triaxi_validate(exits, crash_outside_assert, TRIAXI_VALIDATE_UCRASHED) { raise(SIGABRT); }
triaxi_validate(exits, exit_outside_assert, TRIAXI_VALIDATE_UEXITED) { exit(0); }
triaxi_validate(exits, exit_pass, TRIAXI_VALIDATE_PASSED) { triax_assert_exit(5, exit(5)); }
triaxi_validate(exits, exit_noexit_fail, TRIAXI_VALIDATE_FAILED) { triax_assert_exit(0, (void)0); }
triaxi_validate(exits, exit_wrongcode_fail, TRIAXI_VALIDATE_FAILED) {
  triax_assert_exit(5, exit(0));
}
triaxi_validate(exits, exit_ucrash, TRIAXI_VALIDATE_UCRASHED) { triax_assert_exit(5, raise(SIGABRT)); }
triaxi_validate(exits, crash_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_fault(TRIAX_FAULT_ABORT, raise(SIGABRT));
}
triaxi_validate(exits, crash_any_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_fault(TRIAX_FAULT_ANY, raise(SIGABRT));
}
triaxi_validate(exits, crash_nocrash_fail, TRIAXI_VALIDATE_FAILED) {
  triax_assert_fault(TRIAX_FAULT_ABORT, (void)0);
}
triaxi_validate(exits, crash_wrongcode_fail, TRIAXI_VALIDATE_FAILED) {
  triax_assert_fault(TRIAX_FAULT_MEMORY, raise(SIGABRT));
}

// New coverage: the negated (n-prefixed) exit/fault assertions — "assert
// this does NOT exit/crash at all" — had zero test coverage before this;
// only their non-negated assert_exit/assert_fault counterparts were tested.
triaxi_validate(exits, nfault_pass, TRIAXI_VALIDATE_PASSED) { triax_assert_nfault((void)0); }
triaxi_validate(exits, nfault_fail, TRIAXI_VALIDATE_FAILED) { triax_assert_nfault(raise(SIGABRT)); }
triaxi_validate(exits, nexit_pass, TRIAXI_VALIDATE_PASSED) { triax_assert_nexit((void)0); }
triaxi_validate(exits, nexit_fail, TRIAXI_VALIDATE_FAILED) { triax_assert_nexit(exit(0)); }
triaxi_validate(exits, crash_uexit, TRIAXI_VALIDATE_UEXITED) {
  triax_assert_fault(TRIAX_FAULT_ABORT, exit(SIGABRT));
}
// 200ms rather than the original 10ms: this is meant to prove "completing
// well within budget passes cleanly", which a razor-thin margin undermines
// under slower/instrumented builds (fork+exec+re-init overhead is much
// higher under ASan+UBSan) — that's a real flake this project's own CI hit,
// not a hypothetical.
triaxi_validate_opts(exits, timeout_pass, TRIAXI_VALIDATE_PASSED, 200,
                     TRIAXI_VALIDATE_ISOLATION_INHERIT) {}
triaxi_validate_opts(exits, timeout_fail, TRIAXI_VALIDATE_FAILED, 100,
                     TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  triax_assert_true(0);
}
triaxi_validate_opts(exits, timeout_timeout, TRIAXI_VALIDATE_TIMEOUT, 1,
                     TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  sleep(10);
}

static inline int spin_forever(void) {
  for (;;) { printf(""); }
}
triax_suite(timeout, .isolation = TRIAX_ISOLATION_ON);
triaxi_validate_opts(timeout, timeout_in_assert, TRIAXI_VALIDATE_TIMEOUT, 10,
                     TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  triax_expect_true(spin_forever());
}
triaxi_validate_opts(timeout, timeout_before_assert, TRIAXI_VALIDATE_TIMEOUT, 10,
                     TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  spin_forever();
}
triaxi_validate_opts(timeout, timeout_after_assert, TRIAXI_VALIDATE_TIMEOUT, 10,
                     TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  spin_forever();
  triax_expect_true(1);
}
triaxi_validate_opts(timeout, timeout_before_after_assert, TRIAXI_VALIDATE_TIMEOUT, 10,
                     TRIAXI_VALIDATE_ISOLATION_INHERIT) {
  triax_expect_true(1);
  spin_forever();
  triax_expect_true(1);
}
