/**
*
*
*   Only to be included by qray_math.h: file contains Vector3 function implementations.
*
*
**/
#pragma once

// Forward declare.
struct Vector3;

// Vector with components value 0.0f
QM_API_CONSTEXPR Vector3 QM_Vector3Zero( void ) {
	Vector3 result = { 0.0f, 0.0f, 0.0f };
    return result;
}

// Vector with components value 1.0f
QM_API_CONSTEXPR Vector3 QM_Vector3One( void ) {
    Vector3 result = { 1.0f, 1.0f, 1.0f };
    return result;
}

// Vector3 with x and y component of Vector2.
#ifdef __cplusplus
QM_API Vector2 QM_Vector2FromVector3( const Vector3 &v1 ) {
    const Vector2 result = { v1.x, v1.y };
    return result;
}
#endif

// Add two vectors
QM_API_CONSTEXPR Vector3 QM_Vector3Add( const Vector3 &v1, const Vector3 &v2 ) {
    Vector3 result = { 
        v1.x + v2.x, 
        v1.y + v2.y, 
        v1.z + v2.z 
    };
    return result;
}

// Add vector and float value
QM_API_CONSTEXPR Vector3 QM_Vector3AddValue( const Vector3 &v, const float add ) {
    Vector3 result = { v.x + add, v.y + add, v.z + add };

    return result;
}

// Subtract two vectors
QM_API_CONSTEXPR Vector3 QM_Vector3Subtract( const Vector3 &v1, const Vector3 &v2 ) {
    Vector3 result = { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };

    return result;
}

// Subtract vector by float value
QM_API_CONSTEXPR Vector3 QM_Vector3SubtractValue( const Vector3 &v, const float sub ) {
    Vector3 result = { v.x - sub, v.y - sub, v.z - sub };

    return result;
}

// Multiply vector by scalar
QM_API_CONSTEXPR Vector3 QM_Vector3Scale( const Vector3 &v, const float scalar ) {
    Vector3 result = { v.x * scalar, v.y * scalar, v.z * scalar };

    return result;
}

// Multiply vector by vector
QM_API_CONSTEXPR Vector3 QM_Vector3Multiply( const Vector3 &v1, const Vector3 &v2 ) {
    Vector3 result = { v1.x * v2.x, v1.y * v2.y, v1.z * v2.z };

    return result;
}

// Returns The vector 'v' + ('add' * 'multiply').
QM_API Vector3 QM_Vector3MultiplyAdd( const Vector3 &v, const float multiply, const Vector3 &add ) {
    #ifdef __cplusplus
    Vector3 result = { 
        std::fmaf( add.x, multiply, v.x ), 
        std::fmaf( add.y, multiply, v.y ),
        std::fmaf( add.z, multiply, v.z ),
    };
    #else
    Vector3 result = {
        fmaf( add.x, multiply, v.x ),
        fmaf( add.y, multiply, v.y ),
        fmaf( add.z, multiply, v.z ),
    };
    #endif

    return result;
}

// Calculate two vectors cross product
QM_API_CONSTEXPR Vector3 QM_Vector3CrossProduct( const Vector3 &v1, const Vector3 &v2 ) {
    Vector3 result = { v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x };

    return result;
}

// Calculate one vector perpendicular vector
QM_API Vector3 QM_Vector3Perpendicular( const Vector3 &v ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    float min = (float)fabs( v.x );
    Vector3 cardinalAxis = { 1.0f, 0.0f, 0.0f };

    if ( fabsf( v.y ) < min ) {
        min = (float)fabs( v.y );
        Vector3 tmp = { 0.0f, 1.0f, 0.0f };
        cardinalAxis = tmp;
    }

    if ( fabsf( v.z ) < min ) {
        Vector3 tmp = { 0.0f, 0.0f, 1.0f };
        cardinalAxis = tmp;
    }

    // Cross product between vectors
    result.x = v.y * cardinalAxis.z - v.z * cardinalAxis.y;
    result.y = v.z * cardinalAxis.x - v.x * cardinalAxis.z;
    result.z = v.x * cardinalAxis.y - v.y * cardinalAxis.x;

    return result;
}

// Calculate the fabs() of the vector.
QM_API Vector3 QM_Vector3Fabs( const Vector3 &v1 ) {
    #ifdef __cplusplus
    Vector3 result = { 
        std::fabs( v1.x ),
        std::fabs( v1.y ),
        std::fabs( v1.z )
    };
    #else
    Vector3 result = {
        fabsf( v1.x ),
        fabsf( v1.y ),
        fabsf( v1.z )
    };
    #endif

    return result;
}

// Calculate vector length
QM_API float QM_Vector3Length( const Vector3 &v ) {
    #ifdef __cplusplus
    const float result = std::sqrt( v.x * v.x + v.y * v.y + v.z * v.z );
    #else
    const float result = sqrtf( v.x * v.x + v.y * v.y + v.z * v.z );
    #endif
    return result;
}

// Calculate vector square length
QM_API float QM_Vector3LengthSqr( const Vector3 &v ) {
    const float result = v.x * v.x + v.y * v.y + v.z * v.z;

    return result;
}

// Calculate two vectors dot product
QM_API float QM_Vector3DotProduct( const Vector3 &v1, const Vector3 &v2 ) {
    const float result = ( v1.x * v2.x + v1.y * v2.y + v1.z * v2.z );

    return result;
}

