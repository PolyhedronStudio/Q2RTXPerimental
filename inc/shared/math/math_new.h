/*
  HandmadeMath.h v2.0.0

  This is a single header file with a bunch of useful types and functions for
  games and graphics. Consider it a lightweight alternative to GLM that works
  both C and C++.

  =============================================================================
  CONFIG
  =============================================================================

  By default, all angles in Handmade Math are specified in radians. However, it
  can be configured to use degrees or turns instead. Use one of the following
  defines to specify the default unit for angles:

    #define HANDMADE_MATH_USE_RADIANS
    #define HANDMADE_MATH_USE_DEGREES
    #define HANDMADE_MATH_USE_TURNS

  Regardless of the default angle, you can use the following functions to
  specify an angle in a particular unit:

    QM_AngleRad(radians)
    QM_AngleDeg(degrees)
    QM_AngleTurn(turns)

  The definitions of these functions change depending on the default unit.

  -----------------------------------------------------------------------------

  Handmade Math ships with SSE (SIMD) implementations of several common
  operations. To disable the use of SSE intrinsics, you must define
  HANDMADE_MATH_NO_SSE before including this file:

    #define HANDMADE_MATH_NO_SSE
    #include "HandmadeMath.h"

  -----------------------------------------------------------------------------

  To use Handmade Math without the C runtime library, you must provide your own
  implementations of basic math functions. Otherwise, HandmadeMath.h will use
  the runtime library implementation of these functions.

  Define HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS and provide your own
  implementations of QM_SINF, QM_COSF, QM_TANF, QM_ACOSF, and QM_SQRTF
  before including HandmadeMath.h, like so:

    #define HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS
    #define QM_SINF MySinF
    #define QM_COSF MyCosF
    #define QM_TANF MyTanF
    #define QM_ACOSF MyACosF
    #define QM_SQRTF MySqrtF
    #include "HandmadeMath.h"

  By default, it is assumed that your math functions take radians. To use
  different units, you must define QM_ANGLE_USER_TO_INTERNAL and
  QM_ANGLE_INTERNAL_TO_USER. For example, if you want to use degrees in your
  code but your math functions use turns:

    #define QM_ANGLE_USER_TO_INTERNAL(a) ((a)*QM_DegToTurn)
    #define QM_ANGLE_INTERNAL_TO_USER(a) ((a)*QM_TurnToDeg)

  =============================================================================

  LICENSE

  This software is in the public domain. Where that dedication is not
  recognized, you are granted a perpetual, irrevocable license to copy,
  distribute, and modify this file as you see fit.

  =============================================================================

  CREDITS

  Originally written by Zakary Strange.

  Functionality:
   Zakary Strange (strangezak@protonmail.com && @strangezak)
   Matt Mascarenhas (@miblo_)
   Aleph
   FieryDrake (@fierydrake)
   Gingerbill (@TheGingerBill)
   Ben Visness (@bvisness)
   Trinton Bullard (@Peliex_Dev)
   @AntonDan
   Logan Forman (@dev_dwarf)

  Fixes:
   Jeroen van Rijn (@J_vanRijn)
   Kiljacken (@Kiljacken)
   Insofaras (@insofaras)
   Daniel Gibson (@DanielGibson)
*/

#ifndef HANDMADE_MATH_H
#define HANDMADE_MATH_H

// Dummy macros for when test framework is not present.
#ifndef COVERAGE
# define COVERAGE(a, b)
#endif

#ifndef ASSERT_COVERED
# define ASSERT_COVERED(a)
#endif

#ifdef HANDMADE_MATH_NO_SSE
# warning "HANDMADE_MATH_NO_SSE is deprecated, use HANDMADE_MATH_NO_SIMD instead"
# define HANDMADE_MATH_NO_SIMD
#endif 

/* let's figure out if SSE is really available (unless disabled anyway)
   (it isn't on non-x86/x86_64 platforms or even x86 without explicit SSE support)
   => only use "#ifdef HANDMADE_MATH__USE_SSE" to check for SSE support below this block! */
#ifndef HANDMADE_MATH_NO_SIMD
# ifdef _MSC_VER /* MSVC supports SSE in amd64 mode or _M_IX86_FP >= 1 (2 means SSE2) */
#  if defined(_M_AMD64) || ( defined(_M_IX86_FP) && _M_IX86_FP >= 1 )
#   define HANDMADE_MATH__USE_SSE 1
#  endif
# else /* not MSVC, probably GCC, clang, icc or something that doesn't support SSE anyway */
#  ifdef __SSE__ /* they #define __SSE__ if it's supported */
#   define HANDMADE_MATH__USE_SSE 1
#  endif /*  __SSE__ */
# endif /* not _MSC_VER */
# ifdef __ARM_NEON
#  define HANDMADE_MATH__USE_NEON 1
# endif /* NEON Supported */
#endif /* #ifndef HANDMADE_MATH_NO_SIMD */

#if (!defined(__cplusplus) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
# define HANDMADE_MATH__USE_C11_GENERICS 1
#endif

#ifdef HANDMADE_MATH__USE_SSE
# include <xmmintrin.h>
#endif

#ifdef HANDMADE_MATH__USE_NEON
# include <arm_neon.h>
#endif

#ifdef _MSC_VER
#pragma warning(disable:4201)
#endif

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wfloat-equal"
# if (defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 8)) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wmissing-braces"
# endif
# ifdef __clang__
#  pragma GCC diagnostic ignored "-Wgnu-anonymous-struct"
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
# endif
#endif

#if defined(__GNUC__) || defined(__clang__)
# define QM_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
# define QM_DEPRECATED(msg) __declspec(deprecated(msg))
#else
# define QM_DEPRECATED(msg)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(HANDMADE_MATH_USE_DEGREES) \
    && !defined(HANDMADE_MATH_USE_TURNS) \
    && !defined(HANDMADE_MATH_USE_RADIANS)
# define HANDMADE_MATH_USE_RADIANS
#endif

#define QM_PI 3.14159265358979323846
#define QM_PI32 3.14159265359f
#define QM_DEG180 180.0
#define QM_DEG18032 180.0f
#define QM_TURNHALF 0.5
#define QM_TURNHALF32 0.5f
#define QM_RadToDeg ((float)(QM_DEG180/QM_PI))
#define QM_RadToTurn ((float)(QM_TURNHALF/QM_PI))
#define QM_DegToRad ((float)(QM_PI/QM_DEG180))
#define QM_DegToTurn ((float)(QM_TURNHALF/QM_DEG180))
#define QM_TurnToRad ((float)(QM_PI/QM_TURNHALF))
#define QM_TurnToDeg ((float)(QM_DEG180/QM_TURNHALF))

#if defined(HANDMADE_MATH_USE_RADIANS)
# define QM_AngleRad(a) (a)
# define QM_AngleDeg(a) ((a)*QM_DegToRad)
# define QM_AngleTurn(a) ((a)*QM_TurnToRad)
#elif defined(HANDMADE_MATH_USE_DEGREES)
# define QM_AngleRad(a) ((a)*QM_RadToDeg)
# define QM_AngleDeg(a) (a)
# define QM_AngleTurn(a) ((a)*QM_TurnToDeg)
#elif defined(HANDMADE_MATH_USE_TURNS)
# define QM_AngleRad(a) ((a)*QM_RadToTurn)
# define QM_AngleDeg(a) ((a)*QM_DegToTurn)
# define QM_AngleTurn(a) (a)
#endif

#if !defined(HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS)
# include <math.h>
# define QM_SINF sinf
# define QM_COSF cosf
# define QM_TANF tanf
# define QM_SQRTF sqrtf
# define QM_ACOSF acosf
#endif

#if !defined(QM_ANGLE_USER_TO_INTERNAL)
# define QM_ANGLE_USER_TO_INTERNAL(a) (QM_ToRad(a))
#endif

#if !defined(QM_ANGLE_INTERNAL_TO_USER)
# if defined(HANDMADE_MATH_USE_RADIANS)
#  define QM_ANGLE_INTERNAL_TO_USER(a) (a)
# elif defined(HANDMADE_MATH_USE_DEGREES)
#  define QM_ANGLE_INTERNAL_TO_USER(a) ((a)*QM_RadToDeg)
# elif defined(HANDMADE_MATH_USE_TURNS)
#  define QM_ANGLE_INTERNAL_TO_USER(a) ((a)*QM_RadToTurn)
# endif
#endif

#define QM_MIN(a, b) ((a) > (b) ? (b) : (a))
#define QM_MAX(a, b) ((a) < (b) ? (b) : (a))
#define QM_ABS(a) ((a) > 0 ? (a) : -(a))
#define QM_MOD(a, m) (((a) % (m)) >= 0 ? ((a) % (m)) : (((a) % (m)) + (m)))
#define QM_SQUARE(x) ((x) * (x))

typedef union QM_Vec2
{
    struct
    {
        float X, Y;
    };

    struct
    {
        float U, V;
    };

    struct
    {
        float Left, Right;
    };

    struct
    {
        float Width, Height;
    };

    float Elements[2];

#ifdef __cplusplus
    inline float &operator[](int Index) { return Elements[Index]; }
    inline const float& operator[](int Index) const { return Elements[Index]; }
#endif
} QM_Vec2;

typedef union QM_Vec3
{
    struct
    {
        float X, Y, Z;
    };

    struct
    {
        float U, V, W;
    };

    struct
    {
        float R, G, B;
    };

    struct
    {
        QM_Vec2 XY;
        float _Ignored0;
    };

    struct
    {
        float _Ignored1;
        QM_Vec2 YZ;
    };

    struct
    {
        QM_Vec2 UV;
        float _Ignored2;
    };

    struct
    {
        float _Ignored3;
        QM_Vec2 VW;
    };

    float Elements[3];

#ifdef __cplusplus
    inline float &operator[](int Index) { return Elements[Index]; }
    inline const float &operator[](int Index) const { return Elements[Index]; }
#endif
} QM_Vec3;

typedef union QM_Vec4
{
    struct
    {
        union
        {
            QM_Vec3 XYZ;
            struct
            {
                float X, Y, Z;
            };
        };

        float W;
    };
    struct
    {
        union
        {
            QM_Vec3 RGB;
            struct
            {
                float R, G, B;
            };
        };

        float A;
    };

    struct
    {
        QM_Vec2 XY;
        float _Ignored0;
        float _Ignored1;
    };

    struct
    {
        float _Ignored2;
        QM_Vec2 YZ;
        float _Ignored3;
    };

    struct
    {
        float _Ignored4;
        float _Ignored5;
        QM_Vec2 ZW;
    };

    float Elements[4];

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSE;
#endif

#ifdef HANDMADE_MATH__USE_NEON
    float32x4_t NEON;
#endif 

#ifdef __cplusplus
    inline float &operator[](int Index) { return Elements[Index]; }
    inline const float &operator[](int Index) const { return Elements[Index]; }
#endif
} QM_Vec4;

typedef union QM_Mat2
{
    float Elements[2][2];
    QM_Vec2 Columns[2];

#ifdef __cplusplus
    inline QM_Vec2 &operator[](int Index) { return Columns[Index]; }
    inline const QM_Vec2 &operator[](int Index) const { return Columns[Index]; }
#endif
} QM_Mat2;

typedef union QM_Mat3
{
    float Elements[3][3];
    QM_Vec3 Columns[3];

#ifdef __cplusplus
    inline QM_Vec3 &operator[](int Index) { return Columns[Index]; }
    inline const QM_Vec3 &operator[](int Index) const { return Columns[Index]; }
#endif
} QM_Mat3;

typedef union QM_Mat4
{
    float Elements[4][4];
    QM_Vec4 Columns[4];

#ifdef __cplusplus
    inline QM_Vec4 &operator[](int Index) { return Columns[Index]; }
    inline const QM_Vec4 &operator[](int Index) const { return Columns[Index]; }
#endif
} QM_Mat4;

typedef union QM_Quat
{
    struct
    {
        union
        {
            QM_Vec3 XYZ;
            struct
            {
                float X, Y, Z;
            };
        };

        float W;
    };

    float Elements[4];

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSE;
#endif
#ifdef HANDMADE_MATH__USE_NEON
    float32x4_t NEON;
#endif
} QM_Quat;

typedef signed int QM_Bool;

/*
 * Angle unit conversion functions
 */
static inline float QM_ToRad(float Angle)
{
#if defined(HANDMADE_MATH_USE_RADIANS)
    float Result = Angle;
#elif defined(HANDMADE_MATH_USE_DEGREES)
    float Result = Angle * QM_DegToRad;
#elif defined(HANDMADE_MATH_USE_TURNS)
    float Result = Angle * QM_TurnToRad;
#endif

    return Result;
}

static inline float QM_ToDeg(float Angle)
{
#if defined(HANDMADE_MATH_USE_RADIANS)
    float Result = Angle * QM_RadToDeg;
#elif defined(HANDMADE_MATH_USE_DEGREES)
    float Result = Angle;
#elif defined(HANDMADE_MATH_USE_TURNS)
    float Result = Angle * QM_TurnToDeg;
#endif

    return Result;
}

static inline float QM_ToTurn(float Angle)
{
#if defined(HANDMADE_MATH_USE_RADIANS)
    float Result = Angle * QM_RadToTurn;
#elif defined(HANDMADE_MATH_USE_DEGREES)
    float Result = Angle * QM_DegToTurn;
#elif defined(HANDMADE_MATH_USE_TURNS)
    float Result = Angle;
#endif

    return Result;
}

/*
 * Floating-point math functions
 */

COVERAGE(QM_SinF, 1)
static inline float QM_SinF(float Angle)
{
    ASSERT_COVERED(QM_SinF);
    return QM_SINF(QM_ANGLE_USER_TO_INTERNAL(Angle));
}

COVERAGE(QM_CosF, 1)
static inline float QM_CosF(float Angle)
{
    ASSERT_COVERED(QM_CosF);
    return QM_COSF(QM_ANGLE_USER_TO_INTERNAL(Angle));
}

COVERAGE(QM_TanF, 1)
static inline float QM_TanF(float Angle)
{
    ASSERT_COVERED(QM_TanF);
    return QM_TANF(QM_ANGLE_USER_TO_INTERNAL(Angle));
}

COVERAGE(QM_ACosF, 1)
static inline float QM_ACosF(float Arg)
{
    ASSERT_COVERED(QM_ACosF);
    return QM_ANGLE_INTERNAL_TO_USER(QM_ACOSF(Arg));
}

COVERAGE(QM_SqrtF, 1)
static inline float QM_SqrtF(float Float)
{
    ASSERT_COVERED(QM_SqrtF);

    float Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 In = _mm_set_ss(Float);
    __m128 Out = _mm_sqrt_ss(In);
    Result = _mm_cvtss_f32(Out);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t In = vdupq_n_f32(Float);
    float32x4_t Out = vsqrtq_f32(In);
    Result = vgetq_lane_f32(Out, 0);
#else
    Result = QM_SQRTF(Float);
#endif

    return Result;
}

COVERAGE(QM_InvSqrtF, 1)
static inline float QM_InvSqrtF(float Float)
{
    ASSERT_COVERED(QM_InvSqrtF);

    float Result;

    Result = 1.0f/QM_SqrtF(Float);

    return Result;
}


/*
 * Utility functions
 */

COVERAGE(QM_Lerp, 1)
static inline float QM_Lerp(float A, float Time, float B)
{
    ASSERT_COVERED(QM_Lerp);
    return (1.0f - Time) * A + Time * B;
}

COVERAGE(QM_Clamp, 1)
static inline float QM_Clamp(float Min, float Value, float Max)
{
    ASSERT_COVERED(QM_Clamp);

    float Result = Value;

    if (Result < Min)
    {
        Result = Min;
    }

    if (Result > Max)
    {
        Result = Max;
    }

    return Result;
}


/*
 * Vector initialization
 */

