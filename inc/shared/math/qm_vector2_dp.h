/**
*   @brief  Double-precision 2D vector type and helpers.
*   @note   This header is intended to be included through qm_math_cpp.h.
**/
#pragma once

struct Vector3DP;

#include "shared/math/qm_vector3_dp.h"

/**
*   @brief  High-precision 2D vector type.
*   @note   The Z axis is intentionally omitted because this type is strictly 2D.
**/
struct Vector2DP {
	double x;
	double y;

	/**
	*   @brief  Construct a zero-initialized Vector2DP.
	*   @note   Both components are initialized to 0.0.
	**/
	[[nodiscard]] constexpr inline Vector2DP() : x( 0.0 ), y( 0.0 ) {};

	/**
	*   @brief  Construct a Vector2DP from double-precision components.
	*   @param  x   X component.
	*   @param  y   Y component.
	**/
	[[nodiscard]] constexpr inline Vector2DP( const double x, const double y ) : x( x ), y( y ) {};

	/**
	*   @brief  Construct a Vector2DP from integral or floating-point components.
	*   @param  x   X component.
	*   @param  y   Y component.
	*   @note   This overload preserves convenient construction from integer literals.
	**/
	template<typename T, typename = std::enable_if_t< std::is_floating_point_v<T> || std::is_integral_v<T> >>
	[[nodiscard]] constexpr inline Vector2DP( const T x, const T y ) : x( static_cast< double >( x ) ), y( static_cast< double >( y ) ) {};

	/**
	*   @brief  Construct a Vector2DP from a float Vector2.
	*   @param  v   Source vector.
	**/
	[[nodiscard]] constexpr inline explicit Vector2DP( const Vector2 &v ) : x( static_cast< double >( v.x ) ), y( static_cast< double >( v.y ) ) {};

	/**
	*   @brief  Construct a Vector2DP from a float Vector3.
	*   @param  v   Source vector.
	*   @note   The Z component is intentionally ignored.
	**/
	[[nodiscard]] constexpr inline explicit Vector2DP( const Vector3 &v ) : x( static_cast< double >( v.x ) ), y( static_cast< double >( v.y ) ) {};

	/**
	*   @brief  Construct a Vector2DP from a double-precision Vector3DP.
	*   @param  v   Source vector.
	*   @note   The Z component is intentionally ignored.
	**/
	[[nodiscard]] constexpr inline explicit Vector2DP( const Vector3DP &v ) : x( v.x ), y( v.y ) {};

	/**
	*   @brief  Copy construct a Vector2DP.
	*   @param  v   Source vector.
	**/
	[[nodiscard]] constexpr inline Vector2DP( const Vector2DP &v ) = default;

	/**
	*   @brief  Convert to a float Vector2.
	*   @return  A Vector2 containing the same X/Y components.
	**/
	[[nodiscard]] constexpr inline explicit operator Vector2() const {
		return { static_cast< float >( x ), static_cast< float >( y ) };
	}

	/**
	*   @brief  Access a component by index.
	*   @param  i   Component index where 0 is X and 1 is Y.
	*   @return  Reference to the requested component.
	**/
	[[nodiscard]] constexpr inline const double &operator[]( const size_t i ) const {
		return ( &x )[ i ];
	}

	/**
	*   @brief  Access a mutable component by index.
	*   @param  i   Component index where 0 is X and 1 is Y.
	*   @return  Mutable reference to the requested component.
	**/
	[[nodiscard]] constexpr inline double &operator[]( const size_t i ) {
		return ( &x )[ i ];
	}

	/**
	*   @brief  Negate both components.
	*   @return  Negated vector.
	**/
	[[nodiscard]] constexpr inline Vector2DP operator-() const {
		return { -x, -y };
	}
};

/**
*   @brief  Add two Vector2DP values.
*   @param  v1  Left-hand vector.
*   @param  v2  Right-hand vector.
*   @return  Component-wise sum.
**/
[[nodiscard]] constexpr inline Vector2DP operator+( const Vector2DP &v1, const Vector2DP &v2 ) {
	return { v1.x + v2.x, v1.y + v2.y };
}

