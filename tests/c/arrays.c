// New coverage: triax_{assert,expect}_arreq/arrneq and their explicit-count
// _n variants had zero test coverage before this file (they weren't part of
// the ported rk_test suite). Each test is wrapped in triaxi_validate: the
// outer run spawns a filtered child copy of this test and asserts it
// produces the designed outcome.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

triaxi_validate(arrays, arreq_pass, TRIAXI_VALIDATE_PASSED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 4};
  triax_expect_arreq(a, b);
}
triaxi_validate(arrays, arreq_fail, TRIAXI_VALIDATE_FAILED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 5};
  triax_expect_arreq(a, b);
}
triaxi_validate(arrays, arrneq_pass, TRIAXI_VALIDATE_PASSED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 5};
  triax_expect_arrneq(a, b);
}
triaxi_validate(arrays, arrneq_fail, TRIAXI_VALIDATE_FAILED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 4};
  triax_expect_arrneq(a, b);
}

// Explicit-count variants: two arrays that only agree/disagree within the
// first N elements, so the count parameter is actually exercised rather
// than just matching the whole array anyway.
triaxi_validate(arrays, arreq_n_pass, TRIAXI_VALIDATE_PASSED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 999}; // differ at index 3
  triax_expect_arreq_n(a, b, 3);                  // only checks indices 0..2
}
triaxi_validate(arrays, arreq_n_fail, TRIAXI_VALIDATE_FAILED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 9, 4}; // differ at index 2
  triax_expect_arreq_n(a, b, 3);                // covers the mismatch
}
triaxi_validate(arrays, arrneq_n_pass, TRIAXI_VALIDATE_PASSED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 9, 4}; // differ at index 2
  triax_expect_arrneq_n(a, b, 3);
}
triaxi_validate(arrays, arrneq_n_fail, TRIAXI_VALIDATE_FAILED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 999}; // agree within first 3
  triax_expect_arrneq_n(a, b, 3);                 // no mismatch in that range
}