// Calculate distance between two vectors
QM_API float QM_Vector3Distance( const Vector3 &v1, const Vector3 &v2 ) {
    const float dx = v2.x - v1.x;
    const float dy = v2.y - v1.y;
    const float dz = v2.z - v1.z;
    const float result = sqrtf( dx * dx + dy * dy + dz * dz );

    return result;
}

// Calculate square distance between two vectors
QM_API float QM_Vector3DistanceSqr( const Vector3 &v1, const Vector3 &v2 ) {
    const float dx = v2.x - v1.x;
    const float dy = v2.y - v1.y;
    const float dz = v2.z - v1.z;
    const float result = dx * dx + dy * dy + dz * dz;

    return result;
}

// Calculate angle between two vectors
QM_API float QM_Vector3Angle( const Vector3 &v1, const Vector3 &v2 ) {
    const Vector3 cross = { v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x };
    const float len = sqrtf( cross.x * cross.x + cross.y * cross.y + cross.z * cross.z );
    const float dot = ( v1.x * v2.x + v1.y * v2.y + v1.z * v2.z );
    const float result = atan2f( len, dot );

    return result;
}

// Negate provided vector (invert direction)
[[nodiscard]] constexpr inline const Vector3 QM_Vector3Negate( const Vector3 &v ) {
    return { -v.x, -v.y, -v.z };
}

// Divide vector by vector
[[nodiscard]] constexpr inline const Vector3 &QM_Vector3Divide( const Vector3 &v1, const Vector3 &v2 ) {
    return { v1.x / v2.x, v1.y / v2.y, v1.z / v2.z };
}

// Q2RTXPerimental: Divide vector by vector
[[nodiscard]] constexpr inline const Vector3 &QM_Vector3DivideValue( const Vector3 &v1, const float scalar ) {
    return { v1.x / scalar, v1.y / scalar, v1.z / scalar };
}

// Normalize provided vector
QM_API Vector3 QM_Vector3Normalize( const Vector3 &v ) {
    Vector3 result = v;

    float length = sqrtf( v.x * v.x + v.y * v.y + v.z * v.z );
    if ( length != 0.0f ) {
        float ilength = 1.0f / length;

        result.x *= ilength;
        result.y *= ilength;
        result.z *= ilength;
    }

    return result;
}

//Calculate the projection of the vector v1 on to v2
QM_API Vector3 QM_Vector3Project( const Vector3 &v1, const Vector3 &v2 ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    const float v1dv2 = ( v1.x * v2.x + v1.y * v2.y + v1.z * v2.z );
    const float v2dv2 = ( v2.x * v2.x + v2.y * v2.y + v2.z * v2.z );

    const float mag = v1dv2 / v2dv2;

    result.x = v2.x * mag;
    result.y = v2.y * mag;
    result.z = v2.z * mag;

    return result;
}

//Calculate the rejection of the vector v1 on to v2
QM_API Vector3 QM_Vector3Reject( const Vector3 &v1, const Vector3 &v2 ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    const float v1dv2 = ( v1.x * v2.x + v1.y * v2.y + v1.z * v2.z );
    const float v2dv2 = ( v2.x * v2.x + v2.y * v2.y + v2.z * v2.z );

    const float mag = v1dv2 / v2dv2;

    result.x = v1.x - ( v2.x * mag );
    result.y = v1.y - ( v2.y * mag );
    result.z = v1.z - ( v2.z * mag );

    return result;
}

// Orthonormalize provided vectors
// Makes vectors normalized and orthogonal to each other
// Gram-Schmidt function implementation
QM_API_DISCARD void QM_Vector3OrthoNormalize( Vector3 *v1, Vector3 *v2 ) {
    float length = 0.0f;
    float ilength = 0.0f;

    // Vector3Normalize(*v1);
    Vector3 v = *v1;
    length = sqrtf( v.x * v.x + v.y * v.y + v.z * v.z );
    if ( length == 0.0f ) length = 1.0f;
    ilength = 1.0f / length;
    v1->x *= ilength;
    v1->y *= ilength;
    v1->z *= ilength;

    // Vector3CrossProduct(*v1, *v2)
    Vector3 vn1 = { v1->y * v2->z - v1->z * v2->y, v1->z * v2->x - v1->x * v2->z, v1->x * v2->y - v1->y * v2->x };

    // Vector3Normalize(vn1);
    v = vn1;
    length = sqrtf( v.x * v.x + v.y * v.y + v.z * v.z );
    if ( length == 0.0f ) length = 1.0f;
    ilength = 1.0f / length;
    vn1.x *= ilength;
    vn1.y *= ilength;
    vn1.z *= ilength;

    // Vector3CrossProduct(vn1, *v1)
    Vector3 vn2 = { vn1.y * v1->z - vn1.z * v1->y, vn1.z * v1->x - vn1.x * v1->z, vn1.x * v1->y - vn1.y * v1->x };

    *v2 = vn2;
}

// Transforms a Vector3 by a given Matrix
QM_API Vector3 QM_Vector3Transform( const Vector3 &v, Matrix mat ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    float x = v.x;
    float y = v.y;
    float z = v.z;

    result.x = mat.m0 * x + mat.m4 * y + mat.m8 * z + mat.m12;
    result.y = mat.m1 * x + mat.m5 * y + mat.m9 * z + mat.m13;
    result.z = mat.m2 * x + mat.m6 * y + mat.m10 * z + mat.m14;

    return result;
}