COVERAGE(QM_V2, 1)
static inline QM_Vec2 QM_V2(float X, float Y)
{
    ASSERT_COVERED(QM_V2);

    QM_Vec2 Result;
    Result.X = X;
    Result.Y = Y;

    return Result;
}

COVERAGE(QM_V3, 1)
static inline QM_Vec3 QM_V3(float X, float Y, float Z)
{
    ASSERT_COVERED(QM_V3);

    QM_Vec3 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;

    return Result;
}

COVERAGE(QM_V4, 1)
static inline QM_Vec4 QM_V4(float X, float Y, float Z, float W)
{
    ASSERT_COVERED(QM_V4);

    QM_Vec4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(X, Y, Z, W);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t v = {X, Y, Z, W};
    Result.NEON = v;
#else
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
#endif

    return Result;
}

COVERAGE(QM_V4V, 1)
static inline QM_Vec4 QM_V4V(QM_Vec3 Vector, float W)
{
    ASSERT_COVERED(QM_V4V);

    QM_Vec4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(Vector.X, Vector.Y, Vector.Z, W);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t v = {Vector.X, Vector.Y, Vector.Z, W};
    Result.NEON = v;
#else
    Result.XYZ = Vector;
    Result.W = W;
#endif

    return Result;
}


/*
 * Binary vector operations
 */

COVERAGE(QM_AddV2, 1)
static inline QM_Vec2 QM_AddV2(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_AddV2);

    QM_Vec2 Result;
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;

    return Result;
}

COVERAGE(QM_AddV3, 1)
static inline QM_Vec3 QM_AddV3(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_AddV3);

    QM_Vec3 Result;
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;
    Result.Z = Left.Z + Right.Z;

    return Result;
}

COVERAGE(QM_AddV4, 1)
static inline QM_Vec4 QM_AddV4(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_AddV4);

    QM_Vec4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_add_ps(Left.SSE, Right.SSE);
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.NEON = vaddq_f32(Left.NEON, Right.NEON);
#else
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;
    Result.Z = Left.Z + Right.Z;
    Result.W = Left.W + Right.W;
#endif

    return Result;
}

COVERAGE(QM_SubV2, 1)
static inline QM_Vec2 QM_SubV2(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_SubV2);

    QM_Vec2 Result;
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;

    return Result;
}

COVERAGE(QM_SubV3, 1)
static inline QM_Vec3 QM_SubV3(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_SubV3);

    QM_Vec3 Result;
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;
    Result.Z = Left.Z - Right.Z;

    return Result;
}

COVERAGE(QM_SubV4, 1)
static inline QM_Vec4 QM_SubV4(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_SubV4);

    QM_Vec4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_sub_ps(Left.SSE, Right.SSE);
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.NEON = vsubq_f32(Left.NEON, Right.NEON);
#else
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;
    Result.Z = Left.Z - Right.Z;
    Result.W = Left.W - Right.W;
#endif

    return Result;
}

COVERAGE(QM_MulV2, 1)
static inline QM_Vec2 QM_MulV2(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_MulV2);

    QM_Vec2 Result;
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;

    return Result;
}

COVERAGE(QM_MulV2F, 1)
static inline QM_Vec2 QM_MulV2F(QM_Vec2 Left, float Right)
{
    ASSERT_COVERED(QM_MulV2F);

    QM_Vec2 Result;
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;

    return Result;
}

COVERAGE(QM_MulV3, 1)
static inline QM_Vec3 QM_MulV3(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_MulV3);

    QM_Vec3 Result;
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;
    Result.Z = Left.Z * Right.Z;

    return Result;
}

COVERAGE(QM_MulV3F, 1)
static inline QM_Vec3 QM_MulV3F(QM_Vec3 Left, float Right)
{
    ASSERT_COVERED(QM_MulV3F);

    QM_Vec3 Result;
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;
    Result.Z = Left.Z * Right;

    return Result;
}

COVERAGE(QM_MulV4, 1)
static inline QM_Vec4 QM_MulV4(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_MulV4);

    QM_Vec4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_mul_ps(Left.SSE, Right.SSE);
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.NEON = vmulq_f32(Left.NEON, Right.NEON);
#else
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;
    Result.Z = Left.Z * Right.Z;
    Result.W = Left.W * Right.W;
#endif

    return Result;
}

COVERAGE(QM_MulV4F, 1)
static inline QM_Vec4 QM_MulV4F(QM_Vec4 Left, float Right)
{
    ASSERT_COVERED(QM_MulV4F);

    QM_Vec4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Right);
    Result.SSE = _mm_mul_ps(Left.SSE, Scalar);
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.NEON = vmulq_n_f32(Left.NEON, Right);
#else
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;
    Result.Z = Left.Z * Right;
    Result.W = Left.W * Right;
#endif

    return Result;
}

COVERAGE(QM_DivV2, 1)
static inline QM_Vec2 QM_DivV2(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_DivV2);

    QM_Vec2 Result;
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;

    return Result;
}

COVERAGE(QM_DivV2F, 1)
static inline QM_Vec2 QM_DivV2F(QM_Vec2 Left, float Right)
{
    ASSERT_COVERED(QM_DivV2F);

    QM_Vec2 Result;
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;

    return Result;
}

COVERAGE(QM_DivV3, 1)
static inline QM_Vec3 QM_DivV3(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_DivV3);

    QM_Vec3 Result;
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;
    Result.Z = Left.Z / Right.Z;

    return Result;
}

COVERAGE(QM_DivV3F, 1)
static inline QM_Vec3 QM_DivV3F(QM_Vec3 Left, float Right)
{
    ASSERT_COVERED(QM_DivV3F);

    QM_Vec3 Result;
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;
    Result.Z = Left.Z / Right;

    return Result;
}

COVERAGE(QM_DivV4, 1)
static inline QM_Vec4 QM_DivV4(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_DivV4);

    QM_Vec4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_div_ps(Left.SSE, Right.SSE);
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.NEON = vdivq_f32(Left.NEON, Right.NEON);
#else
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;
    Result.Z = Left.Z / Right.Z;
    Result.W = Left.W / Right.W;
#endif

    return Result;
}

COVERAGE(QM_DivV4F, 1)
static inline QM_Vec4 QM_DivV4F(QM_Vec4 Left, float Right)
{
    ASSERT_COVERED(QM_DivV4F);

    QM_Vec4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Right);
    Result.SSE = _mm_div_ps(Left.SSE, Scalar);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t Scalar = vdupq_n_f32(Right);
    Result.NEON = vdivq_f32(Left.NEON, Scalar);
#else
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;
    Result.Z = Left.Z / Right;
    Result.W = Left.W / Right;
#endif

    return Result;
}

COVERAGE(QM_EqV2, 1)
static inline QM_Bool QM_EqV2(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_EqV2);
    return Left.X == Right.X && Left.Y == Right.Y;
}

COVERAGE(QM_EqV3, 1)
static inline QM_Bool QM_EqV3(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_EqV3);
    return Left.X == Right.X && Left.Y == Right.Y && Left.Z == Right.Z;
}

COVERAGE(QM_EqV4, 1)
static inline QM_Bool QM_EqV4(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_EqV4);
    return Left.X == Right.X && Left.Y == Right.Y && Left.Z == Right.Z && Left.W == Right.W;
}

COVERAGE(QM_DotV2, 1)
static inline float QM_DotV2(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_DotV2);
    return (Left.X * Right.X) + (Left.Y * Right.Y);
}

COVERAGE(QM_DotV3, 1)
static inline float QM_DotV3(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_DotV3);
    return (Left.X * Right.X) + (Left.Y * Right.Y) + (Left.Z * Right.Z);
}

COVERAGE(QM_DotV4, 1)
static inline float QM_DotV4(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_DotV4);

    float Result;

    // NOTE(zak): IN the future if we wanna check what version SSE is support
    // we can use _mm_dp_ps (4.3) but for now we will use the old way.
    // Or a r = _mm_mul_ps(v1, v2), r = _mm_hadd_ps(r, r), r = _mm_hadd_ps(r, r) for SSE3
#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSEResultOne = _mm_mul_ps(Left.SSE, Right.SSE);
    __m128 SSEResultTwo = _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(2, 3, 0, 1));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    SSEResultTwo = _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(0, 1, 2, 3));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    _mm_store_ss(&Result, SSEResultOne);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t NEONMultiplyResult = vmulq_f32(Left.NEON, Right.NEON);
    float32x4_t NEONHalfAdd = vpaddq_f32(NEONMultiplyResult, NEONMultiplyResult);
    float32x4_t NEONFullAdd = vpaddq_f32(NEONHalfAdd, NEONHalfAdd);
    Result = vgetq_lane_f32(NEONFullAdd, 0);
#else
    Result = ((Left.X * Right.X) + (Left.Z * Right.Z)) + ((Left.Y * Right.Y) + (Left.W * Right.W));
#endif

    return Result;
}

COVERAGE(QM_Cross, 1)
static inline QM_Vec3 QM_Cross(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_Cross);

    QM_Vec3 Result;
    Result.X = (Left.Y * Right.Z) - (Left.Z * Right.Y);
    Result.Y = (Left.Z * Right.X) - (Left.X * Right.Z);
    Result.Z = (Left.X * Right.Y) - (Left.Y * Right.X);

    return Result;
}


/*
 * Unary vector operations
 */

COVERAGE(QM_LenSqrV2, 1)
static inline float QM_LenSqrV2(QM_Vec2 A)
{
    ASSERT_COVERED(QM_LenSqrV2);
    return QM_DotV2(A, A);
}

COVERAGE(QM_LenSqrV3, 1)
static inline float QM_LenSqrV3(QM_Vec3 A)
{
    ASSERT_COVERED(QM_LenSqrV3);
    return QM_DotV3(A, A);
}

COVERAGE(QM_LenSqrV4, 1)
static inline float QM_LenSqrV4(QM_Vec4 A)
{
    ASSERT_COVERED(QM_LenSqrV4);
    return QM_DotV4(A, A);
}

COVERAGE(QM_LenV2, 1)
static inline float QM_LenV2(QM_Vec2 A)
{
    ASSERT_COVERED(QM_LenV2);
    return QM_SqrtF(QM_LenSqrV2(A));
}

COVERAGE(QM_LenV3, 1)
static inline float QM_LenV3(QM_Vec3 A)
{
    ASSERT_COVERED(QM_LenV3);
    return QM_SqrtF(QM_LenSqrV3(A));
}

COVERAGE(QM_LenV4, 1)
static inline float QM_LenV4(QM_Vec4 A)
{
    ASSERT_COVERED(QM_LenV4);
    return QM_SqrtF(QM_LenSqrV4(A));
}

COVERAGE(QM_NormV2, 1)
static inline QM_Vec2 QM_NormV2(QM_Vec2 A)
{
    ASSERT_COVERED(QM_NormV2);
    return QM_MulV2F(A, QM_InvSqrtF(QM_DotV2(A, A)));
}

COVERAGE(QM_NormV3, 1)
static inline QM_Vec3 QM_NormV3(QM_Vec3 A)
{
    ASSERT_COVERED(QM_NormV3);
    return QM_MulV3F(A, QM_InvSqrtF(QM_DotV3(A, A)));
}

COVERAGE(QM_NormV4, 1)
static inline QM_Vec4 QM_NormV4(QM_Vec4 A)
{
    ASSERT_COVERED(QM_NormV4);
    return QM_MulV4F(A, QM_InvSqrtF(QM_DotV4(A, A)));
}

/*
 * Utility vector functions
 */

COVERAGE(QM_LerpV2, 1)
static inline QM_Vec2 QM_LerpV2(QM_Vec2 A, float Time, QM_Vec2 B)
{
    ASSERT_COVERED(QM_LerpV2);
    return QM_AddV2(QM_MulV2F(A, 1.0f - Time), QM_MulV2F(B, Time));
}

COVERAGE(QM_LerpV3, 1)
static inline QM_Vec3 QM_LerpV3(QM_Vec3 A, float Time, QM_Vec3 B)
{
    ASSERT_COVERED(QM_LerpV3);
    return QM_AddV3(QM_MulV3F(A, 1.0f - Time), QM_MulV3F(B, Time));
}

COVERAGE(QM_LerpV4, 1)
static inline QM_Vec4 QM_LerpV4(QM_Vec4 A, float Time, QM_Vec4 B)
{
    ASSERT_COVERED(QM_LerpV4);
    return QM_AddV4(QM_MulV4F(A, 1.0f - Time), QM_MulV4F(B, Time));
}

/*
 * SSE stuff
 */

COVERAGE(QM_LinearCombineV4M4, 1)
static inline QM_Vec4 QM_LinearCombineV4M4(QM_Vec4 Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_LinearCombineV4M4);

    QM_Vec4 Result;
#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_mul_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, 0x00), Right.Columns[0].SSE);
    Result.SSE = _mm_add_ps(Result.SSE, _mm_mul_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, 0x55), Right.Columns[1].SSE));
    Result.SSE = _mm_add_ps(Result.SSE, _mm_mul_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, 0xaa), Right.Columns[2].SSE));
    Result.SSE = _mm_add_ps(Result.SSE, _mm_mul_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, 0xff), Right.Columns[3].SSE));
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.NEON = vmulq_laneq_f32(Right.Columns[0].NEON, Left.NEON, 0);
    Result.NEON = vfmaq_laneq_f32(Result.NEON, Right.Columns[1].NEON, Left.NEON, 1);
    Result.NEON = vfmaq_laneq_f32(Result.NEON, Right.Columns[2].NEON, Left.NEON, 2);
    Result.NEON = vfmaq_laneq_f32(Result.NEON, Right.Columns[3].NEON, Left.NEON, 3);
#else
    Result.X = Left.Elements[0] * Right.Columns[0].X;
    Result.Y = Left.Elements[0] * Right.Columns[0].Y;
    Result.Z = Left.Elements[0] * Right.Columns[0].Z;
    Result.W = Left.Elements[0] * Right.Columns[0].W;

    Result.X += Left.Elements[1] * Right.Columns[1].X;
    Result.Y += Left.Elements[1] * Right.Columns[1].Y;
    Result.Z += Left.Elements[1] * Right.Columns[1].Z;
    Result.W += Left.Elements[1] * Right.Columns[1].W;

    Result.X += Left.Elements[2] * Right.Columns[2].X;
    Result.Y += Left.Elements[2] * Right.Columns[2].Y;
    Result.Z += Left.Elements[2] * Right.Columns[2].Z;
    Result.W += Left.Elements[2] * Right.Columns[2].W;

    Result.X += Left.Elements[3] * Right.Columns[3].X;
    Result.Y += Left.Elements[3] * Right.Columns[3].Y;
    Result.Z += Left.Elements[3] * Right.Columns[3].Z;
    Result.W += Left.Elements[3] * Right.Columns[3].W;
#endif

    return Result;
}

/*
 * 2x2 Matrices
 */

COVERAGE(QM_M2, 1)
static inline QM_Mat2 QM_M2(void)
{
    ASSERT_COVERED(QM_M2);
    QM_Mat2 Result = {0};
    return Result;
}

COVERAGE(QM_M2D, 1)
static inline QM_Mat2 QM_M2D(float Diagonal)
{
    ASSERT_COVERED(QM_M2D);

    QM_Mat2 Result = {0};
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;

    return Result;
}

COVERAGE(QM_TransposeM2, 1)
static inline QM_Mat2 QM_TransposeM2(QM_Mat2 Matrix)
{
    ASSERT_COVERED(QM_TransposeM2);

    QM_Mat2 Result = Matrix;

    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];

    return Result;
}

COVERAGE(QM_AddM2, 1)
static inline QM_Mat2 QM_AddM2(QM_Mat2 Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_AddM2);

    QM_Mat2 Result;

    Result.Elements[0][0] = Left.Elements[0][0] + Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] + Right.Elements[0][1];
    Result.Elements[1][0] = Left.Elements[1][0] + Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] + Right.Elements[1][1];

    return Result;
}

