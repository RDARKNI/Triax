// Ported from rk_lib/tests/test_rk_test.c (TEST_STRINGS). Each test is
// wrapped in triaxi_validate: the outer run spawns a filtered child copy of
// this test and asserts it produces the designed outcome, so a deliberate
// "_fail" case still reports an overall PASS in ctest instead of needing a
// separate, unverified demo binary.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

triax_suite(strings, .verbosity = TRIAX_VERBOSITY_ALWAYS, .isolation = TRIAX_ISOLATION_ON);

triaxi_validate(strings, streq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_streq("hello", "hello");
  triax_expect_streq("", "");
  triax_expect_streq((const char*)NULL, (const char*)NULL);
}
triaxi_validate(strings, streq_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_streq((const char*)NULL, "");
}
triaxi_validate(strings, strneq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_strneq("hello", "world");
  triax_expect_strneq("hello", "");
  triax_expect_strneq((const char*)NULL, "");
  triax_expect_strneq("", (const char*)NULL);
}
triaxi_validate(strings, strneq_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_strneq("hello", "hello");
  triax_expect_strneq("", "");
  triax_expect_strneq((const char*)NULL, (const char*)NULL);
}
triaxi_validate(strings, strv_eq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_streq(triax_str("hello", 5), triax_str("hello", 5));
  triax_expect_streq(triax_str("hello", 3), triax_str("helloworld", 3)); // same first 3 bytes
  triax_expect_streq(triax_str("", 0), triax_str("", 0));
  triax_expect_streq(triax_str((const char*)NULL, 0), triax_str((const char*)NULL, 0));
}
triaxi_validate(strings, strv_eq_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_streq(triax_str("hello", 5), triax_str("world", 5));
  triax_expect_streq(triax_str("hello", 5), triax_str("hello", 3)); // same content, different length
}
triaxi_validate(strings, strv_neq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_strneq(triax_str("hello", 5), triax_str("world", 5));
  triax_expect_strneq(triax_str("hello", 5), triax_str("hello", 3));
}
triaxi_validate(strings, strv_neq_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_strneq(triax_str("hello", 5), triax_str("hello", 5));
  triax_expect_strneq(triax_str("", 0), triax_str("", 0));
}
triaxi_validate(strings, startswith_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_str_startswith(triax_str("hello world", 11), triax_str("hello", 5));
  triax_expect_str_startswith(triax_str("hello", 5), triax_str("hello", 5)); // exact match
  triax_expect_str_startswith(triax_str("hello", 5), triax_str("", 0));      // empty prefix
}
triaxi_validate(strings, startswith_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_str_startswith(triax_str("hello", 5), triax_str("world", 5));
  triax_expect_str_startswith(triax_str("hello", 5), triax_str("hello!", 6)); // prefix longer than str
}
triaxi_validate(strings, endswith_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_str_endswith(triax_str("hello world", 11), triax_str("world", 5));
  triax_expect_str_endswith(triax_str("hello", 5), triax_str("hello", 5));
  triax_expect_str_endswith(triax_str("hello", 5), triax_str("", 0));
}
triaxi_validate(strings, endswith_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_str_endswith(triax_str("hello world", 11), triax_str("hello", 5));
  triax_expect_str_endswith(triax_str("hello", 5), triax_str("world!", 6));
}

// New coverage: contains and the negated (n-prefixed) predicates had zero
// test coverage before this — "contains" itself (non-negated) was only ever
// exercised via the assert variant in core.c, never expect_/n-forms here.
triaxi_validate(strings, contains_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_str_contains(triax_str("hello world", 11), triax_str("lo wo", 5));
  triax_expect_str_contains(triax_str("hello", 5), triax_str("", 0)); // empty needle
}
triaxi_validate(strings, contains_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_str_contains(triax_str("hello", 5), triax_str("world", 5));
}
triaxi_validate(strings, ncontains_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_str_ncontains(triax_str("hello", 5), triax_str("world", 5));
}
triaxi_validate(strings, ncontains_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_str_ncontains(triax_str("hello world", 11), triax_str("lo wo", 5));
}
triaxi_validate(strings, nstartswith_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_str_nstartswith(triax_str("hello", 5), triax_str("world", 5));
}
triaxi_validate(strings, nstartswith_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_str_nstartswith(triax_str("hello world", 11), triax_str("hello", 5));
}
triaxi_validate(strings, nendswith_pass, TRIAXI_VALIDATE_PASSED) {
  triax_expect_str_nendswith(triax_str("hello", 5), triax_str("world", 5));
}
triaxi_validate(strings, nendswith_fail, TRIAXI_VALIDATE_FAILED) {
  triax_expect_str_nendswith(triax_str("hello world", 11), triax_str("world", 5));
}
