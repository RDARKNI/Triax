// See multitu_c_a.c.
#define TRIAX_MULTI_TU
#include "triax.h"

TRIAX_NONPEDANTIC_BEGIN()

triax_test(multitu, from_b_pass, 0) { triax_assert(1); }
triax_test(multitu, from_b_fail, 0) { triax_assert(0); }

TRIAX_NONPEDANTIC_END()
