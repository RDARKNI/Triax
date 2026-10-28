// Exception assertions (triax_{assert,expect}_{throw,nthrow}). Passing cases
// are plain tests; failures, aborts and API errors go through triaxi_validate
// so the outer run checks the child's outcome. Failure cases run in-process
// (isolation off) to cover the throw/catch control flow inside the runner;
// cases that prove (non-)continuation do so with std::exit after the
// assertion, which needs isolation: an exit means execution got there.
#define TRIAX_MULTI_TU
#include "triax.h"
#include "triax_selfverify.h"

#include <cstdlib>
#include <stdexcept>

namespace {
struct Base {
  virtual ~Base() {}
};
struct Derived : Base {};
struct Other {};
struct Custom {
  int value;
  explicit Custom(int v) : value(v) {}
};
struct Lifetime {
  int* counter;
  explicit Lifetime(int* p) : counter(p) {}
  ~Lifetime() { ++*counter; }
};
// The flag keeps a returning path in the destructor (MSVC C4722 otherwise).
struct ExitOnDestroy {
  bool armed = true;
  ~ExitOnDestroy() {
    if (armed) { std::exit(0); }
  }
};

int  invocation_count = 0;
void raise_int() {
  ++invocation_count;
  throw 17;
}
void no_raise() { ++invocation_count; }
void raise_std() { throw std::runtime_error("payload"); }
} // namespace

#define EXC_INPROCESS(name, expected)                                                              \
  triaxi_validate_opts(exceptions, name, expected, TRIAXI_VALIDATE_TIMEOUT_INHERIT,                \
                       TRIAXI_VALIDATE_ISOLATION_OFF)
#define EXC_ISOLATED(name, expected)                                                               \
  triaxi_validate_opts(exceptions, name, expected, TRIAXI_VALIDATE_TIMEOUT_INHERIT,                \
                       TRIAXI_VALIDATE_ISOLATION_ON)
#define EXC_USER_ERROR(name, reason)                                                               \
  triaxi_validate_opts_user_error(exceptions, name, reason, TRIAXI_VALIDATE_TIMEOUT_INHERIT,       \
                                  TRIAXI_VALIDATE_ISOLATION_OFF)

// Matching
triax_test(exceptions, typed_exact, ) { triax_expect_throw(std::runtime_error, raise_std()); }
triax_test(exceptions, typed_base_class, ) { triax_expect_throw(std::exception, raise_std()); }
triax_test(exceptions, custom_base_class, ) { triax_expect_throw(Base, throw Derived{}); }
triax_test(exceptions, custom_exact, ) { triax_expect_throw(Custom, throw Custom(4)); }
triax_test(exceptions, throw_primitive, ) { triax_expect_throw(int, throw 42); }
triax_test(exceptions, throw_pointer, ) {
  int x = 2;
  triax_expect_throw(int*, throw &x);
}
triax_test(exceptions, any_std, ) { triax_expect_throw(TRIAX_EXCEPTION_ANY, raise_std()); }
triax_test(exceptions, any_primitive, ) { triax_expect_throw(TRIAX_EXCEPTION_ANY, throw 7); }
triax_test(exceptions, any_custom, ) { triax_expect_throw(TRIAX_EXCEPTION_ANY, throw Other{}); }
triax_test(exceptions, nthrow, ) { triax_expect_nthrow(no_raise()); }
triax_test(exceptions, nthrow_after_local_catch, ) {
  triax_expect_nthrow(try { throw 1; } catch (int){});
}

