/**
*
*
*   Only to be included by qray_math.h: file contains Vector4 function implementations.
*
*
**/
#pragma once

// TODO: Implement, these functions never came with raylib1.5 math.

/**
*
*
*   Q2RTXPerimental: C++ Functions/Operators for the Vector3 type.
*
*
**/
#ifdef __cplusplus

/**
*   @brief  Access Vector4 members by their index instead.
*   @return Value of the indexed Vector4 component.
**/
[[nodiscard]] inline constexpr const float &Vector4::operator[]( const size_t i ) const {
    if ( i == 0 )
        return x;
    else if ( i == 1 )
        return y;
    else if ( i == 2 )
        return z;
    else if ( i == 3 )
        return w;
    else
        throw std::out_of_range( "i" );
}
/**
*   @brief  Access Vector4 members by their index instead.
*   @return Value of the indexed Vector4 component.
**/
[[nodiscard]] inline constexpr float &Vector4::operator[]( const size_t i ) {
    if ( i == 0 )
        return x;
    else if ( i == 1 )
        return y;
    else if ( i == 2 )
        return z;
    else if ( i == 3 )
        return w;
    else
        throw std::out_of_range( "i" );
}

///**
//*   @brief  Vector3 C++ 'Plus' operator:
//**/
//RMAPI const Vector3 operator+( const Vector3 &left, const Vector3 &right ) {
//    return QM_Vector3Add( left, right );
//}
//RMAPI const Vector3 operator+( const Vector3 &left, const float &right ) {
//    return QM_Vector3AddValue( left, right );
//}
//
//RMAPI const Vector3 operator+=( Vector3 &left, const Vector3 &right ) {
//    return left = QM_Vector3Add( left, right );
//}
//RMAPI const Vector3 operator+=( Vector3 &left, const float &right ) {
//    return left = QM_Vector3AddValue( left, right );
//}
//
///**
//*   @brief  Vector3 C++ 'Minus' operator:
//**/
//RMAPI const Vector3 operator-( const Vector3 &left, const Vector3 &right ) {
//    return QM_Vector3Subtract( left, right );
//}
//RMAPI const Vector3 operator-( const Vector3 &left, const float &right ) {
//    return QM_Vector3SubtractValue( left, right );
//}
//RMAPI const Vector3 operator-( const Vector3 &v ) {
//    return QM_Vector3Negate( v );
//}
//
//RMAPI const Vector3 &operator-=( Vector3 &left, const Vector3 &right ) {
//    return left = QM_Vector3Subtract( left, right );
//}
//RMAPI const Vector3 &operator-=( Vector3 &left, const float &right ) {
//    return left = QM_Vector3SubtractValue( left, right );
//}
//
///**
//*   @brief  Vector3 C++ 'Multiply' operator:
//**/
//RMAPI const Vector3 operator*( const Vector3 &left, const Vector3 &right ) {
//    return QM_Vector3Multiply( left, right );
//}
//RMAPI const Vector3 operator*( const Vector3 &left, const float &right ) {
//    return QM_Vector3Scale( left, right );
//}
//// for: ConstVector3Ref v1 = floatVal * v2;
//RMAPI const Vector3 operator*( const float &left, const Vector3 &right ) {
//    return QM_Vector3Scale( right, left );
//}
//
//RMAPI const Vector3 &operator*=( Vector3 &left, const Vector3 &right ) {
//    return left = QM_Vector3Multiply( left, right );
//}
//RMAPI const Vector3 &operator*=( Vector3 &left, const float &right ) {
//    return left = QM_Vector3Scale( left, right );
//}
//
///**
//*   @brief  Vector3 C++ 'Divide' operator:
//**/
//RMAPI const Vector3 operator/( const Vector3 &left, const Vector3 &right ) {
//    return QM_Vector3Divide( left, right );
//}
//RMAPI const Vector3 operator/( const Vector3 &left, const float &right ) {
//    return QM_Vector3DivideValue( left, right );
//}
//
//RMAPI const Vector3 &operator/=( Vector3 &left, const Vector3 &right ) {
//    return left = QM_Vector3Divide( left, right );
//}
//RMAPI const Vector3 &operator/=( Vector3 &left, const float &right ) {
//    return left = QM_Vector3DivideValue( left, right );
//}
//
///**
//*   @brief  Vector3 C++ 'Equals' operator:
//**/
//RMAPI bool operator==( const Vector3 &left, const Vector3 &right ) {
//    return QM_Vector3Equals( left, right );
//}
//
///**
//*   @brief  Vector3 C++ 'Not Equals' operator:
//**/
//RMAPI bool operator!=( const Vector3 &left, const Vector3 &right ) {
//    return !QM_Vector3Equals( left, right );
//}

#endif  // __cplusplus