/********************************************************************
*
*
*	SharedGame: Player SlideBox Implementation
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "svg_mmove.h"
#include "svg_mmove_slidemove.h"

// Navigation includes for edge-aligned sliding.
#include "svgame/nav/nav_types.h"
#include "svgame/nav/nav_generate.h"
#include "svgame/nav/nav_path.h"
#include "svgame/svg_utils.h"

//! An actual pointer to the pmove object that we're moving.
//extern pmove_t *pm;

//! Uncomment for enabling second best hit plane tracing results.
//#define SECOND_PLANE_TRACE



/**
*
*
*	Touch Entities List:
*
*
**/
/**
*	@brief	As long as numberOfTraces does not exceed MAX_TOUCH_TRACES, and there is not a duplicate trace registered,
*			this function adds the trace into the touchTraceList array and increases the numberOfTraces.
**/
void SVG_MMove_RegisterTouchTrace( mm_touch_trace_list_t &touchTraceList, svg_trace_t &trace ) {
	// Escape function if we are exceeding maximum touch traces.
	if ( touchTraceList.numberOfTraces >= MM_MAX_TOUCH_TRACES ) {
		return;
	}

	// Iterate for possible duplicates.
	for ( int32_t i = 0; i < touchTraceList.numberOfTraces; i++ ) {
		// Escape function if duplicate.
		if ( touchTraceList.traces[ i ].ent == trace.ent ) {
			return;
		}
	}

	// Add trace to list.
	touchTraceList.traces[ touchTraceList.numberOfTraces++ ] = trace;
}



/**
*
*
*	Monster Step SlideMove:
*
*
**/
/**
*	@brief	Slide off of the impacting object.
**/
// Epsilon to 'halt' at.
static constexpr double MM_STOP_EPSILON = 0.1;
//! Dot-product tolerance for recognizing the same analytical contact plane again.
static constexpr double MM_PLANE_REPEAT_DOT = 0.9999;
//! More forgiving repeated-plane tolerance for nearly vertical wall contacts.
static constexpr double MM_VERTICAL_PLANE_REPEAT_DOT = 0.99;
//! Normalized entering threshold used to avoid clipping planes that movement is leaving.
static constexpr double MM_PLANE_ENTER_THRESHOLD = -0.001;
//! Minimum velocity magnitude required for a meaningful plane-entering test.
static constexpr double MM_MIN_PLANE_CHECK_SPEED = 1e-6;
//! Minimum incoming movement speed required before applying dynamic entity deflection;
//! prevents stationary or nearly-halted entities from micro-sliding or drifting when bumped.
static constexpr double MM_MIN_ENTITY_DEFLECTION_SPEED = 10.0;
//! Maximum proximity distance to a navmesh boundary edge within which edge-aligned sliding is enabled (64.0 units).
static constexpr double MM_MAX_EDGE_SLIDE_PROXIMITY = 64.0;
static constexpr double MM_MAX_EDGE_SLIDE_PROXIMITY_SQR = MM_MAX_EDGE_SLIDE_PROXIMITY * MM_MAX_EDGE_SLIDE_PROXIMITY;
//! Minimum squared length of a normalized edge direction vector to be considered numerically valid.
static constexpr double MM_MIN_VALID_EDGE_DIR_SQR = 0.5;
//! Fraction of incoming speed applied during edge-aligned deflection to maintain forward progress smoothly without overshooting ledges.
static constexpr double MM_EDGE_SLIDE_SPEED_SCALE = 0.5;

/**
* @brief Determine whether velocity is entering a collision plane in double precision.
* @param velocity Current movement velocity.
* @param normal Normalized collision-plane normal.
* @return True when the velocity points into the plane by more than the noise threshold.
**/
static bool SVG_MMove_IsEnteringPlane( const Vector3DP &velocity, const Vector3DP &normal ) {
    /**
    * Normalize the velocity projection so the decision is independent of movement speed.
    **/
    const double speed = QM_Vector3LengthDP( velocity );
    if ( speed <= MM_MIN_PLANE_CHECK_SPEED ) {
        return false;
    }

    return ( QM_Vector3DotProductDP( velocity, normal ) / speed ) < MM_PLANE_ENTER_THRESHOLD;
}

/**
 * @brief Clips the velocity to surface normal in double precision.
 **/
