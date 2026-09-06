#define TRIAX_MULTI_TU
#define TRIAX_IMPL
#include "triax.h"
#include "triax_selfverify.h"

int main(int argc, char** argv) {
  triaxi_validate_init(argv[0]);
  Triax_RunConfig cfg = {0};
  return triax_run_argv(argc, argv, cfg);
}
