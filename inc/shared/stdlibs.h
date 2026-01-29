#pragma once


/**
*	Include based on whether the unit including is .c or .cpp
**/
#ifndef __cplusplus
    // C STD Headers:
    #include <ctype.h>
    #include <errno.h>
    #include <float.h>
    #include <math.h>
    #include <stdio.h>
    #include <stdarg.h>
    #include <string.h>
    #include <stdlib.h>
    #include <stdint.h>
    #include <stdbool.h>
    #include <inttypes.h>
    #include <limits.h>
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