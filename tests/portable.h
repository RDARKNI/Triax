// Portable millisecond sleep for self-tests that deliberately hang or spin,
// relying on Triax's own timeout/isolation machinery to interrupt them.
// POSIX's sleep()/nanosleep() (seconds/struct-timespec granularity) aren't
// available on Windows, which has its own Sleep(ms) in <windows.h> instead —
// the exact duration only ever needs to be "much longer than the test's
// timeout", never precise, so one portable millisecond-based helper covers
// every caller.
#ifndef TRIAX_TESTS_PORTABLE_H
#define TRIAX_TESTS_PORTABLE_H

#ifdef _WIN32
# include <windows.h>
static inline void triaxi_test_sleep_ms(long ms) { Sleep((DWORD)ms); }
#else
# include <time.h>
static inline void triaxi_test_sleep_ms(long ms) {
  struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
  nanosleep(&ts, NULL);
}
#endif

#endif /* TRIAX_TESTS_PORTABLE_H */
