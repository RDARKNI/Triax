// Ported from rk_lib/tests/test_rk_test_inputs.c. Exercises assertion
// dispatch on edge-case inputs (pointer comparisons, mixed pointer/string
// types). Each test is wrapped in triaxi_validate: the outer run spawns a
// filtered child copy of this test and asserts it produces the designed
// outcome, so a deliberate failure case still reports an overall PASS in
// ctest instead of needing a separate, unverified demo binary. The original
// file's dead `#if 0` compile-error demonstration block (ENSURE_NONCOMPATIBLE)
// is intentionally not ported — it exists only to prove mismatched-type
// assertions fail to compile, which isn't something a runnable test can
// exercise.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"
#include <assert.h>

triaxi_validate_opts(crash, assert_fires, TRIAXI_VALIDATE_PASSED, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                     TRIAXI_VALIDATE_ISOLATION_OFF) {
  triax_assert_fault(TRIAX_FAULT_ANY, assert(0));
  triax_expect_true(0);
}

static void*       _ptr  = (void*)20;
static const void* _cptr = (const void*)20;

triaxi_validate(invalid_inputs, ptr_ptr_eq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_eq(_ptr, _ptr);
  triax_expect_eq(_ptr, _cptr);
  triax_expect_eq(_cptr, _ptr);
  triax_expect_eq(_cptr, _cptr);
}
triaxi_validate(invalid_inputs, ptr_ptr_neq_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_neq(_ptr, _ptr);
  triax_expect_neq(_ptr, _cptr);
  triax_expect_neq(_cptr, _ptr);
  triax_expect_neq(_cptr, _cptr);
}
triaxi_validate(invalid_inputs, ptr_ptr_gt_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_gt(_ptr, _ptr);
  triax_expect_gt(_ptr, _cptr);
  triax_expect_gt(_cptr, _ptr);
  triax_expect_gt(_cptr, _cptr);
}
triaxi_validate(invalid_inputs, ptr_ptr_geq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_geq(_ptr, _ptr);
  triax_expect_geq(_ptr, _cptr);
  triax_expect_geq(_cptr, _ptr);
  triax_expect_geq(_cptr, _cptr);
}
triaxi_validate(invalid_inputs, ptr_ptr_lt_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_lt(_ptr, _ptr);
  triax_expect_lt(_ptr, _cptr);
  triax_expect_lt(_cptr, _ptr);
  triax_expect_lt(_cptr, _cptr);
}
triaxi_validate(invalid_inputs, ptr_ptr_leq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_leq(_ptr, _ptr);
  triax_expect_leq(_ptr, _cptr);
  triax_expect_leq(_cptr, _ptr);
  triax_expect_leq(_cptr, _cptr);
}

triaxi_validate(invalid_inputs, wrong_string_comparison, TRIAXI_VALIDATE_FAILED) {
  const char* s = "Hello, this is fine!";
  void*       p = (void*)"This is garbage";
  triax_expect_eq(p, s); // pointer comparison - correct
  triax_expect_eq(s, p); // string comparison - dangerous?

  triax_expect_eq(s, s); // string comparison - dangerous
}
triaxi_validate(invalid_inputs, ptr_str, TRIAXI_VALIDATE_PASSED) {
  void* p = (void*)"hello";
  triax_expect_eq(p, p);
  char*       str  = (char*)p;
  const char* cstr = str;

  // expect pointer comparison
  triax_expect_eq(p, str);
  triax_expect_eq(str, p);

  // expect string comparison
  triax_expect_eq(str, str);
  triax_expect_eq(str, cstr);
  triax_expect_eq(cstr, str);
  triax_expect_eq(cstr, cstr);
}
triaxi_validate(invalid_inputs, signed_unsigned, TRIAXI_VALIDATE_PASSED) {
  triax_expect_gt(10ull, -1);
}
