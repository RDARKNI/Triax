#include "triax.h"
#include <stdint.h>
#include <stdlib.h>
TRIAX_NONPEDANTIC_BEGIN() // optional without the -pedantic flag or on newer C versions

triax_test(demo_tests, assert_vs_expect, ) {
  triax_expect_eq(1, 0); // fails, test continues
  triax_assert_eq(1, 0); // fails, test ends
  triax_expect_eq(1, 1); // does not run
}

static int add(int a, int b) { return a + b; }
triax_test(demo_tests, addition, ) { triax_assert_eq(add(2, 3), 5); }

triax_test(demo_tests, type_safety) {
  triax_expect_eq(add(1, 1), 2);   // int vs int
  triax_expect_eq(UINT32_MAX, -1); // unsigned vs. signed - handled correctly (fails)
  triax_expect_eq(1.0, 1.0);       // float vs float - gets promoted to long double
  // triax_expect_eq(1.0, 1); // float vs integer - compiler error to prevent precision loss
  triax_expect_neq("hello", "world"); // c-string vs c-string - compiles
  Triax_Str hello_str = {"hello", sizeof("hello") - 1};
  triax_expect_neq("hello", "world");   // c-string vs c-string - works
  triax_expect_neq(hello_str, "world"); // Triax_Str vs c-string - works

  // String vs integer comparison, fails to compile
  // triax_expect_eq("hello", 1);
  int  a, *ap = &a;
  long b, *bp = &b;
  triax_expect_neq(ap, bp);      // pointer comparisons work
  triax_expect_neq(ap, "hello"); // pointer comparisons compile
}

triax_test(demo_tests, array_comparisons, .verbosity = TRIAX_VERBOSITY_ALWAYS) {
  int arr1[3] = {1, 2, 3}, arr2[3] = {1, 2, 4};
  triax_expect_arrneq(arr1, arr2, 3);
  triax_expect_arreq(arr1, arr2, 3);
}

triax_test(demo_strings, assertion_types) {
  const char* teststr = "hello test\n";
  triax_expect_str_startswith(teststr, "hello");
  triax_expect_str_endswith(teststr, "test\n");
  triax_expect_str_contains(teststr, "test");
  Triax_Str testStr = triax_str(teststr, strlen(teststr));
  triax_expect_str_startswith(testStr, "hello");
  triax_expect_str_endswith(testStr, "test\n");
  triax_expect_str_contains(testStr, teststr);
}

triax_test(demo_strings, file_captures) {
  printf("hello from stdout\n");
  fprintf(stderr, "hello from stderr\n");
  // stdout and stderr may be captured as Triax_Str objects
  Triax_Str std_out = triax_read_stdout(), std_err = triax_read_stderr();
  triax_expect_neq(std_out, std_err);
  printf("bye from stdout");
  Triax_Str std_out2 = triax_read_stdout();
  triax_expect_neq(std_out, std_out2); // triax_read_stdout returns snapshot
  // Triax_Str std_file = triax_read_file(); // arbitrary files may be read
}

// timeout to prevent slow or infinite tests
triax_test(demo_tests, timeout_demo, .timeout_ms = 100) {
  volatile int x = 0;
  for (;;) {
    if (x) { return; }
  }
}

// Process isolation: expected crashes and exit() calls are caught in a child process instead of
// taking down the whole run.
triax_test(demo_tests, crash_test, .isolation = TRIAX_ISOLATION_ON) {
  triax_expect_fault(TRIAX_FAULT_MEMORY, volatile int* x = 0; printf("%d\n", *x););
}

// Parameterized: runs once per element of the array, fetched with triax_param().
static const struct Cases {
  int input;
  int expected;
} cases[] = {{-3, 3}, {0, 0}, {5, 5}, {0, 5}};
triax_test(demo_tests, absolute_value, .params = triax_as_params(cases)) {
  const struct Cases* c = triax_param(struct Cases);
  triax_assert_eq(abs(c->input), c->expected);
}
triax_suite(demo_tests, .tags = "all");

int main(int argc, char** argv) {
  Triax_RunConfig config = {0};
  return triax_run_main(argc, argv, config);
}

TRIAX_NONPEDANTIC_END()
