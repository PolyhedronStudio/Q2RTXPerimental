/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "svgame/svg_local.h"
#include "sharedgame/sg_skm.h"
#include "svgame/svg_skeletal_hitboxes.h"

#include <algorithm>
#include <cmath>
#include <limits>



/**
*   @brief  Resolve the server-side model resource for a skeletal hitbox query.
*   @param  target  Entity whose model should be queried.
*   @return Model resource when available, otherwise nullptr.
*   @note   We prefer the model handle because it is the canonical loaded asset,
*           but fall back to the model name when the handle path is unavailable.
**/
static const model_t *SVG_SKM_GetEntityModelData( const svg_base_edict_t *target ) {
	// Sanity: we need a target entity and the engine lookup callbacks to do anything useful.
	if ( !target ) {
		return nullptr;
	}

	if ( gi.GetModelDataForHandle && target->s.modelindex > 0 ) {
		if ( const model_t *modelData = gi.GetModelDataForHandle( target->s.modelindex ) ) {
			return modelData;
		}
	}

	if ( gi.GetModelDataForName && target->model ) {
		return gi.GetModelDataForName( target->model );
	}

	return nullptr;
}

/**
*   @brief  Check whether an entity has valid skeletal hitbox data for refinement.
*   @param  target  Entity to inspect.
*   @return True when skeletal refinement data is present and usable.
**/
bool SVG_SkeletalHitboxes_HasRefinableData( const svg_base_edict_t *target ) {
	// Require a valid target and resolved model.
	if ( !target ) {
		return false;
	}

	const model_t *modelData = SVG_SKM_GetEntityModelData( target );
	if ( !modelData || !modelData->skmData ) {
		return false;
	}

	const skm_model_t *skmData = modelData->skmData;
	if ( !skmData->hitboxes || skmData->num_hitboxes == 0 || skmData->num_joints == 0 || !skmData->poses || skmData->num_poses == 0 ) {
		return false;
	}

	const int32_t frameCount = static_cast<int32_t>( skmData->num_frames );
	if ( frameCount <= 0 ) {
		return false;
	}

	return true;
}

/**
*   @brief  Transform a world-space point into entity-local model space.
*   @note   This uses the entity angle basis so the refinement follows the same world-facing pose as rendering.
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
*   @brief  Resolve the model-space origin used for skeletal hitbox queries.
*   @param  target Entity being tested.
*   @return World-space model origin aligned with the model/root space used by hitboxes.
*   @note   Player skeletal roots are offset relative to gameplay origin by mins.z. Without this
*           offset, server hit tests appear shifted upward by roughly half player height.
**/
static Vector3 SVG_SKM_GetModelOriginForHitboxTrace( const svg_base_edict_t *target ) {
	if ( !target ) {
		return {};
	}

	Vector3 modelOrigin = target->currentOrigin;

	if ( target && target->client ) {
		modelOrigin[ 2 ] += target->mins[ 2 ];
	}

	return modelOrigin;
}

