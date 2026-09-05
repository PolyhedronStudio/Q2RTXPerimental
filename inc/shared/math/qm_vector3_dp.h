/**
*
*
*   Only to be included by qm_math_cpp.h: file contains Vector3DP function implementations.
*
*
**/
#pragma once

// Vector3DP type for high-precision operations
struct Vector3DP {
    double x;
    double y;
    double z;

    /**
    *   @brief  Vector3DP Constructors
    **/
    [[nodiscard]] constexpr inline Vector3DP() {
        this->x = 0.0;
        this->y = 0.0;
        this->z = 0.0;
    }
    
    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<double>|| std::is_floating_point_v<float> || std::is_integral_v<T>>>
    [[nodiscard]] constexpr inline Vector3DP( const T x, const T y, const T z ) {
        this->x = static_cast<double>( x );
        this->y = static_cast<double>( y );
        this->z = static_cast<double>( z );
    }
    
    [[nodiscard]] constexpr inline explicit Vector3DP( const Vector3 &v ) {
        this->x = static_cast<double>( v.x );
        this->y = static_cast<double>( v.y );
        this->z = static_cast<double>( v.z );
    }

    [[nodiscard]] constexpr inline Vector3DP( const Vector3DP &v ) {
        this->x = v.x;
        this->y = v.y;
        this->z = v.z;
    }

    [[nodiscard]] constexpr inline explicit operator Vector3() const {
        return { static_cast<float>( this->x ), static_cast<float>( this->y ), static_cast<float>( this->z ) };
    }

    [[nodiscard]] constexpr inline explicit operator Vector2() const {
        return { static_cast<float>( this->x ), static_cast<float>( this->y ) };
    }

    [[nodiscard]] constexpr inline const double &operator[]( const size_t i ) const {
        return ( &x )[ i ];
    }
    [[nodiscard]] constexpr inline double &operator[]( const size_t i ) {
        return ( &x )[ i ];
    }

    [[nodiscard]] constexpr inline Vector3DP operator-() const {
        return { -x, -y, -z };
    }

    /**
    *   @brief  Assign a single-precision Vector3 to this double-precision Vector3DP.
    *   @param  v   Source single-precision vector.
    *   @return Reference to this vector.
    **/
    constexpr inline Vector3DP &operator=( const Vector3 &v ) {
        this->x = static_cast<double>( v.x );
        this->y = static_cast<double>( v.y );
        this->z = static_cast<double>( v.z );
        return *this;
    }

    /**
    *   @brief  Add another Vector3DP into this vector.
    *   @param  right   Right-hand vector to add.
    *   @return Reference to this vector.
    **/
    constexpr inline Vector3DP &operator+=( const Vector3DP &right ) {
        this->x += right.x;
        this->y += right.y;
        this->z += right.z;
        return *this;
    }

    /**
    *   @brief  Subtract another Vector3DP from this vector.
    *   @param  right   Right-hand vector to subtract.
    *   @return Reference to this vector.
    **/
    constexpr inline Vector3DP &operator-=( const Vector3DP &right ) {
        this->x -= right.x;
        this->y -= right.y;
        this->z -= right.z;
        return *this;
    }

    /**
    *   @brief  Scale this vector by a double scalar.
    *   @param  scalar  Double-precision scale factor.
    *   @return Reference to this vector.
    **/
    constexpr inline Vector3DP &operator*=( const double scalar ) {
        this->x *= scalar;
        this->y *= scalar;
        this->z *= scalar;
        return *this;
    }

    /**
    *   @brief  Divide this vector by a double scalar.
    *   @param  scalar  Double-precision divisor.
    *   @return Reference to this vector.
    **/
    constexpr inline Vector3DP &operator/=( const double scalar ) {
        const double inv = 1.0 / scalar;
        this->x *= inv;
        this->y *= inv;
        this->z *= inv;
        return *this;
    }

    /**
    *   @brief  Scale this vector by a float scalar.
    *   @param  scalar  Single-precision scale factor.
    *   @return Reference to this vector.
    **/
    constexpr inline Vector3DP &operator*=( const float scalar ) {
        const double s = static_cast<double>( scalar );
        this->x *= s;
        this->y *= s;
        this->z *= s;
        return *this;
    }

    /**
    *   @brief  Divide this vector by a float scalar.
    *   @param  scalar  Single-precision divisor.
    *   @return Reference to this vector.
    **/
    constexpr inline Vector3DP &operator/=( const float scalar ) {
        const double inv = 1.0 / static_cast<double>( scalar );
        this->x *= inv;
        this->y *= inv;
        this->z *= inv;
        return *this;
    }
};

