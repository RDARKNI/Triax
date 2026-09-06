// Multi-TU x C++ counterpart of multitu_c_a.c/multitu_c_b.c — see that file
// for why this combination (one suite's tests spread across TUs) has its
// own dedicated smoke test.
#define TRIAX_MULTI_TU
#define TRIAX_IMPL
#include "triax.h"

triax_test(multitu_cpp, from_a_pass, .skip(false)) { triax_assert_true(1); }

int main(int argc, char** argv) {
  Triax_RunConfig cfg{};
  return triax_run_argv(argc, argv, cfg);
}
