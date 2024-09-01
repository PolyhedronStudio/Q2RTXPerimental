/**
*
*
*   This type mainly exists for areas of code where we are using C++, thus we implement it like that.
*
*
**/
#pragma once


#ifdef __cplusplus

/**
*   @return The bounding box of 'mins' and 'maxs'
**/
RMAPI const BBox3 QM_BBox3FromMinsMaxs( ConstVector3Ref mins, ConstVector3Ref maxs ) {
    return {
        mins,
        maxs
    };
}

/**
*	@return	The center point of the bounding box.
**/
RMAPI const Vector3 QM_BBox3Center( ConstBBox3Ref bbox ) {
    return QM_Vector3Lerp( bbox.mins, bbox.maxs, .5f );
}

/**
*	@brief	Constructs a vec3_zero centered matching bounding box from the size vector.
*	@return A bbox3 containing the correct mins and maxs matching a zero center origin.
**/
RMAPI const BBox3 QM_BBox3FromSize( ConstVector3Ref size ) {
    return BBox3{
        QM_Vector3Scale( size, -0.5f ),
        QM_Vector3Scale( size, 0.5f )
    };
}
/**
*	@brief	Constructs a vec3_zero centered matching bounding box from the x,y,z values.
*	@return A bbox3 containing the correct mins and maxs matching a zero center origin.
**/
RMAPI const BBox3 QM_BBox3FromSize( const float x, const float y, const float z ) {
    return QM_BBox3FromSize( { x, y, z } );
}

/**
*	@return A zero sized box.
**/
RMAPI const BBox3 QM_BBox3Zero() {
    return BBox3{
        QM_Vector3Zero(),
        QM_Vector3Zero()
    };
}
/**
*	@brief	Constructs an INFINITY sized box which can be used to add points to (ie it scaled down to),
*			ensuring that it remains centered around its zero point.
*	@return A box sized to INFINITY.
**/
RMAPI const BBox3 QM_BBox3Infinity() {
    return BBox3{
        { INFINITY, INFINITY, INFINITY },
        { -INFINITY, -INFINITY, -INFINITY }
    };
}

/**
*	@return	A box with extended bounds if, point < mins, or point > maxs.
**/
RMAPI const BBox3 QM_BBox3Append( ConstBBox3Ref bbox, ConstVector3Ref point ) {
    return BBox3 {
        QM_Vector3Minf( bbox.mins, point ),
        QM_Vector3Maxf( bbox.maxs, point )
    };
}
/**
*	@return	A box that 'unites' both into one.
**/
RMAPI const BBox3 QM_BBox3Union( ConstBBox3Ref bboxA, ConstBBox3Ref bboxB ) {
    return BBox3 {
        QM_Vector3Minf( bboxA.mins, bboxB.mins ),
        QM_Vector3Maxf( bboxA.maxs, bboxB.maxs ),
    };
}

/**
*	@return	A bbox constructed out of the list of points.
**/
RMAPI const BBox3 QM_BBox3FromPoints( const Vector3 *points, const uint32_t numberOfPoints ) {
    // Construct an infinite sized box to work from.
    BBox3 bbox = QM_BBox3Infinity();

    // Append with found points.
    for ( uint32_t i = 0; i < numberOfPoints; i++, points++ ) {
        bbox = QM_BBox3Append( bbox, *points );
    }

    // Return.
    return bbox;
}
/**
*	@brief	Writes the eight corner points of the bounding box to "points".
*			It must be at least 8 `Vector3` wide. The output of the points are in
*			axis order - assuming bitflags of 1 2 4 = X Y Z - where a bit unset is
*			mins and a bit set is maxs.
**/
RMAPI void QM_BBox3ToPoints( ConstBBox3Ref box, Vector3 *points ) {
    for ( int32_t i = 0; i < 8; i++ ) {
        points[ i ] = {
            ( ( i & 1 ) ? box.maxs : box.mins ).x,
            ( ( i & 2 ) ? box.maxs : box.mins ).y,
            ( ( i & 4 ) ? box.maxs : box.mins ).z
        };
    }
}

/**
*	@brief	Returns true if boxA its bounds intersect the bounds of box B, false otherwise.
**/
RMAPI const bool QM_BBox3Intersects( ConstBBox3Ref boxA, ConstBBox3Ref boxB ) {
    if ( boxA.mins.x >= boxB.maxs.x || boxA.mins.y >= boxB.maxs.y || boxA.mins.z >= boxB.maxs.z ) {
        return false;
    }

    if ( boxA.maxs.x <= boxB.mins.x || boxA.maxs.y <= boxB.mins.y || boxA.maxs.z <= boxB.mins.z ) {
        return false;
    }

    return true;
}

