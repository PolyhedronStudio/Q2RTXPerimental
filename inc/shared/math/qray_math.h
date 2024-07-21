/**********************************************************************************************
*
*   A customized version, "Quake RayMath" based on:
*   raymath v1.5 - Math functions to work with Vector2, Vector3, Matrix and Quaternions
*
*
*   Q2RTXPerimental Additions:
*     - Added in operator(s) support for when compiling with C++.
*
*
*   CONVENTIONS:
*     - Matrix structure is defined as row-major (memory layout) but parameters naming AND all
*       math operations performed by the library consider the structure as it was column-major
*       It is like transposed versions of the matrices are used for all the maths
*       It benefits some functions making them cache-friendly and also avoids matrix
*       transpositions sometimes required by OpenGL
*       Example: In memory order, row0 is [m0 m4 m8 m12] but in semantic math row0 is [m0 m1 m2 m3]
*     - Functions are always self-contained, no function use another raymath function inside,
*       required code is directly re-implemented inside
*     - Functions input parameters are always received by value (2 unavoidable exceptions)
*     - Functions use always a "result" variable for return
*     - Functions are always defined inline
*     - Angles are always in radians (QM_DEG2RAD/QM_RAD2DEG macros provided for convenience)
*     - No compound literals used to make sure libray is compatible with C++
*
*
*   CONFIGURATION:
*       #define RAYMATH_IMPLEMENTATION
*           Generates the implementation of the library into the included file.
*           If not defined, the library is in header only mode and can be included in other headers
*           or source files without problems. But only ONE file should hold the implementation.
*
*       #define RAYMATH_STATIC_INLINE
*           Define static inline functions code, so #include header suffices for use.
*           This may use up lots of memory.
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2015-2024 Ramon Santamaria (@raysan5)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#ifndef QRAYMATH_H
#define QRAYMATH_H

#if defined(RAYMATH_IMPLEMENTATION) && defined(RAYMATH_STATIC_INLINE)
#error "Specifying both RAYMATH_IMPLEMENTATION and RAYMATH_STATIC_INLINE is contradictory"
#endif

// Function specifiers definition
#if defined(RAYMATH_IMPLEMENTATION)
#if defined(_WIN32) && defined(BUILD_LIBTYPE_SHARED)
#define RMAPI __declspec(dllexport) extern inline // We are building raylib as a Win32 shared library (.dll)
#elif defined(BUILD_LIBTYPE_SHARED)
#define RMAPI __attribute__((visibility("default"))) // We are building raylib as a Unix shared library (.so/.dylib)
#elif defined(_WIN32) && defined(USE_LIBTYPE_SHARED)
#define RMAPI __declspec(dllimport)         // We are using raylib as a Win32 shared library (.dll)
#else
#define RMAPI extern inline // Provide external definition
#endif
#elif defined(RAYMATH_STATIC_INLINE)
#define RMAPI static inline // Functions may be inlined, no external out-of-line definition
#else
#if defined(__TINYC__)
#define RMAPI static inline // plain inline not supported by tinycc (See issue #435)
#else
#define RMAPI inline        // Functions may be inlined or external definition used
#endif
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef QM_PI
#define QM_PI 3.14159265358979323846f
#endif

#ifndef QM_EPSILON
#define QM_EPSILON 0.000001f
#endif

#ifndef QM_DEG2RAD
#define QM_DEG2RAD (QM_PI/180.0f)
#endif

#ifndef QM_RAD2DEG
#define QM_RAD2DEG (180.0f/QM_PI)
#endif

// Get float vector for Matrix
#ifndef QM_MatrixToFloat
#define QM_MatrixToFloat(mat) ( QM_MatrixToFloatV(mat).v)
#endif

// Get float vector for Vector3
#ifndef QM_Vector3ToFloat
#define QM_Vector3ToFloat(vec) (QM_Vector3ToFloatV(vec).v)
#endif

/**
*   Q2RTXPerimental: Add support for constness float arrays.
**/
// Get const float vector for Vector3
#ifndef QM_Vector3ToConstFloat
#define QM_Vector3ToConstFloat(vec) (QM_Vector3ToConstFloatV(vec).v)
#endif

#ifdef __cplusplus
struct Vector3;
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
#if !defined(RL_VECTOR2_TYPE)
// Vector2 type
typedef struct Vector2 {
    float x;
    float y;

    /**
    *   Q2RTXPerimental: C++ operator support. Implementations are further down this file.
    **/
    #ifdef __cplusplus
    /**
    *   @brief  Vector2 Constructors, including support for passing in a vec2_t
    **/
    [[nodiscard]] inline Vector2() {
        this->x = 0;
        this->y = 0;
    }

    constexpr [[nodiscard]] inline Vector2( const float x, const float y ) {
        this->x = x;
        this->y = y;
    }
    [[nodiscard]] inline Vector2( Vector2 &v ) {
        this->x = v.x;
        this->y = v.y;
    }
    [[nodiscard]] inline Vector2( const Vector2 &v ) {
        this->x = v.x;
        this->y = v.y;
    }
    [[nodiscard]] inline Vector2( vec2_t v2 ) {
        if ( v2 ) {
            this->x = v2[ 0 ];
            this->y = v2[ 1 ];
        }
    }

    /**
    *   Array like component accessors:
    **/
    [[nodiscard]] inline constexpr const float &operator[]( const  size_t i ) const;
    [[nodiscard]] inline constexpr float &operator[]( const size_t i );
    #endif // __cplusplus
} Vector2;
#define RL_VECTOR2_TYPE
#endif

