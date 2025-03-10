/**********************************************************************************************
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
* **********************************************************************************************
*
*   A customized version of RayMath v1.5:
*
*
*   RayMath CONVENTIONS:
*     - Matrix structure is defined as row-major (memory layout) but parameters naming AND all
*       math operations performed by the library consider the structure as it was column-major
*       It is like transposed versions of the matrices are used for all the maths
*       It benefits some functions making them cache-friendly and also avoids matrix
*       transpositions sometimes required by OpenGL
*       Example: In memory order, row0 is [m0 m4 m8 m12] but in semantic math row0 is [m0 m1 m2 m3]
*     - Functions are always defined inline
*     - Angles are always in radians (QM_DEG2RAD/QM_RAD2DEG macros provided for convenience)
*
**********************************************************************************************/
#pragma once



#define QM_API const inline // Functions may be inlined, no external out-of-line definition
#define QM_API_DISCARD const inline // Functions may be inlined, no external out-of-line definition

// Don't reinclude if done so already.
#ifndef _INC_MATH
    #include <math.h>       // Required for: sinf(), cosf(), tan(), atan2f(), sqrtf(), floor(), fminf(), fmaxf(), fabs()
#endif

/**
*   QM PI:
**/
#ifndef QM_PI
    #define QM_PI 3.14159265358979323846
#endif

/**
*   QM Epsilon:
**/
#ifndef QM_EPSILON
    #define QM_EPSILON 0.000001
#endif

/**
*   QM Degrees 2 Radians:
**/
#ifndef QM_DEG2RAD
    #define QM_DEG2RAD ( ( double )( QM_PI / 180.0 ) )
#endif

/**
*   QM Radians 2 Degrees:
**/
#ifndef QM_RAD2DEG
    #define QM_RAD2DEG ( (double)( 180.0 / QM_PI ) )
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
} Vector2;
#endif // #if !defined(RL_VECTOR2_TYPE


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
} Vector3;
#endif // #if !defined(RL_VECTOR3_TYPE)



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
} Vector4;
#endif // #if !defined(RL_VECTOR4_TYPE)



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
#endif // #if !defined(RL_MATRIX_TYPE)


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
} BBox3;
#endif
