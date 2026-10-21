#include <triax.h>
triax_test(demo_suite, eq_test, .skip = false) { triax_expect_eq(1, 1); }
TRIAX_MAIN() // default main
