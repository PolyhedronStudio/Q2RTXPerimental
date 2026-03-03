#pragma once


/**
*	Include based on whether the unit including is .c or .cpp
**/
#ifndef __cplusplus
    // C STD Headers:
    #include <ctype.h>
    #include <errno.h>
    #include <float.h>
    #include <inttypes.h>
    #include <limits.h>
    #include <math.h>
    #include <stdarg.h>
    #include <stdbool.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
	//#include <tgmath.h> // <Q2RTXP>: WID: VS2026(28-02-2026): warning UCRT4000: This header does not conform to the C99 standard. C99 functionality is available when compiling in C11 mode or higher (/std:c11). Functionality equivalent to the type-generic functions provided by tgmath.h is available in <ctgmath> when compiling as C++. If compiling in C++17 mode or higher (/std:c++17), this header will automatically include <ctgmath> instead. You can define _CRT_SILENCE_NONCONFORMING_TGMATH_H to acknowledge that you have received this warning.
    #include <time.h>
#else//__cplusplus
    // C STD Headers:
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
	#include <ctgmath>
    #include <ctime>

    // C++ STL Headers:
    #include <version>
    #include <algorithm> // std::min, std::max etc, buuut still got vkpt code in C so.
    //#include <array>
    //#include <bit>
    #include <chrono>
    #include <type_traits>
    #include <algorithm>
    //#include <array>
	#include <limits>
    #include <list>
    //#include <functional>
    #include <map>
	#include <new>
    #include <numbers>
    #include <numeric>
    #include <unordered_map>
    #include <set>
    #include <unordered_set>
    #include <random>
    #include <string_view>
    #include <variant>
    #include <vector>

#endif//__cplusplus