COVERAGE(QM_SubM2, 1)
static inline QM_Mat2 QM_SubM2(QM_Mat2 Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_SubM2);

    QM_Mat2 Result;

    Result.Elements[0][0] = Left.Elements[0][0] - Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] - Right.Elements[0][1];
    Result.Elements[1][0] = Left.Elements[1][0] - Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] - Right.Elements[1][1];

    return Result;
}

COVERAGE(QM_MulM2V2, 1)
static inline QM_Vec2 QM_MulM2V2(QM_Mat2 Matrix, QM_Vec2 Vector)
{
    ASSERT_COVERED(QM_MulM2V2);

    QM_Vec2 Result;

    Result.X = Vector.Elements[0] * Matrix.Columns[0].X;
    Result.Y = Vector.Elements[0] * Matrix.Columns[0].Y;

    Result.X += Vector.Elements[1] * Matrix.Columns[1].X;
    Result.Y += Vector.Elements[1] * Matrix.Columns[1].Y;

    return Result;
}

COVERAGE(QM_MulM2, 1)
static inline QM_Mat2 QM_MulM2(QM_Mat2 Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_MulM2);

    QM_Mat2 Result;
    Result.Columns[0] = QM_MulM2V2(Left, Right.Columns[0]);
    Result.Columns[1] = QM_MulM2V2(Left, Right.Columns[1]);

    return Result;
}

COVERAGE(QM_MulM2F, 1)
static inline QM_Mat2 QM_MulM2F(QM_Mat2 Matrix, float Scalar)
{
    ASSERT_COVERED(QM_MulM2F);

    QM_Mat2 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] * Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] * Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] * Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] * Scalar;

    return Result;
}

COVERAGE(QM_DivM2F, 1)
static inline QM_Mat2 QM_DivM2F(QM_Mat2 Matrix, float Scalar)
{
    ASSERT_COVERED(QM_DivM2F);

    QM_Mat2 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] / Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] / Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] / Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] / Scalar;

    return Result;
}

COVERAGE(QM_DeterminantM2, 1)
static inline float QM_DeterminantM2(QM_Mat2 Matrix)
{
    ASSERT_COVERED(QM_DeterminantM2);
    return Matrix.Elements[0][0]*Matrix.Elements[1][1] - Matrix.Elements[0][1]*Matrix.Elements[1][0];
}


COVERAGE(QM_InvGeneralM2, 1)
static inline QM_Mat2 QM_InvGeneralM2(QM_Mat2 Matrix)
{
    ASSERT_COVERED(QM_InvGeneralM2);

    QM_Mat2 Result;
    float InvDeterminant = 1.0f / QM_DeterminantM2(Matrix);
    Result.Elements[0][0] = InvDeterminant * +Matrix.Elements[1][1];
    Result.Elements[1][1] = InvDeterminant * +Matrix.Elements[0][0];
    Result.Elements[0][1] = InvDeterminant * -Matrix.Elements[0][1];
    Result.Elements[1][0] = InvDeterminant * -Matrix.Elements[1][0];

    return Result;
}

/*
 * 3x3 Matrices
 */

COVERAGE(QM_M3, 1)
static inline QM_Mat3 QM_M3(void)
{
    ASSERT_COVERED(QM_M3);
    QM_Mat3 Result = {0};
    return Result;
}

COVERAGE(QM_M3D, 1)
static inline QM_Mat3 QM_M3D(float Diagonal)
{
    ASSERT_COVERED(QM_M3D);

    QM_Mat3 Result = {0};
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;
    Result.Elements[2][2] = Diagonal;

    return Result;
}

COVERAGE(QM_TransposeM3, 1)
static inline QM_Mat3 QM_TransposeM3(QM_Mat3 Matrix)
{
    ASSERT_COVERED(QM_TransposeM3);

    QM_Mat3 Result = Matrix;

    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[0][2] = Matrix.Elements[2][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];
    Result.Elements[1][2] = Matrix.Elements[2][1];
    Result.Elements[2][1] = Matrix.Elements[1][2];
    Result.Elements[2][0] = Matrix.Elements[0][2];

    return Result;
}

COVERAGE(QM_AddM3, 1)
static inline QM_Mat3 QM_AddM3(QM_Mat3 Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_AddM3);

    QM_Mat3 Result;

    Result.Elements[0][0] = Left.Elements[0][0] + Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] + Right.Elements[0][1];
    Result.Elements[0][2] = Left.Elements[0][2] + Right.Elements[0][2];
    Result.Elements[1][0] = Left.Elements[1][0] + Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] + Right.Elements[1][1];
    Result.Elements[1][2] = Left.Elements[1][2] + Right.Elements[1][2];
    Result.Elements[2][0] = Left.Elements[2][0] + Right.Elements[2][0];
    Result.Elements[2][1] = Left.Elements[2][1] + Right.Elements[2][1];
    Result.Elements[2][2] = Left.Elements[2][2] + Right.Elements[2][2];

    return Result;
}

COVERAGE(QM_SubM3, 1)
static inline QM_Mat3 QM_SubM3(QM_Mat3 Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_SubM3);

    QM_Mat3 Result;

    Result.Elements[0][0] = Left.Elements[0][0] - Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] - Right.Elements[0][1];
    Result.Elements[0][2] = Left.Elements[0][2] - Right.Elements[0][2];
    Result.Elements[1][0] = Left.Elements[1][0] - Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] - Right.Elements[1][1];
    Result.Elements[1][2] = Left.Elements[1][2] - Right.Elements[1][2];
    Result.Elements[2][0] = Left.Elements[2][0] - Right.Elements[2][0];
    Result.Elements[2][1] = Left.Elements[2][1] - Right.Elements[2][1];
    Result.Elements[2][2] = Left.Elements[2][2] - Right.Elements[2][2];

    return Result;
}

COVERAGE(QM_MulM3V3, 1)
static inline QM_Vec3 QM_MulM3V3(QM_Mat3 Matrix, QM_Vec3 Vector)
{
    ASSERT_COVERED(QM_MulM3V3);

    QM_Vec3 Result;

    Result.X = Vector.Elements[0] * Matrix.Columns[0].X;
    Result.Y = Vector.Elements[0] * Matrix.Columns[0].Y;
    Result.Z = Vector.Elements[0] * Matrix.Columns[0].Z;

    Result.X += Vector.Elements[1] * Matrix.Columns[1].X;
    Result.Y += Vector.Elements[1] * Matrix.Columns[1].Y;
    Result.Z += Vector.Elements[1] * Matrix.Columns[1].Z;

    Result.X += Vector.Elements[2] * Matrix.Columns[2].X;
    Result.Y += Vector.Elements[2] * Matrix.Columns[2].Y;
    Result.Z += Vector.Elements[2] * Matrix.Columns[2].Z;

    return Result;
}

COVERAGE(QM_MulM3, 1)
static inline QM_Mat3 QM_MulM3(QM_Mat3 Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_MulM3);

    QM_Mat3 Result;
    Result.Columns[0] = QM_MulM3V3(Left, Right.Columns[0]);
    Result.Columns[1] = QM_MulM3V3(Left, Right.Columns[1]);
    Result.Columns[2] = QM_MulM3V3(Left, Right.Columns[2]);

    return Result;
}

COVERAGE(QM_MulM3F, 1)
static inline QM_Mat3 QM_MulM3F(QM_Mat3 Matrix, float Scalar)
{
    ASSERT_COVERED(QM_MulM3F);

    QM_Mat3 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] * Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] * Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] * Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] * Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] * Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] * Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] * Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] * Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] * Scalar;

    return Result;
}

COVERAGE(QM_DivM3, 1)
static inline QM_Mat3 QM_DivM3F(QM_Mat3 Matrix, float Scalar)
{
    ASSERT_COVERED(QM_DivM3);

    QM_Mat3 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] / Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] / Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] / Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] / Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] / Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] / Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] / Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] / Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] / Scalar;

    return Result;
}

COVERAGE(QM_DeterminantM3, 1)
static inline float QM_DeterminantM3(QM_Mat3 Matrix)
{
    ASSERT_COVERED(QM_DeterminantM3);

    QM_Mat3 Cross;
    Cross.Columns[0] = QM_Cross(Matrix.Columns[1], Matrix.Columns[2]);
    Cross.Columns[1] = QM_Cross(Matrix.Columns[2], Matrix.Columns[0]);
    Cross.Columns[2] = QM_Cross(Matrix.Columns[0], Matrix.Columns[1]);

    return QM_DotV3(Cross.Columns[2], Matrix.Columns[2]);
}

COVERAGE(QM_InvGeneralM3, 1)
static inline QM_Mat3 QM_InvGeneralM3(QM_Mat3 Matrix)
{
    ASSERT_COVERED(QM_InvGeneralM3);

    QM_Mat3 Cross;
    Cross.Columns[0] = QM_Cross(Matrix.Columns[1], Matrix.Columns[2]);
    Cross.Columns[1] = QM_Cross(Matrix.Columns[2], Matrix.Columns[0]);
    Cross.Columns[2] = QM_Cross(Matrix.Columns[0], Matrix.Columns[1]);

    float InvDeterminant = 1.0f / QM_DotV3(Cross.Columns[2], Matrix.Columns[2]);

    QM_Mat3 Result;
    Result.Columns[0] = QM_MulV3F(Cross.Columns[0], InvDeterminant);
    Result.Columns[1] = QM_MulV3F(Cross.Columns[1], InvDeterminant);
    Result.Columns[2] = QM_MulV3F(Cross.Columns[2], InvDeterminant);

    return QM_TransposeM3(Result);
}

/*
 * 4x4 Matrices
 */

COVERAGE(QM_M4, 1)
static inline QM_Mat4 QM_M4(void)
{
    ASSERT_COVERED(QM_M4);
    QM_Mat4 Result = {0};
    return Result;
}

COVERAGE(QM_M4D, 1)
static inline QM_Mat4 QM_M4D(float Diagonal)
{
    ASSERT_COVERED(QM_M4D);

    QM_Mat4 Result = {0};
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;
    Result.Elements[2][2] = Diagonal;
    Result.Elements[3][3] = Diagonal;

    return Result;
}

COVERAGE(QM_TransposeM4, 1)
static inline QM_Mat4 QM_TransposeM4(QM_Mat4 Matrix)
{
    ASSERT_COVERED(QM_TransposeM4);

    QM_Mat4 Result;
#ifdef HANDMADE_MATH__USE_SSE
    Result = Matrix;
    _MM_TRANSPOSE4_PS(Result.Columns[0].SSE, Result.Columns[1].SSE, Result.Columns[2].SSE, Result.Columns[3].SSE);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4x4_t Transposed = vld4q_f32((float*)Matrix.Columns);
    Result.Columns[0].NEON = Transposed.val[0];
    Result.Columns[1].NEON = Transposed.val[1];
    Result.Columns[2].NEON = Transposed.val[2];
    Result.Columns[3].NEON = Transposed.val[3];
#else
    Result.Elements[0][0] = Matrix.Elements[0][0];
    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[0][2] = Matrix.Elements[2][0];
    Result.Elements[0][3] = Matrix.Elements[3][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];
    Result.Elements[1][1] = Matrix.Elements[1][1];
    Result.Elements[1][2] = Matrix.Elements[2][1];
    Result.Elements[1][3] = Matrix.Elements[3][1];
    Result.Elements[2][0] = Matrix.Elements[0][2];
    Result.Elements[2][1] = Matrix.Elements[1][2];
    Result.Elements[2][2] = Matrix.Elements[2][2];
    Result.Elements[2][3] = Matrix.Elements[3][2];
    Result.Elements[3][0] = Matrix.Elements[0][3];
    Result.Elements[3][1] = Matrix.Elements[1][3];
    Result.Elements[3][2] = Matrix.Elements[2][3];
    Result.Elements[3][3] = Matrix.Elements[3][3];
#endif

    return Result;
}

COVERAGE(QM_AddM4, 1)
static inline QM_Mat4 QM_AddM4(QM_Mat4 Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_AddM4);

    QM_Mat4 Result;

    Result.Columns[0] = QM_AddV4(Left.Columns[0], Right.Columns[0]);
    Result.Columns[1] = QM_AddV4(Left.Columns[1], Right.Columns[1]);
    Result.Columns[2] = QM_AddV4(Left.Columns[2], Right.Columns[2]);
    Result.Columns[3] = QM_AddV4(Left.Columns[3], Right.Columns[3]);

    return Result;
}

COVERAGE(QM_SubM4, 1)
static inline QM_Mat4 QM_SubM4(QM_Mat4 Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_SubM4);

    QM_Mat4 Result;

    Result.Columns[0] = QM_SubV4(Left.Columns[0], Right.Columns[0]);
    Result.Columns[1] = QM_SubV4(Left.Columns[1], Right.Columns[1]);
    Result.Columns[2] = QM_SubV4(Left.Columns[2], Right.Columns[2]);
    Result.Columns[3] = QM_SubV4(Left.Columns[3], Right.Columns[3]);

    return Result;
}

COVERAGE(QM_MulM4, 1)
static inline QM_Mat4 QM_MulM4(QM_Mat4 Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_MulM4);

    QM_Mat4 Result;
    Result.Columns[0] = QM_LinearCombineV4M4(Right.Columns[0], Left);
    Result.Columns[1] = QM_LinearCombineV4M4(Right.Columns[1], Left);
    Result.Columns[2] = QM_LinearCombineV4M4(Right.Columns[2], Left);
    Result.Columns[3] = QM_LinearCombineV4M4(Right.Columns[3], Left);

    return Result;
}

COVERAGE(QM_MulM4F, 1)
static inline QM_Mat4 QM_MulM4F(QM_Mat4 Matrix, float Scalar)
{
    ASSERT_COVERED(QM_MulM4F);

    QM_Mat4 Result;
    
    
#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSEScalar = _mm_set1_ps(Scalar);
    Result.Columns[0].SSE = _mm_mul_ps(Matrix.Columns[0].SSE, SSEScalar);
    Result.Columns[1].SSE = _mm_mul_ps(Matrix.Columns[1].SSE, SSEScalar);
    Result.Columns[2].SSE = _mm_mul_ps(Matrix.Columns[2].SSE, SSEScalar);
    Result.Columns[3].SSE = _mm_mul_ps(Matrix.Columns[3].SSE, SSEScalar);
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.Columns[0].NEON = vmulq_n_f32(Matrix.Columns[0].NEON, Scalar);
    Result.Columns[1].NEON = vmulq_n_f32(Matrix.Columns[1].NEON, Scalar);
    Result.Columns[2].NEON = vmulq_n_f32(Matrix.Columns[2].NEON, Scalar);
    Result.Columns[3].NEON = vmulq_n_f32(Matrix.Columns[3].NEON, Scalar);
#else
    Result.Elements[0][0] = Matrix.Elements[0][0] * Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] * Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] * Scalar;
    Result.Elements[0][3] = Matrix.Elements[0][3] * Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] * Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] * Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] * Scalar;
    Result.Elements[1][3] = Matrix.Elements[1][3] * Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] * Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] * Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] * Scalar;
    Result.Elements[2][3] = Matrix.Elements[2][3] * Scalar;
    Result.Elements[3][0] = Matrix.Elements[3][0] * Scalar;
    Result.Elements[3][1] = Matrix.Elements[3][1] * Scalar;
    Result.Elements[3][2] = Matrix.Elements[3][2] * Scalar;
    Result.Elements[3][3] = Matrix.Elements[3][3] * Scalar;
#endif

    return Result;
}

COVERAGE(QM_MulM4V4, 1)
static inline QM_Vec4 QM_MulM4V4(QM_Mat4 Matrix, QM_Vec4 Vector)
{
    ASSERT_COVERED(QM_MulM4V4);
    return QM_LinearCombineV4M4(Vector, Matrix);
}

