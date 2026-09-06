// Ported from rk_lib/tests/test_rk_test.c (TEST_ASSERTIONS, TEST_ORDERING,
// TEST_FLOAT_TOLERANCE). Each test is wrapped in triaxi_validate: the outer
// run spawns a filtered child copy of this test and asserts it produces the
// designed outcome, so a deliberate "_fail" case still reports an overall
// PASS in ctest instead of needing a separate, unverified demo binary.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

triax_suite(assertions, .verbosity = TRIAX_VERBOSITY_ALWAYS);

triaxi_validate(assertions, eq_neq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_eq((char)'a', (char)'a');
  triax_expect_neq((char)'a', (char)'b');
  triax_expect_eq((void*)NULL, (void*)NULL);
  triax_expect_neq((void*)NULL, (void*)1);

  triax_expect_eq((signed char)1, (signed char)1);
  triax_expect_eq((short)1, (short)1);
  triax_expect_eq((int)1, (int)1);
  triax_expect_eq((long)1, (long)1);
  triax_expect_eq((long long)1, (long long)1);
  triax_expect_neq((signed char)1, (signed char)0);
  triax_expect_neq((short)1, (short)0);
  triax_expect_neq((int)1, (int)0);
  triax_expect_neq((long)1, (long)0);
  triax_expect_neq((long long)1, (long long)0);

  triax_expect_eq((unsigned char)1, (unsigned char)1);
  triax_expect_eq((unsigned short)1, (unsigned short)1);
  triax_expect_eq((unsigned int)1, (unsigned int)1);
  triax_expect_eq((unsigned long)1, (unsigned long)1);
  triax_expect_eq((unsigned long long)1, (unsigned long long)1);
  triax_expect_neq((unsigned char)1, (unsigned char)0);
  triax_expect_neq((unsigned short)1, (unsigned short)0);
  triax_expect_neq((unsigned int)1, (unsigned int)0);
  triax_expect_neq((unsigned long)1, (unsigned long)0);
  triax_expect_neq((unsigned long long)1, (unsigned long long)0);

  // floats — all dispatch to long double via _Generic
  // Uses 0.5+0.25==0.75, which is exactly representable in binary floating
  // point at every precision, unlike 0.1+0.2==0.3 (a classic rounding trap:
  // 0.1+0.2 is 0.300000000000000044409 in double, not bit-equal to the
  // 0.3 literal's 0.299999999999999988898 — plain expect_eq performs a
  // strict '==', not a tolerance-based comparison; see floateq_abstol below
  // for that).
  triax_expect_eq(0.5f + 0.25f, 0.75f); // float
  triax_expect_eq(0.5 + 0.25, 0.75);    // double
  triax_expect_eq(0.5L + 0.25L, 0.75L); // long double
  triax_expect_neq(1.0f, 2.0f);
  triax_expect_neq(1.0, 2.0);
  triax_expect_neq(1.0L, 2.0L);

  // mixed: second arg widens to long double; float 0.1 != double 0.1 (differ by
  // ~1.5e-9)
  triax_expect_eq(1.0f, (double)1.0f); // same real value, passes
  triax_expect_neq(0.1f, 0.1);         // genuinely different representations

  const char *hello1 = "hello", *hello2 = "hello";
  char *      world1 = "world", *world2 = "world";
  triax_expect_eq(hello1, hello2);
  triax_expect_eq(hello2, hello1);
  triax_expect_eq(world1, world2);
  triax_expect_eq(world2, world1);
  triax_expect_neq(hello1, world1);
  triax_expect_neq(world1, hello1);
  triax_expect_neq(world1, hello1);
  triax_expect_neq(hello1, world1);
}

triaxi_validate(assertions, eq_neq_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_neq((char)'a', (char)'a');
  triax_expect_eq((char)'a', (char)'b');
  triax_expect_neq((void*)NULL, (void*)NULL);
  triax_expect_eq((void*)NULL, (void*)1);

  triax_expect_neq((signed char)1, (signed char)1);
  triax_expect_neq((short)1, (short)1);
  triax_expect_neq((int)1, (int)1);
  triax_expect_neq((long)1, (long)1);
  triax_expect_neq((long long)1, (long long)1);
  triax_expect_eq((signed char)1, (signed char)0);
  triax_expect_eq((short)1, (short)0);
  triax_expect_eq((int)1, (int)0);
  triax_expect_eq((long)1, (long)0);
  triax_expect_eq((long long)1, (long long)0);

  triax_expect_neq((unsigned char)1, (unsigned char)1);
  triax_expect_neq((unsigned short)1, (unsigned short)1);
  triax_expect_neq((unsigned int)1, (unsigned int)1);
  triax_expect_neq((unsigned long)1, (unsigned long)1);
  triax_expect_neq((unsigned long long)1, (unsigned long long)1);
  triax_expect_eq((unsigned char)1, (unsigned char)0);
  triax_expect_eq((unsigned short)1, (unsigned short)0);
  triax_expect_eq((unsigned int)1, (unsigned int)0);
  triax_expect_eq((unsigned long)1, (unsigned long)0);
  triax_expect_eq((unsigned long long)1, (unsigned long long)0);

  // floats — all dispatch to long double via _Generic (mirrors eq_neq_pass:
  // 0.5+0.25==0.75 exactly at every precision, so asserting "neq" here is
  // genuinely wrong and correctly fails)
  triax_expect_neq(0.5f + 0.25f, 0.75f);
  triax_expect_neq(0.5 + 0.25, 0.75);
  triax_expect_neq(0.5L + 0.25L, 0.75L);
  triax_expect_eq(1.0f, 2.0f);
  triax_expect_eq(1.0, 2.0);
  triax_expect_eq(1.0L, 2.0L);

  // mixed: second arg widens to long double; float 0.1 != double 0.1 (differ by
  // ~1.5e-9)
  triax_expect_neq(1.0f, (double)1.0f);
  triax_expect_eq(0.1f, 0.1);
}

