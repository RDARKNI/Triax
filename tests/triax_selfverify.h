#ifndef TRIAX_SELFVERIFY_H
#define TRIAX_SELFVERIFY_H

/*
 * Internal self-verification layer for Triax's own tests.
 *
 * Include after triax.h in every translation unit that declares
 * triaxi_validate() tests. Uses the same TRIAX_MULTI_TU / TRIAX_IMPL
 * convention as triax.h itself: if you spread triaxi_validate() tests across
 * multiple .c files, define TRIAX_MULTI_TU in all of them and TRIAX_IMPL in
 * exactly one (the same file that calls triaxi_validate_init and
 * triax_run_argv) — otherwise triaxi_validate_executable would end up as a
 * separate, independently-NULL static in each translation unit.
 * Call triaxi_validate_init(argv[0]) once from main before triax_run_argv().
 *
 * This file deliberately does not modify Triax registration or runner state.
 * A triaxi_validate() case is one ordinary registered Triax test. In the outer
 * run it launches the same executable, filtered to that exact test. In the
 * child run TRIAXI_VALIDATE_CHILD is set, so the same test executes the subject
 * body instead of recursively validating itself. The outer test reads the
 * child's normal JSON report and checks the reported outcome.
 */
#include "triax.h"
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
# include <windows.h>
#else
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>
#endif

typedef enum TRIAXI_ValidateOutcome {
  TRIAXI_VALIDATE_SKIPPED,
  TRIAXI_VALIDATE_PASSED,
  TRIAXI_VALIDATE_FAILED,
  TRIAXI_VALIDATE_UCRASHED,
  TRIAXI_VALIDATE_UEXITED,
  TRIAXI_VALIDATE_UEXCEPTION,
  TRIAXI_VALIDATE_TIMEOUT,
  TRIAXI_VALIDATE_TEST_ERROR
} TRIAXI_ValidateOutcome;

/*
 * Isolation/timeout for the *child* invocation only (see triaxi_validate_opts
 * below). Deliberately not expressed via the registered test's own
 * .isolation/.timeout_ms attrs: those attrs would apply equally to the outer
 * meta-check role (fork+exec+wait+read) and the inner subject-body role,
 * which conflict — e.g. a subject test needing a 1ms timeout would also cap
 * how long the outer role is allowed to spend spawning and waiting on it.
 * Passed instead as CLI overrides (--timeout=/--isolation=) on the child's
 * own invocation, which affects only that single filtered test.
 */
typedef enum TRIAXI_ValidateIsolation {
  TRIAXI_VALIDATE_ISOLATION_INHERIT, /* don't pass --isolation to the child */
  TRIAXI_VALIDATE_ISOLATION_ON,
  TRIAXI_VALIDATE_ISOLATION_OFF
} TRIAXI_ValidateIsolation;

/* Sentinel: don't pass --timeout to the child (0 is a real, meaningful CLI
 * value — "no timeout" — so it can't double as "no override"). */
#define TRIAXI_VALIDATE_TIMEOUT_INHERIT ((uint32_t)-1)

#if defined(TRIAX_IMPL) || !defined(TRIAX_MULTI_TU)
# define TRIAXI_SV_LINKAGE
#else
# define TRIAXI_SV_LINKAGE extern
#endif

TRIAXI_SV_LINKAGE const char* triaxi_validate_executable;

static inline void triaxi_validate_init(const char* argv0) { triaxi_validate_executable = argv0; }

static inline int  triaxi_validate_is_child(void) {
  const char* v = getenv("TRIAXI_VALIDATE_CHILD");
  return v && v[0] == '1' && v[1] == '\0';
}

static inline const char* triaxi_validate_outcome_name(TRIAXI_ValidateOutcome outcome) {
  switch (outcome) {
  case TRIAXI_VALIDATE_SKIPPED   : return "skipped";
  case TRIAXI_VALIDATE_PASSED    : return "passed";
  case TRIAXI_VALIDATE_FAILED    : return "failed";
  case TRIAXI_VALIDATE_UCRASHED  : return "ucrashed";
  case TRIAXI_VALIDATE_UEXITED   : return "uexited";
  case TRIAXI_VALIDATE_UEXCEPTION: return "uexception";
  case TRIAXI_VALIDATE_TIMEOUT   : return "timeout";
  case TRIAXI_VALIDATE_TEST_ERROR: return "test_error";
  default                        : return NULL;
  }
}