COVERAGE(QM_DivM4F, 1)
static inline QM_Mat4 QM_DivM4F(QM_Mat4 Matrix, float Scalar)
{
    ASSERT_COVERED(QM_DivM4F);

    QM_Mat4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSEScalar = _mm_set1_ps(Scalar);
    Result.Columns[0].SSE = _mm_div_ps(Matrix.Columns[0].SSE, SSEScalar);
    Result.Columns[1].SSE = _mm_div_ps(Matrix.Columns[1].SSE, SSEScalar);
    Result.Columns[2].SSE = _mm_div_ps(Matrix.Columns[2].SSE, SSEScalar);
    Result.Columns[3].SSE = _mm_div_ps(Matrix.Columns[3].SSE, SSEScalar);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t NEONScalar = vdupq_n_f32(Scalar);
    Result.Columns[0].NEON = vdivq_f32(Matrix.Columns[0].NEON, NEONScalar);
    Result.Columns[1].NEON = vdivq_f32(Matrix.Columns[1].NEON, NEONScalar);
    Result.Columns[2].NEON = vdivq_f32(Matrix.Columns[2].NEON, NEONScalar);
    Result.Columns[3].NEON = vdivq_f32(Matrix.Columns[3].NEON, NEONScalar);
#else
    Result.Elements[0][0] = Matrix.Elements[0][0] / Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] / Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] / Scalar;
    Result.Elements[0][3] = Matrix.Elements[0][3] / Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] / Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] / Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] / Scalar;
    Result.Elements[1][3] = Matrix.Elements[1][3] / Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] / Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] / Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] / Scalar;
    Result.Elements[2][3] = Matrix.Elements[2][3] / Scalar;
    Result.Elements[3][0] = Matrix.Elements[3][0] / Scalar;
    Result.Elements[3][1] = Matrix.Elements[3][1] / Scalar;
    Result.Elements[3][2] = Matrix.Elements[3][2] / Scalar;
    Result.Elements[3][3] = Matrix.Elements[3][3] / Scalar;
#endif

    return Result;
}

COVERAGE(QM_DeterminantM4, 1)
static inline float QM_DeterminantM4(QM_Mat4 Matrix)
{
    ASSERT_COVERED(QM_DeterminantM4);

    QM_Vec3 C01 = QM_Cross(Matrix.Columns[0].XYZ, Matrix.Columns[1].XYZ);
    QM_Vec3 C23 = QM_Cross(Matrix.Columns[2].XYZ, Matrix.Columns[3].XYZ);
    QM_Vec3 B10 = QM_SubV3(QM_MulV3F(Matrix.Columns[0].XYZ, Matrix.Columns[1].W), QM_MulV3F(Matrix.Columns[1].XYZ, Matrix.Columns[0].W));
    QM_Vec3 B32 = QM_SubV3(QM_MulV3F(Matrix.Columns[2].XYZ, Matrix.Columns[3].W), QM_MulV3F(Matrix.Columns[3].XYZ, Matrix.Columns[2].W));

    return QM_DotV3(C01, B32) + QM_DotV3(C23, B10);
}

COVERAGE(QM_InvGeneralM4, 1)
// Returns a general-purpose inverse of an QM_Mat4. Note that special-purpose inverses of many transformations
// are available and will be more efficient.
static inline QM_Mat4 QM_InvGeneralM4(QM_Mat4 Matrix)
{
    ASSERT_COVERED(QM_InvGeneralM4);

    QM_Vec3 C01 = QM_Cross(Matrix.Columns[0].XYZ, Matrix.Columns[1].XYZ);
    QM_Vec3 C23 = QM_Cross(Matrix.Columns[2].XYZ, Matrix.Columns[3].XYZ);
    QM_Vec3 B10 = QM_SubV3(QM_MulV3F(Matrix.Columns[0].XYZ, Matrix.Columns[1].W), QM_MulV3F(Matrix.Columns[1].XYZ, Matrix.Columns[0].W));
    QM_Vec3 B32 = QM_SubV3(QM_MulV3F(Matrix.Columns[2].XYZ, Matrix.Columns[3].W), QM_MulV3F(Matrix.Columns[3].XYZ, Matrix.Columns[2].W));

    float InvDeterminant = 1.0f / (QM_DotV3(C01, B32) + QM_DotV3(C23, B10));
    C01 = QM_MulV3F(C01, InvDeterminant);
    C23 = QM_MulV3F(C23, InvDeterminant);
    B10 = QM_MulV3F(B10, InvDeterminant);
    B32 = QM_MulV3F(B32, InvDeterminant);

    QM_Mat4 Result;
    Result.Columns[0] = QM_V4V(QM_AddV3(QM_Cross(Matrix.Columns[1].XYZ, B32), QM_MulV3F(C23, Matrix.Columns[1].W)), -QM_DotV3(Matrix.Columns[1].XYZ, C23));
    Result.Columns[1] = QM_V4V(QM_SubV3(QM_Cross(B32, Matrix.Columns[0].XYZ), QM_MulV3F(C23, Matrix.Columns[0].W)), +QM_DotV3(Matrix.Columns[0].XYZ, C23));
    Result.Columns[2] = QM_V4V(QM_AddV3(QM_Cross(Matrix.Columns[3].XYZ, B10), QM_MulV3F(C01, Matrix.Columns[3].W)), -QM_DotV3(Matrix.Columns[3].XYZ, C01));
    Result.Columns[3] = QM_V4V(QM_SubV3(QM_Cross(B10, Matrix.Columns[2].XYZ), QM_MulV3F(C01, Matrix.Columns[2].W)), +QM_DotV3(Matrix.Columns[2].XYZ, C01));

    return QM_TransposeM4(Result);
}

/*
 * Common graphics transformations
 */

COVERAGE(QM_Orthographic_RH_NO, 1)
// Produces a right-handed orthographic projection matrix with Z ranging from -1 to 1 (the GL convention).
// Left, Right, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
static inline QM_Mat4 QM_Orthographic_RH_NO(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    ASSERT_COVERED(QM_Orthographic_RH_NO);

    QM_Mat4 Result = {0};

    Result.Elements[0][0] = 2.0f / (Right - Left);
    Result.Elements[1][1] = 2.0f / (Top - Bottom);
    Result.Elements[2][2] = 2.0f / (Near - Far);
    Result.Elements[3][3] = 1.0f;

    Result.Elements[3][0] = (Left + Right) / (Left - Right);
    Result.Elements[3][1] = (Bottom + Top) / (Bottom - Top);
    Result.Elements[3][2] = (Near + Far) / (Near - Far);

    return Result;
}

COVERAGE(QM_Orthographic_RH_ZO, 1)
// Produces a right-handed orthographic projection matrix with Z ranging from 0 to 1 (the DirectX convention).
// Left, Right, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
static inline QM_Mat4 QM_Orthographic_RH_ZO(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    ASSERT_COVERED(QM_Orthographic_RH_ZO);

    QM_Mat4 Result = {0};

    Result.Elements[0][0] = 2.0f / (Right - Left);
    Result.Elements[1][1] = 2.0f / (Top - Bottom);
    Result.Elements[2][2] = 1.0f / (Near - Far);
    Result.Elements[3][3] = 1.0f;

    Result.Elements[3][0] = (Left + Right) / (Left - Right);
    Result.Elements[3][1] = (Bottom + Top) / (Bottom - Top);
    Result.Elements[3][2] = (Near) / (Near - Far);

    return Result;
}

COVERAGE(QM_Orthographic_LH_NO, 1)
// Produces a left-handed orthographic projection matrix with Z ranging from -1 to 1 (the GL convention).
// Left, Right, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
static inline QM_Mat4 QM_Orthographic_LH_NO(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    ASSERT_COVERED(QM_Orthographic_LH_NO);

    QM_Mat4 Result = QM_Orthographic_RH_NO(Left, Right, Bottom, Top, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];

    return Result;
}

COVERAGE(QM_Orthographic_LH_ZO, 1)
// Produces a left-handed orthographic projection matrix with Z ranging from 0 to 1 (the DirectX convention).
// Left, Right, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
static inline QM_Mat4 QM_Orthographic_LH_ZO(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    ASSERT_COVERED(QM_Orthographic_LH_ZO);

    QM_Mat4 Result = QM_Orthographic_RH_ZO(Left, Right, Bottom, Top, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];

    return Result;
}

COVERAGE(QM_InvOrthographic, 1)
// Returns an inverse for the given orthographic projection matrix. Works for all orthographic
// projection matrices, regardless of handedness or NDC convention.
static inline QM_Mat4 QM_InvOrthographic(QM_Mat4 OrthoMatrix)
{
    ASSERT_COVERED(QM_InvOrthographic);

    QM_Mat4 Result = {0};
    Result.Elements[0][0] = 1.0f / OrthoMatrix.Elements[0][0];
    Result.Elements[1][1] = 1.0f / OrthoMatrix.Elements[1][1];
    Result.Elements[2][2] = 1.0f / OrthoMatrix.Elements[2][2];
    Result.Elements[3][3] = 1.0f;

    Result.Elements[3][0] = -OrthoMatrix.Elements[3][0] * Result.Elements[0][0];
    Result.Elements[3][1] = -OrthoMatrix.Elements[3][1] * Result.Elements[1][1];
    Result.Elements[3][2] = -OrthoMatrix.Elements[3][2] * Result.Elements[2][2];

    return Result;
}

COVERAGE(QM_Perspective_RH_NO, 1)
static inline QM_Mat4 QM_Perspective_RH_NO(float FOV, float AspectRatio, float Near, float Far)
{
    ASSERT_COVERED(QM_Perspective_RH_NO);

    QM_Mat4 Result = {0};

    // See https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml

    float Cotangent = 1.0f / QM_TanF(FOV / 2.0f);
    Result.Elements[0][0] = Cotangent / AspectRatio;
    Result.Elements[1][1] = Cotangent;
    Result.Elements[2][3] = -1.0f;

    Result.Elements[2][2] = (Near + Far) / (Near - Far);
    Result.Elements[3][2] = (2.0f * Near * Far) / (Near - Far);

    return Result;
}

COVERAGE(QM_Perspective_RH_ZO, 1)
static inline QM_Mat4 QM_Perspective_RH_ZO(float FOV, float AspectRatio, float Near, float Far)
{
    ASSERT_COVERED(QM_Perspective_RH_ZO);

    QM_Mat4 Result = {0};

    // See https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml

    float Cotangent = 1.0f / QM_TanF(FOV / 2.0f);
    Result.Elements[0][0] = Cotangent / AspectRatio;
    Result.Elements[1][1] = Cotangent;
    Result.Elements[2][3] = -1.0f;

    Result.Elements[2][2] = (Far) / (Near - Far);
    Result.Elements[3][2] = (Near * Far) / (Near - Far);

    return Result;
}

COVERAGE(QM_Perspective_LH_NO, 1)
static inline QM_Mat4 QM_Perspective_LH_NO(float FOV, float AspectRatio, float Near, float Far)
{
    ASSERT_COVERED(QM_Perspective_LH_NO);

    QM_Mat4 Result = QM_Perspective_RH_NO(FOV, AspectRatio, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];
    Result.Elements[2][3] = -Result.Elements[2][3];

    return Result;
}

COVERAGE(QM_Perspective_LH_ZO, 1)
static inline QM_Mat4 QM_Perspective_LH_ZO(float FOV, float AspectRatio, float Near, float Far)
{
    ASSERT_COVERED(QM_Perspective_LH_ZO);

    QM_Mat4 Result = QM_Perspective_RH_ZO(FOV, AspectRatio, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];
    Result.Elements[2][3] = -Result.Elements[2][3];

    return Result;
}

COVERAGE(QM_InvPerspective_RH, 1)
static inline QM_Mat4 QM_InvPerspective_RH(QM_Mat4 PerspectiveMatrix)
{
    ASSERT_COVERED(QM_InvPerspective_RH);

    QM_Mat4 Result = {0};
    Result.Elements[0][0] = 1.0f / PerspectiveMatrix.Elements[0][0];
    Result.Elements[1][1] = 1.0f / PerspectiveMatrix.Elements[1][1];
    Result.Elements[2][2] = 0.0f;

    Result.Elements[2][3] = 1.0f / PerspectiveMatrix.Elements[3][2];
    Result.Elements[3][3] = PerspectiveMatrix.Elements[2][2] * Result.Elements[2][3];
    Result.Elements[3][2] = PerspectiveMatrix.Elements[2][3];

    return Result;
}

COVERAGE(QM_InvPerspective_LH, 1)
static inline QM_Mat4 QM_InvPerspective_LH(QM_Mat4 PerspectiveMatrix)
{
    ASSERT_COVERED(QM_InvPerspective_LH);

    QM_Mat4 Result = {0};
    Result.Elements[0][0] = 1.0f / PerspectiveMatrix.Elements[0][0];
    Result.Elements[1][1] = 1.0f / PerspectiveMatrix.Elements[1][1];
    Result.Elements[2][2] = 0.0f;

    Result.Elements[2][3] = 1.0f / PerspectiveMatrix.Elements[3][2];
    Result.Elements[3][3] = PerspectiveMatrix.Elements[2][2] * -Result.Elements[2][3];
    Result.Elements[3][2] = PerspectiveMatrix.Elements[2][3];

    return Result;
}

COVERAGE(QM_Translate, 1)
static inline QM_Mat4 QM_Translate(QM_Vec3 Translation)
{
    ASSERT_COVERED(QM_Translate);

    QM_Mat4 Result = QM_M4D(1.0f);
    Result.Elements[3][0] = Translation.X;
    Result.Elements[3][1] = Translation.Y;
    Result.Elements[3][2] = Translation.Z;

    return Result;
}

COVERAGE(QM_InvTranslate, 1)
static inline QM_Mat4 QM_InvTranslate(QM_Mat4 TranslationMatrix)
{
    ASSERT_COVERED(QM_InvTranslate);

    QM_Mat4 Result = TranslationMatrix;
    Result.Elements[3][0] = -Result.Elements[3][0];
    Result.Elements[3][1] = -Result.Elements[3][1];
    Result.Elements[3][2] = -Result.Elements[3][2];

    return Result;
}

COVERAGE(QM_Rotate_RH, 1)
static inline QM_Mat4 QM_Rotate_RH(float Angle, QM_Vec3 Axis)
{
    ASSERT_COVERED(QM_Rotate_RH);

    QM_Mat4 Result = QM_M4D(1.0f);

    Axis = QM_NormV3(Axis);

    float SinTheta = QM_SinF(Angle);
    float CosTheta = QM_CosF(Angle);
    float CosValue = 1.0f - CosTheta;

    Result.Elements[0][0] = (Axis.X * Axis.X * CosValue) + CosTheta;
    Result.Elements[0][1] = (Axis.X * Axis.Y * CosValue) + (Axis.Z * SinTheta);
    Result.Elements[0][2] = (Axis.X * Axis.Z * CosValue) - (Axis.Y * SinTheta);

    Result.Elements[1][0] = (Axis.Y * Axis.X * CosValue) - (Axis.Z * SinTheta);
    Result.Elements[1][1] = (Axis.Y * Axis.Y * CosValue) + CosTheta;
    Result.Elements[1][2] = (Axis.Y * Axis.Z * CosValue) + (Axis.X * SinTheta);

    Result.Elements[2][0] = (Axis.Z * Axis.X * CosValue) + (Axis.Y * SinTheta);
    Result.Elements[2][1] = (Axis.Z * Axis.Y * CosValue) - (Axis.X * SinTheta);
    Result.Elements[2][2] = (Axis.Z * Axis.Z * CosValue) + CosTheta;

    return Result;
}

COVERAGE(QM_Rotate_LH, 1)
static inline QM_Mat4 QM_Rotate_LH(float Angle, QM_Vec3 Axis)
{
    ASSERT_COVERED(QM_Rotate_LH);
    /* NOTE(lcf): Matrix will be inverse/transpose of RH. */
    return QM_Rotate_RH(-Angle, Axis);
}

COVERAGE(QM_InvRotate, 1)
static inline QM_Mat4 QM_InvRotate(QM_Mat4 RotationMatrix)
{
    ASSERT_COVERED(QM_InvRotate);
    return QM_TransposeM4(RotationMatrix);
}

