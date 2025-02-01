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
*   RayMath CONVENTIONS:
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
*   RayMath CONFIGURATION:
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
*   RayMath LICENSE: zlib/libpng
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
#pragma once


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
    #if defined(RAYMATH_QM_INLINE)
        #ifdef __cplusplus
            #define RMAPI [[nodiscard]] const inline // Functions may be inlined, no external out-of-line definition
            #define RMAPI_DISCARD const inline // Functions may be inlined, no external out-of-line definition
        #else
            #define RMAPI inline 
        #endif
    #else
        #define RMAPI static inline // Functions may be inlined, no external out-of-line definition
    #endif
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

// Don't reinclude if done so already.
#ifndef _INC_MATH
#include <math.h>       // Required for: sinf(), cosf(), tan(), atan2f(), sqrtf(), floor(), fminf(), fmaxf(), fabs()
#endif

/**
*   QM PI:
**/
#ifndef QM_PI
#ifdef __cplusplus
static constexpr float QM_PI = 3.14159265358979323846f;
#else
#define QM_PI 3.14159265358979323846f
#endif
#endif

/**
*   QM Epsilon:
**/
#ifndef QM_EPSILON
#ifdef __cplusplus
static constexpr float QM_EPSILON = 0.000001f;
#else
#define QM_EPSILON 0.000001f
#endif
#endif

/**
*   QM Degrees 2 Radians:
**/
#ifndef QM_DEG2RAD
#ifdef __cplusplus
static constexpr float QM_DEG2RAD = ( QM_PI / 180.0f );
#else
#define QM_DEG2RAD (QM_PI/180.0f)
#endif
#endif

/**
*   QM Radians 2 Degrees:
**/
#ifndef QM_RAD2DEG
#ifdef __cplusplus
static constexpr float QM_RAD2DEG = ( 180.0f / QM_PI );
#else
#define QM_RAD2DEG (180.0f/QM_PI)
#endif
#endif

/**
*   Predefine:
**/
#ifdef __cplusplus
    struct Vector2;
    struct Vector3;
    struct Vector4;
    struct BBox3;
    struct QMTime;
#endif



