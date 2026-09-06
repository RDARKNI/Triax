// See multitu_c_a.c.
#define TRIAX_MULTI_TU
#include "triax.h"

triax_test(multitu, from_b_pass, 0) { triax_assert_true(1); }
triax_test(multitu, from_b_fail, 0) { triax_assert_true(0); }