/**
*	@brief	Returns true if 'box' contains point 'point'
**/
RMAPI const bool QM_BBox3ContainsPoint( ConstBBox3Ref box, ConstVector3Ref point ) {
    if ( point.x >= box.maxs.x || point.y >= box.maxs.y || point.z >= box.maxs.z ) {
        return false;
    }

    if ( point.x <= box.mins.x || point.y <= box.mins.y || point.z <= box.mins.z ) {
        return false;
    }

    return true;
}


/**
*	@return	The relative size of the box' bounds. Also works as a vector
*	between the two points of a box.
**/
RMAPI const Vector3 QM_BBox3Size( ConstBBox3Ref box ) {
    return box.maxs - box.mins;
}

/**
*	@return	The distance between the two corners of the box's bounds.
**/
RMAPI const float QM_BBox3Distance( ConstBBox3Ref box ) {
    return QM_Vector3Distance( box.maxs, box.mins );
}

/**
*	@return	The radius of the bounds. A sphere that contains the entire box.
**/
RMAPI const float QM_BBox3Radius( ConstBBox3Ref box ) {
    return QM_BBox3Distance( box ) / 2.f;
}

/**
*	@return	A bounding box based on the 'size' vec3, centered along 'center'.
**/
RMAPI const BBox3 QM_BBox3FromCenterSize( ConstVector3Ref size, ConstVector3Ref center = QM_Vector3Zero() ) {
    const Vector3 halfSize = QM_Vector3Scale( size, .5f );
    return BBox3{
        center - halfSize,
        center + halfSize,
    };
}

/**
*	@return	A bounding box based on the 'radius', centered along 'center'.
**/
RMAPI const BBox3 QM_BBox3FromCenterRadius( const float radius, ConstVector3Ref center = QM_Vector3Zero() ) {
    const Vector3 radiusVec = { radius, radius, radius };
    return BBox3{
        center - radiusVec,
        center + radiusVec,
    };
}

///**
//*	@return A bounding box expanded, or shrunk(in case of negative values), on all axis.
//**/
//RMAPI const BBox3 bbox3_expand_vec3( const BBox3 &box, const Vector3 &expansion ) {
//	return BBox3 {
//		box.mins - expansion,
//		box.maxs + expansion
//	};
//}
///**
//*	@return A bounding box expanded, or shrunk(in case of negative values), on all axis.
//**/
//RMAPI const BBox3 bbox3_expand_bbox3( const BBox3 &box, const BBox3 &expansion) {
//	return BBox3 {
//		box.mins + expansion.mins,
//		box.maxs + expansion.maxs
//	};
//}
///**
//*	@return A bounding box expanded, or shrunk(in case of negative values), on all axis.
//**/
//RMAPI const BBox3 bbox3_expandf( const BBox3 &box, const float expansion ) {
//	return bbox3_expand_vec3( box, Vector3{ expansion, expansion, expansion } );
//}

RMAPI const BBox3 QM_BBox3ExpandVector3( ConstBBox3Ref bounds, ConstVector3Ref expansion ) {
    return BBox3{
        bounds.mins - expansion,
        bounds.maxs + expansion
    };
}
RMAPI const BBox3 QM_BBox3ExpandValue( ConstBBox3Ref bounds, const float expansion ) {
    return QM_BBox3ExpandVector3( bounds, Vector3{ expansion, expansion, expansion } );
}
RMAPI const BBox3 QM_BBox3ExpandBBox3( ConstBBox3Ref boundsA, ConstBBox3Ref boundsB ) {
    return BBox3{
        boundsA.mins + boundsB.mins,
        boundsA.maxs + boundsB.maxs
    };
}

/**
*	@return	The 'point' clamped within the bounds of 'bounds'.
**/
RMAPI const Vector3 QM_BBox3ClampPoint( ConstBBox3Ref bounds, ConstVector3Ref point ) {
    return QM_Vector3Clamp( point, bounds.mins, bounds.maxs );
}

/**
*	@return	The bounds of 'boundsB' clamped within and to the bounds of 'boundsA'.
**/
RMAPI const BBox3 QM_BBox3ClampBounds( ConstBBox3Ref boundsA, ConstBBox3Ref boundsB ) {
    return BBox3{
        QM_Vector3Clamp( boundsB.mins, boundsA.mins, boundsA.maxs ),
        QM_Vector3Clamp( boundsB.maxs, boundsA.mins, boundsA.maxs ),
    };
}

