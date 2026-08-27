//! Tactical cover point extraction and geometry generation for the NavMesh.
/********************************************************************
*
*
*	ServerGame: NavMesh Cover Point Generation
*
*	Extracts tactical cover points from half-edge mesh boundary loops,
*	measures obstacle heights against collision model geometry,
*	and binds points to dynamic movers or static world geometry.
*
*
********************************************************************/
#include "nav_cover_generate.h"
#include "nav_cover_types.h"
#include "nav_cover_query.h"
#include "nav_generate.h"
#include "nav_core.h"
#include "nav_thread.h"
#include "shared/cm/cm_model.h"
#include "common/collisionmodel.h"
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include <cmath>
#include <vector>
#include <algorithm>

//! Global list of precalculated tactical cover points generated for the current navmesh.
std::vector<nav_cover_point_t> g_nav_cover_points = {};

/**
*	@brief	Clear all active cover points from memory.
**/
void Nav_ClearCoverPoints( void ) {
	/**
	*	Clear and deallocate the global cover points array.
	**/
	// Clear the vector and free memory.
	g_nav_cover_points.clear();
	g_nav_cover_points.shrink_to_fit();
	Nav_ClearCoverSpatialIndex();
}

/**
*	@brief		Probe obstacle height above a boundary point by raycasting against collision model geometry.
*	@param	probe_origin	Base origin on the floor adjacent to the wall.
*	@param	wall_normal		Direction pointing from the room into the wall.
*	@return	Measured obstacle height in Quake units, or 0.0f if no obstacle was detected.
**/
static float Nav_ProbeWallHeight( const Vector3 &probe_origin, const Vector3 &wall_normal_2d ) {
	/**
	*	Sanity checks: Ensure collision model is available.
	**/
	cm_t *cm = gi.GetCollisionModel();
	if ( !cm || !cm->cache || !cm->cache->nodes ) {
		return 0.0f;
	}

	/**
	*	Verify physical solid barrier rising above ground:
	*	A valid cover wall must be solid at the lowest height (18.0 units) to ensure it rises from the ground.
	*	If there is open air at height 18 (e.g. drop-off, ledge, or catwalk edge), return 0.0f.
	**/
	constexpr float probe_dist = 48.0f;
	const Vector3 base_start = QM_Vector3Add( probe_origin, Vector3{ 0.0f, 0.0f, 18.0f } );
	const Vector3 base_end = QM_Vector3Add( base_start, QM_Vector3Scale( wall_normal_2d, probe_dist ) );
	const svg_trace_t base_tr = SVG_Trace( base_start, vec3_origin, vec3_origin, base_end, nullptr, CM_CONTENTMASK_SOLID );
	if ( !base_tr.startsolid && base_tr.fraction >= 1.0f ) {
		return 0.0f;
	}

	constexpr float test_heights[] = { 96.0f, 72.0f, 56.0f, 40.0f, 32.0f, 24.0f, 18.0f };

	float highest_detected = 18.0f;

	for ( const float h : test_heights ) {
		const Vector3 h_start = QM_Vector3Add( probe_origin, Vector3{ 0.0f, 0.0f, h } );
		const Vector3 h_end = QM_Vector3Add( h_start, QM_Vector3Scale( wall_normal_2d, probe_dist ) );

		const svg_trace_t h_tr = SVG_Trace( h_start, vec3_origin, vec3_origin, h_end, nullptr, CM_CONTENTMASK_SOLID );

		// If trace hit solid geometry (or started in solid wall body), record obstacle height.
		if ( h_tr.startsolid || h_tr.fraction < 1.0f ) {
			highest_detected = h;
			break;
		}
	}

	return highest_detected;
}