// Transform a vector by quaternion rotation
QM_API Vector3 QM_Vector3RotateByQuaternion( const Vector3 &v, Quaternion q ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    result.x = v.x * ( q.x * q.x + q.w * q.w - q.y * q.y - q.z * q.z ) + v.y * ( 2 * q.x * q.y - 2 * q.w * q.z ) + v.z * ( 2 * q.x * q.z + 2 * q.w * q.y );
    result.y = v.x * ( 2 * q.w * q.z + 2 * q.x * q.y ) + v.y * ( q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z ) + v.z * ( -2 * q.w * q.x + 2 * q.y * q.z );
    result.z = v.x * ( -2 * q.w * q.y + 2 * q.x * q.z ) + v.y * ( 2 * q.w * q.x + 2 * q.y * q.z ) + v.z * ( q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z );

    return result;
}

// Rotates a vector around an axis
QM_API Vector3 QM_Vector3RotateByAxisAngle( const Vector3 &v, Vector3 axis, float angle ) {
    // Using Euler-Rodrigues Formula
    // Ref.: https://en.wikipedia.org/w/index.php?title=Euler%E2%80%93Rodrigues_formula

    Vector3 result = v;

    // Vector3Normalize(axis);
    float length = sqrtf( axis.x * axis.x + axis.y * axis.y + axis.z * axis.z );
    if ( length == 0.0f ) length = 1.0f;
    const float ilength = 1.0f / length;
    axis.x *= ilength;
    axis.y *= ilength;
    axis.z *= ilength;

    angle /= 2.0f;
    float a = sinf( angle );
    const float b = axis.x * a;
    const float c = axis.y * a;
    const float d = axis.z * a;
    a = cosf( angle );
    Vector3 w = { b, c, d };

    // Vector3CrossProduct(w, v)
    Vector3 wv = { w.y * v.z - w.z * v.y, w.z * v.x - w.x * v.z, w.x * v.y - w.y * v.x };

    // Vector3CrossProduct(w, wv)
    Vector3 wwv = { w.y * wv.z - w.z * wv.y, w.z * wv.x - w.x * wv.z, w.x * wv.y - w.y * wv.x };

    // Vector3Scale(wv, 2*a)
    a *= 2;
    wv.x *= a;
    wv.y *= a;
    wv.z *= a;

    // Vector3Scale(wwv, 2)
    wwv.x *= 2;
    wwv.y *= 2;
    wwv.z *= 2;

    result.x += wv.x;
    result.y += wv.y;
    result.z += wv.z;

    result.x += wwv.x;
    result.y += wwv.y;
    result.z += wwv.z;

    return result;
}

// Calculate linear interpolation between two vectors, the slightly less precise version.
QM_API Vector3 QM_Vector3LerpFast( const Vector3 &v1, const Vector3 &v2, const float amount ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    result.x = v1.x + amount * ( v2.x - v1.x );
    result.y = v1.y + amount * ( v2.y - v1.y );
    result.z = v1.z + amount * ( v2.z - v1.z );

    return result;
}

// Calculate linear interpolation between two vectors, slower lerp, but you specify back & front lerp separately.
QM_API Vector3 QM_Vector3LerpBackFront( const Vector3 &v1, const Vector3 &v2, const float backLerp, const float frontLerp ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    result.x = ( v1.x ) * (backLerp) + ( v2.x ) * ( frontLerp );
    result.y = ( v1.y ) * (backLerp) + ( v2.y ) * ( frontLerp );
    result.z = ( v1.z ) * (backLerp) + ( v2.z ) * ( frontLerp );

    return result;
}

// Calculate linear interpolation between two vectors, slower lerp, but is more mathematically precise.
QM_API Vector3 QM_Vector3Lerp( const Vector3 &v1, const Vector3 &v2, const float amount ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    result.x = ( v1.x ) * ( 1.0f - amount ) + ( v2.x ) * ( amount );
    result.y = ( v1.y ) * ( 1.0f - amount ) + ( v2.y ) * ( amount );
    result.z = ( v1.z ) * ( 1.0f - amount ) + ( v2.z ) * ( amount );

    return result;
}

// Calculate reflected vector to normal
QM_API_CONSTEXPR Vector3 &QM_Vector3Reflect( const Vector3 &v, const Vector3 &normal ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    // I is the original vector
    // N is the normal of the incident plane
    // R = I - (2*N*(DotProduct[I, N]))

    const float dotProduct = ( v.x * normal.x + v.y * normal.y + v.z * normal.z );

    result.x = v.x - ( 2.0f * normal.x ) * dotProduct;
    result.y = v.y - ( 2.0f * normal.y ) * dotProduct;
    result.z = v.z - ( 2.0f * normal.z ) * dotProduct;

    return result;
}

// Get min value for each pair of components
QM_API_CONSTEXPR Vector3 &QM_Vector3Minf( const Vector3 &v1, const Vector3 &v2 ) {
    return {
        std::fmin( v1.x, v2.x ),
        std::fmin( v1.y, v2.y ),
        std::fmin( v1.z, v2.z ),
    };
}

// Get max value for each pair of components
QM_API_CONSTEXPR Vector3 &QM_Vector3Maxf( const Vector3 &v1, const Vector3 &v2 ) {
    return {
        std::fmax( v1.x, v2.x ),
        std::fmax( v1.y, v2.y ),
        std::fmax( v1.z, v2.z ),
    };
}

// Get absolute value for each pair of components
QM_API_CONSTEXPR Vector3 &QM_Vector3Absf( const Vector3 &v ) {
    return {
        std::fabs( v.x ),
        std::fabs( v.y ),
        std::fabs( v.z ),
    };
}


