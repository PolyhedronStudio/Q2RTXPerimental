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

#pragma once

#ifndef _WIN32
	#include <pthread.h>
	#include <time.h>
	#include <errno.h>
	#include <stdint.h>

	#ifdef __cplusplus
	extern "C" {
	#endif

	/**
	*	@brief	Query execution CPU time for a POSIX thread in nanoseconds.
	*	@param	thread		Thread handle to inspect (or 0 / pthread_self() for calling thread).
	*	@param	out_time_ns	[out] Receives thread CPU execution time in nanoseconds.
	*	@return	0 on success, or non-zero error code on failure.
	**/
	int pthread_getcputime(pthread_t thread, uint64_t *out_time_ns);

	#ifdef __cplusplus
	}
	#endif
#else

	#include "shared/shared.h"
	#include <stdint.h>

	#define PTHREAD_MUTEX_INITIALIZER   {0}
	#define PTHREAD_COND_INITIALIZER    {0}

	// Extern C
	QEXTERN_C_OPEN

	typedef void pthread_attr_t;
	typedef void pthread_mutexattr_t;
	typedef void pthread_condattr_t;
	typedef unsigned int pthread_t;

	typedef struct {
		void *handle;
	} pthread_mutex_t;

	typedef struct {
		void *handle;
	} pthread_cond_t;

	int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);
	int pthread_mutex_lock(pthread_mutex_t *mutex);
	int pthread_mutex_trylock(pthread_mutex_t *mutex);
	int pthread_mutex_unlock(pthread_mutex_t *mutex);
	int pthread_mutex_destroy(pthread_mutex_t *mutex);

	int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *cond_attr);
	int pthread_cond_signal(pthread_cond_t *cond);
	int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
	int pthread_cond_destroy(pthread_cond_t *cond);

	pthread_t pthread_self(void);
	int pthread_equal(pthread_t t1, pthread_t t2);

	/**
	*	@brief	Query execution CPU time for a Windows thread emulation handle in nanoseconds.
	*	@param	thread		Thread ID to inspect (or 0 / pthread_self() for calling thread).
	*	@param	out_time_ns	[out] Receives thread CPU execution time in nanoseconds.
	*	@return	0 on success, or non-zero error code on failure.
	**/
	int pthread_getcputime(pthread_t thread, uint64_t *out_time_ns);

	int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
					   void *(*start_routine)(void *), void *arg);

	int pthread_join(pthread_t thread, void **retval);

	// Extern C
	QEXTERN_C_CLOSE
#endif
