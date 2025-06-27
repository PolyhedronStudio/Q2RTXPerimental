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

#ifdef __cplusplus
    #define QM_API [[nodiscard]] inline const // Functions may be inlined, no external out-of-line definition
    #define QM_API_DISCARD inline const // Functions may be inlined, no external out-of-line definition

    #define QM_API_CONSTEXPR [[nodiscard]] constexpr inline const // Functions may be inlined, no external out-of-line definition.
    #define QM_API_CONSTEXPR_DISCARD constexpr inline const // Functions may be inlined, no external out-of-line definition.
#else
    #define QM_API const inline // Functions may be inlined, no external out-of-line definition
    #define QM_API_DISCARD const inline // Functions may be inlined, no external out-of-line definition
#endif


// Don't reinclude if done so already.
#ifndef _INC_MATH
    #ifdef __cplusplus
        #include <cmath>
    #else
        #include <math.h>       // Required for: sinf(), cosf(), tan(), atan2f(), sqrtf(), floor(), fminf(), fmaxf(), fabs()
    #endif
#endif

/**
*   QM PI:
**/
#ifndef QM_PI
    static constexpr double QM_PI = std::numbers::pi;
#endif

/**
*   QM Epsilon:
**/
#ifndef QM_EPSILON
    static constexpr float QM_FLOAT_EPSILON = FLT_EPSILON;
    static constexpr double QM_DOUBLE_EPSILON = DBL_EPSILON;
#endif

/**
*   QM Degrees 2 Radians:
**/
#ifndef QM_DEG2RAD
    static constexpr double QM_DEG2RAD = ( QM_PI / 180.0 );
#endif

/**
*   QM Radians 2 Degrees:
**/
#ifndef QM_RAD2DEG
    static constexpr double QM_RAD2DEG = ( 180.0 / QM_PI );
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
struct Vector2 {
    float x;
    float y;