/**
*   @brief  Subtract two Vector2DP values.
*   @param  v1  Left-hand vector.
*   @param  v2  Right-hand vector.
*   @return  Component-wise difference.
**/
[[nodiscard]] constexpr inline Vector2DP operator-( const Vector2DP &v1, const Vector2DP &v2 ) {
	return { v1.x - v2.x, v1.y - v2.y };
}

/**
*   @brief  Scale a Vector2DP by a double scalar.
*   @param  v  Source vector.
*   @param  s  Scale factor.
*   @return  Scaled vector.
**/
[[nodiscard]] constexpr inline Vector2DP operator*( const Vector2DP &v, const double s ) {
	return { v.x * s, v.y * s };
}

/**
*   @brief  Scale a Vector2DP by a double scalar.
*   @param  s  Scale factor.
*   @param  v  Source vector.
*   @return  Scaled vector.
**/
[[nodiscard]] constexpr inline Vector2DP operator*( const double s, const Vector2DP &v ) {
	return { v.x * s, v.y * s };
}

/**
*   @brief  Divide a Vector2DP by a double scalar.
*   @param  v  Source vector.
*   @param  s  Divisor.
*   @return  Divided vector.
**/
[[nodiscard]] constexpr inline Vector2DP operator/( const Vector2DP &v, const double s ) {
	return { v.x / s, v.y / s };
}

/**
*   @brief  Scale a Vector2DP by a float scalar.
*   @param  v  Source vector.
*   @param  s  Scale factor.
*   @return  Scaled vector.
**/
[[nodiscard]] constexpr inline Vector2DP operator*( const Vector2DP &v, const float s ) {
	return { v.x * s, v.y * s };
}

/**
*   @brief  Scale a Vector2DP by a float scalar.
*   @param  s  Scale factor.
*   @param  v  Source vector.
*   @return  Scaled vector.
**/
[[nodiscard]] constexpr inline Vector2DP operator*( const float s, const Vector2DP &v ) {
	return { v.x * s, v.y * s };
}

/**
*   @brief  Divide a Vector2DP by a float scalar.
*   @param  v  Source vector.
*   @param  s  Divisor.
*   @return  Divided vector.
**/
[[nodiscard]] constexpr inline Vector2DP operator/( const Vector2DP &v, const float s ) {
	return { v.x / s, v.y / s };
}

/**
*   @brief  Convert a Vector2DP to a Vector2.
*   @param  v  Source vector.
*   @return  Float-precision vector containing the same X/Y components.
**/
QM_API_CONSTEXPR Vector2 QM_Vector2FromDP( const Vector2DP &v ) {
	return { static_cast< float >( v.x ), static_cast< float >( v.y ) };
}

/**
*   @brief  Legacy compatibility wrapper for Vector2DP conversion.
*   @param  v  Source vector.
*   @return  Float-precision vector containing the same X/Y components.
**/
QM_API_CONSTEXPR Vector2 QM_Vector3FromDP( const Vector2DP &v ) {
	return QM_Vector2FromDP( v );
}

/**
*   @brief  Add two double-precision 2D vectors.
*   @param  v1  Left-hand vector.
*   @param  v2  Right-hand vector.
*   @return  Component-wise sum.
**/
QM_API_CONSTEXPR Vector2DP QM_Vector2AddDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	return { v1.x + v2.x, v1.y + v2.y };
}

/**
*   @brief  Legacy compatibility wrapper for 2D vector addition.
*   @param  v1  Left-hand vector.
*   @param  v2  Right-hand vector.
*   @return  Component-wise sum.
**/
QM_API_CONSTEXPR Vector2DP QM_Vector3AddDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	return QM_Vector2AddDP( v1, v2 );
}

/**
*   @brief  Subtract one double-precision 2D vector from another.
*   @param  v1  Left-hand vector.
*   @param  v2  Right-hand vector.
*   @return  Component-wise difference.
**/
QM_API_CONSTEXPR Vector2DP QM_Vector2SubtractDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	return { v1.x - v2.x, v1.y - v2.y };
}

