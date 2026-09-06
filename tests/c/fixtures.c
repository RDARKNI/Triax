// Ported from the pre-triax self-test suite (rk_lib/tests/test_rk_test.c).
// Kept out of the ctest-gated binary: these tests rely on cross-suite
// execution order (test-level and suite-level fixture lifecycle checks
// interleaved with unrelated suites), which is a stronger ordering
// assumption than the framework guarantees under parallel execution.
#define TRIAX_MULTI_TU
#include "triax.h"

static int*        test_level_mem;
static size_t      test_level_len;
static inline void fixture_test_level_init(void) {
  test_level_len = 1000;
  test_level_mem = (int*)malloc(sizeof(int) * test_level_len);
  for (size_t i = 0; i < test_level_len; ++i) { test_level_mem[i] = (int)i; }
}
static inline void fixture_test_level_fini(void) {
  test_level_len = 0, free(test_level_mem), test_level_mem = 0;
}

triax_test(fixtures_test2, test_fixture_before_init, 0) { triax_assert_eq(test_level_len, 0); }
triax_test(fixtures_test, test_fixture, .init = fixture_test_level_init,
           .fini = fixture_test_level_fini) {
  triax_assert_eq(test_level_len, 1000);
  for (size_t i = 0; i < test_level_len; ++i) { triax_assert_eq(test_level_mem[i], (int)i); }
}
triax_test(fixtures_test2, test_fixture_between_init_fini, 0) {
  triax_assert_eq(test_level_len, 0);
}
triax_test(fixtures_test, test_fixture_independence1, .init = fixture_test_level_init,
           .fini = fixture_test_level_fini) {
  triax_assert_eq(test_level_len, 1000);
  for (size_t i = 0; i < test_level_len; ++i) {
    triax_assert_eq((size_t)test_level_mem[i], i);
    test_level_mem[i] = 3;
  }
}
triax_test(fixtures_test, test_fixture_independence2, .init = fixture_test_level_init,
           .fini = fixture_test_level_fini) {
  triax_assert_eq(test_level_len, 1000);
  for (size_t i = 0; i < test_level_len; ++i) { triax_assert_eq((size_t)test_level_mem[i], i); }
}
triax_test(fixtures_test2, test_fixture_after_fini, 0) { triax_assert_eq(test_level_len, 0); }

static int*        suite_level_mem;
static size_t      suite_level_len;
static inline void fixture_suite_level_init(void) {
  suite_level_len = 1000;
  suite_level_mem = (int*)malloc(sizeof(int) * suite_level_len);
  for (size_t i = 0; i < suite_level_len; ++i) { suite_level_mem[i] = (int)i; }
}
static inline void fixture_suite_level_fini(void) {
  suite_level_len = 0, free(suite_level_mem), suite_level_mem = 0;
}
static inline void fixture_suite_level_reset(void) {
  for (size_t i = 0; i < suite_level_len; ++i) { suite_level_mem[i] = (int)i; }
}

triax_test(fixtures_suite_before, test_suite_fixture_before_init, 0) {
  triax_assert_eq(suite_level_len, 0);
}
triax_suite(fixtures_suite, .init = fixture_suite_level_init, .fini = fixture_suite_level_fini);
triax_test(fixtures_suite, test_suite_fixture_1, 0) {
  triax_assert_eq(suite_level_len, 1000);
  for (size_t i = 0; i < suite_level_len; ++i) { triax_assert_eq((size_t)suite_level_mem[i], i); }
}
triax_test(fixtures_suite, test_suite_fixture_independence1, 0) {
  triax_assert_eq(suite_level_len, 1000);
  for (size_t i = 0; i < suite_level_len; ++i) {
    triax_assert_eq((size_t)suite_level_mem[i], i);
    suite_level_mem[i] = 3; // mutate for next test
  }
}
triax_test(fixtures_suite, test_suite_fixture_independence2, .init = fixture_suite_level_reset) {
  triax_assert_eq(suite_level_len, 1000);
  for (size_t i = 0; i < suite_level_len; ++i) { triax_assert_eq((size_t)suite_level_mem[i], i); }
}
triax_test(fixtures_suite_after, test_suite_fixture_after_fini, 0) {
  triax_assert_eq(suite_level_len, 0);
}