/**
*	@return	A random point within the bounds of 'bounds'.
**/
RMAPI const Vector3 QM_BBox3RandomPoint( ConstBBox3Ref bounds ) {
    return QM_Vector3LerpVector3( bounds.mins, bounds.maxs, QM_Vector3Random() );
}

/**
*	@return	True if box 'a' and box 'b' are equal. False otherwise.
**/
RMAPI const bool QM_BBox3EqualsFast( ConstBBox3Ref boxA, ConstBBox3Ref boxB ) {
    return QM_Vector3EqualsFast( boxA.mins, boxB.mins ) && QM_Vector3EqualsFast( boxA.maxs, boxB.maxs );
}
RMAPI const bool QM_BBox3EqualsEpsilon( ConstBBox3Ref boxA, ConstBBox3Ref boxB ) {
    return QM_Vector3EqualsFast( boxA.mins, boxB.mins ) && QM_Vector3EqualsFast( boxA.maxs, boxB.maxs );
}

/**
*	@return The symmetrical extents of the bbox 'bounds'.
**/
RMAPI const Vector3 QM_BBox3Symmetrical( ConstBBox3Ref bounds ) {
    return QM_Vector3Maxf( QM_Vector3Absf( bounds.mins ), QM_Vector3Absf( bounds.maxs ) );
}

/**
*	@return	A scaled version of 'bounds'.
**/
RMAPI const BBox3 QM_BBox3Scale( ConstBBox3Ref bounds, const float scale ) {
    return BBox3{
        QM_Vector3Scale( bounds.mins, scale ),
        QM_Vector3Scale( bounds.maxs, scale ),
    };
}

/**
*	@brief	"A Simple Method for Box-Sphere Intersection Testing",
*			by Jim Arvo, in "Graphics Gems", Academic Press, 1990.
*
*			This routine tests for intersection between an axis-aligned box (bbox3_t)
*			and a dimensional sphere(sphere_t). The 'testType' argument indicates whether the shapes
*			are to be regarded as plain surfaces, or plain solids.
*
*	@param	testType	Mode:  Meaning:
*
*						0      'Hollow Box' vs 'Hollow Sphere'
*						1      'Hollow Box' vs 'Solid  Sphere'
*						2      'Solid  Box' vs 'Hollow Sphere'
*						3      'Solid  Box' vs 'Solid  Sphere'
**/
//const bool bbox3_intersects_sphere( const BBox3 &boxA, const sphere_t &sphere, const int32_t testType, const float radiusDistEpsilon );
#endif // #ifdef __cplusplus



/**
*
*
*   Q2RTXPerimental: C++ Functions/Operators for the Vector3 type.
*
*
**/
#ifdef __cplusplus
/**
*   @brief  Access Vector3 members by their index instead.
*   @return Value of the indexed Vector3 component.
**/
[[nodiscard]] inline constexpr const float &BBox3::operator[]( const size_t i ) const {
    if ( i == 0 ) {
        return mins.x;
    } else if ( i == 1 ) {
        return mins.y;
    } else if ( i == 2 ) {
        return mins.z;
    } else if ( i == 3 ) {
        return maxs.x;
    } else if ( i == 4 ) {
        return maxs.y;
    } else if ( i == 5 ) {
        return maxs.z;
    } else {
        throw std::out_of_range( "i" );
    }
}
/**
*   @brief  Access Vector3 members by their index instead.
*   @return Value of the indexed Vector3 component.
**/
[[nodiscard]] inline constexpr float &BBox3::operator[]( const size_t i ) {
    if ( i == 0 ) {
        return mins.x;
    } else if ( i == 1 ) {
        return mins.y;
    } else if ( i == 2 ) {
        return mins.z;
    } else if ( i == 3 ) {
        return maxs.x;
    } else if ( i == 4 ) {
        return maxs.y;
    } else if ( i == 5 ) {
        return maxs.z;
    } else {
        throw std::out_of_range( "i" );
    }
}

#if 0
/**
*   @brief  Vector3 C++ 'Plus' operator:
**/
RMAPI const Vector3 operator+( const Vector3 & left, const Vector3 & right ) {
    return QM_Vector3Add( left, right );
}
RMAPI const Vector3 operator+( const Vector3 & left, const float &right ) {
    return QM_Vector3AddValue( left, right );
}

