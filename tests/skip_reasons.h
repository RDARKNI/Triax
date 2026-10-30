#ifndef TRIAX_SKIP_REASONS_H
#define TRIAX_SKIP_REASONS_H

// Shared by the C and C++ runners; validate_formats.sh checks the reported reasons.
// A skip does not hide failures: any failed assertion makes the test fail, and the
// skip reason is still reported. Logging every assertion also exercises post-skip records.
triax_suite(skip_reasons, triax_opt(verbosity, TRIAX_VERBOSITY_ALWAYS));

triaxi_validate(skip_reasons, plain, TRIAXI_VALIDATE_SKIPPED) { triax_skip(); }
triaxi_validate(skip_reasons, formatted, TRIAXI_VALIDATE_SKIPPED) {
  triax_expect(1, "");
  triax_skip("needs %s", "gpu");
}
triaxi_validate(skip_reasons, failure_before_skip, TRIAXI_VALIDATE_FAILED) {
  triax_expect(0, "");
  triax_skip("after a failure");
}

#ifdef __cplusplus
// Destructors run while the skip unwinds and may still assert.
struct TriaxSkipPassingCheck {
  ~TriaxSkipPassingCheck() { triax_expect(1, ""); }
};
struct TriaxSkipFailingCheck {
  ~TriaxSkipFailingCheck() { triax_expect(0, ""); }
};
triaxi_validate(skip_reasons, passing_assert_while_unwinding, TRIAXI_VALIDATE_SKIPPED) {
  TriaxSkipPassingCheck check;
  triax_skip("unwinding");
}
triaxi_validate(skip_reasons, failing_assert_while_unwinding, TRIAXI_VALIDATE_FAILED) {
  TriaxSkipFailingCheck check;
  triax_skip("unwinding");
}
#endif

#endif
