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
QM_API BBox3 QM_BBox3FromMinsMaxs( const Vector3 &mins, const Vector3 &maxs ) {
    return BBox3(
        mins,
        maxs
    );
}

/**
*	@return	The center point of the bounding box.
**/
QM_API Vector3 QM_BBox3Center( const BBox3 &bbox ) {
    return QM_Vector3Lerp( bbox.mins, bbox.maxs, .5f );
}

/**
*	@brief	Constructs a vec3_zero centered matching bounding box from the size vector.
*	@return A bbox3 containing the correct mins and maxs matching a zero center origin.
**/
QM_API BBox3 QM_BBox3FromSize( const Vector3 &size ) {
    return BBox3(
        QM_Vector3Scale( size, -0.5f ),
        QM_Vector3Scale( size, 0.5f )
    );
}
/**
*	@brief	Constructs a vec3_zero centered matching bounding box from the x,y,z values.
*	@return A bbox3 containing the correct mins and maxs matching a zero center origin.
**/
QM_API BBox3 QM_BBox3FromSize( const float x, const float y, const float z ) {
    return QM_BBox3FromSize( { x, y, z } );
}

/**
*	@return A zero sized box.
**/
QM_API BBox3 QM_BBox3Zero() {
    return BBox3(
        QM_Vector3Zero(),
        QM_Vector3Zero()
    );
}
/**
*	@brief	Constructs an INFINITY sized box which can be used to add points to (ie it scaled down to),
*			ensuring that it remains centered around its zero point.
*	@return A box sized to INFINITY.
**/
QM_API BBox3 QM_BBox3Infinity() {
    return BBox3(
        Vector3{ INFINITY, INFINITY, INFINITY },
        Vector3{ -INFINITY, -INFINITY, -INFINITY }
    );
}

/**
*	@return	A box with extended bounds if, point < mins, or point > maxs.
**/
QM_API BBox3 QM_BBox3Append( const BBox3 &bbox, const Vector3 &point ) {
    return BBox3 {
        QM_Vector3Minf( bbox.mins, point ),
        QM_Vector3Maxf( bbox.maxs, point )
    };
}
/**
*	@return	A box that 'unites' both into one.
**/
QM_API BBox3 QM_BBox3Union( const BBox3 &bboxA, const BBox3 &bboxB ) {
    return BBox3 {
        QM_Vector3Minf( bboxA.mins, bboxB.mins ),
        QM_Vector3Maxf( bboxA.maxs, bboxB.maxs ),
    };
}

