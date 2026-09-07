#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

#ifdef __cpp_lib_string_view
static int add(int a, int b) { return a + b; }

triax_test(core, equality, .skip(false)) {
  triax_assert_eq(add(2, 3), 5);
  triax_expect_true(true);
  triax_assert_streq(std::string("triax"), std::string_view("triax"));
}
#endif

triax_test(core, captured_output, .skip(false)) {
  std::cout << "hello-triax";
  Triax_Str out = triax_read_stdout();
  triax_assert_str_contains(out, "hello-triax");
}

triax_test(core, expected_exit, .isolation(TRIAX_ISOLATION_ON)) {
  triax_assert_exit(7, std::exit(7));
}

triax_test(core, expected_fault, .isolation(TRIAX_ISOLATION_ON)) {
  triax_assert_fault(TRIAX_FAULT_ABORT, std::abort());
}

static const int values[] = {1, 2, 3};
triax_test(core, parameterized, .parameterize(values)) { triax_assert_gt(*triax_param(int), 0); }

// Isolates one specific hypothesis for the Windows crash under investigation:
// does a failing assertion's throw -> catch -> longjmp path (see
// triaxi_run_func's C++ wrapper, triaxfwrapped_*) crash when that longjmp,
// called from inside an already-entered catch block, targets a setjmp that
// is itself the controlling expression of an enclosing MSVC __try scope
// (triaxi_win_try)? Deliberately minimal — no capture, no fixtures, no
// parameters — so a crash here can only be attributed to that specific
// control-flow mixing, not to anything else under investigation (e.g. the
// separate, already-confirmed Windows capture-read bug that core::
// captured_output above exercises).
//
// Wrapped in triaxi_validate (not a bare triax_test) for the same reason
// every deliberately-failing test elsewhere in this suite is: the outer
// role spawns a filtered child and checks *it* produced a normal "failed"
// outcome, so ctest itself stays green if this hypothesis is false. If the
// hypothesis is true, the child crashes instead of reporting "failed" — the
// outer role's own checks (expected exit code, valid JSON) then fail, which
// is exactly the signal we want: this test going red in CI confirms the
// crash, without a human needing to read a segfault message to prove it.
triaxi_validate(debug, deliberate_failure, TRIAXI_VALIDATE_FAILED) {
  triax_assert_true(false);
}
