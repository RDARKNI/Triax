// Pre-release combination coverage: individual features (isolation, jobs,
// timeouts, parameters, capture, crashes, fixtures, --debug, each reporter)
// all have their own tests elsewhere, but a bug specific to their
// INTERACTION — e.g. a timeout in one parallel slot blocking a sibling, or
// two concurrent tests' captured stdout getting cross-attributed — wouldn't
// show up in any of those single-axis tests. Each suite below is scoped to
// exactly one combination and is driven from outside the process by
// validate_combinations.sh (which needs to pass --jobs=/--debug/etc. as CLI
// overrides — something triaxi_validate's in-process meta-check can't do —
// so, unlike most of the other test files here, these are plain triax_test
// registrations with no pass/fail meta-wrapping of their own).
#include "triax.h"

#include <signal.h>
#include <time.h>

static inline void combo_sleep_ms(long ms) {
  struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
  nanosleep(&ts, NULL);
}

// ── isolation x jobs: a fixed mix of outcomes, all isolated, run under
// --jobs=N>1 so several land in the same concurrent batch. ──────────────────
triax_suite(combo_isojobs, .isolation = TRIAX_ISOLATION_ON);
triax_test(combo_isojobs, pass1, 0) { triax_assert_true(1); }
triax_test(combo_isojobs, fail1, 0) { triax_assert_true(0); }
triax_test(combo_isojobs, crash1, 0) { raise(SIGABRT); }
triax_test(combo_isojobs, pass2, 0) { triax_assert_true(1); }
triax_test(combo_isojobs, fail2, 0) { triax_assert_true(0); }
triax_test(combo_isojobs, pass3, 0) { triax_assert_true(1); }
triax_test(combo_isojobs, crash2, 0) { raise(SIGFPE); }
triax_test(combo_isojobs, pass4, 0) { triax_assert_true(1); }

// ── timeout x jobs: one slow test with a short timeout alongside several
// measurably-slow-but-passing tests, all under --jobs=N. If a timeout ever
// blocked its job slot instead of being reaped concurrently with the rest,
// total wall time would grow with N instead of staying near the timeout. ───
triax_suite(combo_timeoutjobs, .isolation = TRIAX_ISOLATION_ON);
triax_test(combo_timeoutjobs, slow_timeout, .timeout_ms = 300) { sleep(100); }
triax_test(combo_timeoutjobs, fast1, 0) { combo_sleep_ms(200); triax_assert_true(1); }
triax_test(combo_timeoutjobs, fast2, 0) { combo_sleep_ms(200); triax_assert_true(1); }
triax_test(combo_timeoutjobs, fast3, 0) { combo_sleep_ms(200); triax_assert_true(1); }

// ── parameters x jobs: one parameterized test, several elements, run under
// --jobs=N so invocations execute concurrently across slots — verifies each
// invocation's own parameter/outcome stays attached to it, not swapped with
// a concurrently-running sibling invocation. ────────────────────────────────
static const int combo_param_values[] = {0, 1, 2, 3, 4, 5, 6, 7};
triax_suite(combo_paramjobs, .isolation = TRIAX_ISOLATION_ON);
triax_test(combo_paramjobs, evens_pass, .params = triax_as_params(combo_param_values)) {
  int v = *triax_param(int);
  triax_assert_eq(v % 2, 0); // even values pass, odd values fail
}

// ── capture x jobs: several concurrently-running tests each print a marker
// unique to themselves. Verifies per-slot capture files aren't shared/
// cross-contaminated when multiple tests run at once. ───────────────────────
triax_suite(combo_capturejobs, .isolation = TRIAX_ISOLATION_ON,
            .verbosity = TRIAX_VERBOSITY_ALWAYS);
triax_test(combo_capturejobs, out1, 0) {
  printf("MARKER-OUT-1");
  fprintf(stderr, "MARKER-ERR-1");
  triax_assert_true(1);
}
triax_test(combo_capturejobs, out2, 0) {
  printf("MARKER-OUT-2");
  fprintf(stderr, "MARKER-ERR-2");
  triax_assert_true(1);
}
triax_test(combo_capturejobs, out3, 0) {
  printf("MARKER-OUT-3");
  fprintf(stderr, "MARKER-ERR-3");
  triax_assert_true(1);
}
triax_test(combo_capturejobs, out4, 0) {
  printf("MARKER-OUT-4");
  fprintf(stderr, "MARKER-ERR-4");
  triax_assert_true(1);
}

