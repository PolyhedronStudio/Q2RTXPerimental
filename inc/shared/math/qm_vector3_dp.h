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
    
    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
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

// Calculate square distance between two double-precision vectors
QM_API_CONSTEXPR double QM_Vector3DistanceSqrDP( const Vector3DP &v1, const Vector3DP &v2 ) {
    const double dx = v2.x - v1.x;
    const double dy = v2.y - v1.y;
    const double dz = v2.z - v1.z;
    return ( dx * dx + dy * dy + dz * dz );
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
