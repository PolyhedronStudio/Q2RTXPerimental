/**
*
*
*   Only to be included by qray_math.h: file contains Vector2 function implementations.
*
*
**/
#pragma once

// Vector with components value 0.0f
RMAPI Vector2 QM_Vector2Zero( void ) {
    Vector2 result = { 0.0f, 0.0f };

    return result;
}

// Vector with components value 1.0f
RMAPI Vector2 QM_Vector2One( void ) {
    Vector2 result = { 1.0f, 1.0f };

    return result;
}

// Add two vectors (v1 + v2)
RMAPI Vector2 QM_Vector2Add( Vector2 v1, Vector2 v2 ) {
    Vector2 result = { v1.x + v2.x, v1.y + v2.y };

    return result;
}

// Add vector and float value
RMAPI Vector2 QM_Vector2AddValue( Vector2 v, float add ) {
    Vector2 result = { v.x + add, v.y + add };

    return result;
}

// Subtract two vectors (v1 - v2)
RMAPI Vector2 QM_Vector2Subtract( Vector2 v1, Vector2 v2 ) {
    Vector2 result = { v1.x - v2.x, v1.y - v2.y };

    return result;
}

// Subtract vector by float value
RMAPI Vector2 QM_Vector2SubtractValue( Vector2 v, float sub ) {
    Vector2 result = { v.x - sub, v.y - sub };

    return result;
}

// Calculate vector length
RMAPI float QM_Vector2Length( Vector2 v ) {
    float result = sqrtf( ( v.x * v.x ) + ( v.y * v.y ) );

    return result;
}

// Calculate vector square length
RMAPI float QM_Vector2LengthSqr( Vector2 v ) {
    float result = ( v.x * v.x ) + ( v.y * v.y );

    return result;
}

// Calculate two vectors dot product
RMAPI float QM_Vector2DotProduct( Vector2 v1, Vector2 v2 ) {
    float result = ( v1.x * v2.x + v1.y * v2.y );

    return result;
}

// Calculate distance between two vectors
RMAPI float QM_Vector2Distance( Vector2 v1, Vector2 v2 ) {
    float result = sqrtf( ( v1.x - v2.x ) * ( v1.x - v2.x ) + ( v1.y - v2.y ) * ( v1.y - v2.y ) );

    return result;
}

// Calculate square distance between two vectors
RMAPI float QM_Vector2DistanceSqr( Vector2 v1, Vector2 v2 ) {
    float result = ( ( v1.x - v2.x ) * ( v1.x - v2.x ) + ( v1.y - v2.y ) * ( v1.y - v2.y ) );

    return result;
}

// Calculate angle between two vectors
// NOTE: Angle is calculated from origin point (0, 0)
RMAPI float QM_Vector2Angle( Vector2 v1, Vector2 v2 ) {
    float result = 0.0f;

    float dot = v1.x * v2.x + v1.y * v2.y;
    float det = v1.x * v2.y - v1.y * v2.x;

    result = atan2f( det, dot );

    return result;
}

// Calculate angle defined by a two vectors line
// NOTE: Parameters need to be normalized
// Current implementation should be aligned with glm::angle
RMAPI float QM_Vector2LineAngle( Vector2 start, Vector2 end ) {
    float result = 0.0f;

    // TODO(10/9/2023): Currently angles move clockwise, determine if this is wanted behavior
    result = -atan2f( end.y - start.y, end.x - start.x );

    return result;
}

// Scale vector (multiply by value)
RMAPI Vector2 QM_Vector2Scale( Vector2 v, float scale ) {
    Vector2 result = { v.x * scale, v.y * scale };

    return result;
}

// Multiply vector by vector
RMAPI Vector2 QM_Vector2Multiply( Vector2 v1, Vector2 v2 ) {
    Vector2 result = { v1.x * v2.x, v1.y * v2.y };

    return result;
}

// Negate vector
RMAPI Vector2 QM_Vector2Negate( Vector2 v ) {
    Vector2 result = { -v.x, -v.y };

    return result;
}

// Divide vector by vector
RMAPI Vector2 QM_Vector2Divide( Vector2 v1, Vector2 v2 ) {
    Vector2 result = { v1.x / v2.x, v1.y / v2.y };

    return result;
}