// Arithmetic operators
[[nodiscard]] constexpr inline Vector3DP operator+( const Vector3DP &v1, const Vector3DP &v2 ) {
    return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
}
[[nodiscard]] constexpr inline Vector3DP operator-( const Vector3DP &v1, const Vector3DP &v2 ) {
    return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
}
[[nodiscard]] constexpr inline Vector3DP operator*( const Vector3DP &v, const double s ) {
    return { v.x * s, v.y * s, v.z * s };
}
[[nodiscard]] constexpr inline Vector3DP operator*( const double s, const Vector3DP &v ) {
    return { v.x * s, v.y * s, v.z * s };
}
[[nodiscard]] constexpr inline Vector3DP operator/( const Vector3DP &v, const double s ) {
    return { v.x / s, v.y / s, v.z / s };
}


[[nodiscard]] constexpr inline Vector3DP operator*( const Vector3DP &v, const float s ) {
    return { v.x * s, v.y * s, v.z * s };
}
[[nodiscard]] constexpr inline Vector3DP operator*( const float s, const Vector3DP &v ) {
    return { v.x * s, v.y * s, v.z * s };
}
[[nodiscard]] constexpr inline Vector3DP operator/( const Vector3DP &v, const float s ) {
    return { v.x / s, v.y / s, v.z / s };
}


// Convert Vector3DP to Vector3
QM_API_CONSTEXPR Vector3 QM_Vector3FromDP( const Vector3DP &v ) {
    return { static_cast<float>( v.x ), static_cast<float>( v.y ), static_cast<float>( v.z ) };
}