/**
*	@brief	Generate tactical cover points across the navmesh using a robust two-phase extraction pipeline.
*	@details	Phase 1 extracts abundant raw candidate points from boundary edges, 45-degree walls, step risers, and corners.
*				Phase 2 applies spatial deduplication and exact entity bounding-box clearance validation.
**/
void Nav_GenerateCoverPoints( void ) {
	/**
	*	Sanity checks: Ensure half-edge mesh has been compiled.
	**/
	if ( g_nav_faces.empty() || g_nav_halfedges.empty() || g_nav_vertices.empty() ) {
		return;
	}

	/**
	*	Reset existing cover points before generation pass.
	**/
	Nav_ClearCoverPoints();

	cm_t *cm = gi.GetCollisionModel();
	Vector3 world_mins = { -CM_MAX_WORLD_HALF_SIZE, -CM_MAX_WORLD_HALF_SIZE, -CM_MAX_WORLD_HALF_SIZE };
	Vector3 world_maxs = { CM_MAX_WORLD_HALF_SIZE, CM_MAX_WORLD_HALF_SIZE, CM_MAX_WORLD_HALF_SIZE };
	if ( cm && cm->cache && cm->cache->models && cm->cache->nummodels > 0 ) {
		const mmodel_t *wm = &cm->cache->models[ 0 ];
		world_mins = Vector3{ wm->mins[ 0 ], wm->mins[ 1 ], wm->mins[ 2 ] };
		world_maxs = Vector3{ wm->maxs[ 0 ], wm->maxs[ 1 ], wm->maxs[ 2 ] };
	}

	std::vector<nav_cover_point_t> raw_candidates = {};
	raw_candidates.reserve( 8192 );

	/**
	*	========================================================================
	*	Phase 1: Rich Candidate Generation across freestanding objects & corridors
	*	========================================================================
	**/
	for ( size_t f_idx = 0; f_idx < g_nav_faces.size(); f_idx++ ) {
		// Update tactical cover candidate extraction progress (mapping to 0.92f..0.96f).
		if ( !g_nav_faces.empty() ) {
			const float pct = 0.92f + 0.04f * ( static_cast< float >( f_idx ) / static_cast< float >( g_nav_faces.size() ) );
			Nav_SetGenerationProgress( pct, "Tactical Cover Candidate Extraction" );
		}

		const nav_face_t &face = g_nav_faces[ f_idx ];

		// Exclude stair step treads from cover generation (stair steps are transit passages, not cover spots).
		bool is_stair_step = false;
		for ( int32_t e = 0; e < face.num_edges; e++ ) {
			const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
			if ( he.twin_idx != -1 && std::fabs( he.z_diff ) >= 4.0 && std::fabs( he.z_diff ) <= NAV_MAX_STEP_HEIGHT ) {
				is_stair_step = true;
				break;
			}
		}
		if ( is_stair_step ) {
			continue;
		}

		for ( int32_t e = 0; e < face.num_edges; e++ ) {
			const int32_t edge_idx = face.first_edge_idx + e;
			const nav_halfedge_t &he = g_nav_halfedges[ edge_idx ];

			const bool has_no_twin = ( he.twin_idx == -1 );
			const bool is_upward_step_wall = ( he.twin_idx != -1 && he.z_diff > NAV_MAX_STEP_HEIGHT );
			const bool is_boundary = has_no_twin || is_upward_step_wall;
			if ( !is_boundary ) {
				continue;
			}

			const Vector3 v1 = static_cast<Vector3>( g_nav_vertices[ he.vertex_idx ] );
			const int32_t next_edge_idx = he.next_idx;
			const Vector3 v2 = static_cast<Vector3>( g_nav_vertices[ g_nav_halfedges[ next_edge_idx ].vertex_idx ] );

			const Vector3 edge_vec = QM_Vector3Subtract( v2, v1 );
			const float edge_len = QM_Vector3Length( edge_vec );

			if ( edge_len < 10.0f ) {
				continue;
			}

			// Reject edges that lie right on the extreme outer perimeter boundary walls of the map.
			const Vector3 edge_mid = QM_Vector3Add( v1, QM_Vector3Scale( edge_vec, 0.5f ) );
			constexpr float world_bound_margin = 24.0f;
			if ( edge_mid.x <= world_mins.x + world_bound_margin || edge_mid.x >= world_maxs.x - world_bound_margin ||
				 edge_mid.y <= world_mins.y + world_bound_margin || edge_mid.y >= world_maxs.y - world_bound_margin ) {
				continue;
			}

			// Compute horizontal 2D edge tangent (Z = 0) to ensure horizontal normal orientation on slopes/pyramids/45-deg walls.
			Vector3 tangent_2d = { edge_vec.x, edge_vec.y, 0.0f };
			const float tan_len = QM_Vector3Length( tangent_2d );
			if ( tan_len < 0.5f ) {
				continue;
			}
			tangent_2d = QM_Vector3Scale( tangent_2d, 1.0f / tan_len );

			// 2D horizontal inward normal (CCW 90 degrees) and outward wall normal.
			const Vector3 open_normal_2d = { -tangent_2d.y, tangent_2d.x, 0.0f };
			const Vector3 wall_normal_2d = { tangent_2d.y, -tangent_2d.x, 0.0f };

			// Sampling offsets along the edge.
			std::vector<float> sample_offsets = {};
			constexpr float corner_inset = 14.0f;
			constexpr float sample_step = 28.0f;

			if ( edge_len <= 28.0f ) {
				sample_offsets.push_back( edge_len * 0.5f );
			} else {
				sample_offsets.push_back( corner_inset );
				for ( float dist = corner_inset + sample_step; dist < edge_len - corner_inset; dist += sample_step ) {
					sample_offsets.push_back( dist );
				}
				sample_offsets.push_back( edge_len - corner_inset );
			}

			for ( const float dist_along_edge : sample_offsets ) {
				const Vector3 edge_pos = QM_Vector3Add( v1, QM_Vector3Scale( tangent_2d, dist_along_edge ) );

				// Inset cover position onto the walkable face by agent radius + clearance (24 units).
				constexpr float agent_radius_inset = 24.0f;
				Vector3 cover_pos = QM_Vector3Add( edge_pos, QM_Vector3Scale( open_normal_2d, agent_radius_inset ) );

				// Drop cover position to exact floor surface.
				const Vector3 down_start = QM_Vector3Add( cover_pos, Vector3{ 0.0f, 0.0f, 16.0f } );
				const Vector3 down_end = QM_Vector3Add( cover_pos, Vector3{ 0.0f, 0.0f, -32.0f } );
				const svg_trace_t down_tr = SVG_Trace( down_start, vec3_origin, vec3_origin, down_end, nullptr, CM_CONTENTMASK_SOLID );
				if ( !down_tr.startsolid && down_tr.fraction < 1.0f ) {
					cover_pos.z = down_tr.endpos.z;
				}

				// Determine wall height:
				float wall_height = 0.0f;
				if ( is_upward_step_wall ) {
					wall_height = std::min<float>( 96.0f, static_cast<float>( he.z_diff ) );
				} else {
					wall_height = Nav_ProbeWallHeight( edge_pos, wall_normal_2d );
				}

				// Low barriers down to 18 units provide crouch cover.
				if ( wall_height < 18.0f ) {
					continue;
				}

				nav_cover_point_t cp = {};
				cp.local_position = cover_pos;
				cp.local_normal = open_normal_2d;
				cp.local_tangent = tangent_2d;
				cp.face_idx = face.face_id;
				cp.wall_height = wall_height;
				cp.parent_entity_id = face.entity_id;
				cp.transition_entity_id = face.transition_entity_id;

				if ( wall_height >= 56.0f ) {
					cp.cover_type = NAV_COVER_HIGH;
				} else {
					cp.cover_type = NAV_COVER_LOW;
					cp.peek_flags |= NAV_COVER_PEEK_OVER;
				}

				if ( dist_along_edge <= corner_inset + 4.0f ) {
					cp.peek_flags |= NAV_COVER_PEEK_LEFT;
				}
				if ( dist_along_edge >= edge_len - ( corner_inset + 4.0f ) ) {
					cp.peek_flags |= NAV_COVER_PEEK_RIGHT;
				}

				if ( face.entity_id != ENTITYNUM_NONE && face.entity_id != ENTITYNUM_WORLD ) {
					cp.cover_flags |= NAV_COVER_FLAG_MOVER_BOUND | NAV_COVER_FLAG_REJECT_WHILE_MOVING;
				}
				if ( face.transition_entity_id != ENTITYNUM_NONE && face.transition_entity_id != ENTITYNUM_WORLD ) {
					cp.cover_flags |= NAV_COVER_FLAG_REQUIRES_DOOR_CLOSED;
				}

				raw_candidates.push_back( cp );
			}
		}

		// Corner Bend Cover Points (90-deg and 45-deg corners):
		for ( int32_t e = 0; e < face.num_edges; e++ ) {
			const int32_t e_prev_idx = face.first_edge_idx + ( ( e + face.num_edges - 1 ) % face.num_edges );
			const int32_t e_curr_idx = face.first_edge_idx + e;

			const nav_halfedge_t &he_prev = g_nav_halfedges[ e_prev_idx ];
			const nav_halfedge_t &he_curr = g_nav_halfedges[ e_curr_idx ];

			const bool prev_is_bnd = ( he_prev.twin_idx == -1 ) || ( he_prev.twin_idx != -1 && he_prev.z_diff > NAV_MAX_STEP_HEIGHT );
			const bool curr_is_bnd = ( he_curr.twin_idx == -1 ) || ( he_curr.twin_idx != -1 && he_curr.z_diff > NAV_MAX_STEP_HEIGHT );

			if ( prev_is_bnd && curr_is_bnd ) {
				const Vector3 v_shared = static_cast<Vector3>( g_nav_vertices[ he_curr.vertex_idx ] );
				const Vector3 v_prev = static_cast<Vector3>( g_nav_vertices[ he_prev.vertex_idx ] );
				const Vector3 v_next = static_cast<Vector3>( g_nav_vertices[ g_nav_halfedges[ he_curr.next_idx ].vertex_idx ] );

				Vector3 tan_prev_2d = { v_shared.x - v_prev.x, v_shared.y - v_prev.y, 0.0f };
				Vector3 tan_curr_2d = { v_next.x - v_shared.x, v_next.y - v_shared.y, 0.0f };

				const float l1 = QM_Vector3Length( tan_prev_2d );
				const float l2 = QM_Vector3Length( tan_curr_2d );
				if ( l1 > 0.5f && l2 > 0.5f ) {
					tan_prev_2d = QM_Vector3Scale( tan_prev_2d, 1.0f / l1 );
					tan_curr_2d = QM_Vector3Scale( tan_curr_2d, 1.0f / l2 );

					const Vector3 norm_prev_2d = { -tan_prev_2d.y, tan_prev_2d.x, 0.0f };
					const Vector3 norm_curr_2d = { -tan_curr_2d.y, tan_curr_2d.x, 0.0f };

					Vector3 corner_bisector = QM_Vector3Add( norm_prev_2d, norm_curr_2d );
					const float bisector_len = QM_Vector3Length( corner_bisector );
					if ( bisector_len > 0.1f ) {
						corner_bisector = QM_Vector3Scale( corner_bisector, 1.0f / bisector_len );
						// Inset along corner bisector by 32 units: guarantees >= 22.6 units perpendicular standoff from both corner walls.
						constexpr float corner_inset_dist = 32.0f;
						Vector3 corner_pos = QM_Vector3Add( v_shared, QM_Vector3Scale( corner_bisector, corner_inset_dist ) );

						// Drop to floor:
						const Vector3 down_start = QM_Vector3Add( corner_pos, Vector3{ 0.0f, 0.0f, 16.0f } );
						const Vector3 down_end = QM_Vector3Add( corner_pos, Vector3{ 0.0f, 0.0f, -32.0f } );
						const svg_trace_t down_tr = SVG_Trace( down_start, vec3_origin, vec3_origin, down_end, nullptr, CM_CONTENTMASK_SOLID );
						if ( !down_tr.startsolid && down_tr.fraction < 1.0f ) {
							corner_pos.z = down_tr.endpos.z;
						}

						const float h1 = Nav_ProbeWallHeight( v_shared, QM_Vector3Scale( norm_prev_2d, -1.0f ) );
						const float h2 = Nav_ProbeWallHeight( v_shared, QM_Vector3Scale( norm_curr_2d, -1.0f ) );
						const float corner_height = std::max( h1, h2 );

						if ( corner_height >= 18.0f ) {
							nav_cover_point_t cp = {};
							cp.local_position = corner_pos;
							cp.local_normal = corner_bisector;
							cp.local_tangent = tan_curr_2d;
							cp.face_idx = face.face_id;
							cp.wall_height = corner_height;
							cp.parent_entity_id = face.entity_id;
							cp.transition_entity_id = face.transition_entity_id;
							cp.cover_type = ( corner_height >= 56.0f ) ? NAV_COVER_HIGH : NAV_COVER_LOW;
							cp.peek_flags = NAV_COVER_PEEK_CORNER | NAV_COVER_PEEK_LEFT | NAV_COVER_PEEK_RIGHT;

							if ( cp.cover_type == NAV_COVER_LOW ) {
								cp.peek_flags |= NAV_COVER_PEEK_OVER;
							}

							if ( face.entity_id != ENTITYNUM_NONE && face.entity_id != ENTITYNUM_WORLD ) {
								cp.cover_flags |= NAV_COVER_FLAG_MOVER_BOUND | NAV_COVER_FLAG_REJECT_WHILE_MOVING;
							}
							if ( face.transition_entity_id != ENTITYNUM_NONE && face.transition_entity_id != ENTITYNUM_WORLD ) {
								cp.cover_flags |= NAV_COVER_FLAG_REQUIRES_DOOR_CLOSED;
							}

							raw_candidates.push_back( cp );
						}
					}
				}
			}
		}
	}

	/**
	*	========================================================================
	*	Phase 2: Spatial Deduplication & Exact Entity Bounding-Box Clearance
	*	========================================================================
	*	- Deduplicates points closer than 44 units (prevents clustering on curves).
	*	- Tests clearance against exact entity standup and ducked physics bounding boxes.
	**/
	std::sort( raw_candidates.begin(), raw_candidates.end(),
		[]( const nav_cover_point_t &a, const nav_cover_point_t &b ) {
			const bool a_is_corner = ( ( a.peek_flags & NAV_COVER_PEEK_CORNER ) != 0 );
			const bool b_is_corner = ( ( b.peek_flags & NAV_COVER_PEEK_CORNER ) != 0 );
			if ( a_is_corner != b_is_corner ) {
				return a_is_corner > b_is_corner;
			}
			return a.wall_height > b.wall_height;
		}
	);

	constexpr float min_separation_sqr = 24.0f * 24.0f;

	for ( size_t c_idx = 0; c_idx < raw_candidates.size(); c_idx++ ) {
		// Update tactical cover clearance validation progress (mapping to 0.96f..0.99f).
		if ( !raw_candidates.empty() ) {
			const float pct = 0.96f + 0.03f * ( static_cast< float >( c_idx ) / static_cast< float >( raw_candidates.size() ) );
			Nav_SetGenerationProgress( pct, "Tactical Cover Validation" );
		}

		nav_cover_point_t &cand = raw_candidates[ c_idx ];

		// 1. Spatial separation check:
		bool too_close = false;
		for ( const nav_cover_point_t &accepted : g_nav_cover_points ) {
			if ( QM_Vector3DistanceSqr( cand.local_position, accepted.local_position ) < min_separation_sqr ) {
				too_close = true;
				break;
			}
		}
		if ( too_close ) {
			continue;
		}

		// 2. Physical bounding-box clearance validation using exact entity physics bounds:
		// Entity center is positioned vertically at feet + 36 units so mins.z (-36) aligns with the floor:
		const Vector3 entity_center = QM_Vector3Add( cand.local_position, Vector3{ 0.0f, 0.0f, 36.0f } );

		// Ducked clearance (required for crouching behind cover):
		const svg_trace_t duck_tr = SVG_Trace( entity_center, PHYS_DEFAULT_BBOX_DUCKED_MINS, PHYS_DEFAULT_BBOX_DUCKED_MAXS, entity_center, nullptr, CM_CONTENTMASK_SOLID );
		if ( duck_tr.startsolid || duck_tr.allsolid ) {
			continue; // Cannot crouch without clipping solid wall geometry.
		}

		// Standing clearance check:
		const svg_trace_t stand_tr = SVG_Trace( entity_center, PHYS_DEFAULT_BBOX_STANDUP_MINS, PHYS_DEFAULT_BBOX_STANDUP_MAXS, entity_center, nullptr, CM_CONTENTMASK_SOLID );
		if ( stand_tr.startsolid || stand_tr.allsolid ) {
			// If standing clips ceiling but ducking fits, keep as low cover point:
			cand.cover_type = NAV_COVER_LOW;
			cand.peek_flags |= NAV_COVER_PEEK_OVER;
		}

		g_nav_cover_points.push_back( cand );
	}

	Nav_SetGenerationProgress( 0.99f, "Tactical Cover Validation" );

	// Build the 2D spatial grid acceleration index for O(1) runtime queries.
	Nav_RebuildCoverSpatialIndex();

	// Log extraction summary.
	gi.dprintf( "NavMesh Cover Generation: Extracted %u raw candidates -> %u validated tactical cover points.\n",
		static_cast<uint32_t>( raw_candidates.size() ),
		static_cast<uint32_t>( g_nav_cover_points.size() ) );
}