    /**
    *   Q2RTXPerimental: C++ operator support. Implementations are further down this file.
    **/
    /**
    *   @brief  Vector2 Constructors, including support for passing in a vec2_t
    **/
    [[nodiscard]] inline Vector2() {
        this->x = 0;
        this->y = 0;
    }

    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
    [[nodiscard]] constexpr inline Vector2( const T x, const T y ) {
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
};
#define RL_VECTOR2_TYPE
#endif // #if !defined(RL_VECTOR2_TYPE


/**
*
*
*   Vector3:
*
*
**/
#if !defined(RL_VECTOR3_TYPE)
struct Vector3;

// Vector3 type
struct Vector3 {
    float x;
    float y;
    float z;
    /**
    *   Q2RTXPerimental: C++ operator support. Certain implementations are further down this file.
    **/
    /**
    *   @brief  Vector3 Constructors, including support for passing in a vec3_t
    **/
    [[nodiscard]] constexpr inline Vector3() {
        this->x = 0;
        this->y = 0;
        this->z = 0;
    }
    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
    [[nodiscard]] constexpr inline Vector3( const T x, const T y, const T z ) {
        this->x = x;
        this->y = y;
        this->z = z;
    }
    [[nodiscard]] constexpr inline Vector3( Vector2 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = 0;
    }
    [[nodiscard]] constexpr inline Vector3( const Vector2 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = 0;
    }
    [[nodiscard]] constexpr inline Vector3( Vector3 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = v.z;
    }
    [[nodiscard]] constexpr inline Vector3( const Vector3 &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = v.z;
    }
    [[nodiscard]] constexpr inline Vector3( vec3_t v3 ) {
        if ( v3 ) {
            this->x = v3[ 0 ];
            this->y = v3[ 1 ];
            this->z = v3[ 2 ];
        } else {
            this->x = this->y = this->z = 0;
        }
    }
    [[nodiscard]] constexpr inline Vector3( const vec3_t v3 ) {
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
    [[nodiscard]] constexpr inline Vector3 &operator+( const Vector3 right ) {
        return *this = QM_Vector3Add( *this, right );
    }
    [[nodiscard]] constexpr inline Vector3 &operator+( const Vector3 &right ) {
        return *this = QM_Vector3Add( *this, right );
    }
    [[nodiscard]] constexpr inline Vector3 &operator+( const float &right ) {
        return *this = QM_Vector3AddValue( *this, right);
    }
    //[[nodiscard]] constexpr inline Vector3 &operator+( const Vector3 &right ) {
	//	return *this = const Vector3{ +x, +y, +z };
    //}

    [[nodiscard]] constexpr inline Vector3 &operator+=( const Vector3 &right ) {
        return *this = QM_Vector3Add( *this, right );
    }
    [[nodiscard]] constexpr inline Vector3 &operator+=( const float &right ) {
        return *this = QM_Vector3AddValue( *this, right );
    }

    /**
    *   @brief  Vector3 C++ 'Minus' operator:
    **/
	//[[nodiscard]] constexpr inline Vector3 &operator-( const Vector3 &right ) {
	//	return *this = QM_Vector3Subtract( *this, right );
	//}
	//[[nodiscard]] constexpr inline Vector3 &operator-( const float &right ) {
	//	return *this = QM_Vector3SubtractValue( *this, right );
	//}
    [[nodiscard]] constexpr inline const Vector3 operator-( const Vector3 right ) {
        return *this = QM_Vector3Subtract( *this, right );
    }
    [[nodiscard]] constexpr inline const Vector3 operator-( const Vector3 &right ) {
        return *this = QM_Vector3Subtract( *this, right );
    }
 //   [[nodiscard]] constexpr inline Vector3 &operator-( const Vector3 &right ) {
	//	return *this = QM_Vector3Subtract( *this, right );
	//}
    [[nodiscard]] constexpr inline Vector3 operator-( const float &right ) {
        return *this = QM_Vector3SubtractValue( *this, right );
    }
    //[[nodiscard]] constexpr inline const Vector3 &operator-( const Vector3 &v ) {
    //    return *this = QM_Vector3Negate( v );
    //}

    [[nodiscard]] constexpr inline Vector3 &operator-=( const Vector3 &right ) {
        return *this = QM_Vector3Subtract( *this, right );
    }
    [[nodiscard]] constexpr inline Vector3 &operator-=( const float &right ) {
        return *this = QM_Vector3SubtractValue( *this, right);
    }

    /**
    *   @brief  Vector3 C++ 'Multiply' operator:
    **/
    [[nodiscard]] constexpr inline Vector3 &operator*( const Vector3 right ) {
        return *this = QM_Vector3Multiply( *this, right );
    }
    [[nodiscard]] constexpr inline Vector3 &operator*( const Vector3 &right ) {
        return *this = QM_Vector3Multiply( *this, right );
    }
    // for: const Vector3 &v1 = floatVal * v2;
    [[nodiscard]] constexpr inline Vector3 &operator*( const float &right ) {
        return *this = QM_Vector3Scale( *this, right );
    }

    [[nodiscard]] constexpr inline Vector3 &operator*=( const Vector3 &right ) {
        return *this = QM_Vector3Multiply( *this, right );
    }
    [[nodiscard]] constexpr inline Vector3 &operator*=( const float &right ) {
        return *this = QM_Vector3Scale( *this, right );
    }

    /**
    *   @brief  Vector3 C++ 'Divide' operator:
    **/
    [[nodiscard]] inline constexpr Vector3 &operator/( const Vector3 right ) {
        return *this = QM_Vector3Divide( *this, right );
    }
    [[nodiscard]] inline constexpr Vector3 &operator/( const Vector3 &right ) {
        return *this = QM_Vector3Divide( *this, right );
    }
    [[nodiscard]] inline constexpr Vector3 &operator/( const float &right ) {
        return *this = QM_Vector3DivideValue( *this, right );
    }

    [[nodiscard]] inline constexpr Vector3 &operator/=( const Vector3 &right ) {
        return *this = QM_Vector3Divide( *this, right );
    }
    [[nodiscard]] inline constexpr Vector3 &operator/=( const float &right ) {
        return *this = QM_Vector3DivideValue( *this, right );
    }

    /**
    *   @brief  Compare each element with s, return vector of 1 or 0 based on test.
    **/
    [[nodiscard]] inline constexpr const Vector3 &operator < ( const float &scalar ) {
		return { ( x < scalar ? 1 : 0 ), ( y < scalar ? 1 : 0 ), ( z < scalar ? 1 : 0 ) };
    }
    [[nodiscard]] inline constexpr const Vector3 &operator > ( const float &scalar ) {
        return { ( x > scalar ? 1 : 0 ), ( y > scalar ? 1 : 0 ), ( z > scalar ? 1 : 0 ) };
    }

    /**
    *   @brief  Element-wise less than comparion, return vector of 1 or 0 based on test.
    **/
    [[nodiscard]] inline constexpr const Vector3 &operator < ( const Vector3 &v ) {
        return { ( x < v.x ? 1 : 0 ), ( y < v.y? 1 : 0 ), ( z < v.z ? 1 : 0 ) };

    }
    /**
    *   @brief  Element-wise greater than comparion, return vector of 1 or 0 based on test.
    **/
    [[nodiscard]] inline constexpr const Vector3 &operator > ( const Vector3 &v ) {
        return { ( x > v.x ? 1 : 0 ), ( y > v.y ? 1 : 0 ), ( z > v.z ? 1 : 0 ) };
    }

    /**
    *   @brief  Vector3 C++ 'Equals' operator:
    **/
    [[nodiscard]] inline const bool operator==( const Vector3 &right ) {
        return QM_Vector3Equals( *this, right, QM_FLOAT_EPSILON );
    }
    /**
    *   @brief  Vector3 C++ 'Not Equals' operator:
    **/
    [[nodiscard]] inline const bool operator!=( const Vector3 &right ) {
        return ( QM_Vector3Equals( *this, right, QM_FLOAT_EPSILON ) ? false : true );
    }
    #endif // 0

    /**
    *   @brief  Compare each element with s, return vector of 1 or 0 based on test.
    **/
    [[nodiscard]] inline constexpr const Vector3 &operator < ( const float &scalar ) {
		return { ( x < scalar ? 1 : 0 ), ( y < scalar ? 1 : 0 ), ( z < scalar ? 1 : 0 ) };
    }
    [[nodiscard]] inline constexpr const Vector3 &operator > ( const float &scalar ) {
        return { ( x > scalar ? 1 : 0 ), ( y > scalar ? 1 : 0 ), ( z > scalar ? 1 : 0 ) };
    }

    /**
    *   @brief  Element-wise less than comparion, return vector of 1 or 0 based on test.
    **/
    [[nodiscard]] inline constexpr const Vector3 &operator < ( const Vector3 &v ) {
        return { ( x < v.x ? 1 : 0 ), ( y < v.y? 1 : 0 ), ( z < v.z ? 1 : 0 ) };

    }
    /**
    *   @brief  Element-wise greater than comparion, return vector of 1 or 0 based on test.
    **/
    [[nodiscard]] inline constexpr const Vector3 &operator > ( const Vector3 &v ) {
        return { ( x > v.x ? 1 : 0 ), ( y > v.y ? 1 : 0 ), ( z > v.z ? 1 : 0 ) };
    }

    /**
    *  C++ Array like component accessors:
    **/
    [[nodiscard]] inline constexpr const float &operator[]( const size_t i ) const;
    [[nodiscard]] inline constexpr float &operator[]( const size_t i );

    /**
    *   @brief  Vector3 C++ 'Equals' operator:
    **/
    QM_API_CONSTEXPR bool operator==( const Vector3 *right ) {
        return this == right;
    }
    /**
    *   @brief  Vector3 C++ 'Equals' operator:
    **/
    QM_API_CONSTEXPR bool operator!=( const Vector3 *right ) {
        return this != right;
    }
};
// Define Type.
#define RL_VECTOR3_TYPE
#endif // #if !defined(RL_VECTOR3_TYPE)



/**
*
*
*   Vector4:
*
*
**/
#if !defined(RL_VECTOR4_TYPE)
struct Vector4 {
    float x;
    float y;
    float z;
    float w;

