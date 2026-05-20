/********************************************************************
*
*
*    ServerGame: Skeletal Hitbox Trace Refinement.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "sharedgame/sg_skm.h"
#include "svgame/svg_skeletal_hitboxes.h"

#include <algorithm>
#include <cmath>

/**
*   @brief  Transform a world-space point into entity-local model space.
**/
static Vector3 SVG_SKM_WorldToEntityPoint( const Vector3 &worldPoint, const Vector3 &entityOrigin, const Vector3 &entityForward, const Vector3 &entityRight, const Vector3 &entityUp ) {
    const Vector3 delta = worldPoint - entityOrigin;
    return {
        QM_Vector3DotProduct( delta, entityForward ),
        QM_Vector3DotProduct( delta, entityRight ),
        QM_Vector3DotProduct( delta, entityUp )
    };
}

/**
*   @brief  Transform an entity-local model-space point into world space.
**/
static Vector3 SVG_SKM_EntityToWorldPoint( const Vector3 &entityPoint, const Vector3 &entityOrigin, const Vector3 &entityForward, const Vector3 &entityRight, const Vector3 &entityUp ) {
    return entityOrigin + ( entityForward * entityPoint.x ) + ( entityRight * entityPoint.y ) + ( entityUp * entityPoint.z );
}

/**
*   @brief  Transform an entity-local direction vector into world space.
**/
static Vector3 SVG_SKM_EntityToWorldDirection( const Vector3 &entityDirection, const Vector3 &entityForward, const Vector3 &entityRight, const Vector3 &entityUp ) {
    return ( entityForward * entityDirection.x ) + ( entityRight * entityDirection.y ) + ( entityUp * entityDirection.z );
}

/**
*   @brief  Transform a point by a row-major 3x4 affine matrix.
**/
static Vector3 SVG_SKM_TransformPoint3x4( const float *matrix3x4, const Vector3 &point ) {
    return {
        matrix3x4[ 0 ] * point.x + matrix3x4[ 1 ] * point.y + matrix3x4[ 2 ] * point.z + matrix3x4[ 3 ],
        matrix3x4[ 4 ] * point.x + matrix3x4[ 5 ] * point.y + matrix3x4[ 6 ] * point.z + matrix3x4[ 7 ],
        matrix3x4[ 8 ] * point.x + matrix3x4[ 9 ] * point.y + matrix3x4[ 10 ] * point.z + matrix3x4[ 11 ]
    };
}

/**
*   @brief  Transform a direction by the 3x3 part of a row-major 3x4 affine matrix.
**/
static Vector3 SVG_SKM_TransformDirection3x3( const float *matrix3x4, const Vector3 &direction ) {
    return {
        matrix3x4[ 0 ] * direction.x + matrix3x4[ 1 ] * direction.y + matrix3x4[ 2 ] * direction.z,
        matrix3x4[ 4 ] * direction.x + matrix3x4[ 5 ] * direction.y + matrix3x4[ 6 ] * direction.z,
        matrix3x4[ 8 ] * direction.x + matrix3x4[ 9 ] * direction.y + matrix3x4[ 10 ] * direction.z
    };
}

/**
*   @brief  Invert a row-major affine 3x4 matrix.
*   @return True when the matrix was invertible.
**/
static bool SVG_SKM_InvertAffine3x4( const float *inMatrix, float *outMatrix ) {
    const float m00 = inMatrix[ 0 ], m01 = inMatrix[ 1 ], m02 = inMatrix[ 2 ];
    const float m10 = inMatrix[ 4 ], m11 = inMatrix[ 5 ], m12 = inMatrix[ 6 ];
    const float m20 = inMatrix[ 8 ], m21 = inMatrix[ 9 ], m22 = inMatrix[ 10 ];

    const float c00 = ( m11 * m22 ) - ( m12 * m21 );
    const float c01 = ( m02 * m21 ) - ( m01 * m22 );
    const float c02 = ( m01 * m12 ) - ( m02 * m11 );
    const float c10 = ( m12 * m20 ) - ( m10 * m22 );
    const float c11 = ( m00 * m22 ) - ( m02 * m20 );
    const float c12 = ( m02 * m10 ) - ( m00 * m12 );
    const float c20 = ( m10 * m21 ) - ( m11 * m20 );
    const float c21 = ( m01 * m20 ) - ( m00 * m21 );
    const float c22 = ( m00 * m11 ) - ( m01 * m10 );

    const float determinant = ( m00 * c00 ) + ( m01 * c10 ) + ( m02 * c20 );
    if ( std::fabs( determinant ) <= 0.000001f ) {
        return false;
    }

    const float invDet = 1.0f / determinant;
    outMatrix[ 0 ] = c00 * invDet;
    outMatrix[ 1 ] = c01 * invDet;
    outMatrix[ 2 ] = c02 * invDet;
    outMatrix[ 4 ] = c10 * invDet;
    outMatrix[ 5 ] = c11 * invDet;
    outMatrix[ 6 ] = c12 * invDet;
    outMatrix[ 8 ] = c20 * invDet;
    outMatrix[ 9 ] = c21 * invDet;
    outMatrix[ 10 ] = c22 * invDet;

    const Vector3 translation = { inMatrix[ 3 ], inMatrix[ 7 ], inMatrix[ 11 ] };
    outMatrix[ 3 ] = -( outMatrix[ 0 ] * translation.x + outMatrix[ 1 ] * translation.y + outMatrix[ 2 ] * translation.z );
    outMatrix[ 7 ] = -( outMatrix[ 4 ] * translation.x + outMatrix[ 5 ] * translation.y + outMatrix[ 6 ] * translation.z );
    outMatrix[ 11 ] = -( outMatrix[ 8 ] * translation.x + outMatrix[ 9 ] * translation.y + outMatrix[ 10 ] * translation.z );

    return true;
}

