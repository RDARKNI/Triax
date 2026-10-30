# Changelog

## Unreleased

- Standardize equality-style assertion arguments as actual first, expected second,
  including string, array, memory, and floating-point comparisons. Ordering
  assertions retain their left/right semantics.
- `triax_{expect,assert}_arr{eq,neq}` now take an explicit element count
  (`arreq(actual, expected, count)`), replacing both the extent-inferring
  two-argument form and the `_n` variants. Operand sizes are no longer checked.

## 1.0.0

Initial public release under the Triax name.

- Single-header C/C++ testing framework.
- Optional per-test process isolation.
- Expected fault and exit assertions.
- Timeouts, tags, fixtures, filters, and fail-fast mode.
- Parallel isolated execution.
- Parameterized tests.
- Captured stdout/stderr access.
- Text, JSON, TAP, and JUnit XML reporting.
- Automatic test registration across supported toolchain families.
