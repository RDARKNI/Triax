#define TRIAX_MULTI_TU
#define TRIAX_IMPL
#include "triax.h"
#include "triax_selfverify.h"
TRIAX_NONPEDANTIC_BEGIN()
#include "../operand_order.h"

int main(int argc, char** argv) {
  triaxi_validate_init(argv[0]);
  Triax_RunConfig cfg{};
  return triax_run_main(argc, argv, cfg);
}

TRIAX_NONPEDANTIC_END()
