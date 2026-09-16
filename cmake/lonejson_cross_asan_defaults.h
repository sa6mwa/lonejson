#ifndef LONEJSON_CROSS_ASAN_DEFAULTS_H
#define LONEJSON_CROSS_ASAN_DEFAULTS_H

/*
 * GCC's LeakSanitizer cannot run under QEMU user-mode emulation.  This header
 * is force-included only by the cross ASan test configuration, so its weak
 * definition is present in every test executable before libasan initializes.
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
const char *__asan_default_options(void);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
const char *__asan_default_options(void)
{
  return "detect_leaks=0:abort_on_error=1:halt_on_error=1";
}

#endif