//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
/**
*
*
*   Vector2:
*
*
**/
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
    /**
    *   Vector2 in:
    **/
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
        } else {
            this->x = 0;
            this->y = 0;
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



/**
*
*
*   Vector3:
*
*
**/
#if !defined(RL_VECTOR3_TYPE)
// Vector3 type
typedef struct Vector3 {
    float x;
    float y;
    float z;

    /**
    *   Q2RTXPerimental: C++ operator support. Certain implementations are further down this file.
    **/
    #ifdef __cplusplus
    /**
    *   @brief  Vector3 Constructors, including support for passing in a vec3_t
    **/
    [[nodiscard]] constexpr inline Vector3() {
        this->x = 0;
        this->y = 0;
        this->z = 0;
    }
    [[nodiscard]] constexpr inline Vector3( const float x, const float y, const float z ) {
        this->x = x;
        this->y = y;
        this->z = z;
    }
    [[nodiscard]] inline Vector3( Vector2 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = 0;
    }
    [[nodiscard]] inline Vector3( const Vector2 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = 0;
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

    #if 0
    /**
    *   @brief  Vector3 C++ 'Plus' operator:
    **/
    [[nodiscard]] constexpr inline Vector3 operator+( const Vector3 &right ) {
        return QM_Vector3Add( *this, right );
    }
    [[nodiscard]] constexpr inline Vector3 operator+( const float &right ) {
        return QM_Vector3AddValue( *this, right);
    }

    [[nodiscard]] constexpr inline const Vector3 operator+=( const Vector3 &right ) {
        return *this = QM_Vector3Add( *this, right );
    }
    [[nodiscard]] constexpr inline const Vector3 operator+=( const float &right ) {
        return *this = QM_Vector3AddValue( *this, right );
    }

    /**
    *   @brief  Vector3 C++ 'Minus' operator:
    **/
    [[nodiscard]] constexpr inline const Vector3 operator-( const Vector3 &right ) {
        return QM_Vector3Subtract( *this, right );
    }
    [[nodiscard]] constexpr inline const Vector3 operator-( const float &right ) {
        return QM_Vector3SubtractValue( *this, right );
    }
    [[nodiscard]] constexpr inline const Vector3 operator-( const Vector3 &v ) {
        return QM_Vector3Negate( v );
    }

    [[nodiscard]] constexpr inline const Vector3 &operator-=( const Vector3 &right ) {
        return *this = QM_Vector3Subtract( *this, right );
    }
    [[nodiscard]] constexpr inline const Vector3 &operator-=( const float &right ) {
        return *this = QM_Vector3SubtractValue( *this, right);
    }

    /**
    *   @brief  Vector3 C++ 'Multiply' operator:
    **/
    RMAPI const Vector3 operator*( const Vector3 &right ) {
        return QM_Vector3Multiply( *this, right );
    }
    RMAPI const Vector3 operator*( const float &right ) {
        return QM_Vector3Scale( *this, right );
    }
    // for: ConstVector3Ref v1 = floatVal * v2;
    RMAPI const Vector3 operator*( const float &right ) {
        return QM_Vector3Scale( *this, right );
    }

    RMAPI const Vector3 &operator*=( const Vector3 &right ) {
        return *this = QM_Vector3Multiply( *this, right );
    }
    RMAPI const Vector3 &operator*=( const float &right ) {
        return *this = QM_Vector3Scale( *this, right );
    }

    /**
    *   @brief  Vector3 C++ 'Divide' operator:
    **/
    RMAPI const Vector3 operator/( const Vector3 &right ) {
        return QM_Vector3Divide( *this, right );
    }
    RMAPI const Vector3 operator/( const float &right ) {
        return QM_Vector3DivideValue( *this, right );
    }

    RMAPI const Vector3 &operator/=( const Vector3 &right ) {
        return *this = QM_Vector3Divide( *this, right );
    }
    RMAPI const Vector3 &operator/=( const float &right ) {
        return *this = QM_Vector3DivideValue( *this, right );
    }

    /**
    *   @brief  Vector3 C++ 'Equals' operator:
    **/
    RMAPI bool operator==( const Vector3 &right ) {
        return QM_Vector3Equals( *this, right );
    }

    /**
    *   @brief  Vector3 C++ 'Not Equals' operator:
    **/
    [[nodiscard]] inline constexpr const bool operator!=( const Vector3 &right ) {
        return !QM_Vector3Equals( *this, right );
    }
    #endif // 0
    /**
    *  C++ Array like component accessors:
    **/
    [[nodiscard]] inline constexpr const float &operator[]( const size_t i ) const;
    [[nodiscard]] inline constexpr float &operator[]( const size_t i );
    #endif // #ifdef __cplusplus
} Vector3;
#define RL_VECTOR3_TYPE
#endif



/**
*
*
*   Vector4:
*
*
**/
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
        } else {
            this->x = this->y = this->z = this->w = 0;
        }
    }
    [[nodiscard]] inline Vector4( const vec4_t v4 ) {
        if ( v4 ) {
            this->x = v4[ 0 ];
            this->y = v4[ 1 ];
            this->z = v4[ 2 ];
            this->w = v4[ 3 ];
        } else {
            this->x = this->y = this->z = this->w = 0;
        }
    }

    //[[nodiscard]] inline Vector4( quat_t v4 ) {
    //    if ( v4 ) {
    //        this->x = v4[ 0 ];
    //        this->y = v4[ 1 ];
    //        this->z = v4[ 2 ];
    //        this->w = v4[ 3 ];
    //    } else {
    //        this->x = this->y = this->z = this->w = 0;
    //    }
    //}
    //[[nodiscard]] inline Vector4( const quat_t v4 ) {
    //    if ( v4 ) {
    //        this->x = v4[ 0 ];
    //        this->y = v4[ 1 ];
    //        this->z = v4[ 2 ];
    //        this->w = v4[ 3 ];
    //    } else {
    //        this->x = this->y = this->z = this->w = 0;
    //    }
    //}
    /**
    *  C++ Array like component accessors:
    **/
    [[nodiscard]] inline constexpr const float &operator[]( const size_t i ) const;
    [[nodiscard]] inline constexpr float &operator[]( const size_t i );
    #endif // #ifdef __cplusplus
} Vector4;
#define RL_VECTOR4_TYPE
#endif



/**
*
*
*   Quaternion:
*
*
**/
#if !defined(RL_QUATERNION_TYPE)
// Quaternion type
typedef Vector4 Quaternion;
#define RL_QUATERNION_TYPE
#endif



/**
*
*
*   Matrix4x4:
*
*
**/
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