// expect continues past failures; assert stops at first
triaxi_validate(assertions, expect_continues, TRIAXI_VALIDATE_FAILED) {
  triax_expect_eq(1, 2); // fail 1
  triax_expect_eq(3, 4); // fail 2 — still reached
  triax_expect_eq(5, 6); // fail 3 — still reached
}
// 200ms rather than the original 10ms: this test isn't about racing a
// timeout, just about assert_eq stopping the test immediately — a razor-thin
// margin was flaky under ASan+UBSan's much higher fork+exec+re-init
// overhead, a real flake this project's own CI hit, not a hypothetical.
triaxi_validate_opts(assertions, assert_stops_early, TRIAXI_VALIDATE_FAILED, 200,
                     TRIAXI_VALIDATE_ISOLATION_ON) {
  triax_assert_eq(1, 2); // stops here; only one failure recorded
  triax_expect_eq(3, 4); // never reached
}

// true / false / null / nonnull
triaxi_validate(assertions, true_false_null_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_true(1);
  triax_expect_true(42);
  triax_expect_true(-1);
  triax_expect_false(0);
  triax_expect_null(NULL);
  triax_expect_nonnull((void*)1);
  triax_expect_nonnull("hello");
}
triaxi_validate(assertions, true_false_null_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_true(0);
  triax_expect_false(1);
  triax_expect_false(-1);
  triax_expect_null((void*)1);
  triax_expect_nonnull(NULL);
}

// ── ordering — lt, leq, gt, geq for ints, floats, strings ──────────────────

triax_suite(ordering, .verbosity = TRIAX_VERBOSITY_ALWAYS);

triaxi_validate(ordering, int_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_lt(1, 2);
  triax_expect_leq(1, 2);
  triax_expect_leq(2, 2); // boundary: equal counts as leq
  triax_expect_gt(2, 1);
  triax_expect_geq(2, 1);
  triax_expect_geq(2, 2); // boundary: equal counts as geq
}
triaxi_validate(ordering, int_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_lt(2, 1);
  triax_expect_lt(2, 2); // equal, not strictly less
  triax_expect_leq(3, 2);
  triax_expect_gt(1, 2);
  triax_expect_gt(2, 2); // equal, not strictly greater
  triax_expect_geq(1, 2);
}
triaxi_validate(ordering, unsigned_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_lt((unsigned)0, (unsigned)1);
  triax_expect_leq((unsigned)0, (unsigned)0);
  triax_expect_gt((unsigned)1, (unsigned)0);
  triax_expect_geq((unsigned)1, (unsigned)1);
}
triaxi_validate(ordering, float_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_lt(1.0, 2.0);
  triax_expect_leq(2.0, 2.0);
  triax_expect_gt(2.0, 1.0);
  triax_expect_geq(2.0, 2.0);
}
triaxi_validate(ordering, float_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_lt(2.0, 1.0);
  triax_expect_gt(1.0, 2.0);
}
triaxi_validate(ordering, str_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_lt("apple", "banana");
  triax_expect_lt("", "a");
  triax_expect_lt(NULL, "");    // NULL < empty string
  triax_expect_lt("ab", "abc"); // prefix: shorter < longer
  triax_expect_leq("apple", "apple");
  triax_expect_leq("apple", "banana");
  triax_expect_gt("banana", "apple");
  triax_expect_geq("banana", "banana");
}
triaxi_validate(ordering, str_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_lt("banana", "apple");
  triax_expect_lt("apple", "apple"); // equal, not strictly less
  triax_expect_gt("apple", "banana");
}

// ── float tolerance — floateq_abstol / floatneq_abstol ─────────────────────
// Note: the old suite named these "_tol"; the current API is "_abstol".

triax_suite(float_tol, .verbosity = TRIAX_VERBOSITY_ALWAYS);

triaxi_validate(float_tol, floateq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_floateq_abstol(1.0, 1.0 + 9e-5, 1e-4); // within tolerance
  triax_expect_floateq_abstol(1.0, 1.0 + 1e-4, 1e-4); // exactly at boundary (<=)
  triax_expect_floateq_abstol(0.0, 0.0, 0.0);         // exact zero, zero tolerance
}
triaxi_validate(float_tol, floateq_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_floateq_abstol(1.0, 2.0, 0.5);           // well outside
  triax_expect_floateq_abstol(1.0, 1.0 + 1.1e-4, 1e-4); // just over boundary
}
triaxi_validate(float_tol, floatneq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_floatneq_abstol(1.0, 2.0, 0.5);
  triax_expect_floatneq_abstol(1.0, 1.0 + 1.1e-4, 1e-4);
}
triaxi_validate(float_tol, floatneq_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_floatneq_abstol(1.0, 1.0 + 9e-5, 1e-4); // within tolerance
  triax_expect_floatneq_abstol(1.0, 1.0, 1e-4);        // exactly equal
}