/**
*   @brief  Legacy compatibility wrapper for 2D vector subtraction.
*   @param  v1  Left-hand vector.
*   @param  v2  Right-hand vector.
*   @return  Component-wise difference.
**/
QM_API_CONSTEXPR Vector2DP QM_Vector3SubtractDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	return QM_Vector2SubtractDP( v1, v2 );
}

/**
*   @brief  Scale a double-precision 2D vector by a scalar.
*   @param  v       Source vector.
*   @param  scalar  Scale factor.
*   @return  Scaled vector.
**/
QM_API_CONSTEXPR Vector2DP QM_Vector2ScaleDP( const Vector2DP &v, const double scalar ) {
	return { v.x * scalar, v.y * scalar };
}

/**
*   @brief  Legacy compatibility wrapper for scalar scaling.
*   @param  v       Source vector.
*   @param  scalar  Scale factor.
*   @return  Scaled vector.
**/
QM_API_CONSTEXPR Vector2DP QM_Vector3ScaleDP( const Vector2DP &v, const double scalar ) {
	return QM_Vector2ScaleDP( v, scalar );
}

/**
*   @brief  Multiply a vector by a scalar and add it to another vector.
*   @param  v1  Base vector.
*   @param  t   Scale factor for v2.
*   @param  v2  Vector to scale and add.
*   @return  Accumulated vector.
**/
QM_API_CONSTEXPR Vector2DP QM_Vector2MultiplyAddDP( const Vector2DP &v1, const double t, const Vector2DP &v2 ) {
	return { v1.x + t * v2.x, v1.y + t * v2.y };
}

/**
*   @brief  Legacy compatibility wrapper for multiply-add.
*   @param  v1  Base vector.
*   @param  t   Scale factor for v2.
*   @param  v2  Vector to scale and add.
*   @return  Accumulated vector.
**/
QM_API_CONSTEXPR Vector2DP QM_Vector3MultiplyAddDP( const Vector2DP &v1, const double t, const Vector2DP &v2 ) {
	return QM_Vector2MultiplyAddDP( v1, t, v2 );
}

/**
*   @brief  Calculate the dot product of two double-precision 2D vectors.
*   @param  v1  First vector.
*   @param  v2  Second vector.
*   @return  Dot product.
**/
QM_API_CONSTEXPR double QM_Vector2DotProductDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	return ( v1.x * v2.x + v1.y * v2.y );
}

/**
*   @brief  Legacy compatibility wrapper for dot product.
*   @param  v1  First vector.
*   @param  v2  Second vector.
*   @return  Dot product.
**/
QM_API_CONSTEXPR double QM_Vector3DotProductDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	return QM_Vector2DotProductDP( v1, v2 );
}

/**
*   @brief  Calculate the 2D cross-product analogue as a signed scalar area.
*   @param  v1  First vector.
*   @param  v2  Second vector.
*   @return  Signed area formed by the two vectors.
**/
QM_API_CONSTEXPR double QM_Vector2CrossProductDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	return ( v1.x * v2.y - v1.y * v2.x );
}

/**
*   @brief  Calculate the squared length of a double-precision 2D vector.
*   @param  v  Source vector.
*   @return  Squared length.
**/
QM_API_CONSTEXPR double QM_Vector2LengthSqrDP( const Vector2DP &v ) {
	return ( v.x * v.x + v.y * v.y );
}

/**
*   @brief  Legacy compatibility wrapper for squared length.
*   @param  v  Source vector.
*   @return  Squared length.
**/
QM_API_CONSTEXPR double QM_Vector3LengthSqrDP( const Vector2DP &v ) {
	return QM_Vector2LengthSqrDP( v );
}

/**
*   @brief  Calculate the length of a double-precision 2D vector.
*   @param  v  Source vector.
*   @return  Vector length.
**/
QM_API double QM_Vector2LengthDP( const Vector2DP &v ) {
	return std::sqrt( v.x * v.x + v.y * v.y );
}

/**
*   @brief  Legacy compatibility wrapper for vector length.
*   @param  v  Source vector.
*   @return  Vector length.
**/
QM_API double QM_Vector3LengthDP( const Vector2DP &v ) {
	return QM_Vector2LengthDP( v );
}