// Divide vector by scalar
RMAPI Vector2 QM_Vector2DivideValue( Vector2 v1, const float scalar ) {
    Vector2 result = { v1.x / scalar, v1.y / scalar };

    return result;
}

// Normalize provided vector
RMAPI Vector2 QM_Vector2Normalize( Vector2 v ) {
    Vector2 result = { 0 };
    float length = sqrtf( ( v.x * v.x ) + ( v.y * v.y ) );

    if ( length > 0 ) {
        float ilength = 1.0f / length;
        result.x = v.x * ilength;
        result.y = v.y * ilength;
    }

    return result;
}

// Transforms a Vector2 by a given Matrix
RMAPI Vector2 QM_Vector2Transform( Vector2 v, Matrix mat ) {
    Vector2 result = { 0 };

    float x = v.x;
    float y = v.y;
    float z = 0;

    result.x = mat.m0 * x + mat.m4 * y + mat.m8 * z + mat.m12;
    result.y = mat.m1 * x + mat.m5 * y + mat.m9 * z + mat.m13;

    return result;
}

// Calculate linear interpolation between two vectors
RMAPI Vector2 QM_Vector2Lerp( Vector2 v1, Vector2 v2, float amount ) {
    Vector2 result = { 0 };

    result.x = v1.x + amount * ( v2.x - v1.x );
    result.y = v1.y + amount * ( v2.y - v1.y );

    return result;
}

// Calculate reflected vector to normal
RMAPI Vector2 QM_Vector2Reflect( Vector2 v, Vector2 normal ) {
    Vector2 result = { 0 };

    float dotProduct = ( v.x * normal.x + v.y * normal.y ); // Dot product

    result.x = v.x - ( 2.0f * normal.x ) * dotProduct;
    result.y = v.y - ( 2.0f * normal.y ) * dotProduct;

    return result;
}

// Rotate vector by angle
RMAPI Vector2 QM_Vector2Rotate( Vector2 v, float angle ) {
    Vector2 result = { 0 };

    float cosres = cosf( angle );
    float sinres = sinf( angle );

    result.x = v.x * cosres - v.y * sinres;
    result.y = v.x * sinres + v.y * cosres;

    return result;
}

// Move Vector towards target
RMAPI Vector2 QM_Vector2MoveTowards( Vector2 v, Vector2 target, float maxDistance ) {
    Vector2 result = { 0 };

    float dx = target.x - v.x;
    float dy = target.y - v.y;
    float value = ( dx * dx ) + ( dy * dy );

    if ( ( value == 0 ) || ( ( maxDistance >= 0 ) && ( value <= maxDistance * maxDistance ) ) ) return target;

    float dist = sqrtf( value );

    result.x = v.x + dx / dist * maxDistance;
    result.y = v.y + dy / dist * maxDistance;

    return result;
}

// Invert the given vector
RMAPI Vector2 QM_Vector2Invert( Vector2 v ) {
    Vector2 result = { 1.0f / v.x, 1.0f / v.y };

    return result;
}

// Clamp the components of the vector between
// min and max values specified by the given vectors
RMAPI Vector2 QM_Vector2Clamp( Vector2 v, Vector2 min, Vector2 max ) {
    Vector2 result = { 0 };

    result.x = fminf( max.x, fmaxf( min.x, v.x ) );
    result.y = fminf( max.y, fmaxf( min.y, v.y ) );

    return result;
}

// Clamp the magnitude of the vector between two min and max values
RMAPI Vector2 QM_Vector2ClampValue( Vector2 v, float min, float max ) {
    Vector2 result = v;

    float length = ( v.x * v.x ) + ( v.y * v.y );
    if ( length > 0.0f ) {
        length = sqrtf( length );

        if ( length < min ) {
            float scale = min / length;
            result.x = v.x * scale;
            result.y = v.y * scale;
        } else if ( length > max ) {
            float scale = max / length;
            result.x = v.x * scale;
            result.y = v.y * scale;
        }
    }

    return result;
}

// Check whether two given vectors are almost equal
RMAPI int QM_Vector2Equals( Vector2 p, Vector2 q ) {
    #if !defined(QM_EPSILON)
    #define QM_EPSILON 0.000001f
    #endif

    int result = ( ( fabsf( p.x - q.x ) ) <= ( QM_EPSILON * fmaxf( 1.0f, fmaxf( fabsf( p.x ), fabsf( q.x ) ) ) ) ) &&
        ( ( fabsf( p.y - q.y ) ) <= ( QM_EPSILON * fmaxf( 1.0f, fmaxf( fabsf( p.y ), fabsf( q.y ) ) ) ) );

    return result;
}