    /**
    *   Q2RTXPerimental: C++ operator support. Implementations are further down this file.
    **/
    /**
    *   @brief  Vector3 Constructors, including support for passing in a vec3_t
    **/
    [[nodiscard]] inline Vector4() {
        this->x = 0;
        this->y = 0;
        this->z = 0;
        this->w = 0;
    }
    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
    [[nodiscard]] constexpr inline Vector4( const T x, const T y, const T z, const T w ) {
        this->x = x;
        this->y = y;
        this->z = z;
        this->w = w;
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
    //[[nodiscard]] inline Vector4( const Vector3 &v ) {
    //    this->x = v.x;
    //    this->y = v.y;
    //    this->z = v.z;
    //    this->w = 1;
    //}
    //[[nodiscard]] inline Vector4( const Vector4 &v ) {
    //    this->x = v.x;
    //    this->y = v.y;
    //    this->z = v.z;
    //    this->w = v.w;
    //}
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
};
#define RL_VECTOR4_TYPE
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
struct Matrix {
    float m0, m4, m8, m12;      // Matrix first row (4 components)
    float m1, m5, m9, m13;      // Matrix second row (4 components)
    float m2, m6, m10, m14;     // Matrix third row (4 components)
    float m3, m7, m11, m15;     // Matrix fourth row (4 components)

