// Multi-TU x C: the rest of this project's self-tests use TRIAX_MULTI_TU
// only for sharing triax_selfverify.h's own static across files — no
// existing test actually spreads a single SUITE's tests across more than
// one translation unit, which is the scenario TRIAX_MULTI_TU exists for.
// This pair of files (with multitu_c_b.c) registers the "multitu" suite's
// tests from two different TUs and checks the runner aggregates them
// correctly (registration merge, not just "it links").
#define TRIAX_MULTI_TU
#define TRIAX_IMPL
#include "triax.h"

triax_test(multitu, from_a_pass, 0) { triax_assert_true(1); }

int main(int argc, char** argv) {
  Triax_RunConfig cfg = {0};
  return triax_run_argv(argc, argv, cfg);
}