static inline int triaxi_validate_expected_exit(TRIAXI_ValidateOutcome outcome) {
  return outcome == TRIAXI_VALIDATE_PASSED || outcome == TRIAXI_VALIDATE_SKIPPED ? 0 : 1;
}

static inline int triaxi_validate_make_tmp(char* buf, size_t cap) {
#ifdef _WIN32
  char  dir[MAX_PATH];
  DWORD n = GetTempPathA((DWORD)sizeof(dir), dir);
  if (!n || n >= sizeof(dir)) { return -1; }
  if (!GetTempFileNameA(dir, "trx", 0, buf)) { return -1; }
  return 0;
#else
  const char* tmp = getenv("TMPDIR");
  if (!tmp || !*tmp) { tmp = "/tmp"; }
  int n = snprintf(buf, cap, "%s/triax-validate-XXXXXX", tmp);
  if (n < 0 || (size_t)n >= cap) { return -1; }
  int fd = mkstemp(buf);
  if (fd < 0) { return -1; }
  if (close(fd) != 0) {
    int e = errno;
    unlink(buf);
    errno = e;
    return -1;
  }
  return 0;
#endif
}

/*
 * execvp requires char *const argv[] — a legacy POSIX signature predating
 * widespread const usage. POSIX guarantees exec() never modifies argv
 * contents, so this discards constness only syntactically, never
 * semantically. Centralized here, with the warning suppressed for just this
 * one line, instead of a raw (char*) cast repeated at every call site.
 */
#if defined(__GNUC__) || defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-qual"
#endif
static inline char* triaxi_validate_argv_str(const char* s) { return (char*)s; }
#if defined(__GNUC__) || defined(__clang__)
# pragma GCC diagnostic pop
#endif

