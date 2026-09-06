// A small, stable-roster binary dedicated to exercising CLI-level behavior
// (--help, --list, --tags, --fail-fast, and argument-error handling) from
// validate_cli.sh. Deliberately separate from triax-selftest-{c,cpp}: those
// binaries' test rosters are expected to keep growing, which would make
// exact fail-fast-ordering and --list-output assertions fragile and
// unrelated to whatever else changed there. This fixture's tests are never
// meant to be individually meaningful — only their count, order, tags, and
// pass/fail shape matter.
#include "triax.h"

// fx_a: two tests, one tagged "alpha" at the suite level, the other adds its
// own "beta" tag on top (used to check tag matching considers both suite-
// and test-level tags, not just one).
triax_suite(fx_a, .tags = "alpha");
triax_test(fx_a, one, 0) { triax_assert_true(1); }
triax_test(fx_a, two, .tags = "beta") { triax_assert_true(1); }

// fx_b: untagged, registered after fx_a. "first" fails; "second" would pass
// if it ran — used to prove --fail-fast stops launching further tests after
// the first non-pass outcome.
triax_suite(fx_b, 0);
triax_test(fx_b, first, 0) { triax_assert_true(0); }
triax_test(fx_b, second, 0) { triax_assert_true(1); }

// fx_c: a single test with malformed .params (non-null ptr, zero element
// size — a well-formed TRIAXI_Params from triax_as_params() never produces
// this combination). Rejected in triaxi_test_start before the body ever
// runs, for every invocation of this (suite, test) with no way to gate that
// dispatch from inside the process — see tests/c/errors.c's comment on why
// this can't live in the triaxi_validate-wrapped suite there. Verified only
// via an explicitly filtered invocation in validate_cli.sh, never run
// unfiltered, since this binary's own default run isn't itself a ctest gate.
static const int fx_c_malformed_params_arr[] = {1, 2, 3};
triax_suite(fx_c, 0);
triax_test(fx_c, malformed_params, .params = {0, 3, fx_c_malformed_params_arr}) {
  triax_assert_true(1); // unreached
}

int main(int argc, char** argv) {
  Triax_RunConfig cfg = {0};
  return triax_run_argv(argc, argv, cfg);
}
