/********************************************************************
*
*
*    ServerGame: Skeletal Hitbox Trace Refinement.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "svgame/svg_skeletal_hitboxes.h"
#include "svgame/svg_utils.h"

#include <algorithm>
#include <cmath>
#include <cstring>

//! Optional visual debug draw for skeletal hitbox refinement.
static cvar_t *s_svg_skeletal_hitboxes_debug_draw = nullptr;

/**
*   @brief  Register optional cvars used by skeletal hitbox debug paths.
**/
static void SVG_SKM_InitDebugCvars() {
    if ( !s_svg_skeletal_hitboxes_debug_draw ) {
        s_svg_skeletal_hitboxes_debug_draw = gi.cvar( "svg_skeletal_hitboxes_debug_draw", "0", 0 );
    }
}

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
*   @brief  Draw one skeletal hitbox in world space as a wireframe box.
**/
static void SVG_SKM_DebugDrawHitboxWorld( const vec3_t localMins, const vec3_t localMaxs, const float *boneLocalMatrix,
    const Vector3 &entityOrigin, const Vector3 &entityForward, const Vector3 &entityRight, const Vector3 &entityUp ) {
    const Vector3 boxCornersBoneSpace[ 8 ] = {
        { localMins[ 0 ], localMins[ 1 ], localMins[ 2 ] },
        { localMaxs[ 0 ], localMins[ 1 ], localMins[ 2 ] },
        { localMaxs[ 0 ], localMaxs[ 1 ], localMins[ 2 ] },
        { localMins[ 0 ], localMaxs[ 1 ], localMins[ 2 ] },
        { localMins[ 0 ], localMins[ 1 ], localMaxs[ 2 ] },
        { localMaxs[ 0 ], localMins[ 1 ], localMaxs[ 2 ] },
        { localMaxs[ 0 ], localMaxs[ 1 ], localMaxs[ 2 ] },
        { localMins[ 0 ], localMaxs[ 1 ], localMaxs[ 2 ] }
    };

    Vector3 boxCornersWorldSpace[ 8 ] = { };
    for ( int32_t i = 0; i < 8; i++ ) {
        const Vector3 cornerEntitySpace = SVG_SKM_TransformPoint3x4( boneLocalMatrix, boxCornersBoneSpace[ i ] );
        boxCornersWorldSpace[ i ] = SVG_SKM_EntityToWorldPoint( cornerEntitySpace, entityOrigin, entityForward, entityRight, entityUp );
    }

    static constexpr int32_t edgePairs[ 12 ][ 2 ] = {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };

    for ( int32_t edgeIndex = 0; edgeIndex < 12; edgeIndex++ ) {
        const Vector3 &start = boxCornersWorldSpace[ edgePairs[ edgeIndex ][ 0 ] ];
        const Vector3 &end = boxCornersWorldSpace[ edgePairs[ edgeIndex ][ 1 ] ];
        SVG_DebugDrawLine_TE( start, end, MULTICAST_PVS, false );
    }
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
*   @brief  Compute player bone local matrices from server-side animation mixer state.
*   @return True when a valid player pose was built.
**/
static bool SVG_SKM_ComputePlayerBoneLocalMatrices( const model_t *modelData, const svg_base_edict_t *target, float outBoneLocalMatrices[ SKM_MAX_BONES ][ 12 ] ) {
    if ( !modelData || !modelData->skmData || !modelData->skmConfig || !target || !target->client ) {
        return false;
    }

    const skm_model_t *skmData = modelData->skmData;
    if ( !skmData->poses || skmData->num_poses <= 0 || skmData->num_joints <= 0 ) {
        return false;
    }

    const sg_skm_animation_mixer_t &animationMixer = target->client->animationMixer;

    skm_transform_t finalPose[ SKM_MAX_BONES ] = { };
    skm_transform_t currentLowerPose[ SKM_MAX_BONES ] = { };
    skm_transform_t lastLowerPose[ SKM_MAX_BONES ] = { };
    skm_transform_t eventLowerPose[ SKM_MAX_BONES ] = { };
    skm_transform_t eventUpperPose[ SKM_MAX_BONES ] = { };

    auto getLerpedPoseForState = [ & ]( const sg_skm_animation_state_t &inputState, const int32_t rootMotionAxisFlags, skm_transform_t *outPose ) -> bool {
        sg_skm_animation_state_t state = inputState;
        int32_t oldFrame = 0;
        int32_t currentFrame = 0;
        double backLerp = 0.0;

        const bool finishedOrInvalid = SG_SKM_ProcessAnimationStateForTime( modelData, &state, level.time, &oldFrame, &currentFrame, &backLerp );
        if ( currentFrame < 0 || oldFrame < 0
            || currentFrame >= static_cast<int32_t>( skmData->num_frames )
            || oldFrame >= static_cast<int32_t>( skmData->num_frames ) ) {
            return false;
        }

        const float clampedBackLerp = static_cast<float>( std::clamp( backLerp, 0.0, 1.0 ) );
        const float frontLerp = 1.0f - clampedBackLerp;
        gi.SKM_ComputeLerpBonePoses( modelData, currentFrame, oldFrame, frontLerp, clampedBackLerp, outPose, 0, rootMotionAxisFlags );

        return !finishedOrInvalid;
    };

    const bool hasCurrentLowerPose = getLerpedPoseForState( animationMixer.currentBodyStates[ SKM_BODY_LOWER ], SKM_POSE_TRANSLATE_ALL, currentLowerPose );
    if ( !hasCurrentLowerPose ) {
        return false;
    }

    std::memcpy( finalPose, currentLowerPose, sizeof( finalPose ) );

    const bool hasLastLowerPose = getLerpedPoseForState( animationMixer.lastBodyStates[ SKM_BODY_LOWER ], SKM_POSE_TRANSLATE_ALL, lastLowerPose );
    const skm_bone_node_t *hipsBone = ( modelData->skmConfig->rootBones.hip ? modelData->skmConfig->rootBones.hip : modelData->skmConfig->boneTree );
    if ( hasLastLowerPose && hipsBone ) {
        const QMTime blendStart = animationMixer.currentBodyStates[ SKM_BODY_LOWER ].timeStart;
        const QMTime blendEnd = blendStart + animationMixer.lastBodyStates[ SKM_BODY_LOWER ].timeDuration;
        const double blendRange = static_cast<double>( blendEnd.Milliseconds() - blendStart.Milliseconds() );
        const double blendElapsed = static_cast<double>( level.time.Milliseconds() - blendStart.Milliseconds() );
        const double blendScale = ( blendRange > 0.0 ? std::clamp( blendElapsed / blendRange, 0.0, 1.0 ) : 1.0 );
        gi.SKM_RecursiveBlendFromBone( finalPose, lastLowerPose, hipsBone, nullptr, 0, blendScale, blendScale );
    }

    const bool hasEventLowerPose = getLerpedPoseForState( animationMixer.eventBodyState[ SKM_BODY_LOWER ], SKM_POSE_TRANSLATE_Z, eventLowerPose );
    if ( hasEventLowerPose && hipsBone ) {
        gi.SKM_RecursiveBlendFromBone( eventLowerPose, finalPose, hipsBone, nullptr, 0, 1.0, 1.0 );
    }

    const bool hasEventUpperPose = getLerpedPoseForState( animationMixer.eventBodyState[ SKM_BODY_UPPER ], SKM_POSE_TRANSLATE_ALL, eventUpperPose );
    const skm_bone_node_t *torsoBone = modelData->skmConfig->rootBones.torso;
    if ( hasEventUpperPose && torsoBone ) {
        gi.SKM_RecursiveBlendFromBone( eventUpperPose, finalPose, torsoBone, nullptr, 0, 1.0, 1.0 );
    }

    gi.SKM_TransformBonePosesLocalSpace( skmData, finalPose, nullptr, &outBoneLocalMatrices[ 0 ][ 0 ] );
    return true;
}

/**
*   @brief  Refine a coarse point trace against IQM skeletal hitboxes for the target entity.
**/
bool SVG_SkeletalHitboxes_RefinePointTrace( svg_trace_t &trace, const Vector3 &shotStart, const Vector3 &shotEnd, const svg_base_edict_t *target ) {
    SVG_SKM_InitDebugCvars();

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
    *   Compute local-space per-bone pose matrices.
    *   Players use animation mixer composition, while non-players use direct frame poses.
    **/
    float boneLocalMatrices[ SKM_MAX_BONES ][ 12 ] = { { 0.0f } };
    bool hasPoseMatrices = false;

    if ( target->client ) {
        hasPoseMatrices = SVG_SKM_ComputePlayerBoneLocalMatrices( modelData, target, boneLocalMatrices );
    }

    if ( !hasPoseMatrices ) {
        const int32_t modelFrame = target->s.frame;
        skm_transform_t relativeBonePoses[ SKM_MAX_BONES ] = { };
        gi.SKM_ComputeLerpBonePoses( modelData, modelFrame, modelFrame, 1.0f, 0.0f, relativeBonePoses, 0, SKM_POSE_TRANSLATE_ALL );
        gi.SKM_TransformBonePosesLocalSpace( skmData, relativeBonePoses, nullptr, &boneLocalMatrices[ 0 ][ 0 ] );
    }

    /**
    *   Iterate all hitboxes and keep the nearest valid hit.
    **/
    bool foundHit = false;
    double bestHitT = 1.0;
    int32_t bestHitBodyID = -1;
    int32_t bestHitboxIndex = -1;
    Vector3 bestWorldNormal = { 0.0f, 0.0f, 1.0f };

    const int32_t debugDrawMode = ( s_svg_skeletal_hitboxes_debug_draw ? s_svg_skeletal_hitboxes_debug_draw->integer : 0 );

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

        if ( debugDrawMode >= 2 ) {
            SVG_SKM_DebugDrawHitboxWorld( hitbox.localMins, hitbox.localMaxs, boneLocalMatrix,
                target->currentOrigin, entityForward, entityRight, entityUp );
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
            bestHitboxIndex = static_cast<int32_t>( hitboxIndex );

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

    if ( debugDrawMode >= 1 && bestHitboxIndex >= 0 && bestHitboxIndex < static_cast<int32_t>( skmData->num_hitboxes ) ) {
        const skm_hitbox_t &bestHitbox = skmData->hitboxes[ bestHitboxIndex ];
        if ( bestHitbox.boneIndex >= 0 && bestHitbox.boneIndex < static_cast<int32_t>( skmData->num_joints ) ) {
            SVG_SKM_DebugDrawHitboxWorld( bestHitbox.localMins, bestHitbox.localMaxs,
                boneLocalMatrices[ bestHitbox.boneIndex ], target->currentOrigin, entityForward, entityRight, entityUp );
        }
    }

    return true;
}
