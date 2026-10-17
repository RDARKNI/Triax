// Message-less triax_expect/triax_assert used to expand to an empty printf
// format, which -Wformat-zero-length rejects under -Werror. CMakeLists.txt
// makes that warning fatal for this file, so it doubles as a compile check.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

triax_suite(messages, 0);

triaxi_validate(messages, check_without_message_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect(1);
  triax_assert(1);
}
triaxi_validate(messages, check_without_message_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect(0);
}