#ifndef _WIN32
static inline int triaxi_validate_spawn(const char* filter, const char* json_path,
                                        uint32_t timeout_ms, TRIAXI_ValidateIsolation isolation) {
  if (!triaxi_validate_executable || !*triaxi_validate_executable) { return -1; }

  char json_arg[4096];
  int  n = snprintf(json_arg, sizeof(json_arg), "--json=%s", json_path);
  if (n < 0 || (size_t)n >= sizeof(json_arg)) { return -1; }

  char timeout_arg[64];
  if (timeout_ms != TRIAXI_VALIDATE_TIMEOUT_INHERIT) {
    n = snprintf(timeout_arg, sizeof(timeout_arg), "--timeout=%" PRIu32, timeout_ms);
    if (n < 0 || (size_t)n >= sizeof(timeout_arg)) { return -1; }
  }
  const char* isolation_arg = isolation == TRIAXI_VALIDATE_ISOLATION_ON    ? "--isolation=on"
                              : isolation == TRIAXI_VALIDATE_ISOLATION_OFF ? "--isolation=off"
                                                                          : NULL;

  pid_t pid = fork();
  if (pid < 0) { return -1; }

  if (pid == 0) {
    if (setenv("TRIAXI_VALIDATE_CHILD", "1", 1) != 0) { _exit(126); }

    char* argv[9];
    int   i    = 0;
    argv[i++]  = triaxi_validate_argv_str(triaxi_validate_executable);
    argv[i++]  = triaxi_validate_argv_str("--text=none");
    argv[i++]  = triaxi_validate_argv_str("--no-color");
    argv[i++]  = json_arg;
    if (timeout_ms != TRIAXI_VALIDATE_TIMEOUT_INHERIT) { argv[i++] = timeout_arg; }
    if (isolation_arg) { argv[i++] = triaxi_validate_argv_str(isolation_arg); }
    argv[i++] = triaxi_validate_argv_str(filter);
    argv[i++] = NULL;

    execvp(triaxi_validate_executable, argv);
    _exit(127);
  }

  int status;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR) { continue; }
    return -1;
  }
  if (WIFEXITED(status)) { return WEXITSTATUS(status); }
  if (WIFSIGNALED(status)) { return 128 + WTERMSIG(status); }
  return -1;
}
#else
static inline int triaxi_validate_spawn(const char* filter, const char* json_path,
                                        uint32_t timeout_ms, TRIAXI_ValidateIsolation isolation) {
  if (!triaxi_validate_executable || !*triaxi_validate_executable) { return -1; }

  char timeout_arg[64] = "";
  if (timeout_ms != TRIAXI_VALIDATE_TIMEOUT_INHERIT) {
    int tn = snprintf(timeout_arg, sizeof(timeout_arg), " --timeout=%" PRIu32, timeout_ms);
    if (tn < 0 || (size_t)tn >= sizeof(timeout_arg)) { return -1; }
  }
  const char* isolation_arg = isolation == TRIAXI_VALIDATE_ISOLATION_ON    ? " --isolation=on"
                              : isolation == TRIAXI_VALIDATE_ISOLATION_OFF ? " --isolation=off"
                                                                          : "";

  char cmd[8192];
  int  n = snprintf(cmd, sizeof(cmd), "\"%s\" --text=none --no-color --json=\"%s\"%s%s \"%s\"",
                    triaxi_validate_executable, json_path, timeout_arg, isolation_arg, filter);
  if (n < 0 || (size_t)n >= sizeof(cmd)) { return -1; }

  char* old     = NULL;
  DWORD old_len = GetEnvironmentVariableA("TRIAXI_VALIDATE_CHILD", NULL, 0);
  if (old_len) {
    old = (char*)malloc(old_len);
    if (!old) { return -1; }
    if (!GetEnvironmentVariableA("TRIAXI_VALIDATE_CHILD", old, old_len)) {
      free(old);
      return -1;
    }
  }
  if (!SetEnvironmentVariableA("TRIAXI_VALIDATE_CHILD", "1")) {
    free(old);
    return -1;
  }

  STARTUPINFOA        si;
  PROCESS_INFORMATION pi;
  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));
  si.cb   = sizeof(si);

  BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

  if (old) {
    SetEnvironmentVariableA("TRIAXI_VALIDATE_CHILD", old);
    free(old);
  } else {
    SetEnvironmentVariableA("TRIAXI_VALIDATE_CHILD", NULL);
  }

  if (!ok) { return -1; }
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 0;
  ok         = GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (!ok || code > INT_MAX) { return -1; }
  return (int)code;
}
#endif

static inline char* triaxi_validate_read_all(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) { return NULL; }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long end = ftell(f);
  if (end < 0) {
    fclose(f);
    return NULL;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return NULL;
  }

  size_t len = (size_t)end;
  char*  s   = (char*)malloc(len + 1);
  if (!s) {
    fclose(f);
    return NULL;
  }
  size_t got = fread(s, 1, len, f);
  if (got != len || ferror(f)) {
    free(s);
    fclose(f);
    return NULL;
  }
  s[len] = '\0';
  fclose(f);
  return s;
}

/*
 * Finds the first `"outcome": "<value>"` field in a triax JSON report and
 * returns whether <value> equals expected_name. Extracting and comparing the
 * exact value (rather than searching for the whole rendered substring) means
 * this can't be fooled by a test's own captured stdout/stderr happening to
 * contain matching text, and doesn't depend on the emitter's exact
 * whitespace, only on the "outcome": "..." key/value shape.
 */
static inline int triaxi_validate_json_has_outcome(const char* json, const char* expected_name) {
  static const char key[] = "\"outcome\"";
  const char*       p     = json;
  while ((p = strstr(p, key)) != NULL) {
    p += sizeof(key) - 1;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { ++p; }
    if (*p != ':') { continue; }
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { ++p; }
    if (*p != '"') { continue; }
    ++p;
    const char* end = strchr(p, '"');
    if (!end) { return 0; }
    size_t len = (size_t)(end - p);
    if (len == strlen(expected_name) && !memcmp(p, expected_name, len)) { return 1; }
    p = end + 1;
  }
  return 0;
}

/*
 * Same shape as triaxi_validate_json_has_outcome, but for the "reason"
 * field emitted alongside a "user_error" (or "fault") termination — see
 * triaxi_print_test_end_json's TRIAXI_EXIT_ERROR/TRIAXI_EXIT_FAULT branches.
 */
