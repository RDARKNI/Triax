// See multitu_cpp_a.cpp.
#define TRIAX_MULTI_TU
#include "triax.h"

TRIAX_NONPEDANTIC_BEGIN()

triax_test(multitu_cpp, from_b_pass, .skip(false)) { triax_assert(1); }
triax_test(multitu_cpp, from_b_fail, .skip(false)) { triax_assert(0); }

TRIAX_NONPEDANTIC_END()