/**
*
*
*   BBox3:
*
*
**/
#if !defined(QM_BBOX3_TYPE)
/**
*	@brief Box 3 type definiton: (mins, maxs). The bounds are implemented like a union class.
**/
typedef struct BBox3 {
    union {
        // X Y Z desegnator accessors.
        Vector3 mins, maxs;
        float points[ 6 ];
    };

    #ifdef __cplusplus
    /**
    *	@brief	Specific Intersection Test Types for use with bbox3_intersects_sphere.
    **/
    struct IntersectType {
        //#ifdef __cplusplus
            // Box VS Sphere Types:
            static constexpr int32_t HollowBox_HollowSphere = 0;
            static constexpr int32_t HollowBox_SolidSphere = 1;
            static constexpr int32_t SolidBox_HollowSphere = 2;
            static constexpr int32_t SolidBox_SolidSphere = 3;
        //#else
        //    // Box VS Sphere Types:
        //    static const int32_t HollowBox_HollowSphere = 0;
        //    static const int32_t HollowBox_SolidSphere = 1;
        //    static const int32_t SolidBox_HollowSphere = 2;
        //    static const int32_t SolidBox_SolidSphere = 3;
        //#endif
    };
    /**
    *	Constructors.
    **/
    // Default.
    constexpr explicit BBox3() { mins = { 0.f, 0.f, 0.f }; maxs = { 0.f, 0.f, 0.f }; }

    constexpr BBox3( BBox3 &bbox ) { mins = bbox.mins; maxs = bbox.maxs; }
    constexpr BBox3( const BBox3 &bbox ) { mins = bbox.mins; maxs = bbox.maxs; }

    // Assign.
    BBox3( const Vector3 &boxMins, const Vector3 &boxMaxs ) { mins = boxMins; maxs = boxMaxs; }

    // Regular *vec_t support.
    constexpr explicit BBox3( Vector3 *vec ) { mins = vec[ 0 ]; maxs = vec[ 1 ]; }
    constexpr explicit BBox3( const Vector3 *vec ) { mins = vec[ 0 ]; maxs = vec[ 1 ]; }
    constexpr explicit BBox3( const float *vec ) { mins = { vec[ 0 ], vec[ 1 ], vec[ 2 ] }; maxs = { vec[ 3 ], vec[ 4 ], vec[ 5 ] }; }

    /**
    *	Operators
    **/
    // Pointer.
    inline explicit operator float *( ) {
        return &mins.x;
    }
    inline explicit operator Vector3 *( ) {
        return &mins;
    }

    // Pointer cast to const float*
    inline explicit operator const float *( ) const {
        return &mins.x;
    }
    inline explicit operator const Vector3 *( ) const {
        return &mins;
    }
    /**
    *  C++ Array like component accessors:
    **/
    [[nodiscard]] inline constexpr const float &operator[]( const size_t i ) const;
    [[nodiscard]] inline constexpr float &operator[]( const size_t i );
    #endif // #ifdef __cplusplus
} BBox3;
#define QM_BBOX3_TYPE
#endif 


/**
*
*
*   Helper Types:
*
*
**/
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



/**
*
*
*   QMath Utilities:
*
*
**/
//----------------------------------------------------------------------------------
// Module Functions Definition - Utils math
//----------------------------------------------------------------------------------
#include "shared/math/qm_utils.h"

// This is so we can pass them by const reference from C++.
// For C it'll be just a general const Vector3.
#ifdef __cplusplus
    typedef const BBox3     &ConstBBox3Ref;
    typedef const Vector2   &ConstVector2Ref;
    typedef const Vector3   &ConstVector3Ref;
    typedef const Vector4   &ConstVector4Ref;
#else
    typedef const BBox3     ConstBBox3Ref;
    typedef const Vector2   ConstVector2Ref;
    typedef const Vector3   ConstVector3Ref;
    typedef const Vector4   ConstVector4Ref;
#endif

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector2 math
//----------------------------------------------------------------------------------
#include "shared/math/qm_vector2.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector3 math
//----------------------------------------------------------------------------------
#include "shared/math/qm_vector3.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector4 math
//----------------------------------------------------------------------------------
#include "shared/math/qm_vector4.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Matrix math
//----------------------------------------------------------------------------------
#include "shared/math/qm_matrix4x4.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Quaternion math
//----------------------------------------------------------------------------------
#include "shared/math/qm_quaternion.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - BBox3 math
//----------------------------------------------------------------------------------
#include "shared/math/qm_bbox3.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Time Wrapper Object, easy maths.
//----------------------------------------------------------------------------------
#include "shared/math/qm_time.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Easing
//----------------------------------------------------------------------------------
#include "shared/math/qm_easing.hpp"

//----------------------------------------------------------------------------------
// Module Functions Definition - Easing State
//----------------------------------------------------------------------------------
#include "shared/math/qm_easing_state.hpp"
