/*
Copyright (C) 2026 Andrey Nazarov

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
#include "system/pthread.h"

#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

/**
*	@brief	Query execution CPU time for a POSIX thread in nanoseconds.
*	@param	thread		Thread handle to inspect (or 0 / pthread_self() for calling thread).
*	@param	out_time_ns	[out] Receives thread CPU execution time in nanoseconds.
*	@return	0 on success, or non-zero error code on failure.
**/
int pthread_getcputime( pthread_t thread, uint64_t *out_time_ns ) {
	// Guard against null output pointer.
	if ( !out_time_ns ) {
		return EINVAL;
	}

	// Direct fast path for the calling thread using standard POSIX CLOCK_THREAD_CPUTIME_ID.
	if ( thread == 0 || pthread_equal( thread, pthread_self() ) ) {
		struct timespec ts;
		if ( clock_gettime( CLOCK_THREAD_CPUTIME_ID, &ts ) == 0 ) {
			*out_time_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
			return 0;
		}
		return errno != 0 ? errno : -1;
	}

#if defined(_POSIX_THREAD_CPUTIME) && ( _POSIX_THREAD_CPUTIME >= 0 )
	clockid_t clock_id;
	if ( pthread_getcpuclockid( thread, &clock_id ) == 0 ) {
		struct timespec ts;
		if ( clock_gettime( clock_id, &ts ) == 0 ) {
			*out_time_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
			return 0;
		}
		return errno != 0 ? errno : -1;
	}
#endif

	return ENOSYS;
}
