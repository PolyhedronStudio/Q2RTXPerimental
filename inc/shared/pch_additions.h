/********************************************************************
*
*
*	Precompiled Header Additions
*
*	Additional heavy STL headers for PCH that are not in stdlibs_cpp.h
*	to reduce redundant parsing across translation units.
*
*	This header should ONLY be included in CMake target_precompile_headers,
*	not directly in source files.
*
*
********************************************************************/
#pragma once

#ifdef __cplusplus
	/**
	*	Heavy/less-common STL headers that are not in stdlibs_cpp.h.
	*	These are in PCH only to optimize build times.
	**/
	#include <chrono>
	#include <mutex>
	#include <shared_mutex>
	#include <thread>
	#include <unordered_map>
	#include <unordered_set>
	#include <tuple>
#endif // __cplusplus