// Compute barycenter coordinates (u, v, w) for point p with respect to triangle (a, b, c)
// NOTE: Assumes P is on the plane of the triangle
QM_API Vector3 QM_Vector3Barycenter( const Vector3 &p, const Vector3 &a, const Vector3 &b, const Vector3 &c ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    const Vector3 v0 = { b.x - a.x, b.y - a.y, b.z - a.z };   // Vector3Subtract(b, a)
    const Vector3 v1 = { c.x - a.x, c.y - a.y, c.z - a.z };   // Vector3Subtract(c, a)
    const Vector3 v2 = { p.x - a.x, p.y - a.y, p.z - a.z };   // Vector3Subtract(p, a)
    const float d00 = ( v0.x * v0.x + v0.y * v0.y + v0.z * v0.z );    // Vector3DotProduct(v0, v0)
    const float d01 = ( v0.x * v1.x + v0.y * v1.y + v0.z * v1.z );    // Vector3DotProduct(v0, v1)
    const float d11 = ( v1.x * v1.x + v1.y * v1.y + v1.z * v1.z );    // Vector3DotProduct(v1, v1)
    const float d20 = ( v2.x * v0.x + v2.y * v0.y + v2.z * v0.z );    // Vector3DotProduct(v2, v0)
    const float d21 = ( v2.x * v1.x + v2.y * v1.y + v2.z * v1.z );    // Vector3DotProduct(v2, v1)

    const float denom = d00 * d11 - d01 * d01;

    result.y = ( d11 * d20 - d01 * d21 ) / denom;
    result.z = ( d00 * d21 - d01 * d20 ) / denom;
    result.x = 1.0f - ( result.z + result.y );

    return result;
}

// Projects a Vector3 from screen space into object space
// NOTE: We are avoiding calling other raymath functions despite available
QM_API Vector3 QM_Vector3Unproject( const Vector3 &source, Matrix projection, Matrix view ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    // Calculate unprojected matrix (multiply view matrix by projection matrix) and invert it
    const Matrix matViewProj = {      // MatrixMultiply(view, projection);
        view.m0 * projection.m0 + view.m1 * projection.m4 + view.m2 * projection.m8 + view.m3 * projection.m12,
        view.m0 * projection.m1 + view.m1 * projection.m5 + view.m2 * projection.m9 + view.m3 * projection.m13,
        view.m0 * projection.m2 + view.m1 * projection.m6 + view.m2 * projection.m10 + view.m3 * projection.m14,
        view.m0 * projection.m3 + view.m1 * projection.m7 + view.m2 * projection.m11 + view.m3 * projection.m15,
        view.m4 * projection.m0 + view.m5 * projection.m4 + view.m6 * projection.m8 + view.m7 * projection.m12,
        view.m4 * projection.m1 + view.m5 * projection.m5 + view.m6 * projection.m9 + view.m7 * projection.m13,
        view.m4 * projection.m2 + view.m5 * projection.m6 + view.m6 * projection.m10 + view.m7 * projection.m14,
        view.m4 * projection.m3 + view.m5 * projection.m7 + view.m6 * projection.m11 + view.m7 * projection.m15,
        view.m8 * projection.m0 + view.m9 * projection.m4 + view.m10 * projection.m8 + view.m11 * projection.m12,
        view.m8 * projection.m1 + view.m9 * projection.m5 + view.m10 * projection.m9 + view.m11 * projection.m13,
        view.m8 * projection.m2 + view.m9 * projection.m6 + view.m10 * projection.m10 + view.m11 * projection.m14,
        view.m8 * projection.m3 + view.m9 * projection.m7 + view.m10 * projection.m11 + view.m11 * projection.m15,
        view.m12 * projection.m0 + view.m13 * projection.m4 + view.m14 * projection.m8 + view.m15 * projection.m12,
        view.m12 * projection.m1 + view.m13 * projection.m5 + view.m14 * projection.m9 + view.m15 * projection.m13,
        view.m12 * projection.m2 + view.m13 * projection.m6 + view.m14 * projection.m10 + view.m15 * projection.m14,
        view.m12 * projection.m3 + view.m13 * projection.m7 + view.m14 * projection.m11 + view.m15 * projection.m15 };

    // Calculate inverted matrix -> MatrixInvert(matViewProj);
    // Cache the matrix values (speed optimization)
    const float a00 = matViewProj.m0, a01 = matViewProj.m1, a02 = matViewProj.m2, a03 = matViewProj.m3;
    const float a10 = matViewProj.m4, a11 = matViewProj.m5, a12 = matViewProj.m6, a13 = matViewProj.m7;
    const float a20 = matViewProj.m8, a21 = matViewProj.m9, a22 = matViewProj.m10, a23 = matViewProj.m11;
    const float a30 = matViewProj.m12, a31 = matViewProj.m13, a32 = matViewProj.m14, a33 = matViewProj.m15;

    const float b00 = a00 * a11 - a01 * a10;
    const float b01 = a00 * a12 - a02 * a10;
    const float b02 = a00 * a13 - a03 * a10;
    const float b03 = a01 * a12 - a02 * a11;
    const float b04 = a01 * a13 - a03 * a11;
    const float b05 = a02 * a13 - a03 * a12;
    const float b06 = a20 * a31 - a21 * a30;
    const float b07 = a20 * a32 - a22 * a30;
    const float b08 = a20 * a33 - a23 * a30;
    const float b09 = a21 * a32 - a22 * a31;
    const float b10 = a21 * a33 - a23 * a31;
    const float b11 = a22 * a33 - a23 * a32;

    // Calculate the invert determinant (inlined to avoid double-caching)
    const float invDet = 1.0f / ( b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06 );

    const Matrix matViewProjInv = {
        ( a11 * b11 - a12 * b10 + a13 * b09 ) * invDet,
        ( -a01 * b11 + a02 * b10 - a03 * b09 ) * invDet,
        ( a31 * b05 - a32 * b04 + a33 * b03 ) * invDet,
        ( -a21 * b05 + a22 * b04 - a23 * b03 ) * invDet,
        ( -a10 * b11 + a12 * b08 - a13 * b07 ) * invDet,
        ( a00 * b11 - a02 * b08 + a03 * b07 ) * invDet,
        ( -a30 * b05 + a32 * b02 - a33 * b01 ) * invDet,
        ( a20 * b05 - a22 * b02 + a23 * b01 ) * invDet,
        ( a10 * b10 - a11 * b08 + a13 * b06 ) * invDet,
        ( -a00 * b10 + a01 * b08 - a03 * b06 ) * invDet,
        ( a30 * b04 - a31 * b02 + a33 * b00 ) * invDet,
        ( -a20 * b04 + a21 * b02 - a23 * b00 ) * invDet,
        ( -a10 * b09 + a11 * b07 - a12 * b06 ) * invDet,
        ( a00 * b09 - a01 * b07 + a02 * b06 ) * invDet,
        ( -a30 * b03 + a31 * b01 - a32 * b00 ) * invDet,
        ( a20 * b03 - a21 * b01 + a22 * b00 ) * invDet };

    // Create quaternion from source point
    const Quaternion quat = { source.x, source.y, source.z, 1.0f };

    // Multiply quat point by unprojecte matrix
    const Quaternion qtransformed = {     // QuaternionTransform(quat, matViewProjInv)
        matViewProjInv.m0 * quat.x + matViewProjInv.m4 * quat.y + matViewProjInv.m8 * quat.z + matViewProjInv.m12 * quat.w,
        matViewProjInv.m1 * quat.x + matViewProjInv.m5 * quat.y + matViewProjInv.m9 * quat.z + matViewProjInv.m13 * quat.w,
        matViewProjInv.m2 * quat.x + matViewProjInv.m6 * quat.y + matViewProjInv.m10 * quat.z + matViewProjInv.m14 * quat.w,
        matViewProjInv.m3 * quat.x + matViewProjInv.m7 * quat.y + matViewProjInv.m11 * quat.z + matViewProjInv.m15 * quat.w };

    // Normalized world points in vectors
    result.x = qtransformed.x / qtransformed.w;
    result.y = qtransformed.y / qtransformed.w;
    result.z = qtransformed.z / qtransformed.w;

    return result;
}