RMAPI const Vector3 operator+=( Vector3 & left, const Vector3 & right ) {
    return left = QM_Vector3Add( left, right );
}
RMAPI const Vector3 operator+=( Vector3 & left, const float &right ) {
    return left = QM_Vector3AddValue( left, right );
}

/**
*   @brief  Vector3 C++ 'Minus' operator:
**/
RMAPI const Vector3 operator-( const Vector3 & left, const Vector3 & right ) {
    return QM_Vector3Subtract( left, right );
}
RMAPI const Vector3 operator-( const Vector3 & left, const float &right ) {
    return QM_Vector3SubtractValue( left, right );
}
RMAPI const Vector3 operator-( const Vector3 & v ) {
    return QM_Vector3Negate( v );
}

RMAPI const Vector3 &operator-=( Vector3 & left, const Vector3 & right ) {
    return left = QM_Vector3Subtract( left, right );
}
RMAPI const Vector3 &operator-=( Vector3 & left, const float &right ) {
    return left = QM_Vector3SubtractValue( left, right );
}

/**
*   @brief  Vector3 C++ 'Multiply' operator:
**/
RMAPI const Vector3 operator*( const Vector3 & left, const Vector3 & right ) {
    return QM_Vector3Multiply( left, right );
}
RMAPI const Vector3 operator*( const Vector3 & left, const float &right ) {
    return QM_Vector3Scale( left, right );
}
// for: ConstVector3Ref v1 = floatVal * v2;
RMAPI const Vector3 operator*( const float &left, const Vector3 & right ) {
    return QM_Vector3Scale( right, left );
}

RMAPI const Vector3 &operator*=( Vector3 & left, const Vector3 & right ) {
    return left = QM_Vector3Multiply( left, right );
}
RMAPI const Vector3 &operator*=( Vector3 & left, const float &right ) {
    return left = QM_Vector3Scale( left, right );
}

/**
*   @brief  Vector3 C++ 'Divide' operator:
**/
RMAPI const Vector3 operator/( const Vector3 & left, const Vector3 & right ) {
    return QM_Vector3Divide( left, right );
}
RMAPI const Vector3 operator/( const Vector3 & left, const float &right ) {
    return QM_Vector3DivideValue( left, right );
}

RMAPI const Vector3 &operator/=( Vector3 & left, const Vector3 & right ) {
    return left = QM_Vector3Divide( left, right );
}
RMAPI const Vector3 &operator/=( Vector3 & left, const float &right ) {
    return left = QM_Vector3DivideValue( left, right );
}

/**
*   @brief  Vector3 C++ 'Equals' operator:
**/
RMAPI bool operator==( const Vector3 & left, const Vector3 & right ) {
    return QM_Vector3Equals( left, right );
}

/**
*   @brief  Vector3 C++ 'Not Equals' operator:
**/
RMAPI bool operator!=( const Vector3 & left, const Vector3 & right ) {
    return !QM_Vector3Equals( left, right );
}

/**
*   @brief  Vector3 C++ 'AngleVectors' method for QMRayLib.
**/
RMAPI void QM_AngleVectors( const Vector3 & angles, Vector3 * forward, Vector3 * right, Vector3 * up ) {
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
RMAPI const float QM_Vector3NormalizeLength( Vector3 & v ) {
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
RMAPI const Vector3 QM_Vector3AngleMod( ConstVector3Ref v ) {
    return Vector3{
        QM_AngleMod( v.x ),
        QM_AngleMod( v.y ),
        QM_AngleMod( v.z )
    };
}

/**
*   @brief  Will lerp between the euler angle, a2 and a1.
**/
RMAPI const Vector3 QM_Vector3LerpAngles( ConstVector3Ref angleVec2, ConstVector3Ref angleVec1, const float fraction ) {
    return {
        LerpAngle( angleVec2.x, angleVec1.x, fraction ),
        LerpAngle( angleVec2.y, angleVec1.y, fraction ),
        LerpAngle( angleVec2.z, angleVec1.z, fraction ),
    };
}

/**
*   @return The closest point of the box that is near vector 'in'.
**/
RMAPI const Vector3 QM_Vector3ClosestPointToBox( ConstVector3Ref in, ConstVector3Ref absmin, ConstVector3Ref absmax ) {
    Vector3 out = {};

    for ( int i = 0; i < 3; i++ ) {
        out[ i ] = ( in[ i ] < absmin[ i ] ) ? absmin[ i ] : ( in[ i ] > absmax[ i ] ) ? absmax[ i ] : in[ i ];
    }

    return out;
}
#endif
#endif  // __cplusplus