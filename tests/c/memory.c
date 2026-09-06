// Ported from rk_lib/tests/test_rk_test.c (TEST_MEMORY). Each test is
// wrapped in triaxi_validate: the outer run spawns a filtered child copy of
// this test and asserts it produces the designed outcome, so a deliberate
// "_fail" case still reports an overall PASS in ctest instead of needing a
// separate, unverified demo binary.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

triax_suite(memory, .verbosity = TRIAX_VERBOSITY_ALWAYS);

triaxi_validate(memory, memeq_pass, TRIAXI_VALIDATE_PASSED) {
  char a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 4};
  triax_expect_memeq(a, b, 4);
  triax_expect_memeq(a, b, 0); // zero length always equal
}
triaxi_validate(memory, memeq_fail, TRIAXI_VALIDATE_FAILED) {
  char a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 5};
  triax_expect_memeq(a, b, 4);
}
triaxi_validate(memory, memneq_pass, TRIAXI_VALIDATE_PASSED) {
  char a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 5};
  triax_expect_memneq(a, b, 4);
}
triaxi_validate(memory, memneq_fail, TRIAXI_VALIDATE_FAILED) {
  char a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 4};
  triax_expect_memneq(a, b, 4);
}
triaxi_validate(memory, memzero_pass, TRIAXI_VALIDATE_PASSED) {
  char a[4] = {0};
  triax_expect_memzero(a, 4);
  triax_expect_memzero(a, 0);
}
triaxi_validate(memory, memzero_fail, TRIAXI_VALIDATE_FAILED) {
  char a[4] = {0, 0, 0, 1};
  triax_expect_memzero(a, 4);
}
triaxi_validate(memory, memnzero_pass, TRIAXI_VALIDATE_PASSED) {
  char a[4] = {0, 0, 0, 1};
  triax_expect_memnzero(a, 4);
}
triaxi_validate(memory, memnzero_fail, TRIAXI_VALIDATE_FAILED) {
  char a[4] = {0};
  triax_expect_memnzero(a, 4);
}