/**
*   @brief  Transform a point by a row-major affine 3x4 matrix.
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
*   @note   The bone pose matrices are affine, so a dedicated inverse is cheaper than a general-purpose matrix routine.
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

	// Reject shots that started inside the AABB. A bullet trace must enter a hitbox
	// from the outside - start-inside cases arise after retracing past an entity and
	// produce false-positive hits on large auto-generated bounds.
	if ( entryT < 0.0 ) {
		return false;
	}

	*outHitT = entryT;
	*outHitNormal = entryNormal;
	return true;
}

/**
*   @brief  Refine a coarse point trace against IQM skeletal hitboxes for the target entity.
*   @param  trace       In/out coarse trace result. Updated when a skeletal hitbox is hit.
*   @param  shotStart   World-space trace start.
*   @param  shotEnd     World-space trace end.
*   @param  target      Entity whose skeletal hitboxes should be tested.
*   @return True when a skeletal hitbox hit was found and trace was refined.
*   @note   This keeps the coarse world trace intact when no body hit is found, while using the
*           current animation pose so the refinement matches the visible monster state.
**/
bool SVG_SkeletalHitboxes_RefinePointTrace( svg_trace_t &trace, const Vector3 &shotStart, const Vector3 &shotEnd, const svg_base_edict_t *target ) {
	/**
	*   Sanity checks / model resolution.
	**/
	if ( !target ) {
		return false;
	}

	const model_t *modelData = SVG_SKM_GetEntityModelData( target );
	if ( !modelData || !modelData->skmData ) {
		return false;
	}

	const skm_model_t *skmData = modelData->skmData;
	if ( skmData->num_hitboxes == 0 || skmData->num_joints == 0 || !skmData->poses || skmData->num_poses == 0 ) {
		return false;
	}

	const int32_t frameCount = static_cast<int32_t>( skmData->num_frames );
	if ( frameCount <= 0 ) {
		return false;
	}

	/**
	*   Recreate the current visible pose from the server entity state.
	*   Recent research note: using the same frame/oldframe/backlerp path as the renderer keeps
	*   hit refinement aligned with the animation that the player actually sees.
	**/
	Vector3 entityForward = { 0.0f, 0.0f, 0.0f };
	Vector3 entityRight = { 0.0f, 0.0f, 0.0f };
	Vector3 entityUp = { 0.0f, 0.0f, 0.0f };
	QM_AngleVectors( target->currentAngles, &entityForward, &entityRight, &entityUp );
	// Match renderer AnglesToAxis convention: axis[1] is inverted right vector.
	entityRight = -entityRight;
	const Vector3 modelOrigin = SVG_SKM_GetModelOriginForHitboxTrace( target );

	const Vector3 localShotStart = SVG_SKM_WorldToEntityPoint( shotStart, modelOrigin, entityForward, entityRight, entityUp );
	const Vector3 localShotEnd = SVG_SKM_WorldToEntityPoint( shotEnd, modelOrigin, entityForward, entityRight, entityUp );

	const int32_t rawCurrentFrame = target->s.frame;
	const int32_t rawOldFrame = target->s.old_frame;
	const int32_t currentFrame = std::clamp( rawCurrentFrame, 0, frameCount - 1 );
	const int32_t oldFrame = std::clamp( rawOldFrame, 0, frameCount - 1 );

	static thread_local skm_transform_t currentRelativeBonePoses[ SKM_MAX_BONES ] = {};
	static thread_local skm_transform_t oldRelativeBonePoses[ SKM_MAX_BONES ] = {};
	static thread_local float currentBoneLocalMatrices[ SKM_MAX_BONES ][ 12 ] = {};
	static thread_local float oldBoneLocalMatrices[ SKM_MAX_BONES ][ 12 ] = {};
	std::fill( std::begin( currentRelativeBonePoses ), std::end( currentRelativeBonePoses ), skm_transform_t{} );
	std::fill( std::begin( oldRelativeBonePoses ), std::end( oldRelativeBonePoses ), skm_transform_t{} );
	std::fill_n( &currentBoneLocalMatrices[ 0 ][ 0 ], SKM_MAX_BONES * 12, 0.0f );
	std::fill_n( &oldBoneLocalMatrices[ 0 ][ 0 ], SKM_MAX_BONES * 12, 0.0f );

	SG_SKM_ComputeLerpBonePoses( modelData, currentFrame, currentFrame, 1.0f, 0.0f, currentRelativeBonePoses, 0, SKM_POSE_TRANSLATE_ALL );
	SG_SKM_TransformBonePosesLocalSpace( skmData, currentRelativeBonePoses, &currentBoneLocalMatrices[ 0 ][ 0 ] );

	if ( oldFrame != currentFrame ) {
		SG_SKM_ComputeLerpBonePoses( modelData, oldFrame, oldFrame, 1.0f, 0.0f, oldRelativeBonePoses, 0, SKM_POSE_TRANSLATE_ALL );
		SG_SKM_TransformBonePosesLocalSpace( skmData, oldRelativeBonePoses, &oldBoneLocalMatrices[ 0 ][ 0 ] );
	}

	/**
	*   Iterate hitboxes and retain the nearest body hit.
	**/
	bool foundHit = false;
	double bestHitT = 1.0;
	int32_t bestHitBodyID = -1;
	Vector3 bestWorldNormal = { 0.0f, 0.0f, 1.0f };

	auto TestHitboxesForPoseSet = [ & ]( const float boneLocalMatrices[ SKM_MAX_BONES ][ 12 ] ) {
		for ( uint32_t hitboxIndex = 0; hitboxIndex < skmData->num_hitboxes; hitboxIndex++ ) {
			const skm_hitbox_t &hitbox = skmData->hitboxes[ hitboxIndex ];
			if ( hitbox.boneIndex < 0 || hitbox.boneIndex >= static_cast<int32_t>( skmData->num_joints ) ) {
				continue;
			}

			float inverseBoneLocalMatrix[ 12 ] = {};
			if ( !SVG_SKM_InvertAffine3x4( boneLocalMatrices[ hitbox.boneIndex ], inverseBoneLocalMatrix ) ) {
				continue;
			}

			const Vector3 boneSpaceStart = SVG_SKM_TransformPoint3x4( inverseBoneLocalMatrix, localShotStart );
			const Vector3 boneSpaceEnd = SVG_SKM_TransformPoint3x4( inverseBoneLocalMatrix, localShotEnd );

			// Auto-generated hitboxes accumulate all vertices where a bone dominates and can
			// produce bounds substantially larger than the visible geometry (e.g. a spine bone
			// that influences shoulder vertices ends up nearly as wide as the full entity bbox).
			// Apply a proportional per-axis inset - 20 % of each half-extent - so large AABBs
			// are shrunk meaningfully while small arm/hand AABBs lose almost nothing.
			static constexpr float autoGenInsetFraction = 0.20f;
			static constexpr float autoGenInsetMin = 0.5f;
			static constexpr float autoGenInsetMax = 5.0f;
			float testMins[ 3 ];
			float testMaxs[ 3 ];
			if ( hitbox.flags & SKM_HITBOX_FLAG_AUTO_GENERATED ) {
				for ( int32_t axis = 0; axis < 3; axis++ ) {
					// Proportional inset: 20 % of this axis's half-extent, clamped.
					const float halfExtent = ( hitbox.localMaxs[ axis ] - hitbox.localMins[ axis ] ) * 0.5f;
					const float inset = std::max( autoGenInsetMin, std::min( autoGenInsetMax, halfExtent * autoGenInsetFraction ) );
					testMins[ axis ] = hitbox.localMins[ axis ] + inset;
					testMaxs[ axis ] = hitbox.localMaxs[ axis ] - inset;
				}
			} else {
				for ( int32_t axis = 0; axis < 3; axis++ ) {
					testMins[ axis ] = hitbox.localMins[ axis ];
					testMaxs[ axis ] = hitbox.localMaxs[ axis ];
				}
			}

			double hitT = 0.0;
			Vector3 hitNormalBoneSpace = { 0.0f, 0.0f, 0.0f };
			if ( !SVG_SKM_SegmentAABBIntersect( boneSpaceStart, boneSpaceEnd, testMins, testMaxs, &hitT, &hitNormalBoneSpace ) ) {
				continue;
			}

			if ( !foundHit || hitT < bestHitT ) {
				foundHit = true;
				bestHitT = hitT;
				bestHitBodyID = hitbox.hitBodyID;

				// Convert the normal back out of bone space so downstream impact logic still has a useful direction.
				const Vector3 modelNormal = SVG_SKM_TransformDirection3x3( boneLocalMatrices[ hitbox.boneIndex ], hitNormalBoneSpace );
				const Vector3 worldNormal = ( entityForward * modelNormal.x ) + ( entityRight * modelNormal.y ) + ( entityUp * modelNormal.z );
				const double worldNormalLength = std::sqrt( QM_Vector3DotProduct( worldNormal, worldNormal ) );
				if ( worldNormalLength > 0.000001 ) {
					bestWorldNormal = worldNormal * static_cast<float>( 1.0 / worldNormalLength );
				}
			}
		}
	};

	// Current frame is authoritative for hit registration to match server-side frame state.
	TestHitboxesForPoseSet( currentBoneLocalMatrices );

	// Old frame is only a fallback when current-frame hitboxes miss during animation transitions.
	if ( !foundHit && oldFrame != currentFrame ) {
		TestHitboxesForPoseSet( oldBoneLocalMatrices );
	}

	if ( !foundHit ) {
		return false;
	}

	/**
	*   Commit the refined hit to the caller's trace.
	**/
	trace.entityNumber = target->s.number;
	trace.brushID = SG_PackEntityBrushID( target->s.number, trace.brushID );
	trace.hitBodyID = bestHitBodyID;
	trace.fraction = bestHitT;
	trace.endpos = shotStart + ( shotEnd - shotStart ) * static_cast<float>( bestHitT );
	trace.plane.normal[ 0 ] = bestWorldNormal.x;
	trace.plane.normal[ 1 ] = bestWorldNormal.y;
	trace.plane.normal[ 2 ] = bestWorldNormal.z;
	trace.plane.dist = QM_Vector3DotProduct( bestWorldNormal, trace.endpos );
	trace.plane.type = PLANE_NON_AXIAL;
	trace.plane.signbits = 0;
	trace.ent = const_cast<svg_base_edict_t *>( target );

	return true;
}