COVERAGE(QM_Scale, 1)
static inline QM_Mat4 QM_Scale(QM_Vec3 Scale)
{
    ASSERT_COVERED(QM_Scale);

    QM_Mat4 Result = QM_M4D(1.0f);
    Result.Elements[0][0] = Scale.X;
    Result.Elements[1][1] = Scale.Y;
    Result.Elements[2][2] = Scale.Z;

    return Result;
}

COVERAGE(QM_InvScale, 1)
static inline QM_Mat4 QM_InvScale(QM_Mat4 ScaleMatrix)
{
    ASSERT_COVERED(QM_InvScale);

    QM_Mat4 Result = ScaleMatrix;
    Result.Elements[0][0] = 1.0f / Result.Elements[0][0];
    Result.Elements[1][1] = 1.0f / Result.Elements[1][1];
    Result.Elements[2][2] = 1.0f / Result.Elements[2][2];

    return Result;
}

static inline QM_Mat4 _QM_LookAt(QM_Vec3 F,  QM_Vec3 S, QM_Vec3 U,  QM_Vec3 Eye)
{
    QM_Mat4 Result;

    Result.Elements[0][0] = S.X;
    Result.Elements[0][1] = U.X;
    Result.Elements[0][2] = -F.X;
    Result.Elements[0][3] = 0.0f;

    Result.Elements[1][0] = S.Y;
    Result.Elements[1][1] = U.Y;
    Result.Elements[1][2] = -F.Y;
    Result.Elements[1][3] = 0.0f;

    Result.Elements[2][0] = S.Z;
    Result.Elements[2][1] = U.Z;
    Result.Elements[2][2] = -F.Z;
    Result.Elements[2][3] = 0.0f;

    Result.Elements[3][0] = -QM_DotV3(S, Eye);
    Result.Elements[3][1] = -QM_DotV3(U, Eye);
    Result.Elements[3][2] = QM_DotV3(F, Eye);
    Result.Elements[3][3] = 1.0f;

    return Result;
}

COVERAGE(QM_LookAt_RH, 1)
static inline QM_Mat4 QM_LookAt_RH(QM_Vec3 Eye, QM_Vec3 Center, QM_Vec3 Up)
{
    ASSERT_COVERED(QM_LookAt_RH);

    QM_Vec3 F = QM_NormV3(QM_SubV3(Center, Eye));
    QM_Vec3 S = QM_NormV3(QM_Cross(F, Up));
    QM_Vec3 U = QM_Cross(S, F);

    return _QM_LookAt(F, S, U, Eye);
}

COVERAGE(QM_LookAt_LH, 1)
static inline QM_Mat4 QM_LookAt_LH(QM_Vec3 Eye, QM_Vec3 Center, QM_Vec3 Up)
{
    ASSERT_COVERED(QM_LookAt_LH);

    QM_Vec3 F = QM_NormV3(QM_SubV3(Eye, Center));
    QM_Vec3 S = QM_NormV3(QM_Cross(F, Up));
    QM_Vec3 U = QM_Cross(S, F);

    return _QM_LookAt(F, S, U, Eye);
}

COVERAGE(QM_InvLookAt, 1)
static inline QM_Mat4 QM_InvLookAt(QM_Mat4 Matrix)
{
    ASSERT_COVERED(QM_InvLookAt);
    QM_Mat4 Result;

    QM_Mat3 Rotation = {0};
    Rotation.Columns[0] = Matrix.Columns[0].XYZ;
    Rotation.Columns[1] = Matrix.Columns[1].XYZ;
    Rotation.Columns[2] = Matrix.Columns[2].XYZ;
    Rotation = QM_TransposeM3(Rotation);

    Result.Columns[0] = QM_V4V(Rotation.Columns[0], 0.0f);
    Result.Columns[1] = QM_V4V(Rotation.Columns[1], 0.0f);
    Result.Columns[2] = QM_V4V(Rotation.Columns[2], 0.0f);
    Result.Columns[3] = QM_MulV4F(Matrix.Columns[3], -1.0f);
    Result.Elements[3][0] = -1.0f * Matrix.Elements[3][0] /
        (Rotation.Elements[0][0] + Rotation.Elements[0][1] + Rotation.Elements[0][2]);
    Result.Elements[3][1] = -1.0f * Matrix.Elements[3][1] /
        (Rotation.Elements[1][0] + Rotation.Elements[1][1] + Rotation.Elements[1][2]);
    Result.Elements[3][2] = -1.0f * Matrix.Elements[3][2] /
        (Rotation.Elements[2][0] + Rotation.Elements[2][1] + Rotation.Elements[2][2]);
    Result.Elements[3][3] = 1.0f;

    return Result;
}

/*
 * Quaternion operations
 */

COVERAGE(QM_Q, 1)
static inline QM_Quat QM_Q(float X, float Y, float Z, float W)
{
    ASSERT_COVERED(QM_Q);

    QM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(X, Y, Z, W);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t v = { X, Y, Z, W };
    Result.NEON = v;
#else
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
#endif

    return Result;
}

COVERAGE(QM_QV4, 1)
static inline QM_Quat QM_QV4(QM_Vec4 Vector)
{
    ASSERT_COVERED(QM_QV4);

    QM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = Vector.SSE;
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.NEON = Vector.NEON;
#else
    Result.X = Vector.X;
    Result.Y = Vector.Y;
    Result.Z = Vector.Z;
    Result.W = Vector.W;
#endif

    return Result;
}

COVERAGE(QM_AddQ, 1)
static inline QM_Quat QM_AddQ(QM_Quat Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_AddQ);

    QM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_add_ps(Left.SSE, Right.SSE);
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.NEON = vaddq_f32(Left.NEON, Right.NEON);
#else

    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;
    Result.Z = Left.Z + Right.Z;
    Result.W = Left.W + Right.W;
#endif

    return Result;
}

COVERAGE(QM_SubQ, 1)
static inline QM_Quat QM_SubQ(QM_Quat Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_SubQ);

    QM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_sub_ps(Left.SSE, Right.SSE);
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.NEON = vsubq_f32(Left.NEON, Right.NEON);
#else
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;
    Result.Z = Left.Z - Right.Z;
    Result.W = Left.W - Right.W;
#endif

    return Result;
}

COVERAGE(QM_MulQ, 1)
static inline QM_Quat QM_MulQ(QM_Quat Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_MulQ);

    QM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSEResultOne = _mm_xor_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(0, 0, 0, 0)), _mm_setr_ps(0.f, -0.f, 0.f, -0.f));
    __m128 SSEResultTwo = _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(0, 1, 2, 3));
    __m128 SSEResultThree = _mm_mul_ps(SSEResultTwo, SSEResultOne);

    SSEResultOne = _mm_xor_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(1, 1, 1, 1)) , _mm_setr_ps(0.f, 0.f, -0.f, -0.f));
    SSEResultTwo = _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(1, 0, 3, 2));
    SSEResultThree = _mm_add_ps(SSEResultThree, _mm_mul_ps(SSEResultTwo, SSEResultOne));

    SSEResultOne = _mm_xor_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(2, 2, 2, 2)), _mm_setr_ps(-0.f, 0.f, 0.f, -0.f));
    SSEResultTwo = _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(2, 3, 0, 1));
    SSEResultThree = _mm_add_ps(SSEResultThree, _mm_mul_ps(SSEResultTwo, SSEResultOne));

    SSEResultOne = _mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(3, 3, 3, 3));
    SSEResultTwo = _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(3, 2, 1, 0));
    Result.SSE = _mm_add_ps(SSEResultThree, _mm_mul_ps(SSEResultTwo, SSEResultOne));
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t Right1032 = vrev64q_f32(Right.NEON);
    float32x4_t Right3210 = vcombine_f32(vget_high_f32(Right1032), vget_low_f32(Right1032));
    float32x4_t Right2301 = vrev64q_f32(Right3210);
    
    float32x4_t FirstSign = {1.0f, -1.0f, 1.0f, -1.0f};
    Result.NEON = vmulq_f32(Right3210, vmulq_f32(vdupq_laneq_f32(Left.NEON, 0), FirstSign));
    float32x4_t SecondSign = {1.0f, 1.0f, -1.0f, -1.0f};
    Result.NEON = vfmaq_f32(Result.NEON, Right2301, vmulq_f32(vdupq_laneq_f32(Left.NEON, 1), SecondSign));
    float32x4_t ThirdSign = {-1.0f, 1.0f, 1.0f, -1.0f};
    Result.NEON = vfmaq_f32(Result.NEON, Right1032, vmulq_f32(vdupq_laneq_f32(Left.NEON, 2), ThirdSign));
    Result.NEON = vfmaq_laneq_f32(Result.NEON, Right.NEON, Left.NEON, 3);
    
#else
    Result.X =  Right.Elements[3] * +Left.Elements[0];
    Result.Y =  Right.Elements[2] * -Left.Elements[0];
    Result.Z =  Right.Elements[1] * +Left.Elements[0];
    Result.W =  Right.Elements[0] * -Left.Elements[0];

    Result.X += Right.Elements[2] * +Left.Elements[1];
    Result.Y += Right.Elements[3] * +Left.Elements[1];
    Result.Z += Right.Elements[0] * -Left.Elements[1];
    Result.W += Right.Elements[1] * -Left.Elements[1];

    Result.X += Right.Elements[1] * -Left.Elements[2];
    Result.Y += Right.Elements[0] * +Left.Elements[2];
    Result.Z += Right.Elements[3] * +Left.Elements[2];
    Result.W += Right.Elements[2] * -Left.Elements[2];

    Result.X += Right.Elements[0] * +Left.Elements[3];
    Result.Y += Right.Elements[1] * +Left.Elements[3];
    Result.Z += Right.Elements[2] * +Left.Elements[3];
    Result.W += Right.Elements[3] * +Left.Elements[3];
#endif

    return Result;
}

COVERAGE(QM_MulQF, 1)
static inline QM_Quat QM_MulQF(QM_Quat Left, float Multiplicative)
{
    ASSERT_COVERED(QM_MulQF);

    QM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Multiplicative);
    Result.SSE = _mm_mul_ps(Left.SSE, Scalar);
#elif defined(HANDMADE_MATH__USE_NEON)
    Result.NEON = vmulq_n_f32(Left.NEON, Multiplicative);
#else
    Result.X = Left.X * Multiplicative;
    Result.Y = Left.Y * Multiplicative;
    Result.Z = Left.Z * Multiplicative;
    Result.W = Left.W * Multiplicative;
#endif

    return Result;
}

COVERAGE(QM_DivQF, 1)
static inline QM_Quat QM_DivQF(QM_Quat Left, float Divnd)
{
    ASSERT_COVERED(QM_DivQF);

    QM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Divnd);
    Result.SSE = _mm_div_ps(Left.SSE, Scalar);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t Scalar = vdupq_n_f32(Divnd);
    Result.NEON = vdivq_f32(Left.NEON, Scalar);
#else
    Result.X = Left.X / Divnd;
    Result.Y = Left.Y / Divnd;
    Result.Z = Left.Z / Divnd;
    Result.W = Left.W / Divnd;
#endif

    return Result;
}

COVERAGE(QM_DotQ, 1)
static inline float QM_DotQ(QM_Quat Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_DotQ);

    float Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSEResultOne = _mm_mul_ps(Left.SSE, Right.SSE);
    __m128 SSEResultTwo = _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(2, 3, 0, 1));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    SSEResultTwo = _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(0, 1, 2, 3));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    _mm_store_ss(&Result, SSEResultOne);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t NEONMultiplyResult = vmulq_f32(Left.NEON, Right.NEON);
    float32x4_t NEONHalfAdd = vpaddq_f32(NEONMultiplyResult, NEONMultiplyResult);
    float32x4_t NEONFullAdd = vpaddq_f32(NEONHalfAdd, NEONHalfAdd);
    Result = vgetq_lane_f32(NEONFullAdd, 0);
#else
    Result = ((Left.X * Right.X) + (Left.Z * Right.Z)) + ((Left.Y * Right.Y) + (Left.W * Right.W));
#endif

    return Result;
}

COVERAGE(QM_InvQ, 1)
static inline QM_Quat QM_InvQ(QM_Quat Left)
{
    ASSERT_COVERED(QM_InvQ);

    QM_Quat Result;
    Result.X = -Left.X;
    Result.Y = -Left.Y;
    Result.Z = -Left.Z;
    Result.W = Left.W;

    return QM_DivQF(Result, (QM_DotQ(Left, Left)));
}

COVERAGE(QM_NormQ, 1)
static inline QM_Quat QM_NormQ(QM_Quat Quat)
{
    ASSERT_COVERED(QM_NormQ);

    /* NOTE(lcf): Take advantage of SSE implementation in QM_NormV4 */
    QM_Vec4 Vec = {Quat.X, Quat.Y, Quat.Z, Quat.W};
    Vec = QM_NormV4(Vec);
    QM_Quat Result = {Vec.X, Vec.Y, Vec.Z, Vec.W};

    return Result;
}

static inline QM_Quat _QM_MixQ(QM_Quat Left, float MixLeft, QM_Quat Right, float MixRight) {
    QM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 ScalarLeft = _mm_set1_ps(MixLeft);
    __m128 ScalarRight = _mm_set1_ps(MixRight);
    __m128 SSEResultOne = _mm_mul_ps(Left.SSE, ScalarLeft);
    __m128 SSEResultTwo = _mm_mul_ps(Right.SSE, ScalarRight);
    Result.SSE = _mm_add_ps(SSEResultOne, SSEResultTwo);
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t ScaledLeft = vmulq_n_f32(Left.NEON, MixLeft);
    float32x4_t ScaledRight = vmulq_n_f32(Right.NEON, MixRight);
    Result.NEON = vaddq_f32(ScaledLeft, ScaledRight);
#else
    Result.X = Left.X*MixLeft + Right.X*MixRight;
    Result.Y = Left.Y*MixLeft + Right.Y*MixRight;
    Result.Z = Left.Z*MixLeft + Right.Z*MixRight;
    Result.W = Left.W*MixLeft + Right.W*MixRight;
#endif

    return Result;
}

COVERAGE(QM_NLerp, 1)
static inline QM_Quat QM_NLerp(QM_Quat Left, float Time, QM_Quat Right)
{
    ASSERT_COVERED(QM_NLerp);

    QM_Quat Result = _QM_MixQ(Left, 1.0f-Time, Right, Time);
    Result = QM_NormQ(Result);

    return Result;
}

COVERAGE(QM_SLerp, 1)
static inline QM_Quat QM_SLerp(QM_Quat Left, float Time, QM_Quat Right)
{
    ASSERT_COVERED(QM_SLerp);

    QM_Quat Result;

    float Cos_Theta = QM_DotQ(Left, Right);

    if (Cos_Theta < 0.0f) { /* NOTE(lcf): Take shortest path on Hyper-sphere */
        Cos_Theta = -Cos_Theta;
        Right = QM_Q(-Right.X, -Right.Y, -Right.Z, -Right.W);
    }

    /* NOTE(lcf): Use Normalized Linear interpolation when vectors are roughly not L.I. */
    if (Cos_Theta > 0.9995f) {
        Result = QM_NLerp(Left, Time, Right);
    } else {
        float Angle = QM_ACosF(Cos_Theta);
        float MixLeft = QM_SinF((1.0f - Time) * Angle);
        float MixRight = QM_SinF(Time * Angle);

        Result = _QM_MixQ(Left, MixLeft, Right, MixRight);
        Result = QM_NormQ(Result);
    }

    return Result;
}

