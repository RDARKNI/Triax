// triax_{assert,expect}_arreq/arrneq. The count is part of the API contract;
// the arrays agree/disagree only beyond or within it, so the count is
// actually exercised rather than just matching the whole array anyway. Each
// test is wrapped in triaxi_validate: the outer run spawns a filtered child
// copy of this test and asserts it produces the designed outcome.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

TRIAX_NONPEDANTIC_BEGIN()

triaxi_validate(arrays, arreq_pass, TRIAXI_VALIDATE_PASSED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 4};
  triax_expect_arreq(a, b, 4);
}
triaxi_validate(arrays, arreq_fail, TRIAXI_VALIDATE_FAILED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 5};
  triax_expect_arreq(a, b, 4);
}
triaxi_validate(arrays, arrneq_pass, TRIAXI_VALIDATE_PASSED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 5};
  triax_expect_arrneq(a, b, 4);
}
triaxi_validate(arrays, arrneq_fail, TRIAXI_VALIDATE_FAILED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 4};
  triax_expect_arrneq(a, b, 4);
}

// Prefix counts: only the first N elements are compared.
triaxi_validate(arrays, arreq_prefix_pass, TRIAXI_VALIDATE_PASSED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 999}; // differ at index 3
  triax_expect_arreq(a, b, 3);                    // only checks indices 0..2
}
triaxi_validate(arrays, arreq_prefix_fail, TRIAXI_VALIDATE_FAILED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 9, 4}; // differ at index 2
  triax_expect_arreq(a, b, 3);                  // covers the mismatch
}
triaxi_validate(arrays, arrneq_prefix_pass, TRIAXI_VALIDATE_PASSED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 9, 4}; // differ at index 2
  triax_expect_arrneq(a, b, 3);
}
triaxi_validate(arrays, arrneq_prefix_fail, TRIAXI_VALIDATE_FAILED) {
  int a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 999}; // agree within first 3
  triax_expect_arrneq(a, b, 3);                   // no mismatch in that range
}

// Pointers carry no extent; the count makes them comparable directly.
triaxi_validate(arrays, arreq_pointer_fail, TRIAXI_VALIDATE_FAILED) {
  int        a[3] = {1, 2, 3}, b[3] = {1, 2, 4};
  const int *pa = a, *pb = b;
  triax_expect_arreq(pa, pb, 3);
}

TRIAX_NONPEDANTIC_END()