/**
*   @brief  Segment vs AABB test in local space.
*   @return True on intersection, returning nearest hit parametric t and local-space normal.
**/
static bool SVG_SKM_SegmentAABBIntersect( const Vector3 &segmentStart, const Vector3 &segmentEnd, const vec3_t mins, const vec3_t maxs, double *outHitT, Vector3 *outHitNormal ) {
    static constexpr float epsilon = 0.000001f;

    const Vector3 segmentDirection = segmentEnd - segmentStart;

    double entryT = 0.0;
    double exitT = 1.0;
    Vector3 entryNormal = { 0.0f, 0.0f, 0.0f };
    Vector3 exitNormal = { 0.0f, 0.0f, 0.0f };

    for ( int32_t axis = 0; axis < 3; axis++ ) {
        const double startComponent = segmentStart[ axis ];
        const double directionComponent = segmentDirection[ axis ];

        if ( std::fabs( directionComponent ) <= epsilon ) {
            if ( startComponent < mins[ axis ] || startComponent > maxs[ axis ] ) {
                return false;
            }
            continue;
        }

        double t1 = ( mins[ axis ] - startComponent ) / directionComponent;
        double t2 = ( maxs[ axis ] - startComponent ) / directionComponent;

        Vector3 n1 = { 0.0f, 0.0f, 0.0f };
        Vector3 n2 = { 0.0f, 0.0f, 0.0f };
        n1[ axis ] = -1.0f;
        n2[ axis ] = 1.0f;

        if ( t1 > t2 ) {
            std::swap( t1, t2 );
            std::swap( n1, n2 );
        }

        if ( t1 > entryT ) {
            entryT = t1;
            entryNormal = n1;
        }

        if ( t2 < exitT ) {
            exitT = t2;
            exitNormal = n2;
        }

        if ( entryT > exitT ) {
            return false;
        }
    }

    if ( exitT < 0.0 || entryT > 1.0 ) {
        return false;
    }

    double finalHitT = entryT;
    Vector3 finalHitNormal = entryNormal;

    if ( finalHitT < 0.0 ) {
        finalHitT = exitT;
        finalHitNormal = exitNormal;
    }

    if ( finalHitT < 0.0 || finalHitT > 1.0 ) {
        return false;
    }

    *outHitT = finalHitT;
    *outHitNormal = finalHitNormal;
    return true;
}

/**
*   @brief  Resolve model data for an entity from modelindex or model path.
**/
static const model_t *SVG_SKM_GetEntityModelData( const svg_base_edict_t *target ) {
    const qhandle_t modelHandle = target->s.modelindex;
    if ( modelHandle > 0 && gi.GetModelDataForHandle ) {
        if ( const model_t *modelData = gi.GetModelDataForHandle( modelHandle ) ) {
            return modelData;
        }
    }

    if ( gi.GetModelDataForName && target->model ) {
        return gi.GetModelDataForName( target->model );
    }

    return nullptr;
}

