#include "triax.h"
#include <stdlib.h>

static int add(int a, int b) { return a + b; }

triax_test(math, addition, .skip = false) { triax_assert_eq(add(2, 3), 5); }
triax_suite(math, .isolation = TRIAX_ISOLATION_OFF);

// Parameterized: runs once per element of the array, fetched with triax_param().
struct AddCase { int a, b, expected; };
static const struct AddCase add_cases[] = {{1, 2, 3}, {2, 3, 5}, {-1, 1, 0}};

triax_test(math, addition_cases, .params = triax_as_params(add_cases)) {
  const struct AddCase* c = triax_param(struct AddCase);
  triax_assert_eq(add(c->a, c->b), c->expected);
}

// Process isolation: expected crashes and exit() calls are caught in a child
// process instead of taking down the whole run.
triax_test(process, expected_abort, .isolation = TRIAX_ISOLATION_ON) {
  triax_assert_fault(TRIAX_FAULT_ABORT, abort());
}

triax_test(process, expected_exit, .isolation = TRIAX_ISOLATION_ON) {
  triax_assert_exit(7, exit(7));
}

// Per-test timeout: an isolated test that overruns is killed and reported as
// a timeout instead of hanging the run.
triax_test(process, within_timeout, .isolation = TRIAX_ISOLATION_ON, .timeout_ms = 1000) {
  triax_assert_true(1);
}

// Captured stdout: bytes the test writes are available to assert on, even
// when the test runs in an isolated child process.
triax_test(io, capture, .isolation = TRIAX_ISOLATION_ON) {
  printf("hello from test");
  triax_assert_str_contains(triax_read_stdout(), "hello");
}

int main(int argc, char** argv) {
  Triax_RunConfig config = {0};
  return triax_run_argv(argc, argv, config);
}