// Get Vector3 as float array
QM_API float3 QM_Vector3ToFloatV( const Vector3 &v ) {
    float3 buffer = { 0 };

    buffer.v[ 0 ] = v.x;
    buffer.v[ 1 ] = v.y;
    buffer.v[ 2 ] = v.z;

    return buffer;
}

// Q2RTXPerimental: Get Vector3 as vec3_t array.
QM_API qfloat3 QM_Vector3ToQFloatV( const Vector3 &v ) {
    qfloat3 buffer = { 0 };

    buffer.v[ 0 ] = v.x;
    buffer.v[ 1 ] = v.y;
    buffer.v[ 2 ] = v.z;

    return buffer;
}

// Invert the given vector
QM_API Vector3 QM_Vector3Invert( const Vector3 &v ) {
    Vector3 result = { 1.0f / v.x, 1.0f / v.y, 1.0f / v.z };

    return result;
}

// Clamp the components of the vector between
// min and max values specified by the given vectors
QM_API Vector3 QM_Vector3Clamp( const Vector3 &v, const Vector3 &min, const Vector3 &max ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    result.x = fminf( max.x, fmaxf( min.x, v.x ) );
    result.y = fminf( max.y, fmaxf( min.y, v.y ) );
    result.z = fminf( max.z, fmaxf( min.z, v.z ) );

    return result;
}

// Clamp the magnitude of the vector between two values
QM_API Vector3 QM_Vector3ClampValue( const Vector3 &v, const float min, const float max ) {
    Vector3 result = v;

    float length = ( v.x * v.x ) + ( v.y * v.y ) + ( v.z * v.z );
    if ( length > 0.0f ) {
        length = sqrtf( length );

        if ( length < min ) {
            float scale = min / length;
            result.x = v.x * scale;
            result.y = v.y * scale;
            result.z = v.z * scale;
        } else if ( length > max ) {
            float scale = max / length;
            result.x = v.x * scale;
            result.y = v.y * scale;
            result.z = v.z * scale;
        }
    }

    return result;
}