// Evaluation
triax_test(exceptions, multiple_statements_throw, ) {
  int a = 1;
  triax_expect_throw(int, a += 2; if (a == 3) throw a;);
  triax_expect_eq(a, 3);
}
triax_test(exceptions, multiple_statements_nthrow, ) {
  int a = 1;
  triax_expect_nthrow(a += 2; a += 3;);
  triax_expect_eq(a, 6);
}
triax_test(exceptions, evaluated_once_throw, ) {
  invocation_count = 0;
  triax_expect_throw(int, raise_int());
  triax_expect_eq(invocation_count, 1);
}
triax_test(exceptions, evaluated_once_nthrow, ) {
  invocation_count = 0;
  triax_expect_nthrow(no_raise());
  triax_expect_eq(invocation_count, 1);
}
triax_test(exceptions, locals_unwound, ) {
  int destroys = 0;
  triax_expect_throw(int, {
    Lifetime x(&destroys);
    throw 1;
  });
  triax_expect_eq(destroys, 1);
}
triax_test(exceptions, inside_user_try, ) {
  try {
    triax_expect_throw(int, throw 1);
  } catch (...) { triax_expect_true(false); }
}
triax_test(exceptions, assert_pass_continues, ) {
  triax_assert_throw(int, throw 1);
  triax_assert_nthrow((void)0);
  triax_expect_true(true);
}

// Failures
EXC_INPROCESS(typed_missing, TRIAXI_VALIDATE_FAILED) { triax_expect_throw(int, (void)0); }
EXC_INPROCESS(typed_wrong_primitive, TRIAXI_VALIDATE_FAILED) { triax_expect_throw(int, throw 2.0); }
EXC_INPROCESS(typed_wrong_class, TRIAXI_VALIDATE_FAILED) {
  triax_expect_throw(Base, throw Other{});
}
EXC_INPROCESS(typed_unrelated_std, TRIAXI_VALIDATE_FAILED) {
  triax_expect_throw(std::logic_error, raise_std());
}
EXC_INPROCESS(any_missing, TRIAXI_VALIDATE_FAILED) {
  triax_expect_throw(TRIAX_EXCEPTION_ANY, (void)0);
}
EXC_INPROCESS(nthrow_primitive, TRIAXI_VALIDATE_FAILED) { triax_expect_nthrow(throw 1); }
EXC_INPROCESS(nthrow_std, TRIAXI_VALIDATE_FAILED) { triax_expect_nthrow(raise_std()); }
EXC_INPROCESS(assert_throw_missing, TRIAXI_VALIDATE_FAILED) { triax_assert_throw(int, (void)0); }
EXC_INPROCESS(assert_nthrow_thrown, TRIAXI_VALIDATE_FAILED) { triax_assert_nthrow(throw 1); }

// Continuation: an expect failure runs on, an assert failure stops the body.
EXC_ISOLATED(expect_throw_failure_continues, TRIAXI_VALIDATE_UEXITED) {
  triax_expect_throw(int, (void)0);
  std::exit(0);
}
EXC_ISOLATED(expect_nthrow_failure_continues, TRIAXI_VALIDATE_UEXITED) {
  triax_expect_nthrow(throw 1);
  std::exit(0);
}
EXC_ISOLATED(assert_throw_missing_stops, TRIAXI_VALIDATE_FAILED) {
  triax_assert_throw(int, (void)0);
  std::exit(0);
}
EXC_ISOLATED(assert_throw_wrong_stops, TRIAXI_VALIDATE_FAILED) {
  triax_assert_throw(int, throw 2.5);
  std::exit(0);
}
EXC_ISOLATED(assert_nthrow_stops, TRIAXI_VALIDATE_FAILED) {
  triax_assert_nthrow(throw 1);
  std::exit(0);
}
// A fatal failure unwinds the body's locals rather than jumping over them.
EXC_ISOLATED(assert_failure_unwinds, TRIAXI_VALIDATE_UEXITED) {
  ExitOnDestroy sentinel;
  triax_assert_throw(int, (void)0);
}

// Framework control flow is not a user exception: assertions and skips
// inside the evaluated expression are API errors, not caught by the wildcard.
EXC_USER_ERROR(nested_assert_in_throw, "assertion_in_fixture") {
  triax_expect_throw(TRIAX_EXCEPTION_ANY, triax_assert_true(false));
}
EXC_USER_ERROR(nested_assert_in_nthrow, "assertion_in_fixture") {
  triax_expect_nthrow(triax_assert_true(false));
}
EXC_USER_ERROR(skip_in_throw, "skip_in_fixture") {
  triax_expect_throw(TRIAX_EXCEPTION_ANY, triax_skip());
}
EXC_USER_ERROR(skip_in_nthrow, "skip_in_fixture") { triax_expect_nthrow(triax_skip()); }