static inline int triaxi_validate_json_has_reason(const char* json, const char* expected_reason) {
  static const char key[] = "\"reason\"";
  const char*       p     = json;
  while ((p = strstr(p, key)) != NULL) {
    p += sizeof(key) - 1;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { ++p; }
    if (*p != ':') { continue; }
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { ++p; }
    if (*p != '"') { continue; }
    ++p;
    const char* end = strchr(p, '"');
    if (!end) { return 0; }
    size_t len = (size_t)(end - p);
    if (len == strlen(expected_reason) && !memcmp(p, expected_reason, len)) { return 1; }
    p = end + 1;
  }
  return 0;
}

/*
 * Shared implementation behind triaxi_validate_run(_opts) and the
 * user-error variants below. expected_reason is checked in addition to the
 * outcome when non-NULL — the "outcome": "test_error" wrapper is the same
 * for every TRIAXI_Error kind, so without this a test could assert
 * TRIAXI_VALIDATE_TEST_ERROR and pass even though the wrong error path
 * fired (e.g. a malformed-params test that actually failed on parameter
 * access instead).
 */
static inline void triaxi_validate_run_ex(const char* filter, TRIAXI_ValidateOutcome expected,
                                          uint32_t timeout_ms, TRIAXI_ValidateIsolation isolation,
                                          const char* expected_reason) {
  const char* outcome = triaxi_validate_outcome_name(expected);
  triax_assert_nonnull(outcome);
  triax_assert(triaxi_validate_executable != NULL,
               "triaxi_validate_init(argv[0]) was not called from main");

  char json_path[4096];
  triax_assert_eq(triaxi_validate_make_tmp(json_path, sizeof(json_path)), 0);

  int code = triaxi_validate_spawn(filter, json_path, timeout_ms, isolation);
  triax_expect_eq(triaxi_validate_expected_exit(expected), code);

  char* json = triaxi_validate_read_all(json_path);
  remove(json_path);
  triax_assert_nonnull(json);

  triax_expect_true(triaxi_validate_json_has_outcome(json, outcome));
  if (expected_reason) { triax_expect_true(triaxi_validate_json_has_reason(json, expected_reason)); }

  free(json);
}

static inline void triaxi_validate_run_opts(const char* filter, TRIAXI_ValidateOutcome expected,
                                            uint32_t                 timeout_ms,
                                            TRIAXI_ValidateIsolation isolation) {
  triaxi_validate_run_ex(filter, expected, timeout_ms, isolation, NULL);
}

static inline void triaxi_validate_run(const char* filter, TRIAXI_ValidateOutcome expected) {
  triaxi_validate_run_opts(filter, expected, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                           TRIAXI_VALIDATE_ISOLATION_INHERIT);
}

/*
 * Checks that the child produced a "test_error" outcome for the specific
 * TRIAXI_Error `reason` (the string from triaxi_error_name, e.g.
 * "parameter_access" or "timeout_without_isolation") rather than just any
 * user_error at all.
 */
static inline void triaxi_validate_run_user_error(const char* filter, const char* reason) {
  triaxi_validate_run_ex(filter, TRIAXI_VALIDATE_TEST_ERROR, TRIAXI_VALIDATE_TIMEOUT_INHERIT,
                         TRIAXI_VALIDATE_ISOLATION_INHERIT, reason);
}

static inline void triaxi_validate_run_user_error_opts(const char* filter, const char* reason,
                                                        uint32_t                 timeout_ms,
                                                        TRIAXI_ValidateIsolation isolation) {
  triaxi_validate_run_ex(filter, TRIAXI_VALIDATE_TEST_ERROR, timeout_ms, isolation, reason);
}

#define TRIAXI_SV_CAT_(a, b)     a##b
#define TRIAXI_SV_CAT(a, b)      TRIAXI_SV_CAT_(a, b)
#define TRIAXI_SV_CAT3_(a, b, c) a##b##c
#define TRIAXI_SV_CAT3(a, b, c)  TRIAXI_SV_CAT3_(a, b, c)

