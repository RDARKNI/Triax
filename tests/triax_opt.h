// Language-neutral test/suite options, so the tests/c files also compile as
// C++ (see the triax-selftest-c-as-cpp target). Options are separated by
// whitespace, not commas:
//
//   triax_test(s, n, triax_opt(isolation, TRIAX_ISOLATION_ON) triax_opt(timeout_ms, 500)) {}
//
// Pass an empty argument for no options: triax_test(s, n, ) {}
#ifndef TRIAX_TESTS_TRIAX_OPT_H
#define TRIAX_TESTS_TRIAX_OPT_H

#ifdef __cplusplus
# define triax_opt(field, ...) .field(__VA_ARGS__)
# define triax_opt_params(arr) .parameterize(arr)
#else
# define triax_opt(field, ...) .field = __VA_ARGS__,
# define triax_opt_params(arr) .params = triax_as_params(arr),
#endif

#endif /* TRIAX_TESTS_TRIAX_OPT_H */