COVERAGE(QM_QToM4, 1)
static inline QM_Mat4 QM_QToM4(QM_Quat Left)
{
    ASSERT_COVERED(QM_QToM4);

    QM_Mat4 Result;

    QM_Quat NormalizedQ = QM_NormQ(Left);

    float XX, YY, ZZ,
          XY, XZ, YZ,
          WX, WY, WZ;

    XX = NormalizedQ.X * NormalizedQ.X;
    YY = NormalizedQ.Y * NormalizedQ.Y;
    ZZ = NormalizedQ.Z * NormalizedQ.Z;
    XY = NormalizedQ.X * NormalizedQ.Y;
    XZ = NormalizedQ.X * NormalizedQ.Z;
    YZ = NormalizedQ.Y * NormalizedQ.Z;
    WX = NormalizedQ.W * NormalizedQ.X;
    WY = NormalizedQ.W * NormalizedQ.Y;
    WZ = NormalizedQ.W * NormalizedQ.Z;

    Result.Elements[0][0] = 1.0f - 2.0f * (YY + ZZ);
    Result.Elements[0][1] = 2.0f * (XY + WZ);
    Result.Elements[0][2] = 2.0f * (XZ - WY);
    Result.Elements[0][3] = 0.0f;

    Result.Elements[1][0] = 2.0f * (XY - WZ);
    Result.Elements[1][1] = 1.0f - 2.0f * (XX + ZZ);
    Result.Elements[1][2] = 2.0f * (YZ + WX);
    Result.Elements[1][3] = 0.0f;

    Result.Elements[2][0] = 2.0f * (XZ + WY);
    Result.Elements[2][1] = 2.0f * (YZ - WX);
    Result.Elements[2][2] = 1.0f - 2.0f * (XX + YY);
    Result.Elements[2][3] = 0.0f;

    Result.Elements[3][0] = 0.0f;
    Result.Elements[3][1] = 0.0f;
    Result.Elements[3][2] = 0.0f;
    Result.Elements[3][3] = 1.0f;

    return Result;
}

// This method taken from Mike Day at Insomniac Games.
// https://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2015/01/matrix-to-quat.pdf
//
// Note that as mentioned at the top of the paper, the paper assumes the matrix
// would be *post*-multiplied to a vector to rotate it, meaning the matrix is
// the transpose of what we're dealing with. But, because our matrices are
// stored in column-major order, the indices *appear* to match the paper.
//
// For example, m12 in the paper is row 1, column 2. We need to transpose it to
// row 2, column 1. But, because the column comes first when referencing
// elements, it looks like M.Elements[1][2].
//
// Don't be confused! Or if you must be confused, at least trust this
// comment. :)
COVERAGE(QM_M4ToQ_RH, 4)
static inline QM_Quat QM_M4ToQ_RH(QM_Mat4 M)
{
    float T;
    QM_Quat Q;

    if (M.Elements[2][2] < 0.0f) {
        if (M.Elements[0][0] > M.Elements[1][1]) {
            ASSERT_COVERED(QM_M4ToQ_RH);

            T = 1 + M.Elements[0][0] - M.Elements[1][1] - M.Elements[2][2];
            Q = QM_Q(
                T,
                M.Elements[0][1] + M.Elements[1][0],
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] - M.Elements[2][1]
            );
        } else {
            ASSERT_COVERED(QM_M4ToQ_RH);

            T = 1 - M.Elements[0][0] + M.Elements[1][1] - M.Elements[2][2];
            Q = QM_Q(
                M.Elements[0][1] + M.Elements[1][0],
                T,
                M.Elements[1][2] + M.Elements[2][1],
                M.Elements[2][0] - M.Elements[0][2]
            );
        }
    } else {
        if (M.Elements[0][0] < -M.Elements[1][1]) {
            ASSERT_COVERED(QM_M4ToQ_RH);

            T = 1 - M.Elements[0][0] - M.Elements[1][1] + M.Elements[2][2];
            Q = QM_Q(
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] + M.Elements[2][1],
                T,
                M.Elements[0][1] - M.Elements[1][0]
            );
        } else {
            ASSERT_COVERED(QM_M4ToQ_RH);

            T = 1 + M.Elements[0][0] + M.Elements[1][1] + M.Elements[2][2];
            Q = QM_Q(
                M.Elements[1][2] - M.Elements[2][1],
                M.Elements[2][0] - M.Elements[0][2],
                M.Elements[0][1] - M.Elements[1][0],
                T
            );
        }
    }

    Q = QM_MulQF(Q, 0.5f / QM_SqrtF(T));

    return Q;
}

COVERAGE(QM_M4ToQ_LH, 4)
static inline QM_Quat QM_M4ToQ_LH(QM_Mat4 M)
{
    float T;
    QM_Quat Q;

    if (M.Elements[2][2] < 0.0f) {
        if (M.Elements[0][0] > M.Elements[1][1]) {
            ASSERT_COVERED(QM_M4ToQ_LH);

            T = 1 + M.Elements[0][0] - M.Elements[1][1] - M.Elements[2][2];
            Q = QM_Q(
                T,
                M.Elements[0][1] + M.Elements[1][0],
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[2][1] - M.Elements[1][2]
            );
        } else {
            ASSERT_COVERED(QM_M4ToQ_LH);

            T = 1 - M.Elements[0][0] + M.Elements[1][1] - M.Elements[2][2];
            Q = QM_Q(
                M.Elements[0][1] + M.Elements[1][0],
                T,
                M.Elements[1][2] + M.Elements[2][1],
                M.Elements[0][2] - M.Elements[2][0]
            );
        }
    } else {
        if (M.Elements[0][0] < -M.Elements[1][1]) {
            ASSERT_COVERED(QM_M4ToQ_LH);

            T = 1 - M.Elements[0][0] - M.Elements[1][1] + M.Elements[2][2];
            Q = QM_Q(
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] + M.Elements[2][1],
                T,
                M.Elements[1][0] - M.Elements[0][1]
            );
        } else {
            ASSERT_COVERED(QM_M4ToQ_LH);

            T = 1 + M.Elements[0][0] + M.Elements[1][1] + M.Elements[2][2];
            Q = QM_Q(
                M.Elements[2][1] - M.Elements[1][2],
                M.Elements[0][2] - M.Elements[2][0],
                M.Elements[1][0] - M.Elements[0][2],
                T
            );
        }
    }

    Q = QM_MulQF(Q, 0.5f / QM_SqrtF(T));

    return Q;
}


COVERAGE(QM_QFromAxisAngle_RH, 1)
static inline QM_Quat QM_QFromAxisAngle_RH(QM_Vec3 Axis, float Angle)
{
    ASSERT_COVERED(QM_QFromAxisAngle_RH);

    QM_Quat Result;

    QM_Vec3 AxisNormalized = QM_NormV3(Axis);
    float SineOfRotation = QM_SinF(Angle / 2.0f);

    Result.XYZ = QM_MulV3F(AxisNormalized, SineOfRotation);
    Result.W = QM_CosF(Angle / 2.0f);

    return Result;
}

COVERAGE(QM_QFromAxisAngle_LH, 1)
static inline QM_Quat QM_QFromAxisAngle_LH(QM_Vec3 Axis, float Angle)
{
    ASSERT_COVERED(QM_QFromAxisAngle_LH);

    return QM_QFromAxisAngle_RH(Axis, -Angle);
}

COVERAGE(QM_RotateV2, 1)
static inline QM_Vec2 QM_RotateV2(QM_Vec2 V, float Angle)
{
    ASSERT_COVERED(QM_RotateV2)

    float sinA = QM_SinF(Angle);
    float cosA = QM_CosF(Angle);

    return QM_V2(V.X * cosA - V.Y * sinA, V.X * sinA + V.Y * cosA);
}

// implementation from
// https://blog.molecular-matters.com/2013/05/24/a-faster-quaternion-vector-multiplication/
COVERAGE(QM_RotateV3Q, 1)
static inline QM_Vec3 QM_RotateV3Q(QM_Vec3 V, QM_Quat Q)
{
    ASSERT_COVERED(QM_RotateV3Q);

    QM_Vec3 t = QM_MulV3F(QM_Cross(Q.XYZ, V), 2);
    return QM_AddV3(V, QM_AddV3(QM_MulV3F(t, Q.W), QM_Cross(Q.XYZ, t)));
}

COVERAGE(QM_RotateV3AxisAngle_LH, 1)
static inline QM_Vec3 QM_RotateV3AxisAngle_LH(QM_Vec3 V, QM_Vec3 Axis, float Angle) {
    ASSERT_COVERED(QM_RotateV3AxisAngle_LH);

    return QM_RotateV3Q(V, QM_QFromAxisAngle_LH(Axis, Angle));
}

COVERAGE(QM_RotateV3AxisAngle_RH, 1)
static inline QM_Vec3 QM_RotateV3AxisAngle_RH(QM_Vec3 V, QM_Vec3 Axis, float Angle) {
    ASSERT_COVERED(QM_RotateV3AxisAngle_RH);

    return QM_RotateV3Q(V, QM_QFromAxisAngle_RH(Axis, Angle));
}


#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

COVERAGE(QM_LenV2CPP, 1)
static inline float QM_Len(QM_Vec2 A)
{
    ASSERT_COVERED(QM_LenV2CPP);
    return QM_LenV2(A);
}

COVERAGE(QM_LenV3CPP, 1)
static inline float QM_Len(QM_Vec3 A)
{
    ASSERT_COVERED(QM_LenV3CPP);
    return QM_LenV3(A);
}

COVERAGE(QM_LenV4CPP, 1)
static inline float QM_Len(QM_Vec4 A)
{
    ASSERT_COVERED(QM_LenV4CPP);
    return QM_LenV4(A);
}

COVERAGE(QM_LenSqrV2CPP, 1)
static inline float QM_LenSqr(QM_Vec2 A)
{
    ASSERT_COVERED(QM_LenSqrV2CPP);
    return QM_LenSqrV2(A);
}

COVERAGE(QM_LenSqrV3CPP, 1)
static inline float QM_LenSqr(QM_Vec3 A)
{
    ASSERT_COVERED(QM_LenSqrV3CPP);
    return QM_LenSqrV3(A);
}

COVERAGE(QM_LenSqrV4CPP, 1)
static inline float QM_LenSqr(QM_Vec4 A)
{
    ASSERT_COVERED(QM_LenSqrV4CPP);
    return QM_LenSqrV4(A);
}

COVERAGE(QM_NormV2CPP, 1)
static inline QM_Vec2 QM_Norm(QM_Vec2 A)
{
    ASSERT_COVERED(QM_NormV2CPP);
    return QM_NormV2(A);
}

COVERAGE(QM_NormV3CPP, 1)
static inline QM_Vec3 QM_Norm(QM_Vec3 A)
{
    ASSERT_COVERED(QM_NormV3CPP);
    return QM_NormV3(A);
}

COVERAGE(QM_NormV4CPP, 1)
static inline QM_Vec4 QM_Norm(QM_Vec4 A)
{
    ASSERT_COVERED(QM_NormV4CPP);
    return QM_NormV4(A);
}

COVERAGE(QM_NormQCPP, 1)
static inline QM_Quat QM_Norm(QM_Quat A)
{
    ASSERT_COVERED(QM_NormQCPP);
    return QM_NormQ(A);
}

COVERAGE(QM_DotV2CPP, 1)
static inline float QM_Dot(QM_Vec2 Left, QM_Vec2 VecTwo)
{
    ASSERT_COVERED(QM_DotV2CPP);
    return QM_DotV2(Left, VecTwo);
}

COVERAGE(QM_DotV3CPP, 1)
static inline float QM_Dot(QM_Vec3 Left, QM_Vec3 VecTwo)
{
    ASSERT_COVERED(QM_DotV3CPP);
    return QM_DotV3(Left, VecTwo);
}

COVERAGE(QM_DotV4CPP, 1)
static inline float QM_Dot(QM_Vec4 Left, QM_Vec4 VecTwo)
{
    ASSERT_COVERED(QM_DotV4CPP);
    return QM_DotV4(Left, VecTwo);
}

COVERAGE(QM_LerpV2CPP, 1)
static inline QM_Vec2 QM_Lerp(QM_Vec2 Left, float Time, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_LerpV2CPP);
    return QM_LerpV2(Left, Time, Right);
}

COVERAGE(QM_LerpV3CPP, 1)
static inline QM_Vec3 QM_Lerp(QM_Vec3 Left, float Time, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_LerpV3CPP);
    return QM_LerpV3(Left, Time, Right);
}

COVERAGE(QM_LerpV4CPP, 1)
static inline QM_Vec4 QM_Lerp(QM_Vec4 Left, float Time, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_LerpV4CPP);
    return QM_LerpV4(Left, Time, Right);
}

COVERAGE(QM_TransposeM2CPP, 1)
static inline QM_Mat2 QM_Transpose(QM_Mat2 Matrix)
{
    ASSERT_COVERED(QM_TransposeM2CPP);
    return QM_TransposeM2(Matrix);
}

COVERAGE(QM_TransposeM3CPP, 1)
static inline QM_Mat3 QM_Transpose(QM_Mat3 Matrix)
{
    ASSERT_COVERED(QM_TransposeM3CPP);
    return QM_TransposeM3(Matrix);
}

COVERAGE(QM_TransposeM4CPP, 1)
static inline QM_Mat4 QM_Transpose(QM_Mat4 Matrix)
{
    ASSERT_COVERED(QM_TransposeM4CPP);
    return QM_TransposeM4(Matrix);
}

COVERAGE(QM_DeterminantM2CPP, 1)
static inline float QM_Determinant(QM_Mat2 Matrix)
{
    ASSERT_COVERED(QM_DeterminantM2CPP);
    return QM_DeterminantM2(Matrix);
}

COVERAGE(QM_DeterminantM3CPP, 1)
static inline float QM_Determinant(QM_Mat3 Matrix)
{
    ASSERT_COVERED(QM_DeterminantM3CPP);
    return QM_DeterminantM3(Matrix);
}

COVERAGE(QM_DeterminantM4CPP, 1)
static inline float QM_Determinant(QM_Mat4 Matrix)
{
    ASSERT_COVERED(QM_DeterminantM4CPP);
    return QM_DeterminantM4(Matrix);
}

COVERAGE(QM_InvGeneralM2CPP, 1)
static inline QM_Mat2 QM_InvGeneral(QM_Mat2 Matrix)
{
    ASSERT_COVERED(QM_InvGeneralM2CPP);
    return QM_InvGeneralM2(Matrix);
}

COVERAGE(QM_InvGeneralM3CPP, 1)
static inline QM_Mat3 QM_InvGeneral(QM_Mat3 Matrix)
{
    ASSERT_COVERED(QM_InvGeneralM3CPP);
    return QM_InvGeneralM3(Matrix);
}

COVERAGE(QM_InvGeneralM4CPP, 1)
static inline QM_Mat4 QM_InvGeneral(QM_Mat4 Matrix)
{
    ASSERT_COVERED(QM_InvGeneralM4CPP);
    return QM_InvGeneralM4(Matrix);
}

COVERAGE(QM_DotQCPP, 1)
static inline float QM_Dot(QM_Quat QuatOne, QM_Quat QuatTwo)
{
    ASSERT_COVERED(QM_DotQCPP);
    return QM_DotQ(QuatOne, QuatTwo);
}

COVERAGE(QM_AddV2CPP, 1)
static inline QM_Vec2 QM_Add(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_AddV2CPP);
    return QM_AddV2(Left, Right);
}

COVERAGE(QM_AddV3CPP, 1)
static inline QM_Vec3 QM_Add(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_AddV3CPP);
    return QM_AddV3(Left, Right);
}

COVERAGE(QM_AddV4CPP, 1)
static inline QM_Vec4 QM_Add(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_AddV4CPP);
    return QM_AddV4(Left, Right);
}

COVERAGE(QM_AddM2CPP, 1)
static inline QM_Mat2 QM_Add(QM_Mat2 Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_AddM2CPP);
    return QM_AddM2(Left, Right);
}

COVERAGE(QM_AddM3CPP, 1)
static inline QM_Mat3 QM_Add(QM_Mat3 Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_AddM3CPP);
    return QM_AddM3(Left, Right);
}

