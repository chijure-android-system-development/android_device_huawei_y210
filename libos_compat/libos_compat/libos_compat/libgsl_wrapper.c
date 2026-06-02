/*
 * libgsl.so wrapper — pulls in libos_compat (OSAL shim) and libgsl_real
 * before any Adreno blob runs, so os_alog / os_malloc / gsl_cffdump_* etc.
 * are already resolved in the process' symbol table.
 *
 * No code needed: the NEEDED entries in the Android.mk do the work.
 */
void __libgsl_wrapper_placeholder(void) {}
