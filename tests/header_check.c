// Compiled with the full strict warning set as errors (see CMakeLists.txt).
// Uses the standard-conforming forms (explicit, possibly empty, option
// arguments and a "" message argument), plus the short forms inside
// TRIAX_NONPEDANTIC_BEGIN()/END().
#include "triax.h"
#include "triax_opt.h"

#include <stdlib.h>

triax_suite(header_check, );

triax_test(header_check, compliant_forms, ) {
  int values[2] = {1, 2}, others[2] = {1, 3};
  triax_expect(values[0] == 1, "");
  triax_assert(values[1] == 2, "");
  triax_expect_eq(values[0], 1, "");
  triax_expect_arreq(values, values, 2, "");
  triax_expect_arrneq(values, others, 2, "");
  triax_expect_streq("triax", "triax", "");
  triax_expect_eq(values[1], 2, "with a %s", "message");
}

// Exit/fault assertions format their operands in the caller's code; the
// formatting is compiled whether or not it runs, so only passing forms are used.
triax_test(header_check, exit_assertions, triax_opt(isolation, TRIAX_ISOLATION_ON)) {
  triax_expect_nexit((void)0);
  triax_expect_exit(3, exit(3));
}
triax_test(header_check, fault_assertions, triax_opt(isolation, TRIAX_ISOLATION_ON)) {
  triax_expect_nfault((void)0);
  triax_expect_fault(TRIAX_FAULT_ABORT, abort());
}

// The non-conforming short forms, made warning-free by the silencing macros. GCC before 15
// reports the C case without a warning option, so no pragma can suppress it there.
#if defined(__cplusplus) || !defined(__GNUC__) || defined(__clang__) || __GNUC__ >= 15
TRIAX_NONPEDANTIC_BEGIN()
triax_test(header_check, silenced_short_forms) {
  triax_expect_eq(1, 1);
  triax_expect(1);
}
TRIAX_NONPEDANTIC_END()
#endif

TRIAX_MAIN()