/*
 * Usage:
 *
 *   triaxi_validate(assertions, deliberate_failure, TRIAXI_VALIDATE_FAILED) {
 *       triax_assert_eq(1, 2);
 *   }
 *
 * The registered test is ordinary Triax. In the outer run it validates a child
 * execution of itself; in the child run it executes the user's body.
 *
 * triaxi_validate_opts additionally takes the timeout/isolation the *subject*
 * body needs (e.g. a test that's supposed to time out or that must run
 * isolated to crash safely) — passed to the child as CLI overrides rather
 * than attached to the registered test's own attrs, so the outer meta-check
 * role (which forks, execs, waits, and reads a file) isn't also bound by a
 * timeout that was only ever meant for the subject body:
 *
 *   triaxi_validate_opts(exits, timeout, TRIAXI_VALIDATE_TIMEOUT,
 *                        1, TRIAXI_VALIDATE_ISOLATION_ON) {
 *       sleep(100);
 *   }
 */
#define triaxi_validate(suite, name, expected)                                                     \
  static void TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)(void);                         \
  triax_test(suite, name, ) {                                                                      \
    if (triaxi_validate_is_child()) {                                                              \
      TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)();                                     \
      return;                                                                                      \
    }                                                                                              \
    triaxi_validate_run(#suite "::" #name, (expected));                                            \
  }                                                                                                \
  static void TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)(void)

#define triaxi_validate_opts(suite, name, expected, timeout_ms, isolation)                         \
  static void TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)(void);                         \
  triax_test(suite, name, ) {                                                                      \
    if (triaxi_validate_is_child()) {                                                              \
      TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)();                                     \
      return;                                                                                      \
    }                                                                                              \
    triaxi_validate_run_opts(#suite "::" #name, (expected), (timeout_ms), (isolation));            \
  }                                                                                                \
  static void TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)(void)

/*
 * Variants for TRIAXI_Error ("user_error"/"test_error") coverage: these check
 * the specific `reason` string (see triaxi_error_name in triax.h) rather than
 * just "some test_error happened", so a test asserting e.g. malformed_params
 * can't silently pass because a different error fired instead.
 *
 * triaxi_validate_user_error_attrs additionally forwards designated-init
 * attrs (e.g. `.init = some_fixture`) to the registered test. This is needed
 * for errors raised from init/fini fixtures (skip_in_fixture,
 * assertion_in_fixture): unlike the subject body, a fixture attached via
 * .init/.fini runs unconditionally on *every* invocation of the registered
 * test, including the outer run's own meta-check invocation — so the fixture
 * itself must check triaxi_validate_is_child() before misbehaving, or the
 * outer role would trip the same error before it ever gets to spawn a child.
 */
#define triaxi_validate_user_error(suite, name, reason)                                            \
  static void TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)(void);                         \
  triax_test(suite, name, ) {                                                                      \
    if (triaxi_validate_is_child()) {                                                              \
      TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)();                                     \
      return;                                                                                      \
    }                                                                                              \
    triaxi_validate_run_user_error(#suite "::" #name, (reason));                                   \
  }                                                                                                 \
  static void TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)(void)

#define triaxi_validate_opts_user_error(suite, name, reason, timeout_ms, isolation)                \
  static void TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)(void);                         \
  triax_test(suite, name, ) {                                                                      \
    if (triaxi_validate_is_child()) {                                                              \
      TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)();                                     \
      return;                                                                                      \
    }                                                                                              \
    triaxi_validate_run_user_error_opts(#suite "::" #name, (reason), (timeout_ms), (isolation));   \
  }                                                                                                 \
  static void TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)(void)

#define triaxi_validate_user_error_attrs(suite, name, reason, ...)                                 \
  static void TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)(void);                         \
  triax_test(suite, name, __VA_ARGS__) {                                                           \
    if (triaxi_validate_is_child()) {                                                              \
      TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)();                                     \
      return;                                                                                      \
    }                                                                                              \
    triaxi_validate_run_user_error(#suite "::" #name, (reason));                                   \
  }                                                                                                 \
  static void TRIAXI_SV_CAT3(triaxi_validate_body_, suite, _##name)(void)

#endif /* TRIAX_SELFVERIFY_H */
