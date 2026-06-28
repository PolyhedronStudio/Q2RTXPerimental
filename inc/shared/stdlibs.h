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
#endif