    /**
    *  C++ Array like component accessors:
    **/
    [[nodiscard]] inline constexpr const float &operator[]( const size_t i ) const;
    [[nodiscard]] inline constexpr float &operator[]( const size_t i );
    //[[nodiscard]] inline constexpr const float *operator[]( const size_t i ) const;
    //[[nodiscard]] inline constexpr float *operator[]( const size_t i );
};
// Defined Type.
#define RL_MATRIX_TYPE
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
struct BBox3 {
    union {
        // X Y Z desegnator accessors.
        struct {
            Vector3 mins, maxs;
        };
        float points[ 6 ] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    };

    /**
    *	@brief	Specific Intersection Test Types for use with bbox3_intersects_sphere.
    **/
    enum IntersectType {
        // Box VS Sphere Types:
        HollowBox_HollowSphere = 0,
        HollowBox_SolidSphere = 1,
        SolidBox_HollowSphere = 2,
        SolidBox_SolidSphere = 3,
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
};
// Defined type.
#define QM_BBOX3_TYPE
#endif // #if !defined(QM_BBOX3_TYPE)


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
typedef struct float12 {
    float v[ 16 ];
} float12;
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
// Module Functions Definition - Utils
//----------------------------------------------------------------------------------
#include "shared/math/qm_utils.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector2 (Class-)Function Implementations.
//----------------------------------------------------------------------------------
#include "shared/math/qm_vector2.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector3 (Class-)Function Implementations.
//----------------------------------------------------------------------------------
#include "shared/math/qm_vector3.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector4 (Class-)Function Implementations.
//----------------------------------------------------------------------------------
#include "shared/math/qm_vector4.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Matrix (Class-)Function Implementations.
//----------------------------------------------------------------------------------
#include "shared/math/qm_matrix4x4.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Quaternion (Class-)Function Implementations.
//----------------------------------------------------------------------------------
#include "shared/math/qm_quaternion.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - BBox3 (Class-)Function Implementations.
//----------------------------------------------------------------------------------
#include "shared/math/qm_bbox3.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Time Wrapper Object, easy maths.
//----------------------------------------------------------------------------------
#include "shared/math/qm_time.h"

//----------------------------------------------------------------------------------
// Module Functions Definition - Easing Methods
//----------------------------------------------------------------------------------
#include "shared/math/qm_easing_methods.hpp"

//----------------------------------------------------------------------------------
// Module Functions Definition - Easing State
//----------------------------------------------------------------------------------
#include "shared/math/qm_easing_state.hpp"
