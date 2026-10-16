// Ported from rk_lib/tests/test_rk_test_fmt.cpp. Each test is wrapped in
// triaxi_validate: the outer run spawns a filtered child copy of this test
// and asserts it produces the designed outcome, so a deliberate "_fail" case
// still reports an overall PASS in ctest instead of needing a separate,
// unverified demo binary.
//
// Two API differences from the original found while porting:
//  - the old "_tol" float-tolerance assertions are named "_abstol" here.
//  - "assert_inrange"/"expect_inrange" don't exist in triax.h; each call is
//    decomposed into the equivalent pair of ">=" / "<=" assertions.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"
#include <iostream>
#include <limits>

// ── Custom type ────────────────────────────────────────────────────────────
// Exercises the fallback formatting path (operator<< / no operator<<)

struct Point {
  int  x, y;
  bool operator==(const Point& o) const { return x == o.x && y == o.y; }
  bool operator!=(const Point& o) const { return !(*this == o); }
};
static std::ostream& operator<<(std::ostream& os, const Point& p) {
  return os << "(" << p.x << ", " << p.y << ")";
}

struct Opaque {
  int  v;
  bool operator==(const Opaque& o) const { return v == o.v; }
  bool operator!=(const Opaque& o) const { return v != o.v; }
}; // no operator<< — address fallback

triaxi_validate(cpp_fmt, custom_type_pass, TRIAXI_VALIDATE_PASSED) {
  Point a{1, 2}, b{1, 2};
  triax_assert_eq(a, b);
}
triaxi_validate(cpp_fmt, custom_type_fail, TRIAXI_VALIDATE_FAILED) {
  // expect output: eq((1, 2), (3, 4))
  Point a{1, 2}, b{3, 4};
  triax_expect_eq(a, b);
}
triaxi_validate(cpp_fmt, custom_type_no_stream_fail, TRIAXI_VALIDATE_FAILED) {
  // expect output: eq(&<addr>, &<addr>) — address fallback
  Opaque a{1}, b{2};
  triax_expect_eq(a, b);
}

struct NulWriter {
  int  v;
  bool operator==(const NulWriter& o) const { return v == o.v; }
  bool operator!=(const NulWriter& o) const { return v != o.v; }
};
// Simulates a type whose stream representation legitimately contains raw NUL
// bytes (e.g. wrapped binary data), as opposed to the real per-argument
// separator TRIAXI_TestStream writes between args. triaxi_print_unpack_args'
// default encoding has no way to tell the two apart — six or more embedded
// NULs used to overflow its fixed 5-slot args[] array via
// triaxi_unreachable() (actually reached, so undefined behavior, not a safe
// trap) instead of being read back as one argument.
static std::ostream& operator<<(std::ostream& os, const NulWriter& n) {
  for (int i = 0; i < 6; ++i) { os.put('\0'); }
  return os << n.v;
}
triaxi_validate(cpp_fmt, custom_type_embedded_nul_fail, TRIAXI_VALIDATE_FAILED) {
  NulWriter a{1}, b{2};
  triax_expect_eq(a, b);
}

// Mirrors std::optional<double> containing NaN (the original repro for this bug) without needing
// <optional>/C++17: every one of <, <=, ==, >, >= forwards to the double's own operator, so all
// five come back false for NaN — NaN is unordered with everything, itself included.
struct Unordered {
  double v;
  bool   operator==(const Unordered& o) const { return v == o.v; }
  bool   operator!=(const Unordered& o) const { return v != o.v; }
  bool   operator<(const Unordered& o) const { return v < o.v; }
  bool   operator<=(const Unordered& o) const { return v <= o.v; }
  bool   operator>(const Unordered& o) const { return v > o.v; }
  bool   operator>=(const Unordered& o) const { return v >= o.v; }
};
// Inferring gt from <=/< (resp. lt from >=/>) used to treat "mutually unordered" identically to
// a real pass — both expect_gt(a, b) and expect_lt(a, b) wrongly passed even though a > b and
// a < b are both false. Each must now fail (as "unordered"), not pass.
triaxi_validate(cpp_fmt, unordered_gt_fail, TRIAXI_VALIDATE_FAILED) {
  Unordered a{std::numeric_limits<double>::quiet_NaN()}, b{0.0};
  triax_expect_gt(a, b);
}
triaxi_validate(cpp_fmt, unordered_lt_fail, TRIAXI_VALIDATE_FAILED) {
  Unordered a{std::numeric_limits<double>::quiet_NaN()}, b{0.0};
  triax_expect_lt(a, b);
}