/**
*
*
*   Q2RTXPerimental: C++ Operators for the Vector2 type.
*
*
**/
#ifdef __cplusplus
/**
*   @brief  Access Vector2 members by their index instead.
*   @return Value of the indexed Vector2 component.
**/
[[nodiscard]] inline constexpr const float &Vector2::operator[]( const size_t i ) const {
    if ( i == 0 )
        return x;
    else if ( i == 1 )
        return y;
    throw std::out_of_range( "i" );
}
/**
*   @brief  Access Vector2 members by their index instead.
*   @return Value of the indexed Vector2 component.
**/
[[nodiscard]] inline constexpr float &Vector2::operator[]( const size_t i ) {
    if ( i == 0 )
        return x;
    else if ( i == 1 )
        return y;
    throw std::out_of_range( "i" );
}

/**
*   @brief  Vector2 C++ 'Plus' operator:
**/
RMAPI Vector2 operator+( const Vector2 &left, const Vector2 &right ) {
    return QM_Vector2Add( left, right );
}
RMAPI Vector2 operator+( const Vector2 &left, const float &right ) {
    return QM_Vector2AddValue( left, right );
}

RMAPI Vector2 &operator+=( Vector2 &left, const Vector2 &right ) {
    return left = QM_Vector2Add( left, right );
}
RMAPI Vector2 &operator+=( Vector2 &left, const float &right ) {
    return left = QM_Vector2AddValue( left, right );
}

/**
*   @brief  Vector2 C++ 'Minus' operator:
**/
RMAPI Vector2 operator-( const Vector2 &left, const Vector2 &right ) {
    return QM_Vector2Subtract( left, right );
}
RMAPI Vector2 operator-( const Vector2 &left, const float &right ) {
    return QM_Vector2SubtractValue( left, right );
}
RMAPI Vector2 operator-( const Vector2 &v ) {
    return QM_Vector2Negate( v );
}

RMAPI Vector2 &operator-=( Vector2 &left, const Vector2 &right ) {
    return left = QM_Vector2Subtract( left, right );
}
RMAPI Vector2 &operator-=( Vector2 &left, const float &right ) {
    return left = QM_Vector2SubtractValue( left, right );
}

/**
*   @brief  Vector2 C++ 'Multiply' operator:
**/
RMAPI Vector2 operator*( const Vector2 &left, const Vector2 &right ) {
    return QM_Vector2Multiply( left, right );
}
RMAPI Vector2 operator*( const Vector2 &left, const float &right ) {
    return QM_Vector2Scale( left, right );
}
// for: Vector2 v1 = floatVal * v2;
RMAPI Vector2 operator*( const float &left, const Vector2 &right ) {
    return QM_Vector2Scale( right, left );
}

RMAPI Vector2 &operator*=( Vector2 &left, const Vector2 &right ) {
    return left = QM_Vector2Multiply( left, right );
}
RMAPI Vector2 &operator*=( Vector2 &left, const float &right ) {
    return left = QM_Vector2Scale( left, right );
}

/**
*   @brief  Vector2 C++ 'Divide' operator:
**/
RMAPI Vector2 operator/( const Vector2 &left, const Vector2 &right ) {
    return QM_Vector2Divide( left, right );
}
RMAPI Vector2 operator/( const Vector2 &left, const float &right ) {
    return QM_Vector2DivideValue( left, right );
}

RMAPI Vector2 &operator/=( Vector2 &left, const Vector2 &right ) {
    return left = QM_Vector2Divide( left, right );
}
RMAPI Vector2 &operator/=( Vector2 &left, const float &right ) {
    return left = QM_Vector2DivideValue( left, right );
}

/**
*   @brief  Vector2 C++ 'Equals' operator:
**/
RMAPI bool operator==( const Vector2 &left, const Vector2 &right ) {
    return QM_Vector2Equals( left, right );
}

/**
*   @brief  Vector2 C++ 'Not Equals' operator:
**/
RMAPI bool operator!=( const Vector2 &left, const Vector2 &right ) {
    return !QM_Vector2Equals( left, right );
}
#endif  // __cplusplus