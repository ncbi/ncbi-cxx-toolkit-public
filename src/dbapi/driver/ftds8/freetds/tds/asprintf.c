/*
 * vasprintf(3), asprintf(3)
 * 20020809 entropy@tappedin.com
 * public domain.  no warranty.  use at your own risk.  have a nice day.
 *
 * Assumes that mprotect(2) granularity is one page.  May need adjustment
 * on systems with unusual protection semantics.
 */

#include <tds_config.h>

#if !HAVE_VASPRINTF

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef NCBI_FTDS
#ifndef WIN32
#include <unistd.h>
#endif
#endif
#include <string.h>
#ifndef _REENTRANT
#ifndef NCBI_FTDS
#ifndef WIN32
#include <sys/mman.h>
#endif
#endif
#include <setjmp.h>
#include <signal.h>
#include <assert.h>
#endif

static char  software_version[]   = "$Id$";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

#ifndef _REENTRANT
static jmp_buf env;

static void
sigsegv(int sig)
{
  longjmp(env, 1);
}
#endif

#ifdef NCBI_FTDS
#define CHUNKSIZE 512
int
vasprintf(char **ret, const char *fmt, va_list ap)
{
#if HAVE_VSNPRINTF
	int chunks;
	size_t buflen;
	char *buf;
	int len;

	chunks = ((strlen(fmt) + 1) / CHUNKSIZE) + 1;
	buflen = chunks * CHUNKSIZE;
	for (;;) {
		if ((buf = malloc(buflen)) == NULL) {
			*ret = NULL;
			return -1;
		}
		len = vsnprintf(buf, buflen, fmt, ap);
		if (len >= 0 && len < (buflen - 1)) {
			break;
		}
		free(buf);
		buflen = (++chunks) * CHUNKSIZE;
		if (len >= 0 && len >= buflen) {
			buflen = len + 1;
		}
	}
	*ret = buf;
	return len;
#else /* HAVE_VSNPRINTF */
#ifdef _REENTRANT
	FILE *fp;
#else /* !_REENTRANT */
	static FILE *fp = NULL;
#endif /* !_REENTRANT */
	int len;
	char *buf;

	*ret = NULL;

#ifdef _REENTRANT

# ifdef WIN32
#  error Win32 do not have /dev/null, should use vsnprintf version
# endif

	if ((fp = fopen(_PATH_DEVNULL, "w")) == NULL)
		return -1;
#else /* !_REENTRANT */
	if ((fp == NULL) && ((fp = fopen(_PATH_DEVNULL, "w")) == NULL))
		return -1;
#endif /* !_REENTRANT */

	len = vfprintf(fp, fmt, ap);

#ifdef _REENTRANT
	if (fclose(fp) != 0)
		return -1;
#endif /* _REENTRANT */

	if (len < 0)
		return len;
	if ((buf = malloc(len + 1)) == NULL)
		return -1;
	if (vsprintf(buf, fmt, ap) != len)
		return -1;
	*ret = buf;
	return len;
#endif /* HAVE_VSNPRINTF */
}
#else
int
vasprintf(char **ret, const char *fmt, va_list ap)
{
#ifdef _REENTRANT
  FILE *fp;
  int len;
  char *buf;

  *ret = NULL;
  if ((fp = fopen("/dev/null", "w")) == NULL)
    return -1;
  len = vfprintf(fp, fmt, ap);
  if (fclose(fp) != 0)
    return -1;
  if (len < 0)
    return len;
  if ((buf = malloc(len + 1)) == NULL)
    return -1;
  vsprintf(buf, fmt, ap);
  *ret = buf;
  return len;
#else
# ifndef _SC_PAGE_SIZE
#  define _SC_PAGE_SIZE _SC_PAGESIZE
# endif
  volatile char *buf = NULL;
  volatile unsigned int pgs;
  struct sigaction sa, osa;
  int len;
#ifdef HAVE_GETPAGESIZE
  long pgsize = getpagesize();
#else
  long pgsize = sysconf(_SC_PAGE_SIZE);
#endif

  pgs = ((strlen(fmt) + 1) / pgsize) + 1;
  if (sigaction(SIGSEGV, NULL, &osa)) {
    *ret = NULL;
    return -1;
  }
  sa.sa_handler = sigsegv;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (setjmp(env) != 0) {
    mprotect((void *) (buf + pgs * pgsize), pgsize, PROT_READ|PROT_WRITE);
    free((void *) buf);
    pgs++;
  }
  if ((buf = valloc((pgs + 1) * pgsize)) == NULL) {
    *ret = NULL;
    return -1;
  }
  assert(((unsigned long) buf % pgsize) == 0);
  if (sigaction(SIGSEGV, &sa, NULL)) {
    free((void *) buf);
    *ret = NULL;
    return -1;
  }
  mprotect((void *) (buf + pgs * pgsize), pgsize, PROT_NONE);
  len = vsprintf((void *) buf, fmt, ap);
  mprotect((void *) (buf + pgs * pgsize), pgsize, PROT_READ|PROT_WRITE);
  if (sigaction(SIGSEGV, &osa, NULL)) {
    free((void *) buf);
    *ret = NULL;
    return -1;
  }
  if (len < 0) {
    free((void *) buf);
    *ret = NULL;
    return len;
  }
  if ((buf = realloc((void *) buf, len + 1)) == NULL) {
    *ret = NULL;
    return -1;
  }
  *ret = (char *) buf;
  return len;
#endif
}
#endif /* NCBI_FTDS */

int
asprintf(char **ret, const char *fmt, ...)
{
  int len;
  va_list ap;

  va_start(ap, fmt);
  len = vasprintf(ret, fmt, ap);
  va_end(ap);
  return len;
}
#endif /* HAVE_VASPRINTF */