// A fixed-size array of a custom (non-arithmetic, non-string) type routes through
// TRIAXI_AF_FORWARD_ARREQ_XX, which logs via TRIAXI_TestStream (TRIAXI_ENCODING_BLOB) — the index
// field is length-prefixed there, unlike every other arreq path (DEFAULT/STRING), where it's
// NUL-terminated. triaxi_print_unpack_args used to NUL-scan it unconditionally, misreading the
// blob's own length prefix as string content and desyncing everything that followed —
// triaxi_unreachable() once the resulting garbage length check failed.
triaxi_validate(cpp_fmt, custom_type_arreq_fail, TRIAXI_VALIDATE_FAILED) {
  // expect output: [index 1] (1, 2) != (9, 9)
  Point a[3] = {{1, 1}, {1, 2}, {1, 3}};
  Point b[3] = {{1, 1}, {9, 9}, {1, 3}};
  triax_expect_arreq(a, b);
}

// ── Integers (template path for non-long-long types) ──────────────────────

triaxi_validate(cpp_fmt, int_pass, TRIAXI_VALIDATE_PASSED) {
  int a = 42, b = 42;
  triax_assert_eq(a, b);
}

// Mixed signed/unsigned comparison of genuinely equal values — exercises the
// dispatch path without a raw compiler warning, unlike comparing against -1
// (which isn't equal to (unsigned)42 under any interpretation).
triaxi_validate(cpp_fmt, int_warn_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_eq((unsigned)42, 42);
}
triaxi_validate(cpp_fmt, int_fail, TRIAXI_VALIDATE_FAILED) {
  // expect output: eq(1, 2) — decimal, no truncation
  int a = 1, b = 2;
  triax_expect_eq(a, b);
}
triaxi_validate(cpp_fmt, short_fail, TRIAXI_VALIDATE_FAILED) {
  // expect output: eq(100, 200)
  short a = 100, b = 200;
  triax_expect_eq(a, b);
}
triaxi_validate(cpp_fmt, mixed_int_fail, TRIAXI_VALIDATE_FAILED) {
  // T != U — template with two type params
  // expect output: eq(1, 2)
  int   a = 1;
  short b = 2;
  triax_expect_eq(a, b);
}
triaxi_validate(cpp_fmt, int_ordering_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_lt(1, 2);
  triax_assert_leq(2, 2);
  triax_assert_gt(3, 2);
  triax_assert_geq(2, 2);
  // was: rkt_assert_inrange(5, 1, 10) — decomposed, no inrange assertion in triax.h
  triax_assert_geq(5, 1);
  triax_assert_leq(5, 10);
}

// ── Float (must go through epsilon path, not template) ────────────────────

