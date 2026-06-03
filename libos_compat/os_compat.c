/*
 * Qualcomm OSAL compatibility shim for Adreno200 userspace blobs.
 *
 * These symbols existed in Gingerbread libutils/liblog/libgsl but were
 * removed in ICS. Providing them here prevents relocation failures when
 * the Adreno EGL/GLES blobs are loaded by SurfaceFlinger.
 *
 * This library is added to SurfaceFlinger's dependencies so the symbols
 * are already resolved when libEGL_adreno200.so is dlopen-ed.
 */

#include <android/log.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>

/* ── Logging ─────────────────────────────────────────────────────────────── */

int os_alogDebugMask   = 0;
int os_log_get_flags_v = 0;

void os_alog(int level, const char *tag, const char *msg)
{
    __android_log_write(level, tag ? tag : "adreno", msg ? msg : "");
}

void os_log(int level, const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(level, tag ? tag : "adreno", fmt, ap);
    va_end(ap);
}

void os_logsystem(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_DEBUG, "adreno", fmt, ap);
    va_end(ap);
}

int os_log_get_flags(void) { return os_log_get_flags_v; }

/* ── Memory ──────────────────────────────────────────────────────────────── */

void *os_malloc(unsigned int size)               { return malloc(size); }
void *os_calloc(unsigned int n, unsigned int s)  { return calloc(n, s); }
void *os_realloc(void *p, unsigned int size)     { return realloc(p, size); }
void  os_free(void *p)                           { free(p); }

void  os_memset(void *d, int c, unsigned int n)           { memset(d, c, n); }
void *os_memcpy(void *d, const void *s, unsigned int n)   { return memcpy(d, s, n); }
int   os_memcmp(const void *a, const void *b, unsigned int n){ return memcmp(a, b, n); }

void *os_nameshare_malloc(const char *name, unsigned int size){ (void)name; return malloc(size); }
void  os_nameshare_free(void *p)                             { free(p); }
void *os_nameshare_map(const char *name, unsigned int *size) { (void)name; (void)size; return NULL; }

/* ── Strings ─────────────────────────────────────────────────────────────── */

int os_snprintf(char *buf, unsigned int n, const char *fmt, ...)
{
    int r; va_list ap; va_start(ap, fmt); r = vsnprintf(buf, (size_t)n, fmt, ap); va_end(ap);
    return r;
}
int os_vsnprintf(char *buf, unsigned int n, const char *fmt, va_list ap)
{
    return vsnprintf(buf, (size_t)n, fmt, ap);
}
int          os_strcmp(const char *a, const char *b)                   { return strcmp(a, b); }
int          os_strncmp(const char *a, const char *b, unsigned int n)  { return strncmp(a, b, n); }
unsigned int os_strlen(const char *s)                                  { return (unsigned int)strlen(s); }
char        *os_strlcpy(char *d, const char *s, unsigned int n)
{
    if (n == 0) return d;
    strncpy(d, s, n);
    d[n-1] = '\0';
    return d;
}
char        *os_strlcat(char *d, const char *s, unsigned int n)
{
    if (n == 0) return d;
    strncat(d, s, n - strlen(d) - 1);
    return d;
}

/* ── Threading ───────────────────────────────────────────────────────────── */

void *os_mutex_create(void)
{
    pthread_mutex_t *m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (m) pthread_mutex_init(m, NULL);
    return m;
}
void *os_mutex_open(const char *name)  { (void)name; return os_mutex_create(); }
void  os_mutex_free(void *m)           { if (m) { pthread_mutex_destroy((pthread_mutex_t*)m); free(m); } }
void  os_mutex_lock(void *m)           { if (m) pthread_mutex_lock((pthread_mutex_t*)m); }
void  os_mutex_unlock(void *m)         { if (m) pthread_mutex_unlock((pthread_mutex_t*)m); }

/* ── TLS ─────────────────────────────────────────────────────────────────── */

void *os_tls_alloc(void)
{
    pthread_key_t *k = (pthread_key_t *)malloc(sizeof(pthread_key_t));
    if (k && pthread_key_create(k, NULL) != 0) { free(k); return NULL; }
    return k;
}
void  os_tls_free(void *k)            { if (k) { pthread_key_delete(*(pthread_key_t*)k); free(k); } }
void  os_tls_write(void *k, void *v)  { if (k) pthread_setspecific(*(pthread_key_t*)k, v); }
void *os_tls_read(void *k)            { return k ? pthread_getspecific(*(pthread_key_t*)k) : NULL; }

/* ── Misc ─────────────────────────────────────────────────────────────────── */

unsigned int os_process_getid(void) { return (unsigned int)getpid(); }
void         os_exit(int code)      { exit(code); }

unsigned int os_timestamp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

void os_enable_memoryleakcheck(int enable) { (void)enable; }

/* no-op stubs for dynamic-library helpers — Adreno doesn't need these at runtime */
void *os_lib_map(const char *p)            { (void)p; return (void*)1; /* non-NULL sentinel */ }
void *os_lib_getaddr(void *h, const char *s){ (void)h; (void)s; return NULL; }
void  os_lib_unmap(void *h)                { (void)h; }

/* ── gsl_cffdump stubs (debug crash-dump) ─────────────────────────────────── */
void gsl_cffdump_surface_params_write(void *a, void *b, void *c){ (void)a;(void)b;(void)c; }
void gsl_cffdump_writeverifyfile(void *a, void *b)              { (void)a;(void)b; }

/* ── gsl_perfcounter stubs (not available on MSM7x27A) ──────────────────── */
int  gsl_perfcounter_trylock(void *a, void *b) { (void)a;(void)b; return 0; }
void gsl_perfcounter_unlock(void *a)           { (void)a; }

/* ── gsl_memory_* stubs (functions missing from the vendor libgsl.so) ────── */
int gsl_memory_copy(void *dst, void *src, unsigned int nbytes)
    { (void)dst;(void)src;(void)nbytes; return 0; }
int gsl_memory_copy_multiple(void *ops, unsigned int count)
    { (void)ops;(void)count; return 0; }
int gsl_memory_read_multiple(void *ops, unsigned int count)
    { (void)ops;(void)count; return 0; }