COVERAGE(QM_AddM4CPP, 1)
static inline QM_Mat4 QM_Add(QM_Mat4 Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_AddM4CPP);
    return QM_AddM4(Left, Right);
}

COVERAGE(QM_AddQCPP, 1)
static inline QM_Quat QM_Add(QM_Quat Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_AddQCPP);
    return QM_AddQ(Left, Right);
}

COVERAGE(QM_SubV2CPP, 1)
static inline QM_Vec2 QM_Sub(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_SubV2CPP);
    return QM_SubV2(Left, Right);
}

COVERAGE(QM_SubV3CPP, 1)
static inline QM_Vec3 QM_Sub(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_SubV3CPP);
    return QM_SubV3(Left, Right);
}

COVERAGE(QM_SubV4CPP, 1)
static inline QM_Vec4 QM_Sub(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_SubV4CPP);
    return QM_SubV4(Left, Right);
}

COVERAGE(QM_SubM2CPP, 1)
static inline QM_Mat2 QM_Sub(QM_Mat2 Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_SubM2CPP);
    return QM_SubM2(Left, Right);
}

COVERAGE(QM_SubM3CPP, 1)
static inline QM_Mat3 QM_Sub(QM_Mat3 Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_SubM3CPP);
    return QM_SubM3(Left, Right);
}

COVERAGE(QM_SubM4CPP, 1)
static inline QM_Mat4 QM_Sub(QM_Mat4 Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_SubM4CPP);
    return QM_SubM4(Left, Right);
}

COVERAGE(QM_SubQCPP, 1)
static inline QM_Quat QM_Sub(QM_Quat Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_SubQCPP);
    return QM_SubQ(Left, Right);
}

COVERAGE(QM_MulV2CPP, 1)
static inline QM_Vec2 QM_Mul(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_MulV2CPP);
    return QM_MulV2(Left, Right);
}

COVERAGE(QM_MulV2FCPP, 1)
static inline QM_Vec2 QM_Mul(QM_Vec2 Left, float Right)
{
    ASSERT_COVERED(QM_MulV2FCPP);
    return QM_MulV2F(Left, Right);
}

COVERAGE(QM_MulV3CPP, 1)
static inline QM_Vec3 QM_Mul(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_MulV3CPP);
    return QM_MulV3(Left, Right);
}

COVERAGE(QM_MulV3FCPP, 1)
static inline QM_Vec3 QM_Mul(QM_Vec3 Left, float Right)
{
    ASSERT_COVERED(QM_MulV3FCPP);
    return QM_MulV3F(Left, Right);
}

COVERAGE(QM_MulV4CPP, 1)
static inline QM_Vec4 QM_Mul(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_MulV4CPP);
    return QM_MulV4(Left, Right);
}

COVERAGE(QM_MulV4FCPP, 1)
static inline QM_Vec4 QM_Mul(QM_Vec4 Left, float Right)
{
    ASSERT_COVERED(QM_MulV4FCPP);
    return QM_MulV4F(Left, Right);
}

COVERAGE(QM_MulM2CPP, 1)
static inline QM_Mat2 QM_Mul(QM_Mat2 Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_MulM2CPP);
    return QM_MulM2(Left, Right);
}

COVERAGE(QM_MulM3CPP, 1)
static inline QM_Mat3 QM_Mul(QM_Mat3 Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_MulM3CPP);
    return QM_MulM3(Left, Right);
}

COVERAGE(QM_MulM4CPP, 1)
static inline QM_Mat4 QM_Mul(QM_Mat4 Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_MulM4CPP);
    return QM_MulM4(Left, Right);
}

COVERAGE(QM_MulM2FCPP, 1)
static inline QM_Mat2 QM_Mul(QM_Mat2 Left, float Right)
{
    ASSERT_COVERED(QM_MulM2FCPP);
    return QM_MulM2F(Left, Right);
}

COVERAGE(QM_MulM3FCPP, 1)
static inline QM_Mat3 QM_Mul(QM_Mat3 Left, float Right)
{
    ASSERT_COVERED(QM_MulM3FCPP);
    return QM_MulM3F(Left, Right);
}

COVERAGE(QM_MulM4FCPP, 1)
static inline QM_Mat4 QM_Mul(QM_Mat4 Left, float Right)
{
    ASSERT_COVERED(QM_MulM4FCPP);
    return QM_MulM4F(Left, Right);
}

COVERAGE(QM_MulM2V2CPP, 1)
static inline QM_Vec2 QM_Mul(QM_Mat2 Matrix, QM_Vec2 Vector)
{
    ASSERT_COVERED(QM_MulM2V2CPP);
    return QM_MulM2V2(Matrix, Vector);
}

COVERAGE(QM_MulM3V3CPP, 1)
static inline QM_Vec3 QM_Mul(QM_Mat3 Matrix, QM_Vec3 Vector)
{
    ASSERT_COVERED(QM_MulM3V3CPP);
    return QM_MulM3V3(Matrix, Vector);
}

COVERAGE(QM_MulM4V4CPP, 1)
static inline QM_Vec4 QM_Mul(QM_Mat4 Matrix, QM_Vec4 Vector)
{
    ASSERT_COVERED(QM_MulM4V4CPP);
    return QM_MulM4V4(Matrix, Vector);
}

COVERAGE(QM_MulQCPP, 1)
static inline QM_Quat QM_Mul(QM_Quat Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_MulQCPP);
    return QM_MulQ(Left, Right);
}

COVERAGE(QM_MulQFCPP, 1)
static inline QM_Quat QM_Mul(QM_Quat Left, float Right)
{
    ASSERT_COVERED(QM_MulQFCPP);
    return QM_MulQF(Left, Right);
}

COVERAGE(QM_DivV2CPP, 1)
static inline QM_Vec2 QM_Div(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_DivV2CPP);
    return QM_DivV2(Left, Right);
}

COVERAGE(QM_DivV2FCPP, 1)
static inline QM_Vec2 QM_Div(QM_Vec2 Left, float Right)
{
    ASSERT_COVERED(QM_DivV2FCPP);
    return QM_DivV2F(Left, Right);
}

COVERAGE(QM_DivV3CPP, 1)
static inline QM_Vec3 QM_Div(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_DivV3CPP);
    return QM_DivV3(Left, Right);
}

COVERAGE(QM_DivV3FCPP, 1)
static inline QM_Vec3 QM_Div(QM_Vec3 Left, float Right)
{
    ASSERT_COVERED(QM_DivV3FCPP);
    return QM_DivV3F(Left, Right);
}

COVERAGE(QM_DivV4CPP, 1)
static inline QM_Vec4 QM_Div(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_DivV4CPP);
    return QM_DivV4(Left, Right);
}

COVERAGE(QM_DivV4FCPP, 1)
static inline QM_Vec4 QM_Div(QM_Vec4 Left, float Right)
{
    ASSERT_COVERED(QM_DivV4FCPP);
    return QM_DivV4F(Left, Right);
}

COVERAGE(QM_DivM2FCPP, 1)
static inline QM_Mat2 QM_Div(QM_Mat2 Left, float Right)
{
    ASSERT_COVERED(QM_DivM2FCPP);
    return QM_DivM2F(Left, Right);
}

COVERAGE(QM_DivM3FCPP, 1)
static inline QM_Mat3 QM_Div(QM_Mat3 Left, float Right)
{
    ASSERT_COVERED(QM_DivM3FCPP);
    return QM_DivM3F(Left, Right);
}

COVERAGE(QM_DivM4FCPP, 1)
static inline QM_Mat4 QM_Div(QM_Mat4 Left, float Right)
{
    ASSERT_COVERED(QM_DivM4FCPP);
    return QM_DivM4F(Left, Right);
}

COVERAGE(QM_DivQFCPP, 1)
static inline QM_Quat QM_Div(QM_Quat Left, float Right)
{
    ASSERT_COVERED(QM_DivQFCPP);
    return QM_DivQF(Left, Right);
}

COVERAGE(QM_EqV2CPP, 1)
static inline QM_Bool QM_Eq(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_EqV2CPP);
    return QM_EqV2(Left, Right);
}

COVERAGE(QM_EqV3CPP, 1)
static inline QM_Bool QM_Eq(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_EqV3CPP);
    return QM_EqV3(Left, Right);
}

COVERAGE(QM_EqV4CPP, 1)
static inline QM_Bool QM_Eq(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_EqV4CPP);
    return QM_EqV4(Left, Right);
}

COVERAGE(QM_AddV2Op, 1)
static inline QM_Vec2 operator+(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_AddV2Op);
    return QM_AddV2(Left, Right);
}

COVERAGE(QM_AddV3Op, 1)
static inline QM_Vec3 operator+(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_AddV3Op);
    return QM_AddV3(Left, Right);
}

COVERAGE(QM_AddV4Op, 1)
static inline QM_Vec4 operator+(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_AddV4Op);
    return QM_AddV4(Left, Right);
}

COVERAGE(QM_AddM2Op, 1)
static inline QM_Mat2 operator+(QM_Mat2 Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_AddM2Op);
    return QM_AddM2(Left, Right);
}

COVERAGE(QM_AddM3Op, 1)
static inline QM_Mat3 operator+(QM_Mat3 Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_AddM3Op);
    return QM_AddM3(Left, Right);
}

COVERAGE(QM_AddM4Op, 1)
static inline QM_Mat4 operator+(QM_Mat4 Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_AddM4Op);
    return QM_AddM4(Left, Right);
}

COVERAGE(QM_AddQOp, 1)
static inline QM_Quat operator+(QM_Quat Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_AddQOp);
    return QM_AddQ(Left, Right);
}

COVERAGE(QM_SubV2Op, 1)
static inline QM_Vec2 operator-(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_SubV2Op);
    return QM_SubV2(Left, Right);
}

COVERAGE(QM_SubV3Op, 1)
static inline QM_Vec3 operator-(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_SubV3Op);
    return QM_SubV3(Left, Right);
}

COVERAGE(QM_SubV4Op, 1)
static inline QM_Vec4 operator-(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_SubV4Op);
    return QM_SubV4(Left, Right);
}

COVERAGE(QM_SubM2Op, 1)
static inline QM_Mat2 operator-(QM_Mat2 Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_SubM2Op);
    return QM_SubM2(Left, Right);
}

COVERAGE(QM_SubM3Op, 1)
static inline QM_Mat3 operator-(QM_Mat3 Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_SubM3Op);
    return QM_SubM3(Left, Right);
}

COVERAGE(QM_SubM4Op, 1)
static inline QM_Mat4 operator-(QM_Mat4 Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_SubM4Op);
    return QM_SubM4(Left, Right);
}

COVERAGE(QM_SubQOp, 1)
static inline QM_Quat operator-(QM_Quat Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_SubQOp);
    return QM_SubQ(Left, Right);
}

COVERAGE(QM_MulV2Op, 1)
static inline QM_Vec2 operator*(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_MulV2Op);
    return QM_MulV2(Left, Right);
}

COVERAGE(QM_MulV3Op, 1)
static inline QM_Vec3 operator*(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_MulV3Op);
    return QM_MulV3(Left, Right);
}

COVERAGE(QM_MulV4Op, 1)
static inline QM_Vec4 operator*(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_MulV4Op);
    return QM_MulV4(Left, Right);
}

COVERAGE(QM_MulM2Op, 1)
static inline QM_Mat2 operator*(QM_Mat2 Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_MulM2Op);
    return QM_MulM2(Left, Right);
}

COVERAGE(QM_MulM3Op, 1)
static inline QM_Mat3 operator*(QM_Mat3 Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_MulM3Op);
    return QM_MulM3(Left, Right);
}

COVERAGE(QM_MulM4Op, 1)
static inline QM_Mat4 operator*(QM_Mat4 Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_MulM4Op);
    return QM_MulM4(Left, Right);
}

COVERAGE(QM_MulQOp, 1)
static inline QM_Quat operator*(QM_Quat Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_MulQOp);
    return QM_MulQ(Left, Right);
}

COVERAGE(QM_MulV2FOp, 1)
static inline QM_Vec2 operator*(QM_Vec2 Left, float Right)
{
    ASSERT_COVERED(QM_MulV2FOp);
    return QM_MulV2F(Left, Right);
}

COVERAGE(QM_MulV3FOp, 1)
static inline QM_Vec3 operator*(QM_Vec3 Left, float Right)
{
    ASSERT_COVERED(QM_MulV3FOp);
    return QM_MulV3F(Left, Right);
}

COVERAGE(QM_MulV4FOp, 1)
static inline QM_Vec4 operator*(QM_Vec4 Left, float Right)
{
    ASSERT_COVERED(QM_MulV4FOp);
    return QM_MulV4F(Left, Right);
}

COVERAGE(QM_MulM2FOp, 1)
static inline QM_Mat2 operator*(QM_Mat2 Left, float Right)
{
    ASSERT_COVERED(QM_MulM2FOp);
    return QM_MulM2F(Left, Right);
}

COVERAGE(QM_MulM3FOp, 1)
static inline QM_Mat3 operator*(QM_Mat3 Left, float Right)
{
    ASSERT_COVERED(QM_MulM3FOp);
    return QM_MulM3F(Left, Right);
}

COVERAGE(QM_MulM4FOp, 1)
static inline QM_Mat4 operator*(QM_Mat4 Left, float Right)
{
    ASSERT_COVERED(QM_MulM4FOp);
    return QM_MulM4F(Left, Right);
}

COVERAGE(QM_MulQFOp, 1)
static inline QM_Quat operator*(QM_Quat Left, float Right)
{
    ASSERT_COVERED(QM_MulQFOp);
    return QM_MulQF(Left, Right);
}

COVERAGE(QM_MulV2FOpLeft, 1)
static inline QM_Vec2 operator*(float Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_MulV2FOpLeft);
    return QM_MulV2F(Right, Left);
}

COVERAGE(QM_MulV3FOpLeft, 1)
static inline QM_Vec3 operator*(float Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_MulV3FOpLeft);
    return QM_MulV3F(Right, Left);
}

COVERAGE(QM_MulV4FOpLeft, 1)
static inline QM_Vec4 operator*(float Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_MulV4FOpLeft);
    return QM_MulV4F(Right, Left);
}

COVERAGE(QM_MulM2FOpLeft, 1)
static inline QM_Mat2 operator*(float Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_MulM2FOpLeft);
    return QM_MulM2F(Right, Left);
}

COVERAGE(QM_MulM3FOpLeft, 1)
static inline QM_Mat3 operator*(float Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_MulM3FOpLeft);
    return QM_MulM3F(Right, Left);
}

COVERAGE(QM_MulM4FOpLeft, 1)
static inline QM_Mat4 operator*(float Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_MulM4FOpLeft);
    return QM_MulM4F(Right, Left);
}

COVERAGE(QM_MulQFOpLeft, 1)
static inline QM_Quat operator*(float Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_MulQFOpLeft);
    return QM_MulQF(Right, Left);
}

COVERAGE(QM_MulM2V2Op, 1)
static inline QM_Vec2 operator*(QM_Mat2 Matrix, QM_Vec2 Vector)
{
    ASSERT_COVERED(QM_MulM2V2Op);
    return QM_MulM2V2(Matrix, Vector);
}

COVERAGE(QM_MulM3V3Op, 1)
static inline QM_Vec3 operator*(QM_Mat3 Matrix, QM_Vec3 Vector)
{
    ASSERT_COVERED(QM_MulM3V3Op);
    return QM_MulM3V3(Matrix, Vector);
}

COVERAGE(QM_MulM4V4Op, 1)
static inline QM_Vec4 operator*(QM_Mat4 Matrix, QM_Vec4 Vector)
{
    ASSERT_COVERED(QM_MulM4V4Op);
    return QM_MulM4V4(Matrix, Vector);
}

COVERAGE(QM_DivV2Op, 1)
static inline QM_Vec2 operator/(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_DivV2Op);
    return QM_DivV2(Left, Right);
}

COVERAGE(QM_DivV3Op, 1)
static inline QM_Vec3 operator/(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_DivV3Op);
    return QM_DivV3(Left, Right);
}