/**
*   @brief  Normalize a double-precision 2D vector in place and return its original length.
*   @param  v  Vector to normalize.
*   @return  Original vector length.
**/
QM_API double QM_Vector2NormalizeLengthDP( Vector2DP &v ) {
	const double length = std::sqrt( v.x * v.x + v.y * v.y );
	if ( length > 0.0 ) {
		const double ilength = 1.0 / length;
		v.x *= ilength;
		v.y *= ilength;
	}
	return length;
}

/**
*   @brief  Legacy compatibility wrapper for in-place normalization.
*   @param  v  Vector to normalize.
*   @return  Original vector length.
**/
QM_API double QM_Vector3NormalizeLengthDP( Vector2DP &v ) {
	return QM_Vector2NormalizeLengthDP( v );
}

/**
*   @brief  Calculate the distance between two double-precision 2D vectors.
*   @param  v1  First vector.
*   @param  v2  Second vector.
*   @return  Euclidean distance.
**/
QM_API double QM_Vector2DistanceDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	const double dx = v2.x - v1.x;
	const double dy = v2.y - v1.y;
	return std::sqrt( dx * dx + dy * dy );
}

/**
*   @brief  Legacy compatibility wrapper for vector distance.
*   @param  v1  First vector.
*   @param  v2  Second vector.
*   @return  Euclidean distance.
**/
QM_API double QM_Vector3DistanceDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	return QM_Vector2DistanceDP( v1, v2 );
}

/**
*   @brief  Calculate the squared distance between two double-precision 2D vectors.
*   @param  v1  First vector.
*   @param  v2  Second vector.
*   @return  Squared Euclidean distance.
**/
QM_API_CONSTEXPR double QM_Vector2DistanceSqrDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	const double dx = v2.x - v1.x;
	const double dy = v2.y - v1.y;
	return ( dx * dx + dy * dy );
}

/**
*   @brief  Legacy compatibility wrapper for squared vector distance.
*   @param  v1  First vector.
*   @param  v2  Second vector.
*   @return  Squared Euclidean distance.
**/
QM_API_CONSTEXPR double QM_Vector3DistanceSqrDP( const Vector2DP &v1, const Vector2DP &v2 ) {
	return QM_Vector2DistanceSqrDP( v1, v2 );
}

/**
*   @brief  Normalize a double-precision 2D vector.
*   @param  v  Source vector.
*   @return  Normalized vector, or zero when the input is zero-length.
**/
inline Vector2DP QM_Vector2NormalizeDP( const Vector2DP &v ) {
	const double length = std::sqrt( v.x * v.x + v.y * v.y );
	if ( length > 0.0 ) {
		const double ilength = 1.0 / length;
		return { v.x * ilength, v.y * ilength };
	}
	return { 0.0, 0.0 };
}

/**
*   @brief  Legacy compatibility wrapper for vector normalization.
*   @param  v  Source vector.
*   @return  Normalized vector, or zero when the input is zero-length.
**/
inline Vector2DP QM_Vector3NormalizeDP( const Vector2DP &v ) {
	return QM_Vector2NormalizeDP( v );
}

/**
*   @brief  Calculate the yaw angle in degrees from a double-precision 2D direction vector.
*   @param  vec  Direction vector.
*   @return  Yaw angle in degrees.
**/
inline double QM_Vector2ToYawDP( const Vector2DP &vec ) {
	double yaw;
	if ( vec.x == 0.0 ) {
		yaw = 0.0;
		if ( vec.y > 0.0 ) {
			yaw = 90.0;
		} else if ( vec.y < 0.0 ) {
			yaw = 270.0;
		}
	} else {
		yaw = ( std::atan2( vec.y, vec.x ) * 180.0 / QM_PI );
		if ( yaw < 0.0 ) {
			yaw += 360.0;
		}
	}
	return yaw;
}

/**
*   @brief  Legacy compatibility wrapper for yaw conversion.
*   @param  vec  Direction vector.
*   @return  Yaw angle in degrees.
**/
inline double QM_Vector3ToYawDP( const Vector2DP &vec ) {
	return QM_Vector2ToYawDP( vec );
}
