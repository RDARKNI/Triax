// Ported from the C++-only branch of rk_lib/tests/test_rk_test_inputs.c
// (the `#ifdef __cplusplus` section of test_ptr_str). Exercises assertion
// dispatch across std::string / std::string_view / const char*. Wrapped in
// triaxi_validate for consistency with the rest of the ported suite and as a
// regression guard, even though this particular test is expected to pass.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"
#include <string>

triaxi_validate(cpp_inputs, string_comparisons, TRIAXI_VALIDATE_PASSED) {
  const char* str  = "hello";
  const char* cstr = "hello";

  std::string cppstr = "hello";
  triax_expect_eq(cppstr, cppstr);
  triax_expect_eq(cppstr, str);
  triax_expect_eq(str, cppstr);
  triax_expect_eq(cppstr, cstr);
  triax_expect_eq(cstr, cppstr);

#if __cplusplus >= 201703L
  std::string_view cppstrv = "hello";
  triax_expect_eq(cppstrv, str);
  triax_expect_eq(str, cppstrv);
  triax_expect_eq(cppstrv, cstr);
  triax_expect_eq(cstr, cppstrv);
  triax_expect_eq(cppstrv, cppstrv);
  triax_expect_eq(cppstr, cppstrv);
  triax_expect_eq(cppstrv, cppstr);
#endif
}