COVERAGE(QM_DivV4Op, 1)
static inline QM_Vec4 operator/(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_DivV4Op);
    return QM_DivV4(Left, Right);
}

COVERAGE(QM_DivV2FOp, 1)
static inline QM_Vec2 operator/(QM_Vec2 Left, float Right)
{
    ASSERT_COVERED(QM_DivV2FOp);
    return QM_DivV2F(Left, Right);
}

COVERAGE(QM_DivV3FOp, 1)
static inline QM_Vec3 operator/(QM_Vec3 Left, float Right)
{
    ASSERT_COVERED(QM_DivV3FOp);
    return QM_DivV3F(Left, Right);
}

COVERAGE(QM_DivV4FOp, 1)
static inline QM_Vec4 operator/(QM_Vec4 Left, float Right)
{
    ASSERT_COVERED(QM_DivV4FOp);
    return QM_DivV4F(Left, Right);
}

COVERAGE(QM_DivM4FOp, 1)
static inline QM_Mat4 operator/(QM_Mat4 Left, float Right)
{
    ASSERT_COVERED(QM_DivM4FOp);
    return QM_DivM4F(Left, Right);
}

COVERAGE(QM_DivM3FOp, 1)
static inline QM_Mat3 operator/(QM_Mat3 Left, float Right)
{
    ASSERT_COVERED(QM_DivM3FOp);
    return QM_DivM3F(Left, Right);
}

COVERAGE(QM_DivM2FOp, 1)
static inline QM_Mat2 operator/(QM_Mat2 Left, float Right)
{
    ASSERT_COVERED(QM_DivM2FOp);
    return QM_DivM2F(Left, Right);
}

COVERAGE(QM_DivQFOp, 1)
static inline QM_Quat operator/(QM_Quat Left, float Right)
{
    ASSERT_COVERED(QM_DivQFOp);
    return QM_DivQF(Left, Right);
}

COVERAGE(QM_AddV2Assign, 1)
static inline QM_Vec2 &operator+=(QM_Vec2 &Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_AddV2Assign);
    return Left = Left + Right;
}

COVERAGE(QM_AddV3Assign, 1)
static inline QM_Vec3 &operator+=(QM_Vec3 &Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_AddV3Assign);
    return Left = Left + Right;
}

COVERAGE(QM_AddV4Assign, 1)
static inline QM_Vec4 &operator+=(QM_Vec4 &Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_AddV4Assign);
    return Left = Left + Right;
}

COVERAGE(QM_AddM2Assign, 1)
static inline QM_Mat2 &operator+=(QM_Mat2 &Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_AddM2Assign);
    return Left = Left + Right;
}

COVERAGE(QM_AddM3Assign, 1)
static inline QM_Mat3 &operator+=(QM_Mat3 &Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_AddM3Assign);
    return Left = Left + Right;
}

COVERAGE(QM_AddM4Assign, 1)
static inline QM_Mat4 &operator+=(QM_Mat4 &Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_AddM4Assign);
    return Left = Left + Right;
}

COVERAGE(QM_AddQAssign, 1)
static inline QM_Quat &operator+=(QM_Quat &Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_AddQAssign);
    return Left = Left + Right;
}

COVERAGE(QM_SubV2Assign, 1)
static inline QM_Vec2 &operator-=(QM_Vec2 &Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_SubV2Assign);
    return Left = Left - Right;
}

COVERAGE(QM_SubV3Assign, 1)
static inline QM_Vec3 &operator-=(QM_Vec3 &Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_SubV3Assign);
    return Left = Left - Right;
}

COVERAGE(QM_SubV4Assign, 1)
static inline QM_Vec4 &operator-=(QM_Vec4 &Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_SubV4Assign);
    return Left = Left - Right;
}

COVERAGE(QM_SubM2Assign, 1)
static inline QM_Mat2 &operator-=(QM_Mat2 &Left, QM_Mat2 Right)
{
    ASSERT_COVERED(QM_SubM2Assign);
    return Left = Left - Right;
}

COVERAGE(QM_SubM3Assign, 1)
static inline QM_Mat3 &operator-=(QM_Mat3 &Left, QM_Mat3 Right)
{
    ASSERT_COVERED(QM_SubM3Assign);
    return Left = Left - Right;
}

COVERAGE(QM_SubM4Assign, 1)
static inline QM_Mat4 &operator-=(QM_Mat4 &Left, QM_Mat4 Right)
{
    ASSERT_COVERED(QM_SubM4Assign);
    return Left = Left - Right;
}

COVERAGE(QM_SubQAssign, 1)
static inline QM_Quat &operator-=(QM_Quat &Left, QM_Quat Right)
{
    ASSERT_COVERED(QM_SubQAssign);
    return Left = Left - Right;
}

COVERAGE(QM_MulV2Assign, 1)
static inline QM_Vec2 &operator*=(QM_Vec2 &Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_MulV2Assign);
    return Left = Left * Right;
}

COVERAGE(QM_MulV3Assign, 1)
static inline QM_Vec3 &operator*=(QM_Vec3 &Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_MulV3Assign);
    return Left = Left * Right;
}

COVERAGE(QM_MulV4Assign, 1)
static inline QM_Vec4 &operator*=(QM_Vec4 &Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_MulV4Assign);
    return Left = Left * Right;
}

COVERAGE(QM_MulV2FAssign, 1)
static inline QM_Vec2 &operator*=(QM_Vec2 &Left, float Right)
{
    ASSERT_COVERED(QM_MulV2FAssign);
    return Left = Left * Right;
}

COVERAGE(QM_MulV3FAssign, 1)
static inline QM_Vec3 &operator*=(QM_Vec3 &Left, float Right)
{
    ASSERT_COVERED(QM_MulV3FAssign);
    return Left = Left * Right;
}

COVERAGE(QM_MulV4FAssign, 1)
static inline QM_Vec4 &operator*=(QM_Vec4 &Left, float Right)
{
    ASSERT_COVERED(QM_MulV4FAssign);
    return Left = Left * Right;
}

COVERAGE(QM_MulM2FAssign, 1)
static inline QM_Mat2 &operator*=(QM_Mat2 &Left, float Right)
{
    ASSERT_COVERED(QM_MulM2FAssign);
    return Left = Left * Right;
}

COVERAGE(QM_MulM3FAssign, 1)
static inline QM_Mat3 &operator*=(QM_Mat3 &Left, float Right)
{
    ASSERT_COVERED(QM_MulM3FAssign);
    return Left = Left * Right;
}

COVERAGE(QM_MulM4FAssign, 1)
static inline QM_Mat4 &operator*=(QM_Mat4 &Left, float Right)
{
    ASSERT_COVERED(QM_MulM4FAssign);
    return Left = Left * Right;
}

COVERAGE(QM_MulQFAssign, 1)
static inline QM_Quat &operator*=(QM_Quat &Left, float Right)
{
    ASSERT_COVERED(QM_MulQFAssign);
    return Left = Left * Right;
}

COVERAGE(QM_DivV2Assign, 1)
static inline QM_Vec2 &operator/=(QM_Vec2 &Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_DivV2Assign);
    return Left = Left / Right;
}

COVERAGE(QM_DivV3Assign, 1)
static inline QM_Vec3 &operator/=(QM_Vec3 &Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_DivV3Assign);
    return Left = Left / Right;
}

COVERAGE(QM_DivV4Assign, 1)
static inline QM_Vec4 &operator/=(QM_Vec4 &Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_DivV4Assign);
    return Left = Left / Right;
}

COVERAGE(QM_DivV2FAssign, 1)
static inline QM_Vec2 &operator/=(QM_Vec2 &Left, float Right)
{
    ASSERT_COVERED(QM_DivV2FAssign);
    return Left = Left / Right;
}

COVERAGE(QM_DivV3FAssign, 1)
static inline QM_Vec3 &operator/=(QM_Vec3 &Left, float Right)
{
    ASSERT_COVERED(QM_DivV3FAssign);
    return Left = Left / Right;
}

COVERAGE(QM_DivV4FAssign, 1)
static inline QM_Vec4 &operator/=(QM_Vec4 &Left, float Right)
{
    ASSERT_COVERED(QM_DivV4FAssign);
    return Left = Left / Right;
}

COVERAGE(QM_DivM4FAssign, 1)
static inline QM_Mat4 &operator/=(QM_Mat4 &Left, float Right)
{
    ASSERT_COVERED(QM_DivM4FAssign);
    return Left = Left / Right;
}

COVERAGE(QM_DivQFAssign, 1)
static inline QM_Quat &operator/=(QM_Quat &Left, float Right)
{
    ASSERT_COVERED(QM_DivQFAssign);
    return Left = Left / Right;
}

COVERAGE(QM_EqV2Op, 1)
static inline QM_Bool operator==(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_EqV2Op);
    return QM_EqV2(Left, Right);
}

COVERAGE(QM_EqV3Op, 1)
static inline QM_Bool operator==(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_EqV3Op);
    return QM_EqV3(Left, Right);
}

COVERAGE(QM_EqV4Op, 1)
static inline QM_Bool operator==(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_EqV4Op);
    return QM_EqV4(Left, Right);
}

COVERAGE(QM_EqV2OpNot, 1)
static inline QM_Bool operator!=(QM_Vec2 Left, QM_Vec2 Right)
{
    ASSERT_COVERED(QM_EqV2OpNot);
    return !QM_EqV2(Left, Right);
}

COVERAGE(QM_EqV3OpNot, 1)
static inline QM_Bool operator!=(QM_Vec3 Left, QM_Vec3 Right)
{
    ASSERT_COVERED(QM_EqV3OpNot);
    return !QM_EqV3(Left, Right);
}

COVERAGE(QM_EqV4OpNot, 1)
static inline QM_Bool operator!=(QM_Vec4 Left, QM_Vec4 Right)
{
    ASSERT_COVERED(QM_EqV4OpNot);
    return !QM_EqV4(Left, Right);
}

COVERAGE(QM_UnaryMinusV2, 1)
static inline QM_Vec2 operator-(QM_Vec2 In)
{
    ASSERT_COVERED(QM_UnaryMinusV2);

    QM_Vec2 Result;
    Result.X = -In.X;
    Result.Y = -In.Y;

    return Result;
}

COVERAGE(QM_UnaryMinusV3, 1)
static inline QM_Vec3 operator-(QM_Vec3 In)
{
    ASSERT_COVERED(QM_UnaryMinusV3);

    QM_Vec3 Result;
    Result.X = -In.X;
    Result.Y = -In.Y;
    Result.Z = -In.Z;

    return Result;
}

COVERAGE(QM_UnaryMinusV4, 1)
static inline QM_Vec4 operator-(QM_Vec4 In)
{
    ASSERT_COVERED(QM_UnaryMinusV4);

    QM_Vec4 Result;
#if HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_xor_ps(In.SSE, _mm_set1_ps(-0.0f));
#elif defined(HANDMADE_MATH__USE_NEON)
    float32x4_t Zero = vdupq_n_f32(0.0f);
    Result.NEON = vsubq_f32(Zero, In.NEON);
#else
    Result.X = -In.X;
    Result.Y = -In.Y;
    Result.Z = -In.Z;
    Result.W = -In.W;
#endif

    return Result;
}

#endif /* __cplusplus*/

#ifdef HANDMADE_MATH__USE_C11_GENERICS
#define QM_Add(A, B) _Generic((A), \
        QM_Vec2: QM_AddV2, \
        QM_Vec3: QM_AddV3, \
        QM_Vec4: QM_AddV4, \
        QM_Mat2: QM_AddM2, \
        QM_Mat3: QM_AddM3, \
        QM_Mat4: QM_AddM4, \
        QM_Quat: QM_AddQ \
)(A, B)

#define QM_Sub(A, B) _Generic((A), \
        QM_Vec2: QM_SubV2, \
        QM_Vec3: QM_SubV3, \
        QM_Vec4: QM_SubV4, \
        QM_Mat2: QM_SubM2, \
        QM_Mat3: QM_SubM3, \
        QM_Mat4: QM_SubM4, \
        QM_Quat: QM_SubQ \
)(A, B)

#define QM_Mul(A, B) _Generic((B), \
     float: _Generic((A), \
        QM_Vec2: QM_MulV2F, \
        QM_Vec3: QM_MulV3F, \
        QM_Vec4: QM_MulV4F, \
        QM_Mat2: QM_MulM2F, \
        QM_Mat3: QM_MulM3F, \
        QM_Mat4: QM_MulM4F, \
        QM_Quat: QM_MulQF \
     ), \
     QM_Mat2: QM_MulM2, \
     QM_Mat3: QM_MulM3, \
     QM_Mat4: QM_MulM4, \
     QM_Quat: QM_MulQ, \
     default: _Generic((A), \
        QM_Vec2: QM_MulV2, \
        QM_Vec3: QM_MulV3, \
        QM_Vec4: QM_MulV4, \
        QM_Mat2: QM_MulM2V2, \
        QM_Mat3: QM_MulM3V3, \
        QM_Mat4: QM_MulM4V4 \
    ) \
)(A, B)

#define QM_Div(A, B) _Generic((B), \
     float: _Generic((A), \
        QM_Mat2: QM_DivM2F, \
        QM_Mat3: QM_DivM3F, \
        QM_Mat4: QM_DivM4F, \
        QM_Vec2: QM_DivV2F, \
        QM_Vec3: QM_DivV3F, \
        QM_Vec4: QM_DivV4F, \
        QM_Quat: QM_DivQF  \
     ), \
     QM_Mat2: QM_DivM2, \
     QM_Mat3: QM_DivM3, \
     QM_Mat4: QM_DivM4, \
     QM_Quat: QM_DivQ, \
     default: _Generic((A), \
        QM_Vec2: QM_DivV2, \
        QM_Vec3: QM_DivV3, \
        QM_Vec4: QM_DivV4  \
    ) \
)(A, B)

#define QM_Len(A) _Generic((A), \
        QM_Vec2: QM_LenV2, \
        QM_Vec3: QM_LenV3, \
        QM_Vec4: QM_LenV4  \
)(A)

#define QM_LenSqr(A) _Generic((A), \
        QM_Vec2: QM_LenSqrV2, \
        QM_Vec3: QM_LenSqrV3, \
        QM_Vec4: QM_LenSqrV4  \
)(A)

#define QM_Norm(A) _Generic((A), \
        QM_Vec2: QM_NormV2, \
        QM_Vec3: QM_NormV3, \
        QM_Vec4: QM_NormV4  \
)(A)

#define QM_Dot(A, B) _Generic((A), \
        QM_Vec2: QM_DotV2, \
        QM_Vec3: QM_DotV3, \
        QM_Vec4: QM_DotV4  \
)(A, B)

#define QM_Lerp(A, T, B) _Generic((A), \
        float: QM_Lerp, \
        QM_Vec2: QM_LerpV2, \
        QM_Vec3: QM_LerpV3, \
        QM_Vec4: QM_LerpV4 \
)(A, T, B)

#define QM_Eq(A, B) _Generic((A), \
        QM_Vec2: QM_EqV2, \
        QM_Vec3: QM_EqV3, \
        QM_Vec4: QM_EqV4  \
)(A, B)

#define QM_Transpose(M) _Generic((M), \
        QM_Mat2: QM_TransposeM2, \
        QM_Mat3: QM_TransposeM3, \
        QM_Mat4: QM_TransposeM4  \
)(M)

#define QM_Determinant(M) _Generic((M), \
        QM_Mat2: QM_DeterminantM2, \
        QM_Mat3: QM_DeterminantM3, \
        QM_Mat4: QM_DeterminantM4  \
)(M)

#define QM_InvGeneral(M) _Generic((M), \
        QM_Mat2: QM_InvGeneralM2, \
        QM_Mat3: QM_InvGeneralM3, \
        QM_Mat4: QM_InvGeneralM4  \
)(M)

#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif /* HANDMADE_MATH_H */



