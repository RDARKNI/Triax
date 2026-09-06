// Completes combo_fixture.c's reporters-x-outcome coverage with the one
// outcome that's C++-only: an uncaught exception escaping the test body.
// This had zero coverage anywhere in the suite before — nothing exercised
// TRIAXI_OUTCOME_UEXCEPT's reporter output (JSON/TAP/JUnit/text) at all.
#include "triax.h"

#include <stdexcept>

triax_suite(combo_reporters_cpp, .isolation(TRIAX_ISOLATION_ON));
triax_test(combo_reporters_cpp, r_uexception, .skip(false)) {
  throw std::runtime_error("combo uexception");
}

int main(int argc, char** argv) {
  Triax_RunConfig cfg{};
  return triax_run_argv(argc, argv, cfg);
}