// ── crashes x jobs: several different crash types running concurrently —
// verifies each crashing slot's own signal/reason isn't cross-attributed to
// a sibling crashing at close to the same time. ─────────────────────────────
triax_suite(combo_crashjobs, .isolation = TRIAX_ISOLATION_ON);
triax_test(combo_crashjobs, c_abort, 0) { raise(SIGABRT); }
triax_test(combo_crashjobs, c_fpe, 0) { raise(SIGFPE); }
triax_test(combo_crashjobs, c_segv, 0) { raise(SIGSEGV); }
triax_test(combo_crashjobs, c_pass, 0) { triax_assert_true(1); }
triax_test(combo_crashjobs, c_fail, 0) { triax_assert_true(0); }

// ── fixtures x crashes/errors: a hardware crash (not a triaxi_user_error) in
// .init/.fini, plus a sibling registered afterward in the same suite to
// prove a fixture crash doesn't corrupt/hang the suite for later tests. ────
static void combo_fixture_init_crashes(void) { raise(SIGABRT); }
static void combo_fixture_fini_crashes(void) { raise(SIGABRT); }
triax_suite(combo_fixture_crash, .isolation = TRIAX_ISOLATION_ON);
triax_test(combo_fixture_crash, crash_in_init, .init = combo_fixture_init_crashes) {
  triax_assert_true(1); // unreached
}
triax_test(combo_fixture_crash, crash_in_fini, .fini = combo_fixture_fini_crashes) {
  triax_assert_true(1); // this runs and passes; the crash happens after, in fini
}
triax_test(combo_fixture_crash, sibling_runs_fine, 0) { triax_assert_true(1); }

// Same idea, but timeout instead of a hardware crash: a fixture-level
// classified cell (triaxi_test_interpret's state_hdr SETUP/INIT/FINI/
// CLEANUP branch already special-cases exit.type == TIMEOUT) that had no
// test at all before this — every existing timeout test times out from
// plain body code or mid-assertion, never from a hanging fixture.
static void combo_fixture_init_hangs(void) { sleep(100); }
static void combo_fixture_fini_hangs(void) { sleep(100); }
triax_test(combo_fixture_crash, timeout_in_init, .init = combo_fixture_init_hangs,
           .timeout_ms = 300) {
  triax_assert_true(1); // unreached
}
triax_test(combo_fixture_crash, timeout_in_fini, .fini = combo_fixture_fini_hangs,
           .timeout_ms = 300) {
  triax_assert_true(1); // this runs and passes; the timeout occurs after, in fini
}

// ── debug x assertion types: left at INHERIT (no suite-level isolation
// override) so --debug's forced isolation=off actually reaches these tests
// instead of being overridden by an explicit suite/test attribute. ─────────
triax_suite(combo_debug, 0);
triax_test(combo_debug, plain_pass, 0) { triax_assert_true(1); }
triax_test(combo_debug, plain_fail, 0) { triax_assert_true(0); }
triax_test(combo_debug, fault_pass, 0) { triax_assert_fault(TRIAX_FAULT_ABORT, raise(SIGABRT)); }
triax_test(combo_debug, nfault_pass, 0) { triax_assert_nfault((void)0); }
triax_test(combo_debug, exit_needs_isolation, 0) { triax_assert_exit(0, exit(0)); }

// ── all reporters x every outcome (7 of 8 here; uexception is C++-only, see
// combo_fixture.cpp's combo_reporters_cpp) — one test per outcome, run
// together so a single json/tap/junit/text invocation can be checked against
// all of them at once. ───────────────────────────────────────────────────────
triax_suite(combo_reporters, .isolation = TRIAX_ISOLATION_ON);
triax_test(combo_reporters, r_passed, 0) { triax_assert_true(1); }
triax_test(combo_reporters, r_failed, 0) { triax_assert_true(0); }
triax_test(combo_reporters, r_skipped, 0) { triax_skip(); }
triax_test(combo_reporters, r_timeout, .timeout_ms = 50) { sleep(100); }
triax_test(combo_reporters, r_ucrashed, 0) { raise(SIGABRT); }
triax_test(combo_reporters, r_uexited, 0) { exit(3); }
triax_test(combo_reporters, r_test_error, 0) { (void)triax_param(int); /* not parameterized */ }

int main(int argc, char** argv) {
  Triax_RunConfig cfg = {0};
  return triax_run_argv(argc, argv, cfg);
}