/**
*	@return	A bbox constructed out of the list of points.
**/
QM_API BBox3 QM_BBox3FromPoints( const Vector3 *points, const uint32_t numberOfPoints ) {
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
QM_API_DISCARD void QM_BBox3ToPoints( const BBox3 &box, Vector3 *points ) {
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
QM_API bool QM_BBox3Intersects( const BBox3 &boxA, const BBox3 &boxB ) {
    #if 0
    if ( boxA.mins.x >= boxB.maxs.x || boxA.mins.y >= boxB.maxs.y || boxA.mins.z >= boxB.maxs.z ) {
        return false;
    }

    if ( boxA.maxs.x <= boxB.mins.x || boxA.maxs.y <= boxB.mins.y || boxA.maxs.z <= boxB.mins.z ) {
        return false;
    }
    #else
    if ( boxA.mins.x > boxB.maxs.x || boxA.mins.y > boxB.maxs.y || boxA.mins.z > boxB.maxs.z ) {
        return false;
    }

    if ( boxA.maxs.x < boxB.mins.x || boxA.maxs.y < boxB.mins.y || boxA.maxs.z < boxB.mins.z ) {
        return false;
    }
    #endif

    return true;
}

/**
 * @return A `box_t` that is the intersection of the passed bounds.
 */
QM_API BBox3 QM_BBox3Intersection( const BBox3 &a, const BBox3 &b ) {
    if ( !QM_BBox3Intersects( a, b ) ) {
        return QM_BBox3Zero();
    } else {
        return BBox3(
            QM_Vector3Maxf( a.mins, b.mins ),   // Mins.
            QM_Vector3Minf( a.maxs, b.maxs )    // Maxs.
        );
    }
}

/**
*	@brief	Returns true if 'box' contains point 'point'
**/
QM_API bool QM_BBox3ContainsPoint( const BBox3 &box, const Vector3 &point ) {
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
QM_API Vector3 QM_BBox3Size( const BBox3 &box ) {
    return box.maxs - box.mins;
}

/**
*	@return	The distance between the two corners of the box's bounds.
**/
QM_API float QM_BBox3Distance( const BBox3 &box ) {
    return QM_Vector3Distance( box.maxs, box.mins );
}

/**
*	@return	The radius of the bounds. A sphere that contains the entire box.
**/
QM_API float QM_BBox3Radius( const BBox3 &box ) {
    return QM_BBox3Distance( box ) / 2.f;
}

/**
*	@return	A bounding box based on the 'size' vec3, centered along 'center'.
**/
QM_API BBox3 QM_BBox3FromCenterSize( const Vector3 &size, const Vector3 &center = QM_Vector3Zero() ) {
    const Vector3 halfSize = QM_Vector3Scale( size, .5f );
    return BBox3(
        Vector3( center - halfSize ),
        Vector3( center + halfSize )
    );
}

/**
*	@return	A bounding box based on the 'radius', centered along 'center'.
**/
QM_API BBox3 QM_BBox3FromCenterRadius( const float radius, const Vector3 &center = QM_Vector3Zero() ) {
    const Vector3 radiusVec = { radius, radius, radius };
    return BBox3(
        Vector3( center - radiusVec ),
        Vector3( center + radiusVec )
    );
}

/**
*	@return A bounding box expanded, or shrunk(in case of negative values), on all axis.
**/
QM_API BBox3 QM_BBox3ExpandVector3( const BBox3 &bounds, const Vector3 &expansion ) {
    return BBox3(
        Vector3( bounds.mins - expansion ),
		Vector3( bounds.maxs + expansion )
    );
}
/**
*	@return A bounding box expanded, or shrunk(in case of negative values), on all axis.
**/
QM_API BBox3 QM_BBox3ExpandValue( const BBox3 &bounds, const float expansion ) {
    return QM_BBox3ExpandVector3( bounds, Vector3{ expansion, expansion, expansion } );
}
/**
*	@return A bounding box expanded, or shrunk(in case of negative values), on all axis.
**/
QM_API BBox3 QM_BBox3ExpandBBox3( const BBox3 &boundsA, const BBox3 &boundsB ) {
    return BBox3(
        boundsA.mins + boundsB.mins,
        boundsA.maxs + boundsB.maxs
    );
}

/**
*	@return	The 'point' clamped within the bounds of 'bounds'.
**/
QM_API Vector3 QM_BBox3ClampPoint( const BBox3 &bounds, const Vector3 &point ) {
    return QM_Vector3Clamp( point, bounds.mins, bounds.maxs );
}

/**
*	@return	The bounds of 'boundsB' clamped within and to the bounds of 'boundsA'.
**/
QM_API BBox3 QM_BBox3ClampBounds( const BBox3 &boundsA, const BBox3 &boundsB ) {
    return BBox3(
        QM_Vector3Clamp( boundsB.mins, boundsA.mins, boundsA.maxs ),
        QM_Vector3Clamp( boundsB.maxs, boundsA.mins, boundsA.maxs )
    );
}

/**
*	@return	A random point within the bounds of 'bounds'.
**/
QM_API Vector3 QM_BBox3RandomPoint( const BBox3 &bounds ) {
    return QM_Vector3LerpVector3( bounds.mins, bounds.maxs, QM_Vector3Random() );
}

/**
*	@return	True if box 'a' and box 'b' are equal. False otherwise.
**/
QM_API bool QM_BBox3EqualsFast( const BBox3 &boxA, const BBox3 &boxB ) {
    return QM_Vector3EqualsFast( boxA.mins, boxB.mins ) && QM_Vector3EqualsFast( boxA.maxs, boxB.maxs );
}
QM_API bool QM_BBox3EqualsEpsilon( const BBox3 &boxA, const BBox3 &boxB ) {
    return QM_Vector3EqualsFast( boxA.mins, boxB.mins ) && QM_Vector3EqualsFast( boxA.maxs, boxB.maxs );
}

/**
*	@return The symmetrical extents of the bbox 'bounds'.
**/
QM_API Vector3 QM_BBox3Symmetrical( const BBox3 &bounds ) {
    return QM_Vector3Maxf( QM_Vector3Absf( bounds.mins ), QM_Vector3Absf( bounds.maxs ) );
}

/**
*	@return	A scaled version of 'bounds'.
**/
QM_API BBox3 QM_BBox3Scale( const BBox3 &bounds, const float scale ) {
    return BBox3(
        QM_Vector3Scale( bounds.mins, scale ),
        QM_Vector3Scale( bounds.maxs, scale )
    );
}
#endif // #ifdef __cplusplus



/**
*
*
*   Q2RTXPerimental: C++ Functions/Operators for the BBox3 type.
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

#endif  // __cplusplus