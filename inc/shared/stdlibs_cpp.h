#pragma once


/**
*	Include based on whether the unit including is .c or .cpp
**/
#ifdef __cplusplus
	/**
	*	Minimal C++ standard library headers.
	*	Kept minimal to reduce parse time across 362+ translation units.
	*	Heavy/less-common STL headers are now exclusively in the PCH.
	**/

	// C STD Headers (lightweight wrappers):
	#include <cctype>
	#include <cerrno>
	#include <cfloat>
	#include <cinttypes>
	#include <climits>
	#include <cmath>
	#include <cstdarg>
	#include <cstdbool>
	#include <cstdint>
	#include <cstdio>
	#include <cstdlib>
	#include <cstring>
	#include <ctime>

	/**
	*	Mathematical constants definitions.
	*	Portable fallbacks in case the platform doesn't define them in <cmath>.
	**/
	#ifndef M_PI
		#define M_PI 3.14159265358979323846
	#endif
	#ifndef M_PI_2
		#define M_PI_2 1.57079632679489661923
	#endif

	/**
	*	Essential C++ headers needed by shared_cpp.h and core utilities.
	*	Less common headers (chrono, mutex, thread, unordered_*, tuple, etc.)
	*	are now in PCH only to reduce redundant parsing.
	**/
	#include <type_traits>
	#include <limits>
	#include <memory>
	#include <version>
	#include <iterator>

	// Commonly used containers/utilities:
	#include <string>
	#include <vector>
	#include <map>
	#include <set>
	#include <algorithm>

	// Random number support (used by shared_cpp.h):
	#include <random>
#endif//__cplusplus