#if !defined(RL_VECTOR3_TYPE)
// Vector3 type
typedef struct Vector3 {
    float x;
    float y;
    float z;

    /**
    *   Q2RTXPerimental: C++ operator support. Implementations are further down this file.
    **/
    #ifdef __cplusplus
    /**
    *   @brief  Vector3 Constructors, including support for passing in a vec3_t
    **/
    [[nodiscard]] inline Vector3() {
        this->x = 0;
        this->y = 0;
        this->z = 0;
    }
    [[nodiscard]] constexpr inline Vector3( const float x, const float y, const float z ) {
        this->x = x;
        this->y = y;
        this->z = z;
    }
    [[nodiscard]] inline Vector3( Vector3 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = v.z;
    }
    [[nodiscard]] inline Vector3( const Vector3 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = v.z;
    }
    [[nodiscard]] inline Vector3( vec3_t v3 ) {
        if ( v3 ) {
            this->x = v3[ 0 ];
            this->y = v3[ 1 ];
            this->z = v3[ 2 ];
        } else {
            this->x = this->y = this->z = 0;
        }
    }
    [[nodiscard]] inline Vector3( const vec3_t v3 ) {
        if ( v3 ) {
            this->x = v3[ 0 ];
            this->y = v3[ 1 ];
            this->z = v3[ 2 ];
        } else {
            this->x = this->y = this->z = 0;
        }
    }

    /**
    *  C++ Array like component accessors:
    **/
    [[nodiscard]] inline constexpr const float &operator[]( const size_t i ) const;
    [[nodiscard]] inline constexpr float &operator[]( const size_t i );
    #endif // #ifdef __cplusplus
} Vector3;
#define RL_VECTOR3_TYPE
#endif

#if !defined(RL_VECTOR4_TYPE)
// Vector4 type
typedef struct Vector4 {
    float x;
    float y;
    float z;
    float w;

    /**
    *   Q2RTXPerimental: C++ operator support. Implementations are further down this file.
    **/
    #ifdef __cplusplus
    /**
    *   @brief  Vector3 Constructors, including support for passing in a vec3_t
    **/
    [[nodiscard]] inline Vector4() {
        this->x = 0;
        this->y = 0;
        this->z = 0;
        this->w = 0;
    }
    [[nodiscard]] inline constexpr Vector4( const float x, const float y, const float z, const float w ) {
        this->x = x;
        this->y = y;
        this->z = z;
        this->w = w;
    }
    [[nodiscard]] inline Vector4( Vector3 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = v.z;
        this->w = 1;
    }
    [[nodiscard]] inline Vector4( Vector4 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = v.z;
        this->w = v.w;
    }
    [[nodiscard]] inline Vector4( const Vector3 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = v.z;
        this->w = 1;
    }
    [[nodiscard]] inline Vector4( const Vector4 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = v.z;
        this->w = v.w;
    }
    //[[nodiscard]] inline Vector4( vec3_t v3 ) {
    //    if ( v3 ) {
    //        this->x = v3[ 0 ];
    //        this->y = v3[ 1 ];
    //        this->z = v3[ 2 ];
    //        this->w = 1;
    //    }
    //}
    [[nodiscard]] inline Vector4( vec4_t v4 ) {
        if ( v4 ) {
            this->x = v4[ 0 ];
            this->y = v4[ 1 ];
            this->z = v4[ 2 ];
            this->w = v4[ 3 ];
        }
    }
    /**
    *  C++ Array like component accessors:
    **/
    [[nodiscard]] inline constexpr const float &operator[]( const size_t i ) const;
    [[nodiscard]] inline constexpr float &operator[]( const size_t i );
    #endif // #ifdef __cplusplus
} Vector4;
#define RL_VECTOR4_TYPE
#endif

#if !defined(RL_QUATERNION_TYPE)
// Quaternion type
typedef Vector4 Quaternion;
#define RL_QUATERNION_TYPE
#endif

#if !defined(RL_MATRIX_TYPE)
// Matrix type (OpenGL style 4x4 - right handed, column major)
typedef struct Matrix {
    float m0, m4, m8, m12;      // Matrix first row (4 components)
    float m1, m5, m9, m13;      // Matrix second row (4 components)
    float m2, m6, m10, m14;     // Matrix third row (4 components)
    float m3, m7, m11, m15;     // Matrix fourth row (4 components)
} Matrix;
#define RL_MATRIX_TYPE
#endif

// NOTE: Helper types to be used instead of array return types for *ToFloat functions
typedef struct float3 {
    float v[ 3 ];
} float3;

typedef struct float16 {
    float v[ 16 ];
} float16;

// NOTE: Helper types to be used instead of array return types for *ToQVec3_t functions
typedef struct qfloat3 {
    vec3_t v;
} qfloat3;

#include <math.h>       // Required for: sinf(), cosf(), tan(), atan2f(), sqrtf(), floor(), fminf(), fmaxf(), fabs()

//----------------------------------------------------------------------------------
// Module Functions Definition - Utils math
//----------------------------------------------------------------------------------
#include "shared/math/qray_utils.h"

// This is so we can pass them by const reference from C++.
// For C it'll be just a general const Vector3.
#ifdef __cplusplus
    typedef const Vector3 &ConstVector3Ref;
    typedef const Vector2 &ConstVector2Ref;
#else
    typedef const Vector3 ConstVector3Ref;
    typedef const Vector2 ConstVector2Ref;
#endif

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector2 math
//----------------------------------------------------------------------------------
#include "shared/math/qray_vector2.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector3 math
//----------------------------------------------------------------------------------
#include "shared/math/qray_vector3.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector4 math
//----------------------------------------------------------------------------------
#include "shared/math/qray_vector4.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Matrix math
//----------------------------------------------------------------------------------
#include "shared/math/qray_matrix4x4.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Quaternion math
//----------------------------------------------------------------------------------
#include "shared/math/qray_quaternion.h"

#endif  // QRAYMATH_H