triaxi_validate(cpp_fmt, float_epsilon_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_eq(0.1f + 0.2f, 0.3f);
}
// 0.5+0.25==0.75 is exactly representable in double, unlike 0.1+0.2==0.3
// (0.300000000000000044409 != 0.299999999999999988898) — plain assert_eq is
// strict '==', not tolerance-based.
triaxi_validate(cpp_fmt, double_epsilon_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_eq(0.5 + 0.25, 0.75);
}
triaxi_validate(cpp_fmt, float_double_mixed_pass, TRIAXI_VALIDATE_PASSED) {
  // mixed: float + double — both should widen to long double, epsilon holds
  triax_assert_eq(0.1f + 0.2f, (double)(0.1f + 0.2f));
}
triaxi_validate(cpp_fmt, float_fail, TRIAXI_VALIDATE_FAILED) {
  // expect output showing both values at long double precision
  triax_expect_eq(1.0f, 2.0f);
}
triaxi_validate(cpp_fmt, double_fail, TRIAXI_VALIDATE_FAILED) { triax_expect_eq(1.0, 2.0); }
triaxi_validate(cpp_fmt, float_double_mixed_fail, TRIAXI_VALIDATE_FAILED) {
  // mixed types: float vs double, should fail with both values shown
  triax_expect_eq(1.0f, 2.0);
}
triaxi_validate(cpp_fmt, float_tol_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_floateq_abstol(1.0f, 1.0f + 1e-5f, 1e-4f);
}
triaxi_validate(cpp_fmt, float_tol_fail, TRIAXI_VALIDATE_FAILED) {
  // expect output showing all three: val, expected, tolerance
  triax_expect_floateq_abstol(1.0f, 2.0f, 1e-4f);
}
triaxi_validate(cpp_fmt, float_ordering_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_lt(1.0f, 2.0f);
  triax_assert_leq(2.0f, 2.0f);
  triax_assert_gt(3.0, 2.0);
  // was: rkt_assert_inrange(1.5f, 1.0f, 2.0f) — decomposed
  triax_assert_geq(1.5f, 1.0f);
  triax_assert_leq(1.5f, 2.0f);
}

// ── Float formatting boundaries and near-tolerance cases ──────────────────

triaxi_validate(cpp_fmt, float_close_invisible_fail, TRIAXI_VALIDATE_FAILED) {
  // differ by ~1 ULP — with %g both may print as "1" → "1 != 1" in output
  triax_expect_eq(1.0000001f, 1.0000002f);
}
triaxi_validate(cpp_fmt, float_close_visible_fail, TRIAXI_VALIDATE_FAILED) {
  // differ at 6th significant digit — visible with %.9g
  triax_expect_eq(1.000001f, 1.000002f);
}
triaxi_validate(cpp_fmt, float_scientific_notation_fail, TRIAXI_VALIDATE_FAILED) {
  // small magnitude — %g should switch to scientific notation
  triax_expect_eq(1.5e-8f, 2.5e-8f);
}
triaxi_validate(cpp_fmt, float_tol_at_boundary_pass, TRIAXI_VALIDATE_PASSED) {
  // |diff| == tol: !(|diff| <= tol) is false → pass
  triax_assert_floateq_abstol(0.0f, 1e-4f, 1e-4f);
}
triaxi_validate(cpp_fmt, float_tol_just_inside_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_floateq_abstol(1.0f, 1.0f + 9e-5f, 1e-4f);
}
triaxi_validate(cpp_fmt, float_tol_just_outside_fail, TRIAXI_VALIDATE_FAILED) {
  // output shows all three args: value, expected, tolerance
  triax_expect_floateq_abstol(1.0f, 1.0f + 1.1e-4f, 1e-4f);
}
triaxi_validate(cpp_fmt, double_close_fail, TRIAXI_VALIDATE_FAILED) {
  // close doubles — differ at the 13th significant digit
  triax_expect_eq(1.0000000000001, 1.0000000000002);
}

// ── Strings (explicit overloads) ────────────────────────────────────────────

triaxi_validate(cpp_fmt, str_eq_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_eq("hello", "hello");
}
triaxi_validate(cpp_fmt, str_lt_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_lt("apple", "banana");
  triax_assert_lt("ab", "abc"); // prefix: shorter < longer
  triax_assert_lt("", "a");
  triax_assert_lt((const char*)nullptr, "");
}
triaxi_validate(cpp_fmt, str_ordering_pass, TRIAXI_VALIDATE_PASSED) {
  triax_assert_leq("apple", "apple");
  triax_assert_gt("banana", "apple");
  triax_assert_geq("banana", "banana");
  // was: rkt_assert_inrange("cat", "apple", "dog") — decomposed
  triax_assert_geq("cat", "apple");
  triax_assert_leq("cat", "dog");
}
triaxi_validate(cpp_fmt, str_fail, TRIAXI_VALIDATE_FAILED) {
  // expect output: eq(hello, world)
  triax_expect_eq("hello", "world");
}
triaxi_validate(cpp_fmt, str_lt_fail, TRIAXI_VALIDATE_FAILED) {
  // expect output showing both strings
  triax_expect_lt("banana", "apple");
}
