/*
Copyright (C) 2023 Andrey Nazarov

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "shared/shared.h"
#include "shared/util/util_list.h"
#include "system/pthread.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include <errno.h>

typedef struct {
    list_t entry;
    void *(*func)(void *);
    void *arg, *ret;
    HANDLE handle;
    DWORD id;
} winthread_t;

static LIST_DECL(threads);
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static winthread_t *find_thread(pthread_t thread)
{
    winthread_t *t;

    LIST_FOR_EACH(winthread_t, t, &threads, entry)
        if (t->id == thread)
            return t;

    return NULL;
}

static unsigned __stdcall thread_func(void *arg)
{
    winthread_t *t = arg;
    t->ret = t->func(t->arg);
    return 0;
}

/**
*	Mutex operations backed by Windows SRWLOCK.
**/
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr)
{
	(void)mutexattr;
	InitializeSRWLock((PSRWLOCK)&mutex->handle);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	AcquireSRWLockExclusive((PSRWLOCK)&mutex->handle);
	return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	return TryAcquireSRWLockExclusive((PSRWLOCK)&mutex->handle) ? 0 : EBUSY;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	ReleaseSRWLockExclusive((PSRWLOCK)&mutex->handle);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	(void)mutex;
	return 0;
}

/**
*	Condition variable operations backed by Windows CONDITION_VARIABLE.
**/
int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *cond_attr)
{
	(void)cond_attr;
	InitializeConditionVariable((PCONDITION_VARIABLE)&cond->handle);
	return 0;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	WakeConditionVariable((PCONDITION_VARIABLE)&cond->handle);
	return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	return SleepConditionVariableSRW((PCONDITION_VARIABLE)&cond->handle, (PSRWLOCK)&mutex->handle, INFINITE, 0) ? 0 : ETIMEDOUT;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	(void)cond;
	return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg)
{
    int ret = 0;
    pthread_mutex_lock(&mutex);
    winthread_t *t = malloc(sizeof(*t));
    if (!t) {
        ret = EAGAIN;
        goto done;
    }
    t->func = start_routine;
    t->arg = arg;
    t->handle = (HANDLE)_beginthreadex(NULL, 0, thread_func, t, 0, thread);
    if (!t->handle) {
        free(t);
        ret = EAGAIN;
        goto done;
    }
    t->id = *thread;
    List_Append(&threads, &t->entry);
done:
    pthread_mutex_unlock(&mutex);
    return ret;
}

int pthread_join(pthread_t thread, void **retval)
{
    int ret = 0;
    pthread_mutex_lock(&mutex);
    winthread_t *t = find_thread(thread);
    if (!t) {
        ret = ESRCH;
        goto done;
    }
    WaitForSingleObject(t->handle, INFINITE);
    CloseHandle(t->handle);
    List_Remove(&t->entry);
    if (retval)
        *retval = t->ret;
    free(t);
done:
    pthread_mutex_unlock(&mutex);
    return ret;
}

/**
*	@brief	Return thread identifier of calling thread.
*	@return	Calling thread identifier as pthread_t.
**/
pthread_t pthread_self( void ) {
	return (pthread_t)GetCurrentThreadId();
}

/**
*	@brief	Compare two thread identifiers for equality.
*	@param	t1	First thread identifier.
*	@param	t2	Second thread identifier.
*	@return	Non-zero if thread identifiers are equal, 0 otherwise.
**/
int pthread_equal( pthread_t t1, pthread_t t2 ) {
	return ( t1 == t2 );
}

/**
*	@brief	Query execution CPU time for a Windows thread emulation handle in nanoseconds.
*	@param	thread		Thread ID to inspect (or 0 / pthread_self() for calling thread).
*	@param	out_time_ns	[out] Receives thread CPU execution time in nanoseconds.
*	@return	0 on success, or non-zero error code on failure.
**/
int pthread_getcputime( pthread_t thread, uint64_t *out_time_ns ) {
	// Guard against null output pointer.
	if ( !out_time_ns ) {
		return EINVAL;
	}

	HANDLE hThread = (HANDLE)0;
	BOOL mustClose = FALSE;

	// Use fast pseudo-handle for current thread without handle allocation.
	if ( thread == 0 || thread == (pthread_t)GetCurrentThreadId() ) {
		hThread = GetCurrentThread();
	} else {
		// First check whether the thread is tracked in our internal thread registry.
		pthread_mutex_lock( &mutex );
		winthread_t *t = find_thread( thread );
		if ( t && t->handle ) {
			hThread = t->handle;
		}
		pthread_mutex_unlock( &mutex );

		// If untracked, attempt opening thread handle with query privileges.
		if ( !hThread ) {
			hThread = OpenThread( THREAD_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)thread );
			if ( !hThread ) {
				hThread = OpenThread( THREAD_QUERY_INFORMATION, FALSE, (DWORD)thread );
			}
			if ( !hThread ) {
				return ESRCH;
			}
			mustClose = TRUE;
		}
	}

	FILETIME creationTime = { 0 }, exitTime = { 0 }, kernelTime = { 0 }, userTime = { 0 };
	const BOOL ok = GetThreadTimes( hThread, &creationTime, &exitTime, &kernelTime, &userTime );
	if ( mustClose ) {
		CloseHandle( hThread );
	}

	// Validate query success and convert 100-nanosecond intervals to nanoseconds.
	if ( ok ) {
		ULARGE_INTEGER kt, ut;
		kt.LowPart = kernelTime.dwLowDateTime;
		kt.HighPart = kernelTime.dwHighDateTime;
		ut.LowPart = userTime.dwLowDateTime;
		ut.HighPart = userTime.dwHighDateTime;
		*out_time_ns = ( kt.QuadPart + ut.QuadPart ) * 100ULL;
		return 0;
	}

	return EIO;
}