const int32_t SVG_MMove_ClipVelocity( const Vector3DP &in, const Vector3DP &normal, Vector3DP &out, const double overbounce ) {
	// Whether we're actually blocked or not.
	int32_t blocked = MM_VELOCITY_CLIPPED_NONE;
	// If the plane that is blocking us has a positive z component, then assume it's a floor.
	if ( normal.z > 0.0 /*PM_MIN_WALL_NORMAL_Z*/ && normal.z != 1.0 ) {
		blocked |= MM_VELOCITY_CLIPPED_FLOOR;
	}
	// A plane with no upward support is vertical Wall/Step:
	if ( normal.z == 0.0 ) {
		blocked |= MM_VELOCITY_CLIPPED_WALL_OR_STEP;
	}
	// Determine how far to slide based on the incoming direction.
	// Finish it by scaling with overBounce factor.
	const double backoff = QM_Vector3DotProductDP( in, normal ) * overbounce;

	for ( int32_t i = 0; i < 3; i++ ) {
		const double change = normal[ i ] * backoff;
		out[ i ] = in[ i ] - change;
		if ( out[ i ] > -MM_STOP_EPSILON && out[ i ] < MM_STOP_EPSILON ) {
			out[ i ] = 0.0;
		}
	}

	// Return blocked by flag(s).
	return blocked;
}