// Check whether two given vectors are almost equal
#ifdef __cplusplus
QM_API int QM_Vector3EqualsEpsilon( const Vector3 &p, const Vector3 &q, const float epsilon = QM_EPSILON ) {
    #else
QM_API int QM_Vector3EqualsEpsilon( Vector3 p, Vector3 q, const float epsilon ) {
#endif
    int result = ( ( std::fabs( p.x - q.x ) ) <= ( epsilon * std::max( 1.0f, std::max( fabsf( p.x ), std::fabs( q.x ) ) ) ) ) &&
        ( ( std::fabs( p.y - q.y ) ) <= ( epsilon * std::max( 1.0f, std::max( std::fabs( p.y ), std::fabs( q.y ) ) ) ) ) &&
        ( ( std::fabs( p.z - q.z ) ) <= ( epsilon * std::max( 1.0f, std::max( std::fabs( p.z ), std::fabs( q.z ) ) ) ) );

    return result;
}
QM_API bool QM_Vector3Equals( const Vector3 &p, const Vector3 &q ) {
    return QM_Vector3EqualsEpsilon( p, q, QM_EPSILON );
}
// Less precise but more performance friendly check.
QM_API bool QM_Vector3EqualsFast( const Vector3 &p, const Vector3 &q ) {
    return ( p.x == q.x && p.y == q.y && p.z == q.z );
}

// Compute the direction of a refracted ray
// v: normalized direction of the incoming ray
// n: normalized normal vector of the interface of two optical media
// r: ratio of the refractive index of the medium from where the ray comes
//    to the refractive index of the medium on the other side of the surface
QM_API Vector3 QM_Vector3Refract( Vector3 v, const Vector3 &n, const float r ) {
    Vector3 result = { 0.f, 0.f, 0.f };

    float dot = v.x * n.x + v.y * n.y + v.z * n.z;
    float d = 1.0f - r * r * ( 1.0f - dot * dot );

    if ( d >= 0.0f ) {
        d = sqrtf( d );
        v.x = r * v.x - ( r * dot + d ) * n.x;
        v.y = r * v.y - ( r * dot + d ) * n.y;
        v.z = r * v.z - ( r * dot + d ) * n.z;

        result = v;
    }

    return result;
}

#if 0
// OLD: Look at QM_Vector3ToAngles for a reference as to why this is 'rigged'.
// Returns an appropriate 'yaw' angle based on the Vector3.
QM_API float QM_Vector3ToYaw( vec3_t vec ) {
    float   yaw;

    if (/*vec[YAW] == 0 &&*/ vec[ PITCH ] == 0 ) {
        yaw = 0;
        if ( vec[ YAW ] > 0 ) {
            yaw = 90;
        } else if ( vec[ YAW ] < 0 ) {
            yaw = -90;
        }
    } else {
        yaw = (int)RAD2DEG( atan2( vec[ YAW ], vec[ PITCH ] ) );
        if ( yaw < 0 ) {
            yaw += 360;
        }
    }

    return yaw;
}
#else
    #ifdef __cplusplus
    QM_API float QM_Vector3ToYaw( const Vector3 &vec ) {
    #else
    QM_API float QM_Vector3ToYaw( vec3_t vec ) {
    #endif
        float yaw;/*float	tmp, yaw, pitch;*/

        if ( /*vec[ YAW ] == 0 &&*/ vec[ PITCH ] == 0 ) {
            yaw = 0;
            if ( vec[ YAW ] > 0 ) {
                yaw = 270;
            } else {
                yaw = 90;
            }
        } else {
            yaw = ( atan2f( vec[ YAW ], vec[ PITCH ] ) * 180 / M_PI );
            if ( yaw < 0 ) {//if ( yaw < -180 )
                yaw += 360;
            }

            //tmp = sqrt( vec[ PITCH ] * vec[ PITCH ] + vec[ YAW ] * vec[ YAW ] );
            //pitch = ( atan2( -vec[ ROLL ], tmp ) * 180 / M_PI );
            //if ( pitch < 0 )
            //    pitch += 360;
        }

        return yaw;
    }
#endif


#if 0
// OLD 'vectoangles', supposedly this one is rigged.
void QM_Vector3ToAngles( vec3_t value1, vec3_t angles ) {
    float   forward;
    float   yaw, pitch;

    if ( value1[ 1 ] == 0 && value1[ 0 ] == 0 ) {
        yaw = 0;
        if ( value1[ 2 ] > 0 )
            pitch = 90;
        else
            pitch = 270;
    } else {
        if ( value1[ 0 ] )
            yaw = (int)RAD2DEG( atan2( value1[ 1 ], value1[ 0 ] ) );
        else if ( value1[ 1 ] > 0 )
            yaw = 90;
        else
            yaw = -90;
        if ( yaw < 0 )
            yaw += 360;

        forward = sqrtf( value1[ 0 ] * value1[ 0 ] + value1[ 1 ] * value1[ 1 ] );
        pitch = (int)RAD2DEG( atan2( value1[ 2 ], forward ) );
        if ( pitch < 0 )
            pitch += 360;
    }

    angles[ PITCH ] = -pitch;
    angles[ YAW ] = yaw;
    angles[ ROLL ] = 0;
}
#else
// NEW 'vectoangles', taken from https://www.reddit.com/r/HalfLife/comments/6jjhbd/xash_guy_on_stupid_quake_bug/

/**
*   @brief Converts a normalized direction vector(treated as forward) into Euler angles.
*   @param forward  Forward normalized direction vector.
*   @param angles   The output vector3 euler angles.
**/
QM_API_DISCARD void QM_Vector3ToAngles( const Vector3 forward, vec3_t angles ) {
    //float forwardsqrt = 0;
    //float yaw = 0;
    //float pitch = 0;
    //if ( forward.y/*YAW*/ == 0 && forward.x/*PITCH*/ == 0 ) {
    //    yaw = 0;
    //    if ( forward.z/*ROLL*/ > 0 ) {
    //        pitch = 270;
    //    } else {
    //        pitch = 90;
    //    }
    //} else {
    //    yaw = ( atan2( forward.y/*YAW*/, forward.x/*PITCH*/) * QM_RAD2DEG );
    //    if ( yaw < 0 ) {
    //        yaw += 360;
    //    }
    //    forwardsqrt = sqrt( forward.x/*PITCH*/ * forward.x/*PITCH*/ + forward.y/*YAW*/ * forward.y/*YAW*/ );
    //    pitch = ( atan2( -forward.z/*ROLL*/, forwardsqrt ) * QM_RAD2DEG );
    //    if ( pitch < 0 ) {
    //        pitch += 360;
    //    }
    //}
    //angles[ 0 ] = pitch; angles[ 1 ] = yaw; angles[ 2 ] = 0;
    float	tmp, yaw, pitch;

    if ( forward.y == 0 && forward.x == 0 ) {
        yaw = 0;
        if ( forward.z > 0 )
            pitch = 270;
        else
            pitch = 90;
    } else {
        yaw = ( atan2f( forward.y, forward.x ) * 180 / M_PI );
        if ( yaw < 0 )
            yaw += 360;

        tmp = sqrtf( forward.x * forward.x + forward.y * forward.y );
        pitch = ( atan2f( -forward.z, tmp ) * 180 / M_PI );
        if ( pitch < 0 )
            pitch += 360;
    }

    angles[ 0 ] = pitch;
    angles[ 1 ] = yaw;
    angles[ 2 ] = 0;
}
#ifdef __cplusplus
QM_API Vector3 QM_Vector3ToAngles( const Vector3 &forward ) {
    Vector3 angles = {};
    QM_Vector3ToAngles( forward, &angles.x );
    return angles;
}
#endif
#endif

// Vector with z component value 1.0f, pointing upwards in Quake Space.
QM_API Vector3 QM_Vector3Up( void ) {
    Vector3 result = { 0.0f, 0.0f, 1.0f };

    return result;
}

// Vector with z component value -1.0f, pointing downwards in Quake Space.
QM_API Vector3 QM_Vector3Down( void ) {
    Vector3 result = { 0.0f, 0.0f, -1.0f };

    return result;
}

/**
*
*
*   Q2RTXPerimental: C++ Functions/Operators for the Vector3 type.
*
*
**/
#ifdef __cplusplus

/**
*   @brief  Returns the Vector3 '{ FLT_MAX, FLT_MAX, FLT_MAX }'.
* */
QM_API Vector3 QM_Vector3Mins( void ) {
    return Vector3{
        FLT_MAX,// -FLT_MAX
        FLT_MAX,// -FLT_MAX
        FLT_MAX // -FLT_MAX
    };
}
/**
*   @brief  Returns the Vector3 '{ -FLT_MAX, -FLT_MAX, -FLT_MAX }'.
**/
QM_API Vector3 QM_Vector3Maxs( void ) {
    return Vector3{
        -FLT_MAX,// FLT_MAX
        -FLT_MAX,// FLT_MAX
        -FLT_MAX // FLT_MAX
    };
}


/**
*   @brief  Returns the linear interpolation of `a` and `b` using the specified 'lerp' fractions.
**/
QM_API Vector3 QM_Vector3LerpVector3( const Vector3 &a, const Vector3 &b, const Vector3 &lerp ) {
    return QM_Vector3Add( a, QM_Vector3Multiply( QM_Vector3Subtract( b, a ), lerp ) );
}


/**
*   @brief  Return a vector with random values between 'begin' and 'end'.
**/
QM_API Vector3 QM_Vector3RandomRange( const float begin, const float end ) {
    return Vector3{
        frandom( begin, end ),
        frandom( begin, end ),
        frandom( begin, end )
    };
}
/**
*   @brief  A vector with random values between '0' and '1'.
**/
QM_API Vector3 QM_Vector3Random( void ) {
    return QM_Vector3RandomRange( 0.f, 1.f );
}

#if 1
/**
*   @brief  Access Vector3 members by their index instead.
*   @return Value of the indexed Vector3 component.
**/
[[nodiscard]] inline constexpr const float &Vector3::operator[]( const size_t i ) const {
    if ( i == 0 )
        return x;
    else if ( i == 1 )
        return y;
    else if ( i == 2 )
        return z;
    else
        throw std::out_of_range( "i" );
}
/**
*   @brief  Access Vector3 members by their index instead.
*   @return Value of the indexed Vector3 component.
**/
[[nodiscard]] inline constexpr float &Vector3::operator[]( const size_t i ) {
    if ( i == 0 )
        return x;
    else if ( i == 1 )
        return y;
    else if ( i == 2 )
        return z;
    else
        throw std::out_of_range( "i" );
}
/**
*   @brief  Vector3 C++ 'Plus' operator:
**/
QM_API_CONSTEXPR Vector3 operator+( const Vector3 &left, const Vector3 &right ) {
    return QM_Vector3Add( left, right );
}
QM_API_CONSTEXPR Vector3 &operator+( const Vector3 &left, const float &right ) {
    return QM_Vector3AddValue( left, right );
}

QM_API_CONSTEXPR_DISCARD Vector3 &operator+=( Vector3 &left, const Vector3 &right ) {
    return left = QM_Vector3Add( left, right );
}
QM_API_CONSTEXPR_DISCARD Vector3 &operator+=( Vector3 &left, const float &right ) {
    return left = QM_Vector3AddValue( left, right );
}

/**
*   @brief  Vector3 C++ 'Minus' operator:
**/
QM_API_CONSTEXPR Vector3 operator-( const Vector3 &left, const Vector3 &right ) {
    return QM_Vector3Subtract( left, right );
}
QM_API_CONSTEXPR Vector3 &operator-( const Vector3 &left, const float &right ) {
    return QM_Vector3SubtractValue( left, right );
}
QM_API_CONSTEXPR Vector3 &operator-( const Vector3 &v ) {
    return QM_Vector3Negate( v );
}

QM_API_CONSTEXPR_DISCARD Vector3 &operator-=( Vector3 &left, const Vector3 &right ) {
    return left = QM_Vector3Subtract( left, right );
}
QM_API_CONSTEXPR_DISCARD Vector3 &operator-=( Vector3 &left, const float &right ) {
    return left = QM_Vector3SubtractValue( left, right );
}

/**
*   @brief  Vector3 C++ 'Multiply' operator:
**/
QM_API_CONSTEXPR Vector3 operator*( const Vector3 &left, const Vector3 &right ) {
    return QM_Vector3Multiply( left, right );
}
QM_API_CONSTEXPR Vector3 operator*( const Vector3 &left, const float &right ) {
    return QM_Vector3Scale( left, right );
}
// for: const Vector3 &v1 = floatVal * v2;
QM_API_CONSTEXPR Vector3 operator*( const float &left, const Vector3 &right ) {
    return QM_Vector3Scale( right, left );
}

QM_API_CONSTEXPR_DISCARD Vector3 &operator*=( Vector3 &left, const Vector3 &right ) {
    return left = QM_Vector3Multiply( left, right );
}
QM_API_CONSTEXPR_DISCARD Vector3 &operator*=( Vector3 &left, const float &right ) {
    return left = QM_Vector3Scale( left, right );
}

/**
*   @brief  Vector3 C++ 'Divide' operator:
**/
QM_API_CONSTEXPR Vector3 operator/( const Vector3 &left, const Vector3 &right ) {
    return QM_Vector3Divide( left, right );
}
QM_API_CONSTEXPR Vector3 &operator/( const Vector3 &left, const float &right ) {
    return QM_Vector3DivideValue( left, right );
}

QM_API_CONSTEXPR_DISCARD Vector3 &operator/=( Vector3 &left, const Vector3 &right ) {
    return left = QM_Vector3Divide( left, right );
}
QM_API_CONSTEXPR_DISCARD Vector3 &operator/=( Vector3 &left, const float &right ) {
    return left = QM_Vector3DivideValue( left, right );
}

///**
//*   @brief  Vector3 C++ 'Equals' operator:
//**/
//QM_API bool operator==( const Vector3 &left, const Vector3 &right ) {
//    return QM_Vector3Equals( left, right );
//}
//
///**
//*   @brief  Vector3 C++ 'Not Equals' operator:
//**/
//QM_API bool operator!=( const Vector3 &left, const Vector3 &right ) {
//    return !QM_Vector3Equals( left, right );
//}
#endif

/**
*   @brief  Vector3 C++ 'AngleVectors' method for QMRayLib.
**/
QM_API_DISCARD void QM_AngleVectors( const Vector3 &angles, Vector3 *forward, Vector3 *right, Vector3 *up ) {
    float        angle;
    float        sr, sp, sy, cr, cp, cy;

    angle = DEG2RAD( angles[ YAW ] );
    sy = sin( angle );
    cy = cos( angle );
    angle = DEG2RAD( angles[ PITCH ] );
    sp = sin( angle );
    cp = cos( angle );
    angle = DEG2RAD( angles[ ROLL ] );
    sr = sin( angle );
    cr = cos( angle );

    if ( forward ) {
        forward->x = cp * cy;
        forward->y = cp * sy;
        forward->z = -sp;
    }
    if ( right ) {
        right->x = ( -1 * sr * sp * cy + -1 * cr * -sy );
        right->y = ( -1 * sr * sp * sy + -1 * cr * cy );
        right->z = -1 * sr * cp;
    }
    if ( up ) {
        up->x = ( cr * sp * cy + -sr * -sy );
        up->y = ( cr * sp * sy + -sr * cy );
        up->z = cr * cp;
    }
}

/**
*   @brief  Normalize provided vector, return the final length.
**/
QM_API float QM_Vector3NormalizeLength( Vector3 &v ) {
    float length = sqrtf( v.x * v.x + v.y * v.y + v.z * v.z );
    if ( length != 0.0f ) {
        float ilength = 1.0f / length;

        v.x *= ilength;
        v.y *= ilength;
        v.z *= ilength;
    }

    return length;
}

/**
*   @brief  AngleMod the entire Vector3.
**/
QM_API Vector3 QM_Vector3AngleMod( const Vector3 &v ) {
    return Vector3{
        QM_AngleMod( v.x ),
        QM_AngleMod( v.y ),
        QM_AngleMod( v.z )
    };
}

/**
*   @brief  Will lerp between the euler angle, a2 and a1.
**/
QM_API Vector3 QM_Vector3LerpAngles( const Vector3 &angleVec2, const Vector3 &angleVec1, const float fraction ) {
    return {
        LerpAngle( angleVec2.x, angleVec1.x, fraction ),
        LerpAngle( angleVec2.y, angleVec1.y, fraction ),
        LerpAngle( angleVec2.z, angleVec1.z, fraction ),
    };
}

/**
*   @return The closest point of the box that is near vector 'in'.
**/
QM_API Vector3 QM_Vector3ClosestPointToBox( const Vector3 &in, const Vector3 &absmin, const Vector3 &absmax ) {
    Vector3 out = {};

    for ( int i = 0; i < 3; i++ ) {
        out[ i ] = ( in[ i ] < absmin[ i ] ) ? absmin[ i ] : ( in[ i ] > absmax[ i ] ) ? absmax[ i ] : in[ i ];
    }

    return out;
}
#endif  // __cplusplus
