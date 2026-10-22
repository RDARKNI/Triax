#include <triax.h>
triax_test(demo_suite, eq_test, .skip = 0) {
  triax_expect_eq(1, 1);
  int arrays[4][3] = {0};
  int expected[3]  = {0};
  int i            = 0;

  triax_expect_arreq(arrays[i++], expected);
}
TRIAX_MAIN() // default main
