#include "triax.h"

static int add(int a, int b) { return a + b; }

triax_test(math, addition, .skip = false) { triax_assert_eq(add(2, 3), 5); }
triax_suite(math);
int main(int argc, char** argv) {
  Triax_RunConfig config = {0};
  return triax_run_argv(argc, argv, config);
}