// Add two double-precision vectors
QM_API_CONSTEXPR Vector3DP QM_Vector3AddDP( const Vector3DP &v1, const Vector3DP &v2 ) {
    Vector3DP result = { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
    return result;
}

// Subtract two double-precision vectors
QM_API_CONSTEXPR Vector3DP QM_Vector3SubtractDP( const Vector3DP &v1, const Vector3DP &v2 ) {
    Vector3DP result = { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
    return result;
}

// Multiply double-precision vector by scalar
QM_API_CONSTEXPR Vector3DP QM_Vector3ScaleDP( const Vector3DP &v, const double scalar ) {
    Vector3DP result = { v.x * scalar, v.y * scalar, v.z * scalar };
    return result;
}

// Multiply double-precision vector by scalar and add to another double-precision vector
QM_API_CONSTEXPR Vector3DP QM_Vector3MultiplyAddDP( const Vector3DP &v1, const double t, const Vector3DP &v2 ) {
    Vector3DP result = { v1.x + t * v2.x, v1.y + t * v2.y, v1.z + t * v2.z };
    return result;
}

// Calculate two double-precision vectors dot product
QM_API_CONSTEXPR double QM_Vector3DotProductDP( const Vector3DP &v1, const Vector3DP &v2 ) {
    return ( v1.x * v2.x + v1.y * v2.y + v1.z * v2.z );
}

// Calculate two double-precision vectors cross product
QM_API_CONSTEXPR Vector3DP QM_Vector3CrossProductDP( const Vector3DP &v1, const Vector3DP &v2 ) {
    Vector3DP result = { 
        v1.y * v2.z - v1.z * v2.y, 
        v1.z * v2.x - v1.x * v2.z, 
        v1.x * v2.y - v1.y * v2.x 
    };
    return result;
}

// Calculate double-precision vector square length
QM_API_CONSTEXPR double QM_Vector3LengthSqrDP( const Vector3DP &v ) {
    return ( v.x * v.x + v.y * v.y + v.z * v.z );
}

// Calculate double-precision vector length
QM_API double QM_Vector3LengthDP( const Vector3DP &v ) {
    return std::sqrt( v.x * v.x + v.y * v.y + v.z * v.z );
}

// Normalize double-precision vector and return original length
QM_API double QM_Vector3NormalizeLengthDP( Vector3DP &v ) {
    const double length = std::sqrt( v.x * v.x + v.y * v.y + v.z * v.z );
    if ( length > 0.0 ) {
        const double ilength = 1.0 / length;
        v.x *= ilength;
        v.y *= ilength;
        v.z *= ilength;
    }
    return length;
}

// Calculate double-precision distance between two double-precision vectors
QM_API double QM_Vector3DistanceDP( const Vector3DP &v1, const Vector3DP &v2 ) {
    const double dx = v2.x - v1.x;
    const double dy = v2.y - v1.y;
    const double dz = v2.z - v1.z;
    return std::sqrt( dx * dx + dy * dy + dz * dz );
}

// Calculate double-precision 2D distance between two double-precision vectors in XY plane
QM_API double QM_Vector3Distance2DDP( const Vector3DP &v1, const Vector3DP &v2 ) {
    const double dx = v2.x - v1.x;
    const double dy = v2.y - v1.y;
    return std::sqrt( dx * dx + dy * dy );
}

// Calculate square distance between two double-precision vectors
QM_API_CONSTEXPR double QM_Vector3DistanceSqrDP( const Vector3DP &v1, const Vector3DP &v2 ) {
    const double dx = v2.x - v1.x;
    const double dy = v2.y - v1.y;
    const double dz = v2.z - v1.z;
    return ( dx * dx + dy * dy + dz * dz );
}

// Calculate square 2D distance between two double-precision vectors in XY plane
QM_API_CONSTEXPR double QM_Vector3Distance2DSqrDP( const Vector3DP &v1, const Vector3DP &v2 ) {
    const double dx = v2.x - v1.x;
    const double dy = v2.y - v1.y;
    return ( dx * dx + dy * dy );
}

// Normalize double-precision vector
inline Vector3DP QM_Vector3NormalizeDP( const Vector3DP &v ) {
    const double length = std::sqrt( v.x * v.x + v.y * v.y + v.z * v.z );
    if ( length > 0.0 ) {
        const double ilength = 1.0 / length;
        return { v.x * ilength, v.y * ilength, v.z * ilength };
    }
    return { 0.0, 0.0, 0.0 };
}

// Calculate yaw angle in degrees from a double-precision direction vector
inline double QM_Vector3ToYawDP( const Vector3DP &vec ) {
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
*	@brief	Vector3DP double-precision 'AngleVectors' method for Euler angle transformation.
*	@param	angles	[in] Euler angles in degrees (PITCH = 0, YAW = 1, ROLL = 2).
*	@param	forward	[out] Normalized forward directional vector in Vector3DP.
*	@param	right	[out] Normalized right directional vector in Vector3DP.
*	@param	up		[out] Normalized up directional vector in Vector3DP.
**/
inline void QM_AngleVectorsDP( const Vector3DP &angles, Vector3DP *forward, Vector3DP *right, Vector3DP *up ) {
	const double deg2rad = QM_PI / 180.0;
	const double angleYaw = angles.y * deg2rad;
	const double sy = std::sin( angleYaw );
	const double cy = std::cos( angleYaw );
	const double anglePitch = angles.x * deg2rad;
	const double sp = std::sin( anglePitch );
	const double cp = std::cos( anglePitch );
	const double angleRoll = angles.z * deg2rad;
	const double sr = std::sin( angleRoll );
	const double cr = std::cos( angleRoll );

	if ( forward ) {
		forward->x = cp * cy;
		forward->y = cp * sy;
		forward->z = -sp;
	}
	if ( right ) {
		right->x = ( -1.0 * sr * sp * cy + -1.0 * cr * -sy );
		right->y = ( -1.0 * sr * sp * sy + -1.0 * cr * cy );
		right->z = -1.0 * sr * cp;
	}
	if ( up ) {
		up->x = ( cr * sp * cy + -sr * -sy );
		up->y = ( cr * sp * sy + -sr * cy );
		up->z = cr * cp;
	}
}

/**
*	@brief	AngleMod the entire Vector3DP in double precision.
*	@param	v	Vector3DP of angles in degrees.
*	@return	Vector3DP with every component wrapped into [0, 360).
**/
inline Vector3DP QM_Vector3AngleModDP( const Vector3DP &v ) {
	return Vector3DP{
		QM_AngleMod( v.x ),
		QM_AngleMod( v.y ),
		QM_AngleMod( v.z )
	};
}

/**
*	@brief	Spherical/Euler linear interpolation between two Vector3DP Euler angles in degrees.
*	@param	angleVec2	Target Euler angle vector in degrees.
*	@param	angleVec1	Source Euler angle vector in degrees.
*	@param	fraction	Interpolation fraction [0.0..1.0].
*	@return	Interpolated Vector3DP angle vector in degrees.
**/
inline Vector3DP QM_Vector3LerpAnglesDP( const Vector3DP &angleVec2, const Vector3DP &angleVec1, const double fraction ) {
	return Vector3DP{
		QM_LerpAngle( angleVec2.x, angleVec1.x, fraction ),
		QM_LerpAngle( angleVec2.y, angleVec1.y, fraction ),
		QM_LerpAngle( angleVec2.z, angleVec1.z, fraction )
	};
}
