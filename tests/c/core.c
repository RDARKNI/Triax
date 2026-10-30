#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_opt.h"

TRIAX_NONPEDANTIC_BEGIN()

// Exception assertions are C++-only (tests/cpp/exceptions.cpp).
#if !defined(__cplusplus) && (defined(triax_expect_throw) || defined(TRIAX_EXCEPTION_ANY))
# error "C++ exception assertions leaked into the C API"
#endif

static int add(int a, int b) { return a + b; }

triax_test(core, equality, triax_opt(skip, false)) {
  triax_assert_eq(add(2, 3), 5);
  triax_expect_true(1);
  triax_assert_streq("triax", "triax");
}

triax_test(core, captured_output, triax_opt(skip, false)) {
  fputs("hello-triax", stdout);
  Triax_Str out = triax_read_stdout();
  triax_assert_str_contains(out, "hello-triax");
}

triax_test(core, expected_exit, triax_opt(isolation, TRIAX_ISOLATION_ON)) {
  triax_assert_exit(7, exit(7));
}

triax_test(core, expected_fault, triax_opt(isolation, TRIAX_ISOLATION_ON)) {
  triax_assert_fault(TRIAX_FAULT_ABORT, abort());
}

static const int values[] = {1, 2, 3};
triax_test(core, parameterized, triax_opt_params(values)) {
  triax_assert_gt(*triax_param(int), 0);
}

TRIAX_NONPEDANTIC_END()
