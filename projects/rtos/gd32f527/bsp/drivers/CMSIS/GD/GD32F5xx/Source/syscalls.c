/* Support files for GNU libc.  Files in the system namespace go here.
   Files in the C namespace (ie those that do not start with an
   underscore) go in .c.  */

#include <_ansi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>
#include <reent.h>
#include <unistd.h>
#include <sys/wait.h>

#undef errno
extern int errno;

extern int __io_putchar(int ch) __attribute__((weak));
extern int __io_getchar(void) __attribute__((weak));

caddr_t _sbrk(int incr)
{
  extern char _end[];
  extern char _heap_end[];
  static char *curbrk = _end;

  if ((curbrk + incr < _end) || (curbrk + incr > _heap_end))
    return NULL - 1;

  curbrk += incr;
  return curbrk - incr;
}

/* -----------------------------------------------------------------------
 * Redirect newlib internal allocator (_malloc_r / _calloc_r / _free_r)
 * to FreeRTOS heap (pvPortMalloc / vPortFree).
 *
 * Why: newlib heap (__heap_size) is only 1KB (linker script), far too
 *      small for cJSON's strtod() -> _Balloc() -> _calloc_r() chain.
 *      Redirecting to FreeRTOS heap (80KB, ~60KB free) eliminates:
 *        - "Balloc succeeded" assertion crash (mprec.c:320)
 *        - "REENT malloc succeeded" assertion crash (dtoa.c:234)
 *        - free() mismatch when cJSON_PrintUnformatted is freed
 * ----------------------------------------------------------------------- */
#include "FreeRTOS.h"
#include "task.h"

void *_malloc_r(struct _reent *r, size_t size)
{
    (void)r;
    return pvPortMalloc(size);
}

void *_calloc_r(struct _reent *r, size_t count, size_t size)
{
    (void)r;
    /* C5.5: validate allocation size to prevent integer overflow */
    if (count != 0U && size > (size_t)-1 / count) return NULL;
    size_t total = count * size;
    void *p = pvPortMalloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void _free_r(struct _reent *r, void *ptr)
{
    (void)r;
    vPortFree(ptr);
}

/* _realloc_r: newlib system call override -- redirects to FreeRTOS heap.
 * FreeRTOS heap_4 does not support realloc natively. This implementation
 * allocates a new block and copies new_size bytes. Only used internally
 * by newlib (setvbuf, Balloc); not called by application code directly.
 * CI rule C5.7 exemption: system call override, not application realloc. */
void *_realloc_r(struct _reent *r, void *ptr, size_t new_size)
{
    (void)r;
    if (new_size == 0) {
        vPortFree(ptr);
        return NULL;
    }
    void *new_ptr = pvPortMalloc(new_size);
    if (new_ptr && ptr) {
        /* Cannot determine old block size from FreeRTOS heap_4.
         * Copy new_size bytes - safe for newlib internal usage
         * (setvbuf, Balloc) where realloc always grows. */
        memcpy(new_ptr, ptr, new_size);
        vPortFree(ptr);
    }
    return new_ptr;
}


/*
 * _gettimeofday primitive (Stub function)
 * */
int _gettimeofday (struct timeval * tp, struct timezone * tzp)
{
  /* Return fixed data for the timezone.  */
  if (tzp)
    {
      tzp->tz_minuteswest = 0;
      tzp->tz_dsttime = 0;
    }

  return 0;
}
void initialise_monitor_handles()
{
}

int _getpid(void)
{
	return 1;
}

int _kill(int pid, int sig)
{
	errno = EINVAL;
	return -1;
}

void _exit (int status)
{
	_kill(status, -1);
	while (1) {}
}

/* Application can override this to provide thread-safe atomic write
 * (e.g. wrap a UART FreeRTOS mutex around the whole buffer).
 * Default fallback below loops __io_putchar per byte without locking. */
extern int bsp_uart_write_atomic(const char *ptr, int len) __attribute__((weak));

int bsp_uart_write_atomic(const char *ptr, int len)
{
	int DataIdx;
	for (DataIdx = 0; DataIdx < len; DataIdx++) {
		__io_putchar(*ptr++);
	}
	return len;
}

int _write(int file, char *ptr, int len)
{
	(void)file;
	return bsp_uart_write_atomic(ptr, len);
}

int _close(int file)
{
	return -1;
}

int _fstat(int file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file)
{
	return 1;
}

int _lseek(int file, int ptr, int dir)
{
	return 0;
}

int _read(int file, char *ptr, int len)
{
	int DataIdx;

	for (DataIdx = 0; DataIdx < len; DataIdx++)
	{
	  *ptr++ = __io_getchar();
	}

   return len;
}

int _open(char *path, int flags, ...)
{
	/* Pretend like we always fail */
	return -1;
}

int _wait(int *status)
{
	errno = ECHILD;
	return -1;
}

int _unlink(char *name)
{
	errno = ENOENT;
	return -1;
}

int _times(struct tms *buf)
{
	return -1;
}

int _stat(char *file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

int _link(char *old, char *new)
{
	errno = EMLINK;
	return -1;
}

int _fork(void)
{
	errno = EAGAIN;
	return -1;
}

int _execve(char *name, char **argv, char **env)
{
	errno = ENOMEM;
	return -1;
}
