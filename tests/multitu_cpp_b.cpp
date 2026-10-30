// See multitu_cpp_a.cpp.
#define TRIAX_MULTI_TU
#include "triax.h"

TRIAX_NONPEDANTIC_BEGIN()

triax_test(multitu_cpp, from_b_pass, ) { triax_assert(1); }
triax_test(multitu_cpp, from_b_fail, ) { triax_assert(0); }

TRIAX_NONPEDANTIC_END()
