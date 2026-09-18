#include "../include/triax.h"

// Same tests as basic.c, using C++'s fluent builder syntax instead of
// designated initialisers.

static int add(int a, int b) { return a + b; }

triax_test(math, addition, .skip(0)) { triax_assert_eq(add(2, 3), 5); }
triax_suite(math, .isolation(TRIAX_ISOLATION_OFF));

struct AddCase { int a, b, expected; };
static const AddCase add_cases[] = {{1, 2, 3}, {2, 3, 5}, {-1, 1, 0}};

triax_test(math, addition_cases, .parameterize(add_cases)) {
  const AddCase* c = triax_param(AddCase);
  triax_assert_eq(add(c->a, c->b), c->expected);
}

triax_test(process, expected_abort, .isolation(TRIAX_ISOLATION_ON)) {
  triax_assert_fault(TRIAX_FAULT_ABORT, abort());
}

triax_test(process, expected_exit, .isolation(TRIAX_ISOLATION_ON)) {
  triax_assert_exit(7, exit(7));
}

triax_test(process, within_timeout, .isolation(TRIAX_ISOLATION_ON).timeout_ms(1000)) {
  triax_assert_true(true);
}

triax_test(io, capture, .isolation(TRIAX_ISOLATION_ON)) {
  printf("hello from test");
  triax_assert_str_contains(triax_read_stdout(), "hello");
}

int main(int argc, char** argv) {
  Triax_RunConfig config = {};
  return triax_run_argv(argc, argv, config);
}
