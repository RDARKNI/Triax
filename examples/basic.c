#include <triax.h>
triax_test(demo_suite, eq_test, .skip = 0) { triax_expect_eq(1, 1); }
TRIAX_MAIN() // default main
