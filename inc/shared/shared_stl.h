/********************************************************************
*
*
*	Optional C++ STL-heavy shared include pack.
*	Use this in headers/TUs that require broad STL facilities.
*
*
********************************************************************/
#pragma once

#ifndef __cplusplus
	#error "shared_stl.h is C++-only"
#endif

#include <deque>
#include <list>
#include <map>
#include <new>
#include <numbers>
#include <numeric>
#include <random>
#include <set>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <type_traits>
