// Completes combo_fixture.c's reporters-x-outcome coverage with the one
// outcome that's C++-only: an uncaught exception escaping the test body.
// This had zero coverage anywhere in the suite before — nothing exercised
// TRIAXI_OUTCOME_UEXCEPT's reporter output (JSON/TAP/JUnit/text) at all.
#include "triax.h"

#include <stdexcept>

TRIAX_NONPEDANTIC_BEGIN()

triax_suite(combo_reporters_cpp, .isolation(TRIAX_ISOLATION_ON));
triax_test(combo_reporters_cpp, r_uexception, .skip(false)) {
  throw std::runtime_error("combo uexception");
}

// Exception interactions with assertions and the non-isolated crash handlers.
struct ThrowingEq {
  int  v;
  bool operator==(const ThrowingEq&) const { throw std::runtime_error("operator== threw"); }
  bool operator!=(const ThrowingEq& o) const { return !(*this == o); }
};

triax_suite(combo_exceptions_cpp, .skip(false));
// An exception escaping a comparison used to report uexception while still
// counting the interrupted assertion as passed.
triax_test(combo_exceptions_cpp, throwing_compare, .isolation(TRIAX_ISOLATION_ON)) {
  triax_expect_eq(ThrowingEq{1}, ThrowingEq{1});
}
triax_test(combo_exceptions_cpp, throwing_compare_noniso, .isolation(TRIAX_ISOLATION_OFF)) {
  triax_expect_eq(ThrowingEq{1}, ThrowingEq{1});
}
// MinGW's vectored crash handler used to see first-chance C++ throws and
// longjmp out as a crash before any catch block ran. Only exercised on
// MinGW; elsewhere it's a plain catch.
triax_test(combo_exceptions_cpp, caught_noniso, .isolation(TRIAX_ISOLATION_OFF)) {
  bool caught = false;
  try {
    throw std::runtime_error("caught");
  } catch (const std::runtime_error&) {
    caught = true;
  }
  triax_assert(caught);
}

int main(int argc, char** argv) {
  Triax_RunConfig cfg{};
  return triax_run_main(argc, argv, cfg);
}

TRIAX_NONPEDANTIC_END()