/**
*   @brief  Refine a coarse point trace against IQM skeletal hitboxes for the target entity.
**/
bool SVG_SkeletalHitboxes_RefinePointTrace( svg_trace_t &trace, const Vector3 &shotStart, const Vector3 &shotEnd, const svg_base_edict_t *target ) {
    /**
    *   Validate target and current coarse trace state before refining.
    **/
    if ( !target || trace.fraction >= 1.0 || trace.ent != target ) {
        return false;
    }

    /**
    *   Resolve model data and ensure skeletal hitboxes are available.
    **/
    const model_t *modelData = SVG_SKM_GetEntityModelData( target );
    if ( !modelData || !modelData->skmData || modelData->skmData->num_hitboxes == 0 ) {
        return false;
    }

    const skm_model_t *skmData = modelData->skmData;
    if ( !skmData->poses || skmData->num_poses <= 0 || skmData->num_joints <= 0 ) {
        return false;
    }

    /**
    *   Build entity basis vectors used for world/entity transforms.
    **/
    Vector3 entityForward = { 0.0f, 0.0f, 0.0f };
    Vector3 entityRight = { 0.0f, 0.0f, 0.0f };
    Vector3 entityUp = { 0.0f, 0.0f, 0.0f };
    QM_AngleVectors( target->currentAngles, &entityForward, &entityRight, &entityUp );

    const Vector3 localSegmentStart = SVG_SKM_WorldToEntityPoint( shotStart, target->currentOrigin, entityForward, entityRight, entityUp );
    const Vector3 localSegmentEnd = SVG_SKM_WorldToEntityPoint( shotEnd, target->currentOrigin, entityForward, entityRight, entityUp );

    /**
    *   Compute local-space per-bone pose matrices for the entity's current frame.
    **/
    const int32_t modelFrame = target->s.frame;
    skm_transform_t relativeBonePoses[ SKM_MAX_BONES ] = { };
    float boneLocalMatrices[ SKM_MAX_BONES ][ 12 ] = { { 0.0f } };

	SKM_ComputeLerpBonePoses( modelData, modelFrame, modelFrame, 1.0f, 0.0f, relativeBonePoses, 0, SKM_POSE_TRANSLATE_ALL );
	SKM_TransformBonePosesLocalSpace( skmData, relativeBonePoses, &boneLocalMatrices[ 0 ][ 0 ] );

    /**
    *   Iterate all hitboxes and keep the nearest valid hit.
    **/
    bool foundHit = false;
    double bestHitT = 1.0;
    int32_t bestHitBodyID = -1;
    Vector3 bestWorldNormal = { 0.0f, 0.0f, 1.0f };

    for ( uint32_t hitboxIndex = 0; hitboxIndex < skmData->num_hitboxes; hitboxIndex++ ) {
        const skm_hitbox_t &hitbox = skmData->hitboxes[ hitboxIndex ];
        if ( hitbox.boneIndex < 0 || hitbox.boneIndex >= ( int32_t )skmData->num_joints ) {
            continue;
        }

        const float *boneLocalMatrix = boneLocalMatrices[ hitbox.boneIndex ];
        float inverseBoneLocalMatrix[ 12 ] = { 0.0f };

        if ( !SVG_SKM_InvertAffine3x4( boneLocalMatrix, inverseBoneLocalMatrix ) ) {
            continue;
        }

        const Vector3 boneSpaceStart = SVG_SKM_TransformPoint3x4( inverseBoneLocalMatrix, localSegmentStart );
        const Vector3 boneSpaceEnd = SVG_SKM_TransformPoint3x4( inverseBoneLocalMatrix, localSegmentEnd );

        double hitT = 0.0;
        Vector3 hitNormalBoneSpace = { 0.0f, 0.0f, 0.0f };

        if ( !SVG_SKM_SegmentAABBIntersect( boneSpaceStart, boneSpaceEnd, hitbox.localMins, hitbox.localMaxs, &hitT, &hitNormalBoneSpace ) ) {
            continue;
        }

        if ( !foundHit || hitT < bestHitT ) {
            foundHit = true;
            bestHitT = hitT;
            bestHitBodyID = hitbox.hitBodyID;

            const Vector3 normalEntitySpace = QM_Vector3Normalize( SVG_SKM_TransformDirection3x3( boneLocalMatrix, hitNormalBoneSpace ) );
            bestWorldNormal = QM_Vector3Normalize( SVG_SKM_EntityToWorldDirection( normalEntitySpace, entityForward, entityRight, entityUp ) );
        }
    }

    if ( !foundHit ) {
        return false;
    }

    /**
    *   Apply refined hit result back to trace output.
    **/
    Vector3 shotDelta = shotEnd - shotStart;
    VectorMA( shotStart, bestHitT, shotDelta, trace.endpos );
    trace.fraction = bestHitT;
    trace.hitBodyID = bestHitBodyID;
    VectorCopy( bestWorldNormal, trace.plane.normal );

    return true;
}