/**
*	@brief	Attempts to trace clip into velocity direction for the current frametime in double precision.
**/
const mm_slide_move_flags_t SVG_MMove_SlideMove( Vector3DP &origin, Vector3DP &velocity, const double frametime, const Vector3 &mins, const Vector3 &maxs, svg_base_edict_t *passEntity, mm_touch_trace_list_t &touch_traces, const bool has_time, mm_trace_shape_t override_shape, const nav_path_policy_t *navPolicy ) {
	Vector3DP dir = {};

	Vector3DP planes[ MM_MAX_CLIP_PLANES ] = {};

	svg_trace_t	trace = {};
	Vector3DP	end = {};

	double d = 0.0;
	double time_left = 0.0;

	int i = 0, j = 0;
	int32_t bumpcount = 0;
	int32_t numbumps = MM_MAX_CLIP_PLANES - 1;

	Vector3DP primal_velocity = velocity;
	Vector3DP last_valid_origin = origin;
	int32_t numplanes = 0;

	int32_t blockedMask = 0;

	time_left = frametime;

	for ( bumpcount = 0; bumpcount < numbumps; bumpcount++ ) {
		end = origin + ( velocity * time_left );
		trace = SVG_MMove_Trace( QM_Vector3FromDP( origin ), mins, maxs, QM_Vector3FromDP( end ), passEntity, CONTENTS_NONE, override_shape );

		if ( trace.allsolid ) {
			// Entity is trapped in another solid, DON'T build up falling damage.
			velocity.z = 0.0;
			// Save entity for contact.
			SVG_MMove_RegisterTouchTrace( touch_traces, trace );
			// Return trapped mask.
			return MM_SLIDEMOVEFLAG_TRAPPED;
		}


		// [Paril-KEX] experimental attempt to fix stray collisions on curved
		// surfaces; easiest to see on q2dm1 by running/jumping against the sides
		// of the curved map.
		#ifdef SECOND_PLANE_TRACE
		if ( trace.surface2 ) {
			Vector3DP clipped_a = {};
			Vector3DP clipped_b = {};
			SVG_MMove_ClipVelocity( velocity, Vector3DP( trace.plane.normal ), clipped_a, 1.01 );
			SVG_MMove_ClipVelocity( velocity, Vector3DP( trace.plane2.normal ), clipped_b, 1.01 );

			bool better = false;

			for ( int i = 0; i < 3; i++ ) {
				if ( std::fabs( clipped_a[ i ] ) < std::fabs( clipped_b[ i ] ) ) {
					better = true;
					break;
				}
			}

			if ( better ) {
				trace.plane = trace.plane2;
				trace.surface = trace.surface2;
				trace.material = trace.material2;
			}
		}
		#endif

		if ( trace.fraction > 0.0f ) {
			// actually covered some distance
			origin = Vector3DP( trace.endpos );
			last_valid_origin = Vector3DP( trace.endpos );

			//numplanes = 0;
		}

		if ( trace.fraction == 1.0f ) {
			blockedMask = MM_SLIDEMOVEFLAG_MOVED;
			break;     // moved the entire distance
		}

		// Save entity for contact.
		SVG_MMove_RegisterTouchTrace( touch_traces, trace );

		// At this point we are blocked but not trapped.
		blockedMask |= MM_SLIDEMOVEFLAG_BLOCKED;
		const Vector3DP contactNormal( trace.plane.normal );
		// Is it a vertical wall?
		if ( contactNormal.z < MM_MIN_WALL_NORMAL_Z ) {
			blockedMask |= MM_SLIDEMOVEFLAG_WALL_BLOCKED;
		}

		// Subtract the fraction of time used, from the whole fraction of the move.
		time_left -= time_left * static_cast<double>( trace.fraction );

		/**
		* Treat near-identical analytical normals as one plane so floating-point variation
		* does not consume the clip-plane budget at a stair seam.
		**/
		for ( i = 0; i < numplanes; i++ ) {
			const bool isVerticalWall = std::fabs( contactNormal.z ) < 0.1 && std::fabs( planes[ i ].z ) < 0.1;
			const double repeatDot = isVerticalWall ? MM_VERTICAL_PLANE_REPEAT_DOT : MM_PLANE_REPEAT_DOT;
			if ( QM_Vector3DotProductDP( contactNormal, planes[ i ] ) > repeatDot ) {
				velocity = velocity + contactNormal;
				break;
			}
		}
		if ( i < numplanes ) {
			// Repeat the trace after nudging velocity away from the already-known plane.
			continue;
		}

		/**
		* Store the new contact plane before solving the one-, two-, and three-plane cases.
		**/
		if ( numplanes >= MM_MAX_CLIP_PLANES ) {
			// Exhausting the plane budget means the mover is genuinely wedged.
			velocity = Vector3DP{ 0., 0., 0. };
			blockedMask = MM_SLIDEMOVEFLAG_TRAPPED;
			break;
		}
		planes[ numplanes ] = contactNormal;
		numplanes++;

		/**
		* Find a velocity that leaves every accumulated plane without clipping against surfaces
		* that the mover is already leaving.
		**/
		const Vector3DP original_velocity = velocity;
		Vector3DP clipVelocity = {};
		bool foundClipVelocity = false;
		for ( int32_t clipPlaneIndex = 0; clipPlaneIndex < numplanes; clipPlaneIndex++ ) {
			if ( !SVG_MMove_IsEnteringPlane( original_velocity, planes[ clipPlaneIndex ] ) ) {
				continue;
			}
			foundClipVelocity = true;

			SVG_MMove_ClipVelocity( original_velocity, planes[ clipPlaneIndex ], clipVelocity, 1.01 );
			int32_t secondPlaneIndex = 0;
			for ( secondPlaneIndex = 0; secondPlaneIndex < numplanes; secondPlaneIndex++ ) {
				if ( secondPlaneIndex == clipPlaneIndex || !SVG_MMove_IsEnteringPlane( clipVelocity, planes[ secondPlaneIndex ] ) ) {
					continue;
				}

				SVG_MMove_ClipVelocity( clipVelocity, planes[ secondPlaneIndex ], clipVelocity, 1.01 );
				if ( QM_Vector3DotProductDP( clipVelocity, planes[ clipPlaneIndex ] ) >= 0.0 ) {
					continue;
				}

				/**
				* Move along the crease of two blocking planes, then distinguish a valid flat-ground
				* corner from a true three-plane wedge.
				**/
				dir = QM_Vector3CrossProductDP( planes[ clipPlaneIndex ], planes[ secondPlaneIndex ] );
				d = QM_Vector3DotProductDP( dir, original_velocity );
				clipVelocity = dir * d;

				for ( int32_t thirdPlaneIndex = 0; thirdPlaneIndex < numplanes; thirdPlaneIndex++ ) {
					if ( thirdPlaneIndex == clipPlaneIndex || thirdPlaneIndex == secondPlaneIndex || !SVG_MMove_IsEnteringPlane( clipVelocity, planes[ thirdPlaneIndex ] ) ) {
						continue;
					}

					const bool firstIsVertical = std::fabs( planes[ clipPlaneIndex ].z ) < 0.1;
					const bool secondIsVertical = std::fabs( planes[ secondPlaneIndex ].z ) < 0.1;
					if ( firstIsVertical && secondIsVertical && planes[ thirdPlaneIndex ].z >= MM_MIN_STEP_NORMAL ) {
						// A pair of walls plus a walkable floor is a valid flat-ground corner.
						SVG_MMove_ClipVelocity( clipVelocity, planes[ thirdPlaneIndex ], clipVelocity, 1.01 );
						continue;
					}

					// A third entering plane with no walkable floor is a genuine wedge trap.
					velocity = Vector3DP{ 0., 0., 0. };
					return blockedMask | MM_SLIDEMOVEFLAG_TRAPPED;
				}
			}
		}

		if ( !foundClipVelocity ) {
			/**
			* A contact that the mover is grazing or leaving is not a blocking plane.
			* Preserve that valid velocity and let the next trace clear the contact;
			* zeroing it here makes edge-following movement stick at fraction zero.
			**/
			if ( QM_Vector3LengthDP( original_velocity ) <= MM_MIN_PLANE_CHECK_SPEED ) {
				velocity = Vector3DP{ 0., 0., 0. };
				break;
			}
			velocity = original_velocity;
			continue;
		}

		/**
		* If the clipped velocity magnitude is below the stop threshold, stop sliding.
		**/
		if ( QM_Vector3LengthSqrDP( clipVelocity ) < ( MM_STOP_EPSILON * MM_STOP_EPSILON ) ) {
			// Dynamic Entity Deflection: If blocked head-on by a living entity (player or monster),
			// deflect along the surface or boundary edge to make room without falling off brush edges.
			const bool allowDeflect = ( navPolicy == nullptr || navPolicy->allow_entity_deflection );
			if ( allowDeflect && trace.ent && ( trace.ent->client || ( ( trace.ent->svFlags & SVF_MONSTER ) != 0 ) ) ) {
				const double origSpeed = QM_Vector3LengthDP( original_velocity );
				// Condition check: require kinetic incoming velocity before deflecting to prevent stationary entities from drifting
				if ( origSpeed > MM_MIN_ENTITY_DEFLECTION_SPEED && numplanes > 0 ) {
					bool edgeAligned = false;

					// 1. If standing on a walkable navmesh face with boundary edges (narrow planks, catwalks, corridors),
					// align the deflection velocity strictly parallel along the edge to hug the side and make room without falling off:
					if ( !g_nav_faces.empty() ) {
						const Vector3 originV3 = QM_Vector3FromDP( origin );
						int32_t faceIdx = Nav_FindFaceInLeafStrict( originV3 );
						const int32_t moverSolid = passEntity ? passEntity->solid : SOLID_BOUNDS_BOX; // SOLID_CYLINDER? SOLID_CAPSULE?
						const Vector3 feetPos = SVG_GetEntityFeetOrigin( originV3, mins, maxs, moverSolid );

						if ( faceIdx < 0 || faceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
							faceIdx = Nav_FindFaceInLeafStrict( feetPos );
						}

						if ( faceIdx >= 0 && faceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
							const nav_face_t &face = g_nav_faces[ faceIdx ];
							double closestEdgeDistSq = std::numeric_limits<double>::max();
							Vector3DP bestEdgeDir = { 0.0, 0.0, 0.0 };

							// Iterate over boundary edges of the face to find the nearest edge to align with:
							for ( int32_t e = 0; e < face.num_edges; ++e ) {
								const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
								if ( he.twin_idx == -1 ) {
									const Vector3DP &v0 = g_nav_vertices[ he.vertex_idx ];
									const Vector3DP &v1 = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];
									const double distSq = Nav_DistancePointToSegment2DSqr( Vector3DP( feetPos ), v0, v1 );
									if ( distSq < closestEdgeDistSq ) {
										closestEdgeDistSq = distSq;
										Vector3DP edgeVec = v1 - v0;
										edgeVec.z = 0.0;
										const double edgeLen = QM_Vector3LengthDP( edgeVec );
										if ( edgeLen > 0.001 ) {
											bestEdgeDir = edgeVec * ( 1.0 / edgeLen );
										}
									}
								}
							}

							// If entity is within proximity of a boundary edge (e.g. elevated plank or corridor edge):
							if ( closestEdgeDistSq <= MM_MAX_EDGE_SLIDE_PROXIMITY_SQR && QM_Vector3LengthSqrDP( bestEdgeDir ) > MM_MIN_VALID_EDGE_DIR_SQR ) {
								// Choose edge direction that preserves forward movement progress:
								const double dotFwd = ( bestEdgeDir.x * original_velocity.x ) + ( bestEdgeDir.y * original_velocity.y );
								const Vector3DP slideDir = ( dotFwd >= 0.0 ) ? bestEdgeDir : ( bestEdgeDir * -1.0 );
								clipVelocity = slideDir * ( origSpeed * MM_EDGE_SLIDE_SPEED_SCALE );
								edgeAligned = true;
							}
						}
					}

					// 2. Open space entity collision: project velocity tangentially along the contact plane normal
					// to allow entities to peel apart or slide past each other without locking in mutual deadlock,
					// while dampening speed to prevent bouncing or ricochets:
					if ( !edgeAligned ) {
						// Calculate tangent velocity component by removing movement directly into the entity normal:
						const double dotNorm = QM_Vector3DotProductDP( original_velocity, contactNormal );
						if ( dotNorm < 0.0 ) {
							Vector3DP tangentVel = original_velocity - ( contactNormal * dotNorm );
							tangentVel.z = 0.0;
							//! Velocity dampening factor applied when sliding tangentially past another entity in open space.
							static constexpr double MM_ENTITY_TANGENT_SLIDE_DAMP = 0.5;
							clipVelocity = tangentVel * MM_ENTITY_TANGENT_SLIDE_DAMP;
						} else {
							// Moving away from the contact normal: preserve velocity:
							clipVelocity = original_velocity;
						}
					}
				}
			}

			if ( QM_Vector3LengthSqrDP( clipVelocity ) < ( MM_STOP_EPSILON * MM_STOP_EPSILON ) ) {
				velocity = Vector3DP{ 0., 0., 0. };
				break;
			}
		}

		// Commit the resolved slide direction and continue with the remaining frame time.
		velocity = clipVelocity;
	}

	if ( has_time ) {
		velocity = primal_velocity;
	}

	return blockedMask;
}
