/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2012  Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef _WIN32

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/thread.h>
#include <freetds/time.h>

/* implementation for systems that support Condition Variables */
typedef VOID(WINAPI * init_cv_t) (TDS_CONDITION_VARIABLE * cv);
typedef BOOL(WINAPI * sleep_cv_t) (TDS_CONDITION_VARIABLE * cv, CRITICAL_SECTION * crit, DWORD milli);
typedef VOID(WINAPI * wake_cv_t) (TDS_CONDITION_VARIABLE * cv);

static init_cv_t init_cv = NULL;
static sleep_cv_t sleep_cv = NULL;
static wake_cv_t wake_cv = NULL;

static int
new_cond_init(tds_condition * cond)
{
	init_cv(&cond->cv);
	return 0;
}

static int
new_cond_destroy(tds_condition * cond)
{
	return 0;
}

static int
new_cond_signal(tds_condition * cond)
{
	wake_cv(&cond->cv);
	return 0;
}

static int
new_cond_timedwait(tds_condition * cond, tds_raw_mutex * mtx, int timeout_sec)
{
	if (sleep_cv(&cond->cv, &mtx->crit, timeout_sec <= 0 ? INFINITE : timeout_sec * 1000))
		return 0;
	return ETIMEDOUT;
}


/* implementation for systems that do not support Condition Variables */
static int
old_cond_init(tds_condition * cond)
{
	cond->ev = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!cond->ev)
		return ENOMEM;
	return 0;
}

static int
old_cond_destroy(tds_condition * cond)
{
	CloseHandle(cond->ev);
	return 0;
}

static int
old_cond_signal(tds_condition * cond)
{
	SetEvent(cond->ev);
	return 0;
}

static int
old_cond_timedwait(tds_condition * cond, tds_raw_mutex * mtx, int timeout_sec)
{
	int res;

	LeaveCriticalSection(&mtx->crit);
	res = WaitForSingleObject(cond->ev, timeout_sec < 0 ? INFINITE : timeout_sec * 1000);
	EnterCriticalSection(&mtx->crit);
	return res == WAIT_TIMEOUT ? ETIMEDOUT : 0;
}


/* dummy implementation used to detect which version above to use */
static void
detect_cond(void)
{
	/* detect if this Windows support condition variables */
	HMODULE mod = GetModuleHandle(TEXT("kernel32"));

	init_cv  = (init_cv_t)  GetProcAddress(mod, "InitializeConditionVariable");
	sleep_cv = (sleep_cv_t) GetProcAddress(mod, "SleepConditionVariableCS");
	wake_cv  = (wake_cv_t)  GetProcAddress(mod, "WakeConditionVariable");

	if (init_cv && sleep_cv && wake_cv) {
		tds_raw_cond_init      = new_cond_init;
		tds_raw_cond_destroy   = new_cond_destroy;
		tds_raw_cond_signal    = new_cond_signal;
		tds_raw_cond_timedwait = new_cond_timedwait;
	} else {
		tds_raw_cond_init      = old_cond_init;
		tds_raw_cond_destroy   = old_cond_destroy;
		tds_raw_cond_signal    = old_cond_signal;
		tds_raw_cond_timedwait = old_cond_timedwait;
	}
}

static int
detect_cond_init(tds_condition * cond)
{
	detect_cond();
	return tds_raw_cond_init(cond);
}

static int
detect_cond_destroy(tds_condition * cond)
{
	detect_cond();
	return tds_raw_cond_destroy(cond);
}

static int
detect_cond_signal(tds_condition * cond)
{
	detect_cond();
	return tds_raw_cond_signal(cond);
}

static int
detect_cond_timedwait(tds_condition * cond, tds_raw_mutex * mtx, int timeout_sec)
{
	detect_cond();
	return tds_raw_cond_timedwait(cond, mtx, timeout_sec);
}

int (*tds_raw_cond_init) (tds_condition * cond) = detect_cond_init;
int (*tds_raw_cond_destroy) (tds_condition * cond) = detect_cond_destroy;
int (*tds_raw_cond_signal) (tds_condition * cond) = detect_cond_signal;
int (*tds_raw_cond_timedwait) (tds_condition * cond, tds_raw_mutex * mtx, int timeout_sec) = detect_cond_timedwait;

#elif defined(TDS_HAVE_PTHREAD_MUTEX) && !defined(TDS_NO_THREADSAFE)

#include <freetds/thread.h>
#include <freetds/time.h>

/* check if we can use pthread_cond_timedwait_relative_np */
#undef USE_COND_TIMEDWAIT_RELATIVE
#if defined(HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP) && \
	(!defined(__ANDROID__) || ((__ANDROID_API__ < 21) && defined(HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE)))
#define USE_COND_TIMEDWAIT_RELATIVE 1
#endif

/* check if we can use clock_gettime */
#undef USE_CLOCK_GETTIME_IN_COND
#if !defined(USE_COND_TIMEDWAIT_RELATIVE) && \
	defined(HAVE_CLOCK_GETTIME) && (defined(CLOCK_REALTIME) || defined(CLOCK_MONOTONIC))
#define USE_CLOCK_GETTIME_IN_COND 1
#endif

/* check if we can use CLOCK_MONOTONIC for conditions */
#undef USE_MONOTONIC_CLOCK_IN_COND
#if defined(USE_CLOCK_GETTIME_IN_COND) && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK) && defined(CLOCK_MONOTONIC)
#define USE_MONOTONIC_CLOCK_IN_COND 1
#endif

int tds_raw_cond_init(tds_condition *cond)
{
#ifdef USE_MONOTONIC_CLOCK_IN_COND
	int res;
	pthread_condattr_t attr;

	pthread_condattr_init(&attr);
	res = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	if (!res)
		res = pthread_cond_init(cond, &attr);
	pthread_condattr_destroy(&attr);
	return res;
#else
	return pthread_cond_init(cond, NULL);
#endif
}

int tds_raw_cond_timedwait(tds_condition *cond, tds_raw_mutex *mtx, int timeout_sec)
{
	struct timespec ts;

	if (timeout_sec <= 0)
		return tds_raw_cond_wait(cond, mtx);

#if defined(USE_COND_TIMEDWAIT_RELATIVE)
	ts.tv_sec = timeout_sec;
	ts.tv_nsec = 0;
	return pthread_cond_timedwait_relative_np(cond, mtx, &ts);
#else

#  if defined(USE_MONOTONIC_CLOCK_IN_COND)
	clock_gettime(CLOCK_MONOTONIC, &ts);
#  elif defined(USE_CLOCK_GETTIME_IN_COND)
	clock_gettime(CLOCK_REALTIME, &ts);
#  elif defined(HAVE_GETTIMEOFDAY)
	do {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		ts.tv_sec = tv.tv_sec;
		ts.tv_nsec = tv.tv_usec * 1000u;
	} while(0);
#  else
#  error No way to get a proper time!
#  endif

	ts.tv_sec += timeout_sec;
	return pthread_cond_timedwait(cond, mtx, &ts);
#endif
}

#endif