/**
*   @brief  Trace a point segment against the entity animated model bounds envelope.
*   @param  trace       In/out trace result that receives the fallback hit on success.
*   @param  shotStart   World-space trace start.
*   @param  shotEnd     World-space trace end.
*   @param  target      Entity whose animated frame bounds should be tested.
*   @return True when the segment intersects the animated bounds envelope.
*   @note   Uses the union of current and old frame bounds to keep the fallback conservative.
**/
bool SVG_SkeletalHitboxes_TracePointAgainstAnimatedBounds( svg_trace_t &trace, const Vector3 &shotStart, const Vector3 &shotEnd, const svg_base_edict_t *target ) {
	/**
	*   Sanity checks and model resolution.
	**/
	if ( !target ) {
		return false;
	}

	const model_t *modelData = SVG_SKM_GetEntityModelData( target );
	if ( !modelData || !modelData->skmData ) {
		return false;
	}

	const skm_model_t *skmData = modelData->skmData;
	if ( !skmData->hitboxes || skmData->num_hitboxes == 0 || skmData->num_joints == 0 || !skmData->poses || skmData->num_poses == 0 ) {
		return false;
	}

	/**
	*   Build an animated envelope from posed hitboxes so protrusions outside coarse hull are represented.
	**/
	const int32_t frameCount = static_cast<int32_t>( skmData->num_frames );
	if ( frameCount <= 0 ) {
		return false;
	}

	const int32_t rawCurrentFrame = target->s.frame;
	const int32_t rawOldFrame = target->s.old_frame;
	const int32_t currentFrame = std::clamp( rawCurrentFrame, 0, frameCount - 1 );
	const int32_t oldFrame = std::clamp( rawOldFrame, 0, frameCount - 1 );

	static thread_local skm_transform_t currentRelativeBonePoses[ SKM_MAX_BONES ] = {};
	static thread_local skm_transform_t oldRelativeBonePoses[ SKM_MAX_BONES ] = {};
	static thread_local float currentBoneLocalMatrices[ SKM_MAX_BONES ][ 12 ] = {};
	static thread_local float oldBoneLocalMatrices[ SKM_MAX_BONES ][ 12 ] = {};
	std::fill( std::begin( currentRelativeBonePoses ), std::end( currentRelativeBonePoses ), skm_transform_t{} );
	std::fill( std::begin( oldRelativeBonePoses ), std::end( oldRelativeBonePoses ), skm_transform_t{} );
	std::fill_n( &currentBoneLocalMatrices[ 0 ][ 0 ], SKM_MAX_BONES * 12, 0.0f );
	std::fill_n( &oldBoneLocalMatrices[ 0 ][ 0 ], SKM_MAX_BONES * 12, 0.0f );

	SG_SKM_ComputeLerpBonePoses( modelData, currentFrame, currentFrame, 1.0f, 0.0f, currentRelativeBonePoses, 0, SKM_POSE_TRANSLATE_ALL );
	SG_SKM_TransformBonePosesLocalSpace( skmData, currentRelativeBonePoses, &currentBoneLocalMatrices[ 0 ][ 0 ] );

	if ( oldFrame != currentFrame ) {
		SG_SKM_ComputeLerpBonePoses( modelData, oldFrame, oldFrame, 1.0f, 0.0f, oldRelativeBonePoses, 0, SKM_POSE_TRANSLATE_ALL );
		SG_SKM_TransformBonePosesLocalSpace( skmData, oldRelativeBonePoses, &oldBoneLocalMatrices[ 0 ][ 0 ] );
	}

	vec3_t envelopeMins = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	vec3_t envelopeMaxs = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
	bool hasEnvelopePoints = false;

	auto ExpandFromHitboxPoseSet = [ & ]( const float boneMatrices[ SKM_MAX_BONES ][ 12 ] ) {
		for ( uint32_t hitboxIndex = 0; hitboxIndex < skmData->num_hitboxes; hitboxIndex++ ) {
			const skm_hitbox_t &hitbox = skmData->hitboxes[ hitboxIndex ];
			if ( hitbox.boneIndex < 0 || hitbox.boneIndex >= static_cast<int32_t>( skmData->num_joints ) ) {
				continue;
			}

			const float *boneMatrix = boneMatrices[ hitbox.boneIndex ];
			const float xValues[ 2 ] = { hitbox.localMins[ 0 ], hitbox.localMaxs[ 0 ] };
			const float yValues[ 2 ] = { hitbox.localMins[ 1 ], hitbox.localMaxs[ 1 ] };
			const float zValues[ 2 ] = { hitbox.localMins[ 2 ], hitbox.localMaxs[ 2 ] };

			for ( int32_t xi = 0; xi < 2; xi++ ) {
				for ( int32_t yi = 0; yi < 2; yi++ ) {
					for ( int32_t zi = 0; zi < 2; zi++ ) {
						const Vector3 cornerLocal = { xValues[ xi ], yValues[ yi ], zValues[ zi ] };
						const Vector3 cornerModel = SVG_SKM_TransformPoint3x4( boneMatrix, cornerLocal );

						envelopeMins[ 0 ] = std::min( envelopeMins[ 0 ], cornerModel.x );
						envelopeMins[ 1 ] = std::min( envelopeMins[ 1 ], cornerModel.y );
						envelopeMins[ 2 ] = std::min( envelopeMins[ 2 ], cornerModel.z );
						envelopeMaxs[ 0 ] = std::max( envelopeMaxs[ 0 ], cornerModel.x );
						envelopeMaxs[ 1 ] = std::max( envelopeMaxs[ 1 ], cornerModel.y );
						envelopeMaxs[ 2 ] = std::max( envelopeMaxs[ 2 ], cornerModel.z );
						hasEnvelopePoints = true;
					}
				}
			}
		}
	};

	ExpandFromHitboxPoseSet( currentBoneLocalMatrices );
	if ( oldFrame != currentFrame ) {
		ExpandFromHitboxPoseSet( oldBoneLocalMatrices );
	}

	if ( !hasEnvelopePoints ) {
		return false;
	}

	/**
	*   Convert world segment into entity-local model space using renderer-parity basis.
	**/
	Vector3 entityForward = { 0.0f, 0.0f, 0.0f };
	Vector3 entityRight = { 0.0f, 0.0f, 0.0f };
	Vector3 entityUp = { 0.0f, 0.0f, 0.0f };
	QM_AngleVectors( target->currentAngles, &entityForward, &entityRight, &entityUp );
	// Match renderer AnglesToAxis convention: axis[1] is inverted right vector.
	entityRight = -entityRight;
	const Vector3 modelOrigin = SVG_SKM_GetModelOriginForHitboxTrace( target );
	const Vector3 modelOriginShiftLocal = SVG_SKM_WorldToEntityPoint( target->currentOrigin, modelOrigin, entityForward, entityRight, entityUp );

	const Vector3 localShotStart = SVG_SKM_WorldToEntityPoint( shotStart, modelOrigin, entityForward, entityRight, entityUp );
	const Vector3 localShotEnd = SVG_SKM_WorldToEntityPoint( shotEnd, modelOrigin, entityForward, entityRight, entityUp );

	// Require real expansion beyond the coarse collision hull; otherwise this fallback is just re-accepting bbox hits.
	const float coarseMins[ 3 ] = {
		target->mins.x + modelOriginShiftLocal.x,
		target->mins.y + modelOriginShiftLocal.y,
		target->mins.z + modelOriginShiftLocal.z,
	};
	const float coarseMaxs[ 3 ] = {
		target->maxs.x + modelOriginShiftLocal.x,
		target->maxs.y + modelOriginShiftLocal.y,
		target->maxs.z + modelOriginShiftLocal.z,
	};
	static constexpr float expansionEpsilon = 0.05f;
	const bool hasAnimatedExpansion =
		( envelopeMins[ 0 ] < ( coarseMins[ 0 ] - expansionEpsilon ) ) ||
		( envelopeMins[ 1 ] < ( coarseMins[ 1 ] - expansionEpsilon ) ) ||
		( envelopeMins[ 2 ] < ( coarseMins[ 2 ] - expansionEpsilon ) ) ||
		( envelopeMaxs[ 0 ] > ( coarseMaxs[ 0 ] + expansionEpsilon ) ) ||
		( envelopeMaxs[ 1 ] > ( coarseMaxs[ 1 ] + expansionEpsilon ) ) ||
		( envelopeMaxs[ 2 ] > ( coarseMaxs[ 2 ] + expansionEpsilon ) );
	if ( !hasAnimatedExpansion ) {
		return false;
	}

	/**
	*   Segment vs animated-envelope test.
	**/
	double hitT = 0.0;
	Vector3 localHitNormal = { 0.0f, 0.0f, 0.0f };
	if ( !SVG_SKM_SegmentAABBIntersect( localShotStart, localShotEnd, envelopeMins, envelopeMaxs, &hitT, &localHitNormal ) ) {
		return false;
	}

	// Accept fallback only if the animated envelope is hit before the coarse box would be hit.
	// This prevents normal bbox shots from being falsely confirmed by this fallback path.
	double coarseHitT = 0.0;
	Vector3 coarseHitNormal = { 0.0f, 0.0f, 0.0f };
	if ( SVG_SKM_SegmentAABBIntersect( localShotStart, localShotEnd, coarseMins, coarseMaxs, &coarseHitT, &coarseHitNormal ) ) {
		static constexpr double entryLeadEpsilon = 0.0005;
		if ( !( hitT + entryLeadEpsilon < coarseHitT ) ) {
			return false;
		}
	}

	/**
	*   Commit fallback trace result using world-space normal/endpoint.
	**/
	Vector3 worldNormal = ( entityForward * localHitNormal.x ) + ( entityRight * localHitNormal.y ) + ( entityUp * localHitNormal.z );
	const double worldNormalLength = std::sqrt( QM_Vector3DotProduct( worldNormal, worldNormal ) );
	if ( worldNormalLength > 0.000001 ) {
		worldNormal = worldNormal * static_cast<float>( 1.0 / worldNormalLength );
	} else {
		worldNormal = { 0.0f, 0.0f, 1.0f };
	}

	trace.entityNumber = target->s.number;
	trace.hitBodyID = -1;
	trace.fraction = hitT;
	trace.endpos = shotStart + ( shotEnd - shotStart ) * static_cast<float>( hitT );
	trace.plane.normal[ 0 ] = worldNormal.x;
	trace.plane.normal[ 1 ] = worldNormal.y;
	trace.plane.normal[ 2 ] = worldNormal.z;
	trace.plane.dist = QM_Vector3DotProduct( worldNormal, trace.endpos );
	trace.plane.type = PLANE_NON_AXIAL;
	trace.plane.signbits = 0;
	trace.ent = const_cast<svg_base_edict_t *>( target );

	return true;
}
