#define TRIAX_MULTI_TU
#include "triax.h"

static int add(int a, int b) { return a + b; }

#ifdef __cpp_lib_string_view
triax_test(core, equality, .skip(false)) {
  triax_assert_eq(add(2, 3), 5);
  triax_expect_true(true);
  triax_assert_streq(std::string("triax"), std::string_view("triax"));
}
#endif

triax_test(core, captured_output, .skip(false)) {
  std::cout << "hello-triax";
  Triax_Str out = triax_read_stdout();
  triax_assert_str_contains(out, "hello-triax");
}

triax_test(core, expected_exit, .isolation(TRIAX_ISOLATION_ON)) {
  triax_assert_exit(7, std::exit(7));
}

triax_test(core, expected_fault, .isolation(TRIAX_ISOLATION_ON)) {
  triax_assert_fault(TRIAX_FAULT_ABORT, std::abort());
}

static const int values[] = {1, 2, 3};
triax_test(core, parameterized, .parameterize(values)) { triax_assert_gt(*triax_param(int), 0); }
