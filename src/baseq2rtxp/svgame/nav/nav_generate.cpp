/********************************************************************
*
*
*	ServerGame: Navigation edge-mesh generation and KD-tree construction.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "nav_generate.h"
#include "nav_path.h"
#include "nav_thread.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>
#include "shared/cm/cm_model.h"
#include "shared/formats/format_bsp.h"

// Entity includes for IsSubClassType checking
#include "svgame/entities/func/svg_func_door.h"
#include "svgame/entities/func/svg_func_door_rotating.h"
#include "svgame/entities/func/svg_func_rotating.h"
#include "svgame/entities/func/svg_func_wall.h"
#include "svgame/entities/func/svg_func_areaportal.h"



/**
*	Global navmesh data containers.
**/
//! Global navmesh polygon data (temporary during build).
nav_vector_t<nav_poly_t> g_nav_polys;

//! Half-edge mesh global vertices data
std::vector<Vector3DP> g_nav_vertices;
//! Half-edge mesh global half-edges data
std::vector<nav_halfedge_t> g_nav_halfedges;
//! Half-edge mesh global faces data
std::vector<nav_face_t> g_nav_faces;
//! Entity graph traversal edge sets
std::vector<std::vector<int32_t>> g_nav_entity_edges;

//! Global navmesh kdtree node data.
nav_vector_t<nav_kdtree_node_t> g_nav_nodes;
//! Global navmesh leaf link data.
nav_vector_t<nav_leaf_link_t> g_nav_leaf_links;
//! Global navmesh leaf polygon ID data.
nav_vector_t<int32_t> g_nav_leaf_poly_ids;

//! Maximum bin count used for KD-tree Surface Area Heuristic (SAH) spatial partitioning.
static constexpr int32_t KDTREE_MAX_BIN = 32;
//! Maximum recursion depth permitted when constructing the KD-tree.
static constexpr int32_t KDTREE_MAX_DEPTH = 24;

//! Node result for negative space during BSP tree construction.
static constexpr int32_t NODE_NEGATIVE = -1;

/**
* @brief Bounded diagnostics for locating topology changes during one navmesh generation.
* @note This intentionally stores aggregate counts only; per-vertex logging would obscure the stage that introduces a defect.
**/
struct nav_generation_diagnostics_t {
	//! Number of polygons emitted by extraction.
	int32_t extracted_polys = 0;
	//! Number of extracted polygons owned by dynamic entities.
	int32_t extracted_dynamic_polys = 0;
	//! Number of distinct dynamic entities represented by extracted polygons.
	int32_t extracted_dynamic_entities = 0;
	//! Number of polygons presented to spatial partitioning.
	int32_t partition_input_polys = 0;
	//! Number of accepted two-sided partition operations.
	int32_t partition_accepted_splits = 0;
	//! Number of rejected partition candidates that preserved their source polygon.
	int32_t partition_rejected_splits = 0;
	//! Number of dynamic polygons preserved without partitioning.
	int32_t partition_dynamic_preserved = 0;
	//! Number of door-transition polygons preserved without partitioning.
	int32_t partition_transition_preserved = 0;
	//! Number of contained dynamic-brush clipping attempts skipped during extraction.
	int32_t contained_dynamic_clip_skips = 0;
	//! Number of dynamic origin helper brushes excluded during model brush collection.
	int32_t dynamic_origin_brushes_skipped = 0;
	//! Number of non-origin dynamic brush instances collected for extraction.
	int32_t dynamic_brushes_collected = 0;
	//! Number of linked dynamic/world portal pairs registered for runtime state updates.
	int32_t dynamic_transition_portals = 0;
	//! Number of world-to-world splice mutations.
	int32_t world_splices = 0;
	//! Number of dynamic same-entity splice mutations.
	int32_t dynamic_splices = 0;
	//! Number of splice mutations grouped by dynamic entity ID.
	std::unordered_map<int32_t, int32_t> splices_by_entity = {};
	//! Number of extracted polygons grouped by dynamic entity ID.
	std::unordered_map<int32_t, int32_t> extracted_polys_by_entity = {};
	//! Number of registered dynamic/world portal pairs grouped by dynamic entity ID.
	std::unordered_map<int32_t, int32_t> transition_portals_by_entity = {};
};

//! Aggregate generation diagnostics reset at the start of each navmesh build.
static nav_generation_diagnostics_t s_nav_generation_diagnostics = {};

/**
* @brief Reset the aggregate navmesh generation diagnostics for a fresh build.
**/
static void ResetNavGenerationDiagnostics( void ) {
	// Zero out all diagnostic counters and clear entity maps for clean accounting.
	s_nav_generation_diagnostics = {};
}

/**
* @brief Print a bounded summary of the generation diagnostics for a named stage.
* @param stage Stage label to include in the console output.
**/
static void LogNavGenerationDiagnostics( const char *stage ) {
	// Emit aggregate stage diagnostic summary to server developer console.
	gi.dprintf(
		"NavMesh Diagnostics [%s]: extracted=%d dynamicExtracted=%d dynamicEntities=%d dynamicBrushes=%d dynamicOriginSkips=%d partitionInput=%d acceptedSplits=%d rejectedSplits=%d dynamicPreserved=%d transitionPreserved=%d containedClipSkips=%d transitionPortals=%d worldSplices=%d dynamicSplices=%d\n",
		stage,
		s_nav_generation_diagnostics.extracted_polys,
		s_nav_generation_diagnostics.extracted_dynamic_polys,
		s_nav_generation_diagnostics.extracted_dynamic_entities,
		s_nav_generation_diagnostics.dynamic_brushes_collected,
		s_nav_generation_diagnostics.dynamic_origin_brushes_skipped,
		s_nav_generation_diagnostics.partition_input_polys,
		s_nav_generation_diagnostics.partition_accepted_splits,
		s_nav_generation_diagnostics.partition_rejected_splits,
		s_nav_generation_diagnostics.partition_dynamic_preserved,
		s_nav_generation_diagnostics.partition_transition_preserved,
		s_nav_generation_diagnostics.contained_dynamic_clip_skips,
		s_nav_generation_diagnostics.dynamic_transition_portals,
		s_nav_generation_diagnostics.world_splices,
		s_nav_generation_diagnostics.dynamic_splices );

	// Emit per-entity extraction counts capped to 8 entities to avoid console spam.
	int32_t printed_entities = 0;
	for ( const auto &entry : s_nav_generation_diagnostics.extracted_polys_by_entity ) {
		if ( printed_entities >= 8 ) {
			break;
		}

		const auto splice_it = s_nav_generation_diagnostics.splices_by_entity.find( entry.first );
		const int32_t splice_count = ( splice_it != s_nav_generation_diagnostics.splices_by_entity.end() ) ? splice_it->second : 0;
		const auto portal_it = s_nav_generation_diagnostics.transition_portals_by_entity.find( entry.first );
		const int32_t portal_count = ( portal_it != s_nav_generation_diagnostics.transition_portals_by_entity.end() ) ? portal_it->second : 0;
		gi.dprintf( "NavMesh Diagnostics [%s] entity=%d extracted=%d portals=%d splices=%d\n",
			stage,
			entry.first,
			entry.second,
			portal_count,
			splice_count );
		printed_entities++;
	}

	/**
	*   Emit a small, geometry-bearing portal sample only after final half-edge linking.
	*   This is capped per entity so a malformed compound door remains diagnosable without
	*   producing per-edge generation spam.
	**/
	if ( std::strcmp( stage, "HalfEdge" ) != 0 ) {
		return;
	}

	// Loop over transition portals grouped by entity.
	for ( const auto &entry : s_nav_generation_diagnostics.transition_portals_by_entity ) {
		const int32_t entity_id = entry.first;
		// Sanity check entity ID bounds.
		if ( entity_id <= 0 || entity_id >= static_cast< int32_t >( g_nav_entity_edges.size() ) ) {
			continue;
		}

		int32_t printed_portals = 0;
		const std::vector<int32_t> &entity_edges = g_nav_entity_edges[ entity_id ];
		for ( const int32_t edge_index : entity_edges ) {
			// Cap portal print count to 8 entries.
			if ( printed_portals >= 8 || edge_index < 0 || edge_index >= static_cast< int32_t >( g_nav_halfedges.size() ) ) {
				continue;
			}

			const nav_halfedge_t &edge = g_nav_halfedges[ edge_index ];
			if ( edge.edge_entity_id != entity_id || edge.twin_idx < 0 || edge.twin_idx >= static_cast< int32_t >( g_nav_halfedges.size() ) || edge_index > edge.twin_idx ) {
				continue;
			}

			const nav_halfedge_t &twin = g_nav_halfedges[ edge.twin_idx ];
			const Vector3DP edge_start = Vector3DP( g_nav_vertices[ edge.vertex_idx ] );
			const Vector3DP edge_end = Vector3DP( g_nav_vertices[ g_nav_halfedges[ edge.next_idx ].vertex_idx ] );
			const Vector3DP twin_start = Vector3DP( g_nav_vertices[ twin.vertex_idx ] );
			const Vector3DP twin_end = Vector3DP( g_nav_vertices[ g_nav_halfedges[ twin.next_idx ].vertex_idx ] );
			gi.dprintf(
				"NavMesh Portal [entity=%d pair=%d/%d faces=%d/%d] edge=(%.1f %.1f %.1f)->(%.1f %.1f %.1f) twin=(%.1f %.1f %.1f)->(%.1f %.1f %.1f) dz=%.1f\n",
				entity_id,
				edge_index,
				edge.twin_idx,
				edge.face_idx,
				twin.face_idx,
				edge_start.x,
				edge_start.y,
				edge_start.z,
				edge_end.x,
				edge_end.y,
				edge_end.z,
				twin_start.x,
				twin_start.y,
				twin_start.z,
				twin_end.x,
				twin_end.y,
				twin_end.z,
				edge.z_diff );
			printed_portals++;
		}
	}
}

/**
* @brief Describes the active model instance that owns one BSP brush during extraction.
* @note The BSP brush array is shared by the world and inline models, so ownership must remain explicit while clipping polygons.
**/
struct nav_brush_ownership_t {
	//! Global BSP brush index referenced by this model instance.
	int32_t brush_num = -1;
	//! Inline model number, or zero for world geometry.
	int32_t model_num = 0;
	//! Unique active runtime model instance identifier.
	int32_t instance_id = 0;
	//! Runtime entity number that owns this brush, or ENTITYNUM_NONE for world geometry.
	int32_t entity_id = ENTITYNUM_NONE;
	//! World-space translation applied to this brush's local planes.
	Vector3DP offset = {};
	//! Runtime Euler orientation applied after local BSP plane construction.
	Vector3DP angles = {};
};

/**
* @brief Determine whether one active brush may modify a polygon produced by another brush.
* @param source Source brush ownership for the walkable polygon.
* @param clipper Candidate brush ownership used for subtraction or entity splitting.
* @return True when both brushes belong to a compatible extraction domain.
* @note World polygons may still be split by active door-like entities so door transitions remain represented in the mesh.
**/
static bool AreBrushesCompatibleForClipping( const nav_brush_ownership_t &source, const nav_brush_ownership_t &clipper ) {
	/**
	* Brushes from one runtime model instance share the same local coordinate system and may clip one another normally.
	**/
	if ( source.instance_id == clipper.instance_id ) {
		return true;
	}

	/**
	* Preserve the special world-floor interaction that marks the footprint of an active door without allowing arbitrary model overlap.
	**/
	const bool isWorldToDynamicEntityTransition = source.model_num == 0 && clipper.model_num != 0 && clipper.entity_id != ENTITYNUM_NONE;
	if ( isWorldToDynamicEntityTransition ) {
		return true;
	}

	return false;
}

/**
*	@brief	Determine whether an active brush instance represents dynamic transition geometry.
*	@param	brush_instance	Active brush ownership record to classify.
*	@return	True when the brush belongs to a runtime mover entity.
*	@note	Dynamic transition brushes are not extracted as standalone floors. Instead, they
*			cut compatible world floors into geometry that later receives entity-edge metadata.
**/
static bool IsDynamicTransitionBrush( const nav_brush_ownership_t &brush_instance ) {
	/**
	*	A runtime entity number is the sole ownership signal for the existing transition
	*	path. Keep this helper behavior-neutral so later compound-door classification can
	*	build on one explicit decision point.
	**/
	return brush_instance.entity_id != ENTITYNUM_NONE;
}

/**
*	@brief	Determine whether a BSP texture name identifies a compiler origin helper surface.
*	@param	texture_name	BSP texture name associated with one brush side.
*	@return	True when the final path component is `origin`, ignoring case and separator style.
*	@note	The origin contents bit is not retained in all compiled BSP variants, so this
*			name fallback is required to keep mover pivot brushes out of nav extraction.
**/
static bool IsOriginTextureName( const char *texture_name ) {
	/**
	*	Reject empty texture names before scanning their final path component.
	**/
	if ( texture_name == nullptr || texture_name[ 0 ] == '\0' ) {
		return false;
	}

	// Scan string to extract final leaf name component after slashes.
	const char *leaf_name = texture_name;
	for ( const char *character = texture_name; *character != '\0'; character++ ) {
		// Support BSPs authored with either slash convention.
		if ( *character == '/' || *character == '\\' ) {
			leaf_name = character + 1;
		}
	}

	// Compare leaf name against "origin" string ignoring ASCII case.
	static constexpr char ORIGIN_NAME[ ] = "origin";
	for ( size_t index = 0; index < sizeof( ORIGIN_NAME ) - 1; index++ ) {
		if ( leaf_name[ index ] == '\0' || std::tolower( static_cast< unsigned char >( leaf_name[ index ] ) ) != ORIGIN_NAME[ index ] ) {
			return false;
		}
	}

	return leaf_name[ sizeof( ORIGIN_NAME ) - 1 ] == '\0';
}

/**
*	@brief	Determine whether a brush is the compiler origin brush used to define mover pivots.
*	@param	brush	BSP brush to classify.
*	@return	True when the brush is helper geometry and must not affect navmesh extraction.
*	@note	Origin brushes must be ignored because their small pivot volume otherwise creates
*			false dynamic portals instead of using the mover's visible outer brush geometry.
**/
static bool IsOriginBrush( const mbrush_t *brush ) {
	// Prefer the explicit origin content flag when the compiler preserved it.
	if ( brush && ( brush->contents & CONTENTS_ORIGIN ) != 0 ) {
		return true;
	}

	// Fall back to texture-name detection for BSP data where the origin content bit was stripped.
	const int32_t brush_num_sides = brush != nullptr && brush->authored_firstbrushside != nullptr ? brush->authored_numsides : ( brush != nullptr ? brush->numsides : 0 );
	const mbrushside_t *brush_sides = brush != nullptr && brush->authored_firstbrushside != nullptr ? brush->authored_firstbrushside : ( brush != nullptr ? brush->firstbrushside : nullptr );
	if ( !brush || brush_num_sides <= 0 || !brush_sides ) {
		return false;
	}

	bool foundOrigin = false;

	// Loop over brush sides to inspect texture names.
	for ( int32_t i = 0; i < brush_num_sides; i++ ) {
		const mbrushside_t *side = &brush_sides[ i ];
		if ( !side->texinfo || side->texinfo->name[ 0 ] == '\0' ) {
			continue;
		}

		// Match the helper surface by its case-insensitive leaf texture name.
		if ( IsOriginTextureName( side->texinfo->name ) ) {
			foundOrigin = true;
		}
	}

	return foundOrigin;
}

/**
*	@brief	Return the authored brush-side range used by navigation extraction.
*	@param	brush	Brush whose source geometry is requested.
*	@return	Authored sides when runtime bevels replaced the active range; otherwise the active sides.
*	@note	Synthetic navigation brushes may not have authored metadata and therefore fall back to
*		their active range.
**/
static const mbrushside_t *Nav_GetBrushSides( const mbrush_t *brush ) {
	if ( brush == nullptr ) {
		return nullptr;
	}
	return brush->authored_firstbrushside != nullptr ? brush->authored_firstbrushside : brush->firstbrushside;
}

/**
*	@brief	Return the authored side count used by navigation extraction.
*	@param	brush	Brush whose source geometry count is requested.
*	@return	Authored side count when available, otherwise the active side count.
**/
static int32_t Nav_GetBrushNumSides( const mbrush_t *brush ) {
	if ( brush == nullptr ) {
		return 0;
	}
	return brush->authored_firstbrushside != nullptr ? brush->authored_numsides : brush->numsides;
}

/**
* @brief Determine whether all vertices of a navigation polygon lie on its expected plane.
* @param poly Polygon to validate.
* @param expected_normal Surface normal that defines the polygon plane.
* @return True when the polygon has a usable normal and every vertex is within the planar tolerance.
**/
static bool IsNavPolygonCoplanar( const nav_poly_t &poly, const Vector3DP &expected_normal ) {
	/**
	* Reject an invalid plane normal before using it to classify vertex distances.
	**/
	const double normal_length_sqr = QM_Vector3LengthSqrDP( expected_normal );
	if ( normal_length_sqr <= 0.000001 ) {
		return false;
	}

	/**
	* Normalize the signed distance calculation so the tolerance is independent of normal magnitude.
	**/
	const double normal_length = std::sqrt( normal_length_sqr );
	const Vector3DP origin = Vector3DP( poly.vertices[ 0 ] );
	for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
		// Reject candidate vertices that no longer lie on the source surface plane.
		const double plane_error = std::fabs( QM_Vector3DotProductDP( Vector3DP( poly.vertices[ i ] ) - origin, expected_normal ) ) / normal_length;
		if ( plane_error > 0.25 ) {
			return false;
		}
	}

	return true;
}

/**
* @brief Determine whether a navigation polygon remains strictly convex and non-degenerate.
* @param poly Polygon to validate.
* @param expected_normal Surface normal used to evaluate each winding turn.
* @return True when every edge is non-zero and all non-collinear turns have one winding sign.
**/
static bool IsNavPolygonConvex( const nav_poly_t &poly, const Vector3DP &expected_normal ) {
	/**
	* Reject impossible vertex counts before reading the polygon loop.
	**/
	if ( poly.num_vertices < 3 || poly.num_vertices > MAX_WINDING_POINTS ) {
		return false;
	}

	/**
	* Check every edge and turn so a splice cannot create a duplicate vertex or self-intersection.
	**/
	double winding_sign = 0.0;
	for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
		Vector3DP a_dp( poly.vertices[ i ] );
		Vector3DP b_dp( poly.vertices[ ( i + 1 ) % poly.num_vertices ] );
		Vector3DP c_dp( poly.vertices[ ( i + 2 ) % poly.num_vertices ] );
		if ( QM_Vector3DistanceSqrDP( a_dp, b_dp ) <= 0.000001 ) {
			return false;
		}

		Vector3DP expected_normal_dp( expected_normal );

		const double turn = QM_Vector3DotProductDP( QM_Vector3CrossProductDP( QM_Vector3SubtractDP( b_dp, a_dp ), QM_Vector3SubtractDP( c_dp, b_dp ) ), expected_normal_dp );
		/**
		*    Because the navmesh vertices are stored as single-precision floats (Vector3),
		*    any vertex spliced exactly onto an edge will immediately quantize, baking a microscopic
		*    geometric "kink" into the coordinates.
		*
		*    Since a cross product computes area, a tiny angular kink across a very long edge
		*    (e.g., 256 units) can easily produce a cross-product magnitude > 0.01. Due to catastrophic
		*    cancellation during double subtraction, this can randomly produce false signs, which is why
		*    double-precision math must be used.
		*
		*    Furthermore, we MUST use a scale-invariant tolerance by multiplying the threshold
		*    by the edge lengths. This correctly measures the angle of the kink rather than the area.
		**/
		const double edgeLen1 = std::sqrt( QM_Vector3DistanceSqrDP( a_dp, b_dp ) );
		const double edgeLen2 = std::sqrt( QM_Vector3DistanceSqrDP( b_dp, c_dp ) );
		if ( std::fabs( turn ) <= 0.01 * edgeLen1 * edgeLen2 ) {
			continue;
		}
		if ( winding_sign == 0.0 ) {
			winding_sign = static_cast< double >( turn );
		} else if ( turn * winding_sign < 0.0 ) {
			return false;
		}
	}

	return winding_sign != 0.0;
}

/**
* @brief Determine whether two navigation polygons occupy the same surface plane.
* @param first First polygon to compare.
* @param second Second polygon to compare.
* @return True when the normals agree and the second polygon is coplanar with the first.
* @note This prevents projected 2D overlaps from joining legitimate step transitions.
**/
static bool AreNavPolygonsCoplanar( const nav_poly_t &first, const nav_poly_t &second ) {
	/**
	* Reject malformed polygons before reading their first vertices for plane comparison.
	**/
	if ( first.num_vertices < 1 || second.num_vertices < 1 ) {
		return false;
	}

	/**
	* Require similarly oriented surface normals before comparing plane offsets.
	**/
	const double first_normal_length_sqr = QM_Vector3LengthSqrDP( first.normal );
	const double second_normal_length_sqr = QM_Vector3LengthSqrDP( second.normal );
	if ( first_normal_length_sqr <= 0.000001 || second_normal_length_sqr <= 0.000001 ) {
		return false;
	}

	const double normal_alignment = QM_Vector3DotProductDP( first.normal, second.normal ) /
		std::sqrt( first_normal_length_sqr * second_normal_length_sqr );
	if ( normal_alignment < 0.999 ) {
		return false;
	}

	/**
	* Compare one vertex against the first polygon plane to reject different-height surfaces.
	**/
	const double plane_distance = std::fabs( QM_Vector3DotProductDP( Vector3DP( second.vertices[ 0 ] ) - Vector3DP( first.vertices[ 0 ] ), Vector3DP( first.normal ) ) ) /
		std::sqrt( first_normal_length_sqr );
	return plane_distance <= 0.25;
}

/**
* @brief Determine whether a navigation polygon is safe to use after a topology mutation.
* @param poly Polygon to validate.
* @param expected_normal Surface normal that defines the polygon plane.
* @return True when the polygon is planar, convex, and has no degenerate edges.
* @note This rejects topology repairs that would turn a valid convex surface into a skewed or self-intersecting face.
**/
static bool IsValidNavPolygon( const nav_poly_t &poly, const Vector3DP &expected_normal ) {
	/**
	* Validate the vertex count before the reusable geometry checks access the winding.
	**/
	if ( poly.num_vertices < 3 || poly.num_vertices > MAX_WINDING_POINTS ) {
		return false;
	}

	// Reject mutations that move any vertex off the source surface.
	if ( !IsNavPolygonCoplanar( poly, expected_normal ) ) {
		return false;
	}

	// Reject mutations that create degenerate or non-convex winding order.
	return IsNavPolygonConvex( poly, expected_normal );
}

/**
*
*
*
*	Navigation Mesh Core Command Handlers:
*
*
*
**/
/**
* @brief Generate the navmesh asynchronously from the console command path.
* @note This only submits the worker job; the actual extraction runs off-thread.
**/
void Nav_GenerateCommand() {
	/**
	*	Start the async generation worker job.
	**/
	Nav_StartAsyncGeneration();
}

/**
* @brief Clear all global navmesh containers and release the current build state.
**/
void Nav_Clear() {
	/**
	*	Clear all navmesh global data containers.
	**/
	g_nav_polys.clear();
	g_nav_vertices.clear();
	g_nav_halfedges.clear();
	g_nav_faces.clear();
	g_nav_entity_edges.clear();
	g_nav_nodes.clear();
	g_nav_leaf_links.clear();
	g_nav_leaf_poly_ids.clear();

	// Log memory clear message to server console.
	gi.dprintf( "NavMesh memory cleared.\n" );
}

/**
* @brief Print the current nav generation status to the server console.
* @note This reports the worker progress snapshot without depending on older nav helpers.
**/
void Nav_StatusCommand( void ) {
	/**
	*	Collect the latest progress snapshot from the nav generation worker.
	**/
	const nav_gen_progress_t &progress = Nav_GetGenerationProgress();

	/**
	*	Print a stable summary line so server operators can see whether generation is active and how far it has progressed.
	**/
	gi.dprintf(
		"NavMesh status: generating=%d progress=%.2f%% elapsed=%.2f s remaining=%.2f s\n",
		progress.is_generating ? 1 : 0,
		progress.progress_pct * 100.0,
		progress.time_taken_ms / 1000.0f,
		progress.estimated_time_left_ms / 1000.0f );
}

/**
*	@brief	Retrieve the root world model's bounding box and size extents.
*	@param	out_mins	[out] Minimum coordinates of the root world model (models[0]).
*	@param	out_maxs	[out] Maximum coordinates of the root world model (models[0]).
*	@return	True when collision model and root world model bounds are valid.
*	@note	Uses the root world model (models[0]) from the collision model cache as the authoritative reference frame.
**/
static bool Nav_GetWorldModelBounds( Vector3DP *out_mins, Vector3DP *out_maxs ) {
	cm_t *cm = gi.GetCollisionModel();
	if ( cm != nullptr && cm->cache != nullptr && cm->cache->models != nullptr && cm->cache->nummodels > 0 ) {
		const mmodel_t *world_model = &cm->cache->models[ 0 ];
		if ( out_mins != nullptr ) {
			*out_mins = Vector3DP( world_model->mins[ 0 ], world_model->mins[ 1 ], world_model->mins[ 2 ] );
		}
		if ( out_maxs != nullptr ) {
			*out_maxs = Vector3DP( world_model->maxs[ 0 ], world_model->maxs[ 1 ], world_model->maxs[ 2 ] );
		}
		return true;
	}
	if ( out_mins != nullptr ) {
		*out_mins = Vector3DP( -CM_MAX_WORLD_HALF_SIZE, -CM_MAX_WORLD_HALF_SIZE, -CM_MAX_WORLD_HALF_SIZE );
	}
	if ( out_maxs != nullptr ) {
		*out_maxs = Vector3DP( CM_MAX_WORLD_HALF_SIZE, CM_MAX_WORLD_HALF_SIZE, CM_MAX_WORLD_HALF_SIZE );
	}
	return false;
}

/**
*	@brief	Compute the dynamic base winding size extent derived from the root world model's bounds.
*	@return	Maximum extent scalar ensuring base plane windings comfortably enclose the world volume.
*	@note	Scales the root world model's maximum bounding span (models[0]) dynamically instead of using fixed constants.
**/
static double Nav_GetWorldWindingExtent( void ) {
	Vector3DP world_mins = {};
	Vector3DP world_maxs = {};
	Nav_GetWorldModelBounds( &world_mins, &world_maxs );

	// Calculate maximum span across X, Y, and Z axes of model 0.
	const double dx = std::abs( world_maxs.x - world_mins.x );
	const double dy = std::abs( world_maxs.y - world_mins.y );
	const double dz = std::abs( world_maxs.z - world_mins.z );

	// Add generous scaling margin so plane windings extend well beyond the root model volume.
	const double max_span = std::max( { dx, dy, dz } );
	return std::max( 16384.0, max_span * 4.0 );
}

/**
* @brief Construct a large base winding for a collision plane.
* @param p Plane to expand into a temporary polygon.
* @return Four-point winding centered on the plane.
**/
static winding_t BaseWindingForPlane( const cm_plane_t *p ) {
	winding_t w;
	w.num_points = 4;

	int32_t max = -1;
	double maxv = -1;
	for ( int32_t i = 0; i < 3; i++ ) {
		double v = fabs( p->normal[ i ] );
		if ( v > maxv ) {
			max = i;
			maxv = v;
		}
	}

	Vector3DP up( 0, 0, 0 );
	if ( max == 2 ) {
		up.x = 1;
	} else {
		up.z = 1;
	}

	Vector3DP p_normal( p->normal[ 0 ], p->normal[ 1 ], p->normal[ 2 ] );
	Vector3DP right = QM_Vector3CrossProductDP( up, p_normal );
	const double lengthA = QM_Vector3NormalizeLengthDP( right );
	up = QM_Vector3CrossProductDP( p_normal, right );
	const double lengthB = QM_Vector3NormalizeLengthDP( up );

	Vector3DP org = Vector3DP( p_normal ) * static_cast< double >( p->dist );

	const double BIGNUMBER = Nav_GetWorldWindingExtent();
	Vector3DP vright = right * static_cast< double >( BIGNUMBER );
	Vector3DP vup = up * static_cast< double >( BIGNUMBER );

	w.push_back( Vector3DP( org - vright + vup ) );
	w.push_back( Vector3DP( org + vright + vup ) );
	w.push_back( Vector3DP( org + vright - vup ) );
	w.push_back( Vector3DP( org - vright - vup ) );
	return w;
}

/**
* @brief Clip a winding against one plane in place.
* @param in Polygon to clip.
* @param split Plane to clip against.
* @param epsilon Tolerance used to classify points near the plane.
* @return True when the winding survives the clip.
**/
static bool ChopWindingInPlace( winding_t *in, const cm_plane_t *split, double epsilon ) {
	nav_vector_t<double> dists;
	dists.resize( static_cast< size_t >( in->num_points + 4 ) );

	nav_vector_t<int32_t> sides;
	sides.resize( static_cast< size_t >( in->num_points + 4 ) );


	int32_t counts[ 3 ] = { 0, 0, 0 };


	for ( int32_t i = 0; i < in->num_points; i++ ) {
		double dot = in->points[ i ].x * split->normal[ 0 ] +
			in->points[ i ].y * split->normal[ 1 ] +
			in->points[ i ].z * split->normal[ 2 ];
		dists[ i ] = dot - split->dist;
		if ( dists[ i ] > epsilon ) {
			sides[ i ] = 1; // front
		} else if ( dists[ i ] < -epsilon ) {
			sides[ i ] = 2; // back
		} else {
			sides[ i ] = 0; // on
		}
		counts[ sides[ i ] ]++;
	}
	sides[ in->num_points ] = sides[ 0 ];
	dists[ in->num_points ] = dists[ 0 ];

	if ( counts[ 2 ] == 0 ) return false; // all front
	if ( counts[ 1 ] == 0 ) return true; // all back

	winding_t out;
	out.clear();
	out.reserve( in->num_points + 2 );

	for ( int32_t i = 0; i < in->num_points; i++ ) {
		Vector3DP p1 = in->points[ i ];
		if ( sides[ i ] != 1 ) { // back or on
			out.push_back( Vector3DP( p1 ) );
		}
		if ( sides[ i ] == 0 || sides[ i ] == sides[ i + 1 ] ) {
			continue;
		}
		if ( sides[ i + 1 ] == 0 ) {
			continue;
		}
		// split
		Vector3DP p2 = in->points[ ( i + 1 ) % in->num_points ];
		double dot = dists[ i ] / ( dists[ i ] - dists[ i + 1 ] );
		Vector3DP mid;
		for ( int32_t j = 0; j < 3; j++ ) {
			if ( split->normal[ j ] == 1 ) mid[ j ] = split->dist;
			else if ( split->normal[ j ] == -1 ) mid[ j ] = -split->dist;
			else mid[ j ] = p1[ j ] + dot * ( p2[ j ] - p1[ j ] );
		}
		out.push_back( Vector3DP( mid ) );
	}

	*in = out;
	return true;
}


/**
* @brief Register a generated dynamic transition edge for runtime entity-state updates.
* @param entity_id Runtime entity that owns the transition.
* @param edge_idx Half-edge index carrying the transition metadata.
**/
static void RegisterNavEntityEdge( const int32_t entity_id, const int32_t edge_idx );

/**
* @brief Determine whether a dynamic transition owner is currently closed or solid.
* @param entity_id Runtime entity number owning the generated transition.
* @return True when a newly registered transition must begin disabled.
* @note Dynamic edge callbacks update only edges that already exist. Nav generation can
*       run after a door reached its closed state, so registry creation must reproduce
*       that current state instead of assuming a future close callback will do so.
**/
static bool IsDynamicTransitionEntityBlockingNavigation( const int32_t entity_id ) {
	if ( entity_id <= 0 || entity_id >= g_edict_pool.num_edicts ) {
		return false;
	}

	svg_base_edict_t *entity = g_edicts[ entity_id ];
	if ( !entity || !SVG_Entity_IsActive( entity ) ) {
		return false;
	}

	if ( entity->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ||
		entity->GetTypeInfo()->IsSubClassType<svg_func_door_rotating_t>() ) {
		const svg_func_door_t *door = static_cast< const svg_func_door_t * >( entity );
		return door->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_CLOSED ||
			door->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_MOVING_TO_CLOSED_STATE;
	}

	if ( entity->GetTypeInfo()->IsSubClassType<svg_func_rotating_t>() ) {
		return entity->solid == SOLID_BSP;
	}

	if ( entity->GetTypeInfo()->IsSubClassType<svg_func_areaportal_t>() ) {
		const svg_func_areaportal_t *areaportal = static_cast< const svg_func_areaportal_t * >( entity );
		return areaportal->count <= 0;
	}

	if ( entity->GetTypeInfo()->IsSubClassType<svg_func_wall_t>() ) {
		return entity->solid == SOLID_BSP;
	}

	return false;
}

/**
* @brief Resolve one face's runtime ownership entity for transition-edge metadata and debug state.
* @param face Face whose ownership metadata is being queried.
* @return Runtime entity id, or ENTITYNUM_NONE for world-owned geometry.
**/
static int32_t GetNavFaceOwningEntityId( const nav_face_t &face ) {
	return face.entity_id != ENTITYNUM_NONE ? face.entity_id : face.transition_entity_id;
}

/**
* @brief Assign dynamic metadata only to a true transition between an entity fragment and world geometry.
* @param edge_a First half-edge in the candidate adjacent pair.
* @param edge_b Opposite half-edge in the candidate adjacent pair.
* @note A face-level entity ID describes the fragment produced by clipping; it must not classify every perimeter edge as a door edge.
**/
static void AssignNavEntityEdgeMetadata( const int32_t edge_a, const int32_t edge_b ) {
	/**
	* Read the owning faces so the edge classification is based on adjacency rather than fragment ownership alone.
	**/
	const nav_face_t &face_a = g_nav_faces[ g_nav_halfedges[ edge_a ].face_idx ];
	const nav_face_t &face_b = g_nav_faces[ g_nav_halfedges[ edge_b ].face_idx ];
	const int32_t entity_a = GetNavFaceOwningEntityId( face_a );
	const int32_t entity_b = GetNavFaceOwningEntityId( face_b );

	/**
	* Only one side may be dynamic. Two dynamic faces are not a world-to-door transition,
	* and two world faces are ordinary navigation topology.
	**/
	const bool aIsDynamic = entity_a != ENTITYNUM_NONE;
	const bool bIsDynamic = entity_b != ENTITYNUM_NONE;
	if ( aIsDynamic == bIsDynamic ) {
		g_nav_halfedges[ edge_a ].edge_entity_id = ENTITYNUM_NONE;
		g_nav_halfedges[ edge_b ].edge_entity_id = ENTITYNUM_NONE;
		return;
	}

	/**
	* Mark both directed sides of the portal so rendering and runtime state updates remain symmetric.
	**/
	const int32_t transition_entity_id = aIsDynamic ? entity_a : entity_b;    // By default, the transition entity is just the one that is dynamic.
	int32_t final_entity_id = transition_entity_id;

	g_nav_halfedges[ edge_a ].edge_entity_id = final_entity_id;
	g_nav_halfedges[ edge_b ].edge_entity_id = final_entity_id;
	RegisterNavEntityEdge( final_entity_id, edge_a );
	RegisterNavEntityEdge( final_entity_id, edge_b );

	/**
	*   Match freshly generated transition state to the current mover state. This is
	*   required when generation occurs after the owner closed, because its close
	*   callback could not disable portal edges that had not been constructed yet.
	**/
	if ( IsDynamicTransitionEntityBlockingNavigation( final_entity_id ) ) {
		g_nav_halfedges[ edge_a ].flags |= NAV_EDGE_DISABLED;
		g_nav_halfedges[ edge_b ].flags |= NAV_EDGE_DISABLED;
	}

	s_nav_generation_diagnostics.dynamic_transition_portals++;
	s_nav_generation_diagnostics.transition_portals_by_entity[ final_entity_id ]++;
}

/**
*	@brief	Determine whether two half-edges may form one navigation portal by face ownership.
*	@param	edge_a	First half-edge index.
*	@param	edge_b	Second half-edge index.
*	@return	True when the face ownership domains permit a shared portal.
*	@note	The caller must still validate geometry, overlap, and step-height constraints.
*			Different dynamic entities never share a portal; same-entity dynamic pairs are
*			internal compound-door topology and dynamic/world pairs are runtime transitions.
**/
static bool AreHalfEdgesInCompatibleNavigationDomains( const int32_t edge_a, const int32_t edge_b ) {
	/**
	*	Reject invalid half-edge references before inspecting their owning faces.
	**/
	if ( edge_a < 0 || edge_b < 0 || edge_a >= static_cast< int32_t >( g_nav_halfedges.size() ) || edge_b >= static_cast< int32_t >( g_nav_halfedges.size() ) ) {
		return false;
	}

	const int32_t face_a = g_nav_halfedges[ edge_a ].face_idx;
	const int32_t face_b = g_nav_halfedges[ edge_b ].face_idx;
	if ( face_a < 0 || face_b < 0 || face_a >= static_cast< int32_t >( g_nav_faces.size() ) || face_b >= static_cast< int32_t >( g_nav_faces.size() ) || face_a == face_b ) {
		return false;
	}

	/**
	*	Classify the two faces by runtime dynamic ownership. World/world and world/dynamic
	*	pairs are legal; dynamic/dynamic pairs are legal only within one compound mover.
	**/
	const int32_t entity_a = g_nav_faces[ face_a ].entity_id;
	const int32_t entity_b = g_nav_faces[ face_b ].entity_id;
	const bool a_is_dynamic = entity_a != ENTITYNUM_NONE;
	const bool b_is_dynamic = entity_b != ENTITYNUM_NONE;
	if ( !a_is_dynamic || !b_is_dynamic ) {
		return true;
	}

	if ( entity_a == entity_b ) {
		return true;
	}

	// Double doors are different entities but form a single physical surface.
	// Allow them to connect if they are part of the same team.
	svg_base_edict_t *edict_a = g_edicts[ entity_a ];
	svg_base_edict_t *edict_b = g_edicts[ entity_b ];
	if ( edict_a && edict_b && edict_a->teammaster && edict_a->teammaster == edict_b->teammaster ) {
		return true;
	}

	return false;
}

/**
* @brief Transform one local BSP plane into an active brush instance's world space.
* @param plane Local collision-model plane.
* @param brush_instance Active model-instance translation and orientation.
* @return World-space plane with a rotated normal and translated distance.
* @note This matches collision tracing's `AnglesToAxis` convention: the model-space
*       normal is rotated by the transposed entity axis, then the plane is offset by
*       the active entity origin. Zero angles preserve ordinary world and sliding-door
*       behavior exactly.
**/
static cm_plane_t GetTransformedPlane( const cm_plane_t *plane, const nav_brush_ownership_t &brush_instance ) {
	cm_plane_t transformed_plane = *plane;

	/**
	*   Rotate the local plane normal into world space when an inline model has an
	*   active angular pose, such as a closed rotating door authored off its pivot.
	**/
	if ( brush_instance.angles.x != 0.0 || brush_instance.angles.y != 0.0 || brush_instance.angles.z != 0.0 ) {
		vec3_t angles = { static_cast< vec_t >( brush_instance.angles.x ), static_cast< vec_t >( brush_instance.angles.y ), static_cast< vec_t >( brush_instance.angles.z ) };
		vec3_t axis[ 3 ] = {};
		vec3_t normal = { transformed_plane.normal[ 0 ], transformed_plane.normal[ 1 ], transformed_plane.normal[ 2 ] };
		AnglesToAxis( angles, axis );
		TransposeAxis( axis );
		RotatePoint( normal, axis );
		transformed_plane.normal[ 0 ] = normal[ 0 ];
		transformed_plane.normal[ 1 ] = normal[ 1 ];
		transformed_plane.normal[ 2 ] = normal[ 2 ];
	}

	// Translate the rotated plane from its model-local pivot into world space.
	transformed_plane.dist += QM_Vector3DotProductDP( Vector3DP( transformed_plane.normal[ 0 ], transformed_plane.normal[ 1 ], transformed_plane.normal[ 2 ] ), brush_instance.offset );
	return transformed_plane;
}


/**
* @brief Computes which planes of a brush are actually part of its physical surface (not redundant).
**/
static std::vector<bool> GetBrushActivePlanes( const mbrush_t *b, const nav_brush_ownership_t &brush_instance ) {
	const int32_t brush_num_sides = Nav_GetBrushNumSides( b );
	const mbrushside_t *brush_sides = Nav_GetBrushSides( b );
	std::vector<bool> active( brush_num_sides, false );
	for ( int32_t i = 0; i < brush_num_sides; i++ ) {
		cm_plane_t p = GetTransformedPlane( brush_sides[ i ].plane, brush_instance );
		winding_t w = BaseWindingForPlane( &p );
		bool valid = true;
		for ( int32_t j = 0; j < brush_num_sides && valid; j++ ) {
			if ( i == j ) continue;
			cm_plane_t clip = GetTransformedPlane( brush_sides[ j ].plane, brush_instance );
			if ( !ChopWindingInPlace( &w, &clip, 0.1 ) ) {
				valid = false;
			}
		}
		if ( valid && w.num_points >= 3 ) {
			active[ i ] = true;
		}
	}
	return active;
}

/**
*	@brief	Determine whether every constructed boundary vertex of one convex brush lies in another convex brush.
*	@param	candidate	Brush whose complete volume may be contained.
*	@param	candidate_instance	World-space translation for the candidate brush.
*	@param	container	Brush that may enclose the candidate.
*	@param	container_instance	World-space translation for the enclosing brush.
*	@return	True when the candidate is fully enclosed by the container.
*	@note	Partial overlap is deliberately not containment. Only a complete enclosure can be
*			removed from a compound door union without changing its visible outer boundary.
**/
static bool IsBrushFullyContainedByBrush( const mbrush_t *candidate, const nav_brush_ownership_t &candidate_instance, const mbrush_t *container, const nav_brush_ownership_t &container_instance ) {
	/**
	*	Reject invalid brush data before constructing convex boundary windings.
	**/
	const int32_t candidate_num_sides = Nav_GetBrushNumSides( candidate );
	const int32_t container_num_sides = Nav_GetBrushNumSides( container );
	const mbrushside_t *candidate_sides = Nav_GetBrushSides( candidate );
	const mbrushside_t *container_sides = Nav_GetBrushSides( container );
	if ( candidate == nullptr || container == nullptr || candidate_num_sides <= 0 || container_num_sides <= 0 || candidate_sides == nullptr || container_sides == nullptr ) {
		return false;
	}

	/**
	*	Build only physical brush faces.
	**/
	const std::vector<bool> candidate_plane_active = GetBrushActivePlanes( candidate, candidate_instance );
	const std::vector<bool> container_plane_active = GetBrushActivePlanes( container, container_instance );
	bool found_candidate_vertex = false;

	/**
	*	Every vertex on every active candidate face must remain behind every active
	*	container plane. A point in front of one outward-facing plane is outside the
	*	container, proving that this candidate adds visible compound-door geometry.
	**/
	for ( int32_t candidate_side_index = 0; candidate_side_index < candidate_num_sides; candidate_side_index++ ) {
		if ( !candidate_plane_active[ candidate_side_index ] ) {
			continue;
		}

		const cm_plane_t candidate_plane = GetTransformedPlane( candidate_sides[ candidate_side_index ].plane, candidate_instance );
		winding_t candidate_face = BaseWindingForPlane( &candidate_plane );
		bool candidate_face_valid = true;
		for ( int32_t clip_side_index = 0; clip_side_index < candidate_num_sides && candidate_face_valid; clip_side_index++ ) {
			if ( candidate_side_index == clip_side_index ) {
				continue;
			}

			const cm_plane_t candidate_clip_plane = GetTransformedPlane( candidate_sides[ clip_side_index ].plane, candidate_instance );
			candidate_face_valid = ChopWindingInPlace( &candidate_face, &candidate_clip_plane, 0.1 );
		}

		// Ignore an invalid generated face; valid convex brushes expose at least one usable boundary face.
		if ( !candidate_face_valid || candidate_face.num_points < 3 ) {
			continue;
		}

		for ( int32_t point_index = 0; point_index < candidate_face.num_points; point_index++ ) {
			found_candidate_vertex = true;
			const Vector3DP &candidate_vertex = candidate_face.points[ point_index ];
			for ( int32_t container_side_index = 0; container_side_index < container_num_sides; container_side_index++ ) {
				if ( !container_plane_active[ container_side_index ] ) {
					continue;
				}

				const cm_plane_t container_plane = GetTransformedPlane( container_sides[ container_side_index ].plane, container_instance );
				const Vector3DP container_normal( container_plane.normal[ 0 ], container_plane.normal[ 1 ], container_plane.normal[ 2 ] );
				// Keep a shared numerical tolerance for face-boundary contact while rejecting real protrusions.
				if ( QM_Vector3DotProductDP( candidate_vertex, container_normal ) - container_plane.dist > 0.1 ) {
					return false;
				}
			}
		}
	}

	return found_candidate_vertex;
}

/**
*	@brief	Determine whether a dynamic brush is completely hidden inside another brush of the same door entity.
*	@param	bsp	Loaded BSP whose brush storage owns every active brush index.
*	@param	brush_instances	Active runtime brush instances for this navmesh build.
*	@param	candidate_index	Index of the dynamic brush instance to classify.
*	@return	True when the candidate cannot add an outer boundary to its compound door volume.
*	@note	Exact duplicate convex brushes use the lower brush number as the deterministic
*			representative. Partial overlap remains eligible because it can expand the union.
**/
static bool IsContainedDynamicTransitionBrush( const bsp_t *bsp, const std::vector<nav_brush_ownership_t> &brush_instances, const size_t candidate_index ) {
	/**
	*	Validate the candidate and reject ordinary world brushes before evaluating containment.
	**/
	if ( bsp == nullptr || candidate_index >= brush_instances.size() ) {
		return false;
	}

	const nav_brush_ownership_t &candidate_instance = brush_instances[ candidate_index ];
	if ( !IsDynamicTransitionBrush( candidate_instance ) ) {
		return false;
	}

	const mbrush_t *candidate_brush = &bsp->brushes[ candidate_instance.brush_num ];
	for ( size_t container_index = 0; container_index < brush_instances.size(); container_index++ ) {
		if ( container_index == candidate_index ) {
			continue;
		}

		const nav_brush_ownership_t &container_instance = brush_instances[ container_index ];
		// Containment is meaningful only inside one runtime door model instance.
		if ( container_instance.entity_id != candidate_instance.entity_id || container_instance.instance_id != candidate_instance.instance_id ) {
			continue;
		}

		const mbrush_t *container_brush = &bsp->brushes[ container_instance.brush_num ];
		if ( !IsBrushFullyContainedByBrush( candidate_brush, candidate_instance, container_brush, container_instance ) ) {
			continue;
		}

		/**
		*	Retain a stable representative when two brushes describe the same convex volume.
		*	A non-identical fully enclosed candidate never contributes an outer union boundary.
		**/
		const bool is_duplicate_volume = IsBrushFullyContainedByBrush( container_brush, container_instance, candidate_brush, candidate_instance );
		if ( !is_duplicate_volume || candidate_instance.brush_num > container_instance.brush_num ) {
			return true;
		}
	}

	return false;
}

/**
* @brief Check whether a fragment is completely outside one brush.
* @param frag Candidate polygon fragment.
* @param b Brush to test against.
* @param brush_instance The world offset to apply to the brush's planes.
* @param plane_active Precomputed boolean array of which planes are non-redundant.
* @param expand Margin expansion value.
* @return True when the fragment can be kept without further clipping.
**/
static bool IsFragmentCompletelyOutsideBrush( const winding_t *frag, const mbrush_t *b, const nav_brush_ownership_t &brush_instance, const std::vector<bool> &plane_active, const double expand = 0.0 ) {
	const int32_t brush_num_sides = Nav_GetBrushNumSides( b );
	const mbrushside_t *brush_sides = Nav_GetBrushSides( b );
	for ( int32_t j = 0; j < brush_num_sides; j++ ) {
		if ( !plane_active[ j ] ) continue;

		const mbrushside_t *side = &brush_sides[ j ];
		cm_plane_t p = GetTransformedPlane( side->plane, brush_instance );
		p.dist += expand;

		// If all points of the fragment are strictly in front of this plane,
		// the entire fragment is outside the convex brush.
		bool completely_outside = true;
		for ( int32_t i = 0; i < frag->num_points; i++ ) {
			double d = QM_Vector3DotProductDP( frag->points[ i ], Vector3DP( p.normal[ 0 ], p.normal[ 1 ], p.normal[ 2 ] ) ) - p.dist;
			// 0.1 tolerance to account for floating point inaccuracies
			if ( d <= 0.1 ) {
				completely_outside = false;
				break;
			}
		}
		if ( completely_outside ) {
			return true;
		}
	}
	return false;
}

/**
*	@brief	Compute the world-space bounding box of a transformed convex brush.
*	@param	b	Brush structure to analyze.
*	@param	brush_instance	World-space transformation offset and angles.
*	@param	out_mins	[out] Minimum X, Y, Z coordinates.
*	@param	out_maxs	[out] Maximum X, Y, Z coordinates.
*	@return	True if the brush produces valid convex geometry with measurable bounds.
**/
static bool GetBrushWorldBounds( const mbrush_t *b, const nav_brush_ownership_t &brush_instance, Vector3DP *out_mins, Vector3DP *out_maxs ) {
	if ( b == nullptr || out_mins == nullptr || out_maxs == nullptr ) {
		return false;
	}

	const int32_t brush_num_sides = Nav_GetBrushNumSides( b );
	const mbrushside_t *brush_sides = Nav_GetBrushSides( b );
	if ( brush_num_sides < 4 || brush_sides == nullptr ) {
		return false;
	}

	// Fast axial box check: if all planes are axis-aligned, compute bounding box directly in O(N) with zero winding chops.
	bool all_axial = true;
	Vector3DP axial_mins( -static_cast< double >( CM_MAX_WORLD_HALF_SIZE ), -static_cast< double >( CM_MAX_WORLD_HALF_SIZE ), -static_cast< double >( CM_MAX_WORLD_HALF_SIZE ) );
	Vector3DP axial_maxs( static_cast< double >( CM_MAX_WORLD_HALF_SIZE ), static_cast< double >( CM_MAX_WORLD_HALF_SIZE ), static_cast< double >( CM_MAX_WORLD_HALF_SIZE ) );
	int32_t axial_mask = 0;

	for ( int32_t i = 0; i < brush_num_sides; i++ ) {
		cm_plane_t p = GetTransformedPlane( brush_sides[ i ].plane, brush_instance );
		const float nx = p.normal[ 0 ];
		const float ny = p.normal[ 1 ];
		const float nz = p.normal[ 2 ];

		if ( nx > 0.999f && std::abs( ny ) < 0.001f && std::abs( nz ) < 0.001f ) {
			axial_maxs.x = std::min<double>( axial_maxs.x, p.dist );
			axial_mask |= 1;
		} else if ( nx < -0.999f && std::abs( ny ) < 0.001f && std::abs( nz ) < 0.001f ) {
			axial_mins.x = std::max<double>( axial_mins.x, -p.dist );
			axial_mask |= 2;
		} else if ( ny > 0.999f && std::abs( nx ) < 0.001f && std::abs( nz ) < 0.001f ) {
			axial_maxs.y = std::min<double>( axial_maxs.y, p.dist );
			axial_mask |= 4;
		} else if ( ny < -0.999f && std::abs( nx ) < 0.001f && std::abs( nz ) < 0.001f ) {
			axial_mins.y = std::max<double>( axial_mins.y, -p.dist );
			axial_mask |= 8;
		} else if ( nz > 0.999f && std::abs( nx ) < 0.001f && std::abs( ny ) < 0.001f ) {
			axial_maxs.z = std::min<double>( axial_maxs.z, p.dist );
			axial_mask |= 16;
		} else if ( nz < -0.999f && std::abs( nx ) < 0.001f && std::abs( ny ) < 0.001f ) {
			axial_mins.z = std::max<double>( axial_mins.z, -p.dist );
			axial_mask |= 32;
		} else {
			all_axial = false;
		}
	}

	if ( all_axial && axial_mask == 63 && axial_mins.x <= axial_maxs.x && axial_mins.y <= axial_maxs.y && axial_mins.z <= axial_maxs.z ) {
		*out_mins = axial_mins;
		*out_maxs = axial_maxs;
		return true;
	}

	// For non-axial brushes (cylinders, bevels, arches), compute exact bounds via face winding construction.
	Vector3DP mins( static_cast< double >( CM_MAX_WORLD_HALF_SIZE ), static_cast< double >( CM_MAX_WORLD_HALF_SIZE ), static_cast< double >( CM_MAX_WORLD_HALF_SIZE ) );
	Vector3DP maxs( -static_cast< double >( CM_MAX_WORLD_HALF_SIZE ), -static_cast< double >( CM_MAX_WORLD_HALF_SIZE ), -static_cast< double >( CM_MAX_WORLD_HALF_SIZE ) );
	bool has_points = false;

	for ( int32_t i = 0; i < brush_num_sides; i++ ) {
		cm_plane_t p = GetTransformedPlane( brush_sides[ i ].plane, brush_instance );
		winding_t w = BaseWindingForPlane( &p );
		bool valid = true;

		for ( int32_t j = 0; j < brush_num_sides && valid; j++ ) {
			if ( i == j ) continue;
			cm_plane_t clip = GetTransformedPlane( brush_sides[ j ].plane, brush_instance );
			if ( !ChopWindingInPlace( &w, &clip, 0.1 ) ) {
				valid = false;
			}
		}

		if ( valid && w.num_points >= 3 ) {
			has_points = true;
			for ( int32_t pt = 0; pt < w.num_points; pt++ ) {
				mins.x = std::min( mins.x, w.points[ pt ].x );
				mins.y = std::min( mins.y, w.points[ pt ].y );
				mins.z = std::min( mins.z, w.points[ pt ].z );
				maxs.x = std::max( maxs.x, w.points[ pt ].x );
				maxs.y = std::max( maxs.y, w.points[ pt ].y );
				maxs.z = std::max( maxs.z, w.points[ pt ].z );
			}
		}
	}

	if ( !has_points ) {
		return false;
	}

	*out_mins = mins;
	*out_maxs = maxs;
	return true;
}

/**
*	@brief	Return the maximum Z coordinate of a brush.
*	@param	b				The brush to analyze.
*	@param	brush_instance	World-space transform applied to the brush's planes.
*	@return	The maximum Z height of the brush.
**/
static double GetBrushMaxZ( const mbrush_t *b, const nav_brush_ownership_t &brush_instance ) {
	Vector3DP mins = {}, maxs = {};
	if ( GetBrushWorldBounds( b, brush_instance, &mins, &maxs ) ) {
		return maxs.z;
	}
	return static_cast< double >( CM_MAX_WORLD_HALF_SIZE );
}

/**
* @brief Subtract one blocking brush from a set of polygon fragments.
* @param fragments Fragments that will be clipped in place.
* @param b Blocking brush to subtract.
* @param brush_instance World-space transform applied to the brush's planes.
* @param poly_normal Original polygon normal used for coplanar tests.
* @note When carving horizontal floor surfaces (such as the main world ground),
*       obstacles with angled, sloped, or curved sides are carved using their clean 4-plane
*       axis-aligned bounding box (AABB) footprint. This completely eliminates diagonal rays
*       and sliver stripes across the open terrain while preserving the exact curved/sloped
*       step treads on the elevated walkable surfaces themselves.
**/
static void SubtractBrushFromWindings( std::vector<winding_t> &fragments, const mbrush_t *b, const nav_brush_ownership_t &brush_instance, const Vector3DP &poly_normal ) {
	std::vector<winding_t> next_fragments;
	const int32_t brush_num_sides = Nav_GetBrushNumSides( b );
	const mbrushside_t *brush_sides = Nav_GetBrushSides( b );
	std::vector<bool> plane_active = GetBrushActivePlanes( b, brush_instance );

	// Calculate the world-space bounding box of the brush.
	Vector3DP brush_mins = {}, brush_maxs = {};
	const bool has_bounds = GetBrushWorldBounds( b, brush_instance, &brush_mins, &brush_maxs );

	for ( const winding_t &frag : fragments ) {
		/**
		*   Preserve dynamic door transition fragments so static wall brush planes
		*   passing through doorway openings do not chop or slice dynamic door footprints.
		**/
		if ( frag.entity_id != ENTITYNUM_NONE ) {
			next_fragments.push_back( frag );
			continue;
		}

		// Optimization: If the fragment is entirely in front of any plane of the brush, it is completely outside the brush.
		if ( IsFragmentCompletelyOutsideBrush( &frag, b, brush_instance, plane_active, 0.0 ) ) {
			next_fragments.push_back( frag );
			continue;
		}

		// Quick 2D bounding-box overlap test if bounds are available.
		if ( has_bounds ) {
			nav_aabb_t frag_box;
			frag_box.Clear();
			for ( int32_t pt = 0; pt < frag.num_points; pt++ ) {
				frag_box.AddPoint( frag.points[ pt ] );
			}
			if ( frag_box.maxs.x <= brush_mins.x - 0.1 || frag_box.mins.x >= brush_maxs.x + 0.1 ||
				 frag_box.maxs.y <= brush_mins.y - 0.1 || frag_box.mins.y >= brush_maxs.y + 0.1 ) {
				next_fragments.push_back( frag );
				continue;
			}
		}

		// Direct subtraction using the brush's true authored planes.
		winding_t inside_part = frag;
		for ( int32_t j = 0; j < brush_num_sides; j++ ) {
			if ( !plane_active[ j ] ) continue;

			const mbrushside_t *side = &brush_sides[ j ];
			cm_plane_t p = GetTransformedPlane( side->plane, brush_instance );

			winding_t front = {};
			winding_t back = {};
			SplitWinding( &inside_part, &p, 0.1, Vector3DP( poly_normal ), &front, &back );

			// Anything in front of an outward-facing plane is strictly OUTSIDE the brush. We save it.
			if ( front.num_points >= 3 ) {
				front.entity_id = frag.entity_id;
				front.transition_entity_id = frag.transition_entity_id;
				next_fragments.push_back( front );
			}

			// Anything behind the plane might still be inside the brush, so we keep chopping it.
			inside_part = back;
			if ( inside_part.num_points < 3 ) {
				break;
			}
		}
	}

	fragments = next_fragments;
}

/**
* 	@brief Measure the planar area of fragments currently inside one convex brush volume.
* 	@param fragments Current walkable-floor fragments to inspect without mutation.
* 	@param b Convex brush defining the measured dynamic transition volume.
* 	@param brush_instance World-space transform applied to the brush planes.
* 	@return Total area of the clipped fragment portions inside the brush.
* 	@note Uses the same active planes and split tolerance as extraction so ordered blocker
* 			attribution observes the exact dynamic footprint that would become entity-owned.
**/
static double GetFragmentsInsideBrushArea( const std::vector<winding_t> &fragments, const mbrush_t *b, const nav_brush_ownership_t &brush_instance ) {
	/**
	* 	Reject malformed brush input before constructing measurement fragments.
	**/
	const int32_t brush_num_sides = Nav_GetBrushNumSides( b );
	const mbrushside_t *brush_sides = Nav_GetBrushSides( b );
	if ( b == nullptr || brush_num_sides <= 0 || brush_sides == nullptr ) {
		return 0.0;
	}

	const std::vector<bool> plane_active = GetBrushActivePlanes( b, brush_instance );
	double total_area = 0.0;
	for ( const winding_t &fragment : fragments ) {
		/**
		* 	Clip a local copy to the brush interior. The front side of each outward
		* 	plane lies outside, while the back side remains a volume candidate.
		**/
		winding_t inside_fragment = fragment;
		for ( int32_t side_index = 0; side_index < brush_num_sides; side_index++ ) {
			if ( !plane_active[ side_index ] || inside_fragment.num_points < 3 ) {
				continue;
			}

			const cm_plane_t plane = GetTransformedPlane( brush_sides[ side_index ].plane, brush_instance );
			winding_t front = {};
			winding_t back = {};
			SplitWinding( &inside_fragment, &plane, 0.1, Vector3DP( 0.0, 0.0, 1.0 ), &front, &back );
			inside_fragment = back;
		}

		/**
		* 	Accumulate a triangle fan only for a valid surviving interior polygon.
		**/
		for ( int32_t vertex_index = 2; vertex_index < inside_fragment.num_points; vertex_index++ ) {
			const Vector3DP cross = QM_Vector3CrossProductDP( Vector3DP( inside_fragment.points[ vertex_index - 1 ] ) - Vector3DP( inside_fragment.points[ 0 ] ), Vector3DP( inside_fragment.points[ vertex_index ] ) - Vector3DP( inside_fragment.points[ 0 ] ) );
			total_area += 0.5 * QM_Vector3LengthDP( cross );
		}
	}

	return total_area;
}

/**
* @brief Checks whether the given node pointer securely falls within the loaded bsp nodes or leafs boundaries.
**/
static bool IsNodeValid( const bsp_t *bsp, const mnode_t *node ) {
	if ( !node || !bsp ) return false;

	// Check if it safely falls within the bsp->nodes array.
	if ( node >= bsp->nodes && node < bsp->nodes + bsp->numnodes ) {
		return true;
	}

	// Check if it safely falls within the bsp->leafs array.
	const mleaf_t *leaf = reinterpret_cast< const mleaf_t * >( node );
	if ( leaf >= bsp->leafs && leaf < bsp->leafs + bsp->numleafs ) {
		return true;
	}

	// Likely uninitialized memory, or synthetic hulls. We don't process these here.
	return false;
}

/**
* @brief Recursively traverse a BSP tree node and collect all brush indices that belong to it.
**/
static void CollectModelBrushes( bsp_t *bsp, mnode_t *node, const int32_t model_num, const int32_t instance_id, const int32_t entity_id, const Vector3DP &offset, const Vector3DP &angles, std::vector<nav_brush_ownership_t> &brush_instances, std::vector<bool> &seen_brushes ) {
	/**
	*   Sanity check to prevent out-of-bounds pointer reads if a map happens to have corrupted inline trees
	*   or synthetic engine hulls that point outside normal geometry pools.
	**/
	if ( !IsNodeValid( bsp, node ) ) {
		return;
	}

	// If it's a leaf, process its brushes
	if ( node->plane == nullptr ) {
		mleaf_t *leaf = ( mleaf_t * )node;

		// For each leaf, process its brushes
		for ( int32_t i = 0; i < leaf->numleafbrushes; i++ ) {
			mbrush_t *b = leaf->firstleafbrush[ i ];
			int32_t brush_num = b - bsp->brushes;

			if ( brush_num >= 0 && brush_num < bsp->numbrushes ) {
				// Skip helper origin brushes here so they never become active door geometry.
				if ( IsOriginBrush( b ) ) {
					if ( entity_id != ENTITYNUM_NONE ) {
						s_nav_generation_diagnostics.dynamic_origin_brushes_skipped++;
					}
					continue;
				}

				// A brush can be referenced by several leaves; emit it once for this runtime model instance.
				if ( !seen_brushes[ brush_num ] ) {
					seen_brushes[ brush_num ] = true;
					brush_instances.push_back( { brush_num, model_num, instance_id, entity_id, offset, angles } );
					if ( entity_id != ENTITYNUM_NONE ) {
						s_nav_generation_diagnostics.dynamic_brushes_collected++;
					}
				}
			}
		}
		return;
	}

	// Recurse into children.
	CollectModelBrushes( bsp, node->children[ 0 ], model_num, instance_id, entity_id, offset, angles, brush_instances, seen_brushes );
	CollectModelBrushes( bsp, node->children[ 1 ], model_num, instance_id, entity_id, offset, angles, brush_instances, seen_brushes );
}

/**
 * @brief Split fragments against a door brush. Keeps the outside parts (unchanged) and the inside part (updated to the door's entity ID).
 * @param fragments Polygon fragments to split in-place.
 * @param b Door brush structure.
 * @param brush_instance Active brush model-instance translation and orientation.
 * @param expand Thickness margin expansion.
 * @note The split uses the exact translated brush planes so the generated transition edge remains seated on the door geometry.
**/
static void SplitWindingsByEntityBrush( std::vector<winding_t> &fragments, const mbrush_t *b, const nav_brush_ownership_t &brush_instance, const double expand = 0.0 ) {
	std::vector<winding_t> next_fragments;
	std::vector<bool> plane_active = GetBrushActivePlanes( b, brush_instance );

	for ( const winding_t &frag : fragments ) {
		/**
		*   Preserve an interior generated by an earlier brush of the same door union.
		*   Re-cutting it would create artificial internal transition strips rather than
		*   adding exposed compound-door boundary geometry.
		**/
		if ( frag.entity_id != ENTITYNUM_NONE ) {
			next_fragments.push_back( frag );
			continue;
		}

		if ( IsFragmentCompletelyOutsideBrush( &frag, b, brush_instance, plane_active, expand ) ) {
			// Keep the outside fragment as ordinary world geometry; only the brush interior should become door-owned.
			winding_t outside_frag = frag;
			next_fragments.push_back( outside_frag );
			continue;
		}

		winding_t inside_part = frag;
		const int32_t brush_num_sides = Nav_GetBrushNumSides( b );
		const mbrushside_t *brush_sides = Nav_GetBrushSides( b );

		for ( int32_t j = 0; j < brush_num_sides; j++ ) {
			if ( !plane_active[ j ] ) continue;

			const mbrushside_t *side = &brush_sides[ j ];
			cm_plane_t plane = GetTransformedPlane( side->plane, brush_instance );
			// Keep the split on the authored brush plane; expanding it creates a visible offset from the closed door.
			plane.dist += expand;

			winding_t front = {};
			winding_t back = {};
			SplitWinding( &inside_part, &plane, 0.1, Vector3DP( 0.0, 0.0, 1.0 ) /* not used */, &front, &back );

			if ( front.num_points >= 3 ) {
				/**
				*   Keep the outer world strip as ordinary world geometry.
				*   It borders the door volume, but it is not itself door-owned and must
				*   not be promoted into transition metadata or runtime edge ownership.
				**/
				next_fragments.push_back( front );
			}

			inside_part = back; // The back part is inside this plane, keep checking it against other planes
		}

		// Whatever is left in inside_part after checking all planes is completely inside the brush!
		if ( inside_part.num_points >= 3 ) {
			inside_part.entity_id = brush_instance.entity_id;
			inside_part.transition_entity_id = ENTITYNUM_NONE;
			next_fragments.push_back( inside_part );
		}
	}

	fragments = next_fragments;
}

/**
* @brief Register every generated dynamic transition edge for runtime state updates.
* @param entity_id Runtime entity that owns the transition.
* @param edge_idx Half-edge index carrying the transition metadata.
* @note The registry is indexed by entity number because door callbacks update edges by entity id.
**/
static void RegisterNavEntityEdge( const int32_t entity_id, const int32_t edge_idx ) {
	/**
	* Ignore world geometry and invalid indices because they cannot be updated by a mover callback.
	**/
	if ( entity_id <= 0 || edge_idx < 0 ) {
		return;
	}

	/**
	* Grow the entity-indexed registry lazily to match the runtime edict number.
	**/
	if ( static_cast< size_t >( entity_id ) >= g_nav_entity_edges.size() ) {
		g_nav_entity_edges.resize( static_cast< size_t >( entity_id ) + 1 );
	}

	/**
	* Avoid duplicate registration when both sides of a portal carry the same entity id.
	**/
	std::vector<int32_t> &edges = g_nav_entity_edges[ entity_id ];
	if ( std::find( edges.begin(), edges.end(), edge_idx ) == edges.end() ) {
		edges.push_back( edge_idx );
	}
}

/**
*	@brief	Subdivide a large walkable horizontal floor winding into regular spatial grid tiles.
*	@param	w			Source floor winding to tile.
*	@param	tileSize	Maximum spatial extent along X and Y per tile (e.g. 256.0 units).
*	@param	normal		Surface normal of the floor plane.
*	@param	out_tiles	[out] Vector receiving the resulting subdivided tile windings.
*	@note	Subdividing large open floor surfaces into spatial tiles prevents obstacle
*			CSG cutlines from propagating globally across the world, confining carving to
*			local cells and enabling pristine open-area tiles to merge into maximal rectangles.
**/
static void Nav_SubdivideWindingIntoSpatialTiles( const winding_t &w, const double tileSize, const Vector3DP &normal, std::vector<winding_t> &out_tiles ) {
	if ( w.num_points < 3 || tileSize <= 0.0 ) {
		if ( w.num_points >= 3 ) {
			out_tiles.push_back( w );
		}
		return;
	}

	// Compute 2D bounding box of the winding.
	Vector3DP mins = w.points[ 0 ];
	Vector3DP maxs = w.points[ 0 ];
	for ( int32_t i = 1; i < w.num_points; i++ ) {
		mins.x = std::min<double>( mins.x, w.points[ i ].x );
		mins.y = std::min<double>( mins.y, w.points[ i ].y );
		mins.z = std::min<double>( mins.z, w.points[ i ].z );
		maxs.x = std::max<double>( maxs.x, w.points[ i ].x );
		maxs.y = std::max<double>( maxs.y, w.points[ i ].y );
		maxs.z = std::max<double>( maxs.z, w.points[ i ].z );
	}

	const double extent_x = maxs.x - mins.x;
	const double extent_y = maxs.y - mins.y;

	// If the floor winding is already small (e.g. small step tread or box), no tiling is needed.
	if ( extent_x <= tileSize && extent_y <= tileSize ) {
		out_tiles.push_back( w );
		return;
	}

	// Collect vertical split coordinates along X (multiples of tileSize).
	std::vector<double> x_splits;
	const double start_x = std::floor( ( mins.x + 0.1 ) / tileSize + 1.0 ) * tileSize;
	for ( double x = start_x; x < maxs.x - 0.1; x += tileSize ) {
		x_splits.push_back( x );
	}

	// Split winding into vertical stripes along X.
	std::vector<winding_t> x_stripes;
	std::vector<winding_t> current_x_set;
	current_x_set.push_back( w );

	for ( const double split_x : x_splits ) {
		cm_plane_t plane;
		plane.normal[ 0 ] = 1.0f; plane.normal[ 1 ] = 0.0f; plane.normal[ 2 ] = 0.0f;
		plane.dist = static_cast< float >( split_x );

		std::vector<winding_t> next_x_set;
		for ( const winding_t &item : current_x_set ) {
			winding_t front = {};
			winding_t back = {};
			SplitWinding( &item, &plane, 0.1, normal, &front, &back );
			if ( front.num_points >= 3 ) {
				front.entity_id = item.entity_id;
				front.transition_entity_id = item.transition_entity_id;
				next_x_set.push_back( front );
			}
			if ( back.num_points >= 3 ) {
				back.entity_id = item.entity_id;
				back.transition_entity_id = item.transition_entity_id;
				x_stripes.push_back( back );
			}
		}
		current_x_set = next_x_set;
	}
	for ( const winding_t &rem : current_x_set ) {
		x_stripes.push_back( rem );
	}

	// Collect horizontal split coordinates along Y (multiples of tileSize).
	std::vector<double> y_splits;
	const double start_y = std::floor( ( mins.y + 0.1 ) / tileSize + 1.0 ) * tileSize;
	for ( double y = start_y; y < maxs.y - 0.1; y += tileSize ) {
		y_splits.push_back( y );
	}

	if ( y_splits.empty() ) {
		for ( const auto &stripe : x_stripes ) {
			out_tiles.push_back( stripe );
		}
		return;
	}

	// Split each vertical stripe into rectangular grid tiles along Y.
	for ( const winding_t &stripe : x_stripes ) {
		std::vector<winding_t> current_y_set;
		current_y_set.push_back( stripe );

		for ( const double split_y : y_splits ) {
			cm_plane_t plane;
			plane.normal[ 0 ] = 0.0f; plane.normal[ 1 ] = 1.0f; plane.normal[ 2 ] = 0.0f;
			plane.dist = static_cast< float >( split_y );

			std::vector<winding_t> next_y_set;
			for ( const winding_t &item : current_y_set ) {
				winding_t front = {};
				winding_t back = {};
				SplitWinding( &item, &plane, 0.1, normal, &front, &back );
				if ( front.num_points >= 3 ) {
					front.entity_id = item.entity_id;
					front.transition_entity_id = item.transition_entity_id;
					next_y_set.push_back( front );
				}
				if ( back.num_points >= 3 ) {
					back.entity_id = item.entity_id;
					back.transition_entity_id = item.transition_entity_id;
					out_tiles.push_back( back );
				}
			}
			current_y_set = next_y_set;
		}
		for ( const winding_t &rem : current_y_set ) {
			out_tiles.push_back( rem );
		}
	}
}

/**
* @brief Extract walkable polygons from the current map collision model.
* @note This is the first stage of navmesh generation and fills g_nav_polys.
**/
void Nav_DoExtractionWork() {
	// Get the collision model from the game interface
	cm_t *cm = gi.GetCollisionModel();
	// Safety check: Ensure the collision model and its cache are valid before proceeding.
	if ( !cm || !cm->cache ) {
		gi.dprintf( "Nav_DoExtractionWork: No collision model or cache!\n" );
		return;
	}
	// Get the BSP data from the collision model's cache
	bsp_t *bsp = cm->cache;

	// Now we can safely clear the global navmesh polygon container before starting the extraction process.
	ResetNavGenerationDiagnostics();
	g_nav_polys.clear();
	g_nav_polys.reserve( 4096 );


	// Parse the runtime edicts to find active bmodels while retaining separate ownership for every model instance.
	std::vector<nav_brush_ownership_t> brush_instances;
	brush_instances.reserve( static_cast< size_t >( bsp->numbrushes ) );
	int32_t next_instance_id = 1;

	// World geometry is always active, and has no offset.
	if ( bsp->models != nullptr && bsp->nummodels > 0 && bsp->models[ 0 ].headnode != nullptr ) {
		std::vector<bool> seen_world_brushes( static_cast< size_t >( bsp->numbrushes ), false );
		CollectModelBrushes( bsp, bsp->models[ 0 ].headnode, 0, 0, ENTITYNUM_NONE, Vector3DP( 0.0, 0.0, 0.0 ), Vector3DP( 0.0, 0.0, 0.0 ), brush_instances, seen_world_brushes );
	}

	for ( int32_t i = 1; i < g_edict_pool.num_edicts; i++ ) {
		svg_base_edict_t *edict = g_edicts[ i ];
		if ( !SVG_Entity_IsActive( edict ) ) {
			continue;
		}

		if ( edict->model.ptr != nullptr && edict->model.size() >= 2 && edict->model[ 0 ] == '*' ) {
			/**
			*	Resolve the network model handle to the zero-based BSP submodel index.
			*
			*	The game interface returns a configstring model handle, where zero means no
			*	model and inline model `*1` occupies handle one. The BSP model array is
			*	zero-based and reserves element zero for the world, matching SV_HullForEntity.
			*	Using the handle directly selects the following inline model and associates
			*	this entity with another mover's brush geometry.
			**/
			const int32_t model_handle = edict->s.modelindex;
			const int32_t model_num = model_handle - 1;
			if ( model_num > 0 && model_num < bsp->nummodels && bsp->models != nullptr ) {
				if ( bsp->models[ model_num ].headnode != nullptr ) {
					// Inline BSP planes are authored in local coordinates.
					// Default to current runtime origin and angles.
					Vector3DP offset = Vector3DP( edict->currentOrigin );
					Vector3DP angles = Vector3DP( edict->currentAngles );
					int32_t assigned_ent_id = ENTITYNUM_NONE;

					if ( edict->GetTypeInfo()->IsSubClassType<svg_func_wall_t>() ) {
						assigned_ent_id = i;
					} else if ( edict->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
						const svg_func_door_t *door = static_cast< const svg_func_door_t * >( edict );
						// Extract door brushes using their authored closed position so Pass 1 can cleanly
						// carve the doorway threshold footprint without being displaced by runtime mover states.
						offset = Vector3DP( door->pushMoveInfo.startOrigin );
						angles = Vector3DP( door->pushMoveInfo.startAngles );
						assigned_ent_id = edict->teammaster ? edict->teammaster->s.number : i;
					} else if ( edict->GetTypeInfo()->IsSubClassType<svg_func_door_rotating_t>() ) {
						const svg_func_door_rotating_t *door = static_cast< const svg_func_door_rotating_t * >( edict );
						// Vertical rotating doors (Z axis) are doorway transitions.
						// X/Y axis rotating doors (drawbridges / trapdoors) are walkable floor geometry.
						if ( !( door->spawnflags & ( svg_func_door_rotating_t::SPAWNFLAG_X_AXIS | svg_func_door_rotating_t::SPAWNFLAG_Y_AXIS ) ) ) {
							offset = Vector3DP( door->pushMoveInfo.startOrigin );
							angles = Vector3DP( door->pushMoveInfo.startAngles );
							assigned_ent_id = edict->teammaster ? edict->teammaster->s.number : i;
						}
					}

					std::vector<bool> seen_model_brushes( static_cast< size_t >( bsp->numbrushes ), false );
					CollectModelBrushes( bsp, bsp->models[ model_num ].headnode, model_num, next_instance_id++, assigned_ent_id, offset, angles, brush_instances, seen_model_brushes );
				}
			}
		}
	}

	/**
	*   Print a bounded inventory of the actual dynamic brushes that survived origin
	*   filtering. This identifies whether a small pivot-like volume is a missed helper
	*   brush or genuine authored door geometry without logging world brush detail.
	**/
	int32_t printed_dynamic_brushes = 0;
	for ( const nav_brush_ownership_t &brush_instance : brush_instances ) {
		if ( !IsDynamicTransitionBrush( brush_instance ) || printed_dynamic_brushes >= 8 ) {
			continue;
		}

		const mbrush_t *dynamic_brush = &bsp->brushes[ brush_instance.brush_num ];
		const int32_t dynamic_num_sides = Nav_GetBrushNumSides( dynamic_brush );
		const mbrushside_t *dynamic_sides = Nav_GetBrushSides( dynamic_brush );
		const char *first_texture_name = "<none>";
		for ( int32_t side_index = 0; side_index < dynamic_num_sides; side_index++ ) {
			const mbrushside_t *side = &dynamic_sides[ side_index ];
			if ( side->texinfo != nullptr && side->texinfo->name[ 0 ] != '\0' ) {
				first_texture_name = side->texinfo->name;
				break;
			}
		}

		const mmodel_t &dynamic_model = bsp->models[ brush_instance.model_num ];
		gi.dprintf( "NavMesh DynamicBrush [entity=%d model=%d instance=%d brush=%d contents=0x%08x sides=%d texture=%s offset=(%.1f %.1f %.1f) angles=(%.1f %.1f %.1f) modelLocal=(%.1f %.1f %.1f)->(%.1f %.1f %.1f)]\n",
			brush_instance.entity_id,
			brush_instance.model_num,
			brush_instance.instance_id,
			brush_instance.brush_num,
			static_cast< uint32_t >( dynamic_brush->contents ),
			dynamic_num_sides,
			first_texture_name,
			brush_instance.offset.x,
			brush_instance.offset.y,
			brush_instance.offset.z,
			brush_instance.angles.x,
			brush_instance.angles.y,
			brush_instance.angles.z,
			dynamic_model.mins[ 0 ],
			dynamic_model.mins[ 1 ],
			dynamic_model.mins[ 2 ],
			dynamic_model.maxs[ 0 ],
			dynamic_model.maxs[ 1 ],
			dynamic_model.maxs[ 2 ] );
		printed_dynamic_brushes++;
	}

	int32_t solid_brushes = 0;
	int32_t playerclip_brushes = 0;
	int32_t walkable_sides = 0;
	int32_t sliver_pruned_fragments = 0;
	int32_t printed_dynamic_footprints = 0;
	int32_t printed_pending_dynamic_blockers = 0;

	// Iterate through every active model instance's brush references to extract walkable polygons.
	for ( size_t instance_brush_index = 0; instance_brush_index < brush_instances.size(); instance_brush_index++ ) {
		// Update extraction stage progress (mapping to 0.0f..0.40f).
		if ( !brush_instances.empty() ) {
			const float pct = 0.40f * ( static_cast< float >( instance_brush_index ) / static_cast< float >( brush_instances.size() ) );
			Nav_SetGenerationProgress( pct );
		}

		const nav_brush_ownership_t &brush_instance = brush_instances[ instance_brush_index ];
		const int32_t brush_num = brush_instance.brush_num;
		mbrush_t *b = &bsp->brushes[ brush_num ];
		// Ignore helper origin brushes; they define mover pivots but must not contribute nav geometry.
		if ( IsOriginBrush( b ) ) {
			continue;
		}

		// Dynamic entity brushes are transition volumes, not standalone nav floors; their footprint is represented by splitting world polygons below.
		if ( IsDynamicTransitionBrush( brush_instance ) ) {
			continue;
		}

		// Skip brushes that are not walk-blocking contributors.
		if ( !( b->contents & ( CONTENTS_SOLID | CONTENTS_DETAIL | CONTENTS_MONSTERCLIP ) ) ) {
			continue;
		}
		// Track brush class split for diagnostics.
		if ( ( b->contents & CONTENTS_MONSTERCLIP ) != 0 && ( b->contents & ( CONTENTS_SOLID | CONTENTS_DETAIL ) ) == 0 ) {
			playerclip_brushes++;
		} else {
			solid_brushes++;
		}

		// Discard brushes that belong to the sky.
		const int32_t brush_num_sides = Nav_GetBrushNumSides( b );
		const mbrushside_t *brush_sides = Nav_GetBrushSides( b );
		bool is_sky_brush = false;
		for ( int32_t j = 0; j < brush_num_sides; j++ ) {
			const mbrushside_t *side = &brush_sides[ j ];
			if ( side->texinfo && ( side->texinfo->c.flags & CM_SURFACE_FLAG_SKY ) ) {
				is_sky_brush = true;
				break;
			}
		}
		if ( is_sky_brush ) {
			continue;
		}

		//! Iterate through each authored side of the brush to find walkable surfaces.
		for ( int32_t j = 0; j < brush_num_sides; j++ ) {
			// Get the current authored brush side.
			const mbrushside_t *side = &brush_sides[ j ];

			// Discard sides that belong to sky surfaces or surfaces explicitly flagged to be excluded from navmesh generation.
			if ( side->texinfo != nullptr && ( side->texinfo->c.flags & ( CM_SURFACE_FLAG_SKY | CM_SURFACE_NO_NAVMESH ) ) != 0 ) {
				continue;
			}

			// Discard sides that are not walkable based on their normal's Z component
			if ( side->plane->normal[ 2 ] < NAV_MIN_WALKABLE_Z ) {
				continue;
			}
			walkable_sides++;

			// Create a base winding (polygon) for the current brush side's plane
			cm_plane_t shifted_plane = GetTransformedPlane( side->plane, brush_instance );
			winding_t w = BaseWindingForPlane( &shifted_plane );
			// Store a flag to track if the winding remains valid after clipping against other brush sides
			bool valid = true;
			//! Clip the winding against all other authored sides of the brush to ensure it fits within the brush's volume.
			for ( int32_t k = 0; k < brush_num_sides && valid; k++ ) {
				// Skip clipping against the same side
				if ( j == k ) {
					continue;
				}
				// Get the authored brush side to clip against.
				const mbrushside_t *clip = &brush_sides[ k ];
				cm_plane_t clip_plane = GetTransformedPlane( clip->plane, brush_instance );
				// Clip the winding in place against the plane. If it fails, mark the winding as invalid.
				if ( !ChopWindingInPlace( &w, &clip_plane, 0.1 ) ) {
					valid = false;
				}
			}
			// If the winding is still valid and has at least 3 points, create a nav_poly_t and add it to the global navmesh polygon container.
if ( !valid || w.num_points < 3 ) {
				continue;
			}
			w.entity_id = brush_instance.entity_id;

			// Subtract all other solid brushes from this walkable surface
			Vector3DP normal( shifted_plane.normal[ 0 ], shifted_plane.normal[ 1 ], shifted_plane.normal[ 2 ] );

			// If this is a horizontal floor (normal.z > 0.99), subdivide large floor into 256x256 spatial tiles
			// so obstacle cutlines remain strictly localized within their respective grid cells.
			std::vector<winding_t> base_tiles;
			if ( normal.z > 0.99 ) {
				Nav_SubdivideWindingIntoSpatialTiles( w, 256.0, normal, base_tiles );
			} else {
				base_tiles.push_back( w );
			}

			// Process each spatial tile independently.
			for ( const winding_t &tile : base_tiles ) {
				std::vector<winding_t> fragments;
				fragments.push_back( tile );

				// Calculate 2D bounding box and floor Z height for this tile.
				Vector3DP tile_mins = tile.points[ 0 ];
				Vector3DP tile_maxs = tile.points[ 0 ];
				for ( int32_t p = 1; p < tile.num_points; p++ ) {
					tile_mins.x = std::min<double>( tile_mins.x, tile.points[ p ].x );
					tile_mins.y = std::min<double>( tile_mins.y, tile.points[ p ].y );
					tile_mins.z = std::min<double>( tile_mins.z, tile.points[ p ].z );
					tile_maxs.x = std::max<double>( tile_maxs.x, tile.points[ p ].x );
					tile_maxs.y = std::max<double>( tile_maxs.y, tile.points[ p ].y );
					tile_maxs.z = std::max<double>( tile_maxs.z, tile.points[ p ].z );
				}
				const double floor_max_z = tile_maxs.z;

				// Pass 1: Extract dynamic door transition footprints on this tile.
				for ( size_t other_instance_index = 0; other_instance_index < brush_instances.size(); other_instance_index++ ) {
					if ( other_instance_index == instance_brush_index ) {
						continue;
					}

					const nav_brush_ownership_t &other_instance = brush_instances[ other_instance_index ];
					if ( !AreBrushesCompatibleForClipping( brush_instance, other_instance ) ) {
						continue;
					}

					if ( !IsDynamicTransitionBrush( other_instance ) ) {
						continue;
					}

					mbrush_t *other_b = &bsp->brushes[ other_instance.brush_num ];
					if ( !( other_b->contents & ( CONTENTS_SOLID | CONTENTS_DETAIL | CONTENTS_MONSTERCLIP ) ) ) {
						continue;
					}

					if ( IsContainedDynamicTransitionBrush( bsp, brush_instances, other_instance_index ) ) {
						s_nav_generation_diagnostics.contained_dynamic_clip_skips++;
						continue;
					}

					const std::vector<bool> other_plane_active = GetBrushActivePlanes( other_b, other_instance );
					bool intersectsFragment = false;
					for ( const winding_t &fragment : fragments ) {
						if ( !IsFragmentCompletelyOutsideBrush( &fragment, other_b, other_instance, other_plane_active, 4.0 ) ) {
							intersectsFragment = true;
							break;
						}
					}
					if ( !intersectsFragment ) {
						continue;
					}

					SplitWindingsByEntityBrush( fragments, other_b, other_instance, 0.0 );
				}

				// Pass 2: Subtract only static solid obstacles that ACTUALLY OVERLAP THIS TILE!
				for ( size_t other_instance_index = 0; other_instance_index < brush_instances.size(); other_instance_index++ ) {
					if ( other_instance_index == instance_brush_index ) {
						continue;
					}

					const nav_brush_ownership_t &other_instance = brush_instances[ other_instance_index ];
					if ( !AreBrushesCompatibleForClipping( brush_instance, other_instance ) ) {
						continue;
					}

					if ( IsDynamicTransitionBrush( other_instance ) ) {
						continue;
					}

					mbrush_t *other_b = &bsp->brushes[ other_instance.brush_num ];
					if ( !( other_b->contents & ( CONTENTS_SOLID | CONTENTS_DETAIL | CONTENTS_MONSTERCLIP ) ) ) {
						continue;
					}

					// Fast 3D obstacle bounding-box filter:
					// An obstacle brush can only block this walkable surface if:
					// 1. It horizontally overlaps this tile.
					// 2. Its top is higher than a walkable step (> NAV_MAX_STEP_HEIGHT above this floor).
					// 3. Its bottom descends low enough to intersect the agent's body (< 56 units above floor).
					Vector3DP other_mins = {}, other_maxs = {};
					if ( GetBrushWorldBounds( other_b, other_instance, &other_mins, &other_maxs ) ) {
						// 1. Horizontal overlap test
						if ( other_maxs.x <= tile_mins.x - 0.1 || other_mins.x >= tile_maxs.x + 0.1 ||
							 other_maxs.y <= tile_mins.y - 0.1 || other_mins.y >= tile_maxs.y + 0.1 ) {
							continue;
						}

						// 2. Obstacles below or at floor level: if top is at or below this floor, it cannot block this surface.
						if ( other_maxs.z <= floor_max_z + 0.1 ) {
							continue;
						}

						// 3. Overhead ceiling check: if bottom is above agent head clearance (56 units), do not subtract!
						constexpr double AGENT_HEAD_CLEARANCE = 56.0;
						if ( other_mins.z >= floor_max_z + AGENT_HEAD_CLEARANCE - 0.1 ) {
							continue;
						}
					}

					SubtractBrushFromWindings( fragments, other_b, other_instance, normal );

					if ( fragments.empty() ) {
						break;
					}
				}

				// Merge local fragments within this tile to eliminate redundant seams.
				bool merged = true;
				while ( merged && fragments.size() > 1 ) {
					merged = false;
					for ( size_t f1 = 0; f1 < fragments.size() && !merged; f1++ ) {
						for ( size_t f2 = f1 + 1; f2 < fragments.size(); f2++ ) {
							winding_t merged_w;
							if ( TryMergeWindings( fragments[ f1 ], fragments[ f2 ], Vector3DP( normal ), &merged_w ) ) {
								fragments[ f1 ] = merged_w;
								fragments.erase( fragments.begin() + f2 );
								merged = true;
								break;
							}
						}
					}
				}

				// Add surviving fragments for this tile.
				for ( const winding_t &frag : fragments ) {
					if ( frag.num_points >= 3 ) {
						double area = 0.0;
						Vector3DP mins = frag.points[ 0 ];
						Vector3DP maxs = frag.points[ 0 ];
						for ( int32_t v = 0; v < frag.num_points; v++ ) {
							Vector3DP p = frag.points[ v ];
							mins.x = std::min<double>( mins.x, p.x );
							mins.y = std::min<double>( mins.y, p.y );
							mins.z = std::min<double>( mins.z, p.z );
							maxs.x = std::max<double>( maxs.x, p.x );
							maxs.y = std::max<double>( maxs.y, p.y );
							maxs.z = std::max<double>( maxs.z, p.z );

							if ( v >= 2 ) {
								Vector3DP cross = QM_Vector3CrossProductDP( Vector3DP( frag.points[ v - 1 ] ) - Vector3DP( frag.points[ 0 ] ), p - Vector3DP( frag.points[ 0 ] ) );
								area += 0.5 * QM_Vector3LengthDP( cross );
							}
						}

						double dx = maxs.x - mins.x;
						double dy = maxs.y - mins.y;
						double longest = std::max<double>( dx, dy );

						if ( area < 0.001 || ( longest > 0.001 && ( area / longest ) < 0.01 ) ) {
							sliver_pruned_fragments++;
							continue;
						}
					}

					nav_poly_t poly = {};
					poly.poly_id = g_nav_polys.size();
					poly.num_vertices = std::min( frag.num_points, MAX_WINDING_POINTS );
					Vector3DP center( 0, 0, 0 );
					for ( int32_t v = 0; v < poly.num_vertices; v++ ) {
						poly.vertices[ v ] = static_cast< Vector3DP >( frag.points[ v ] );
						center = center + Vector3DP( poly.vertices[ v ] );
					}
					poly.center = static_cast< Vector3DP >( center / static_cast< double >( poly.num_vertices ) );
					poly.normal = static_cast< Vector3DP >( normal );
					poly.entity_id = frag.entity_id;
					poly.transition_entity_id = frag.transition_entity_id;
					poly.bsp_leaf_id = 0;

					if ( !EnsureNavPolygonCCW( poly ) ) {
						sliver_pruned_fragments++;
						continue;
					}

					center = Vector3DP( 0, 0, 0 );
					for ( int32_t v = 0; v < poly.num_vertices; v++ ) {
						center = center + Vector3DP( poly.vertices[ v ] );
					}
					poly.center = static_cast< Vector3DP >( center / static_cast< double >( poly.num_vertices ) );

					if ( ( poly.entity_id != ENTITYNUM_NONE || poly.transition_entity_id != ENTITYNUM_NONE ) && printed_dynamic_footprints < 16 ) {
						double footprint_area = 0.0;
						Vector3DP footprint_mins = Vector3DP( poly.vertices[ 0 ] );
						Vector3DP footprint_maxs = Vector3DP( poly.vertices[ 0 ] );
						for ( int32_t footprint_vertex_index = 0; footprint_vertex_index < poly.num_vertices; footprint_vertex_index++ ) {
							const Vector3DP footprint_vertex = Vector3DP( poly.vertices[ footprint_vertex_index ] );
							footprint_mins.x = std::min<double>( footprint_mins.x, footprint_vertex.x );
							footprint_mins.y = std::min<double>( footprint_mins.y, footprint_vertex.y );
							footprint_mins.z = std::min<double>( footprint_mins.z, footprint_vertex.z );
							footprint_maxs.x = std::max<double>( footprint_maxs.x, footprint_vertex.x );
							footprint_maxs.y = std::max<double>( footprint_maxs.y, footprint_vertex.y );
							footprint_maxs.z = std::max<double>( footprint_maxs.z, footprint_vertex.z );
							if ( footprint_vertex_index >= 2 ) {
								const Vector3DP footprint_cross = QM_Vector3CrossProductDP( Vector3DP( poly.vertices[ footprint_vertex_index - 1 ] ) - Vector3DP( poly.vertices[ 0 ] ), footprint_vertex - Vector3DP( poly.vertices[ 0 ] ) );
								footprint_area += 0.5 * QM_Vector3LengthDP( footprint_cross );
							}
						}

						gi.dprintf( "NavMesh DynamicFootprint [sourceBrush=%d side=%d entity=%d transition=%d vertices=%d area=%.1f bounds=(%.1f %.1f %.1f)->(%.1f %.1f %.1f)]\n",
							brush_num,
							j,
							poly.entity_id,
							poly.transition_entity_id,
							poly.num_vertices,
							footprint_area,
							footprint_mins.x,
							footprint_mins.y,
							footprint_mins.z,
							footprint_maxs.x,
							footprint_maxs.y,
							footprint_maxs.z );
						printed_dynamic_footprints++;
					}

					// Track how much geometry each ownership class contributes before later partitioning and splicing.
					s_nav_generation_diagnostics.extracted_polys++;
					if ( poly.entity_id != ENTITYNUM_NONE ) {
						s_nav_generation_diagnostics.extracted_dynamic_polys++;
						s_nav_generation_diagnostics.extracted_polys_by_entity[ poly.entity_id ]++;
						s_nav_generation_diagnostics.extracted_dynamic_entities = static_cast< int32_t >( s_nav_generation_diagnostics.extracted_polys_by_entity.size() );
					}

					g_nav_polys.push_back( poly );
				}
			}
		}
	}

	// Print a summary of the extraction process to the server console for debugging and verification.
	gi.dprintf( "Nav_DoExtractionWork: Checked %d brushes. Found %d solid/detail brushes, %d playerclip-only brushes, %d walkable sides. Extracted %d polys, pruned %d sliver fragments.\n",
		bsp->numbrushes, solid_brushes, playerclip_brushes, walkable_sides, ( int )g_nav_polys.size(), sliver_pruned_fragments );
	LogNavGenerationDiagnostics( "Extraction" );
}

/**
*	@brief	Determine whether two polygons meet across a runtime dynamic/world portal boundary.
*	@param	first	First polygon ownership to classify.
*	@param	second	Second polygon ownership to classify.
*	@return	True when exactly one polygon belongs to a dynamic mover.
*	@note	This deliberately excludes world/world and dynamic/dynamic seams. It is used
*			only to make door perimeters conformal before half-edge construction.
**/
static bool AreNavPolygonsDynamicPortalNeighbors( const nav_poly_t &first, const nav_poly_t &second ) {
	const bool first_is_dynamic = first.entity_id != ENTITYNUM_NONE;
	const bool second_is_dynamic = second.entity_id != ENTITYNUM_NONE;
	return first_is_dynamic != second_is_dynamic;
}

/**
*	@brief	Insert a projected source vertex into one coincident target edge without moving either surface.
*	@param	target	Polygon whose edge may need subdivision.
*	@param	edge_index	Index of the target edge start vertex.
*	@param	source_vertex	Existing vertex from the opposite portal polygon.
*	@return	True when one valid target-edge subdivision was committed.
*	@note	The inserted point stays on the target polygon's own plane. This preserves the
*			door floor and surrounding world floor elevations while producing matching XY spans.
**/
static bool InsertDynamicPortalConformanceVertex( nav_poly_t &target, const int32_t edge_index, const Vector3DP &source_vertex ) {
	/**
	*	Ensure that there is capacity for one additional collinear perimeter vertex.
	**/
	if ( target.num_vertices < 3 || target.num_vertices >= MAX_WINDING_POINTS || edge_index < 0 || edge_index >= target.num_vertices ) {
		return false;
	}

	const Vector3DP edge_start = Vector3DP( target.vertices[ edge_index ] );
	const Vector3DP edge_end = Vector3DP( target.vertices[ ( edge_index + 1 ) % target.num_vertices ] );
	Vector3DP edge_direction_2d = edge_end - edge_start;
	edge_direction_2d.z = 0.0;
	const double edge_length_sqr_2d = QM_Vector3LengthSqrDP( edge_direction_2d );
	if ( edge_length_sqr_2d <= 0.001 ) {
		return false;
	}

	/**
	*	Require a strict interior projection on the target segment. Endpoints are already
	*	conformal and must not be duplicated into a zero-length half-edge.
	**/
	Vector3DP start_to_source_2d = source_vertex - edge_start;
	start_to_source_2d.z = 0.0;
	const double edge_fraction = QM_Vector3DotProductDP( start_to_source_2d, edge_direction_2d ) / edge_length_sqr_2d;
	if ( edge_fraction <= 0.001 || edge_fraction >= 0.999 ) {
		return false;
	}

	const Vector3DP projected_vertex = QM_Vector3MultiplyAddDP( edge_start, edge_fraction, edge_end - edge_start );
	Vector3DP projected_vertex_2d = projected_vertex;
	projected_vertex_2d.z = 0.0;
	Vector3DP source_vertex_2d = source_vertex;
	source_vertex_2d.z = 0.0;
	// A portal conformance vertex must already lie closely on the same projected boundary line.
	if ( QM_Vector3DistanceSqrDP( source_vertex_2d, projected_vertex_2d ) > 16.0 ) {
		return false;
	}

	// Dynamic entity footprints and their adjoining world floors must share one walkable elevation, or be within stepping distance.
	if ( std::fabs( source_vertex.z - projected_vertex.z ) > ( NAV_MAX_STEP_SIZE + 4.0 ) ) {
		return false;
	}

	/**
	*	Insert only the target-plane projection. The source polygon remains untouched,
	*	which prevents portal conformance from producing triangular geometry.
	**/
	nav_poly_t candidate = target;
	for ( int32_t vertex_index = candidate.num_vertices; vertex_index > edge_index + 1; vertex_index-- ) {
		candidate.vertices[ vertex_index ] = candidate.vertices[ vertex_index - 1 ];
	}
	candidate.vertices[ edge_index + 1 ] = static_cast< Vector3DP >( projected_vertex );
	candidate.num_vertices++;
	if ( !IsValidNavPolygon( candidate, Vector3DP( candidate.normal ) ) ) {
		return false;
	}

	target = candidate;
	RecomputeNavPolygonCenter( target );
	return true;
}

/**
*	@brief	Make dynamic/world doorway perimeter segments conformal before half-edge pairing.
*	@note	Only existing vertices are inserted on coincident same-height portal edges.
*			No polygon is translated, clipped, or connected across unrelated geometry.
**/
static void Nav_ResolveDynamicPortalTJunctions() {
	/**
	*	A portal conformance pass has no work without at least two extracted polygons.
	**/
	if ( g_nav_polys.size() < 2 ) {
		return;
	}

	int32_t conformance_splices = 0;
	bool made_change = true;
	while ( made_change && conformance_splices < 10000 ) {
		made_change = false;

		/**
		*	Compare opposite dynamic/world polygon boundaries. Restart after every successful
		*	edit so subsequent comparisons use the current, validated winding topology.
		**/
		for ( size_t target_index = 0; target_index < g_nav_polys.size() && !made_change; target_index++ ) {
			for ( size_t source_index = 0; source_index < g_nav_polys.size() && !made_change; source_index++ ) {
				if ( target_index == source_index || !AreNavPolygonsDynamicPortalNeighbors( g_nav_polys[ target_index ], g_nav_polys[ source_index ] ) ) {
					continue;
				}

				for ( int32_t source_vertex_index = 0; source_vertex_index < g_nav_polys[ source_index ].num_vertices && !made_change; source_vertex_index++ ) {
					const Vector3DP source_vertex = Vector3DP( g_nav_polys[ source_index ].vertices[ source_vertex_index ] );
					for ( int32_t target_edge_index = 0; target_edge_index < g_nav_polys[ target_index ].num_vertices; target_edge_index++ ) {
						if ( InsertDynamicPortalConformanceVertex( g_nav_polys[ target_index ], target_edge_index, source_vertex ) ) {
							conformance_splices++;
							made_change = true;
							break;
						}
					}
				}
			}
		}
	}

	if ( conformance_splices > 0 ) {
		gi.dprintf( "NavMesh: Conformed %d dynamic portal edge vertices.\n", conformance_splices );
	}
}

/**
*	@brief	Resolve T-junctions by splicing missing vertices into adjacent polygons.
*	@note	This keeps the later half-edge mesh conformal and guarantees 1-to-1 twin half-edge connectivity.
**/
static void Nav_ResolveTJunctionsByEdgeSplicing() {
	// Early return if no polygons exist to process.
	if ( g_nav_polys.empty() ) return;

	struct PolyAABB {
		double min_x, min_y, min_z, max_x, max_y, max_z;
	};

	// Strict coplanar snapping tolerance used when both polygons share one geometric plane.
	static constexpr double MAX_PLANAR_POINT_DISTANCE = 0.25;
	// Maximum vertical separation eligible for projected step/slope edge subdivision.
	static constexpr double MAX_PROJECTED_STEP_SPLICE_HEIGHT = NAV_MAX_STEP_SIZE;
	// Maximum horizontal distance from an existing edge for a true T-junction vertex.
	static constexpr double MAX_EDGE_PROJECTION_DISTANCE = 0.5;
	// Include walkable step-height neighbors in the spatial index so their 2D seam vertices can conform.
	static constexpr double SPATIAL_INDEX_Z_PADDING = MAX_PROJECTED_STEP_SPLICE_HEIGHT;
	std::vector<PolyAABB> aabbs( g_nav_polys.size() );
	auto UpdateAABB = []( const nav_poly_t &p, PolyAABB &aabb ) {
		if ( p.num_vertices <= 0 ) {
			aabb = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			return;
		}
		double min_x = p.vertices[ 0 ].x, min_y = p.vertices[ 0 ].y, min_z = p.vertices[ 0 ].z;
		double max_x = p.vertices[ 0 ].x, max_y = p.vertices[ 0 ].y, max_z = p.vertices[ 0 ].z;
		for ( int32_t k = 1; k < p.num_vertices; k++ ) {
			min_x = std::min<double>( min_x, p.vertices[ k ].x );
			min_y = std::min<double>( min_y, p.vertices[ k ].y );
			min_z = std::min<double>( min_z, p.vertices[ k ].z );
			max_x = std::max<double>( max_x, p.vertices[ k ].x );
			max_y = std::max<double>( max_y, p.vertices[ k ].y );
			max_z = std::max<double>( max_z, p.vertices[ k ].z );
		}
		aabb = { min_x - 4.0, min_y - 4.0, min_z - SPATIAL_INDEX_Z_PADDING, max_x + 4.0, max_y + 4.0, max_z + SPATIAL_INDEX_Z_PADDING };
		};

	struct GridCell {
		std::vector<int32_t> polys;
	};
	std::unordered_map<int64_t, GridCell> grid;
	static constexpr double cell_size = 256.0;

	for ( size_t i = 0; i < g_nav_polys.size(); i++ ) {
		UpdateAABB( g_nav_polys[ i ], aabbs[ i ] );
		int32_t min_cx = ( int )std::floor( aabbs[ i ].min_x / cell_size );
		int32_t max_cx = ( int )std::floor( aabbs[ i ].max_x / cell_size );
		int32_t min_cy = ( int )std::floor( aabbs[ i ].min_y / cell_size );
		int32_t max_cy = ( int )std::floor( aabbs[ i ].max_y / cell_size );
		int32_t min_cz = ( int )std::floor( aabbs[ i ].min_z / cell_size );
		int32_t max_cz = ( int )std::floor( aabbs[ i ].max_z / cell_size );
		for ( int32_t cx = min_cx; cx <= max_cx; cx++ ) {
			for ( int32_t cy = min_cy; cy <= max_cy; cy++ ) {
				for ( int32_t cz = min_cz; cz <= max_cz; cz++ ) {
					int64_t key = ( cx * 73856093 ) ^ ( cy * 19349663 ) ^ ( cz * 83492791 );
					grid[ key ].polys.push_back( ( int32_t )i );
				}
			}
		}
	}

	int32_t totalSplices = 0;
	bool made_change = true;
	int32_t pass_num = 0;

	while ( made_change && totalSplices < 1000 && pass_num < 4 ) {
		made_change = false;
		pass_num++;

		for ( size_t i = 0; i < g_nav_polys.size() && !made_change; i++ ) {
			// Dynamic union interiors are authored portal boundaries and must not be reshaped by world T-junction repair.
			if ( g_nav_polys[ i ].entity_id != ENTITYNUM_NONE ) {
				continue;
			}
			bool poly_modified = false;

			for ( int32_t e = 0; e < g_nav_polys[ i ].num_vertices && !poly_modified; e++ ) {
				Vector3DP pA( g_nav_polys[ i ].vertices[ e ] );
				Vector3DP pB( g_nav_polys[ i ].vertices[ ( e + 1 ) % g_nav_polys[ i ].num_vertices ] );

				Vector3DP edgeVec = QM_Vector3SubtractDP( pB, pA );
				double edgeLenSqr = QM_Vector3LengthSqrDP( edgeVec );

				if ( edgeLenSqr < 1.0 ) continue;

				Vector3DP pA_2d( pA.x, pA.y, 0.0 );
				Vector3DP pB_2d( pB.x, pB.y, 0.0 );
				Vector3DP edgeVec_2d = QM_Vector3SubtractDP( pB_2d, pA_2d );
				double edgeLenSqr_2d = QM_Vector3LengthSqrDP( edgeVec_2d );

				if ( edgeLenSqr_2d < 0.001 ) continue;

				int32_t min_cx = ( int )std::floor( aabbs[ i ].min_x / cell_size );
				int32_t max_cx = ( int )std::floor( aabbs[ i ].max_x / cell_size );
				int32_t min_cy = ( int )std::floor( aabbs[ i ].min_y / cell_size );
				int32_t max_cy = ( int )std::floor( aabbs[ i ].max_y / cell_size );
				int32_t min_cz = ( int )std::floor( aabbs[ i ].min_z / cell_size );
				int32_t max_cz = ( int )std::floor( aabbs[ i ].max_z / cell_size );

				std::vector<int32_t> checked_j; // Simple deduplication for cell overlap

				for ( int32_t cx = min_cx; cx <= max_cx && !poly_modified; cx++ ) {
					for ( int32_t cy = min_cy; cy <= max_cy && !poly_modified; cy++ ) {
						for ( int32_t cz = min_cz; cz <= max_cz && !poly_modified; cz++ ) {
							int64_t key = ( cx * 73856093 ) ^ ( cy * 19349663 ) ^ ( cz * 83492791 );
							auto it = grid.find( key );
							if ( it == grid.end() ) continue;

							for ( int32_t j : it->second.polys ) {
								if ( poly_modified ) break;
								if ( i == j ) continue;
								bool already_checked = false;
								for ( int32_t c : checked_j ) {
									if ( c == j ) { already_checked = true; break; }
								}
								if ( already_checked ) continue;
								checked_j.push_back( j );

								if ( aabbs[ j ].max_x < aabbs[ i ].min_x || aabbs[ j ].min_x > aabbs[ i ].max_x ||
									aabbs[ j ].max_y < aabbs[ i ].min_y || aabbs[ j ].min_y > aabbs[ i ].max_y ||
									aabbs[ j ].max_z < aabbs[ i ].min_z || aabbs[ j ].min_z > aabbs[ i ].max_z ) {
									continue;
								}

								/**
								*   A T-junction repair is always valid for coplanar neighbors.
								*   For non-coplanar neighbors (e.g. floor<->slope seams), keep the
								*   repair path open as long as both faces are walkable and not
								*   opposing each other.
								**/
								const bool surfaces_are_coplanar = AreNavPolygonsCoplanar( g_nav_polys[ i ], g_nav_polys[ j ] );
								if ( !surfaces_are_coplanar ) {
									if ( g_nav_polys[ i ].normal.z < NAV_MIN_WALKABLE_Z || g_nav_polys[ j ].normal.z < NAV_MIN_WALKABLE_Z ) {
										continue;
									}
									const double normal_dot = QM_Vector3DotProductDP( g_nav_polys[ i ].normal, g_nav_polys[ j ].normal );
									if ( normal_dot < 0.0 ) {
										continue;
									}
								}

								for ( int32_t vj = 0; vj < g_nav_polys[ j ].num_vertices && !poly_modified; vj++ ) {
									Vector3DP vC( g_nav_polys[ j ].vertices[ vj ] );
									Vector3DP vC_2d( vC.x, vC.y, 0.0 );

									Vector3DP toC_2d = QM_Vector3SubtractDP( vC_2d, pA_2d );
									double t = QM_Vector3DotProductDP( toC_2d, edgeVec_2d ) / edgeLenSqr_2d;

									if ( t > 0.0 && t < 1.0 ) {
										Vector3DP projC_2d = QM_Vector3MultiplyAddDP( pA_2d, t, edgeVec_2d );

										if ( QM_Vector3DistanceSqrDP( vC_2d, projC_2d ) < ( MAX_EDGE_PROJECTION_DISTANCE * MAX_EDGE_PROJECTION_DISTANCE ) ) {
											Vector3DP projC_3d = QM_Vector3MultiplyAddDP( pA, t, edgeVec );
											double dz = static_cast< double >( std::abs( vC.z - projC_3d.z ) );

											// Non-coplanar walkable seams may differ by one step height, but must retain their individual surface elevations.
											const double max_point_distance = surfaces_are_coplanar ? MAX_PLANAR_POINT_DISTANCE : MAX_PROJECTED_STEP_SPLICE_HEIGHT;
											if ( dz <= max_point_distance ) {
												if ( surfaces_are_coplanar && QM_Vector3DistanceSqrDP( pA_2d, projC_2d ) <= ( MAX_EDGE_PROJECTION_DISTANCE * MAX_EDGE_PROJECTION_DISTANCE ) ) {
													// Snap vC to corner pA
													nav_poly_t candidate = g_nav_polys[ j ];
													candidate.vertices[ vj ] = ( pA );
													if ( !IsValidNavPolygon( candidate, Vector3DP( candidate.normal ) ) ) {
														continue;
													}
													g_nav_polys[ j ] = candidate;
													RecomputeNavPolygonCenter( g_nav_polys[ j ] );
													UpdateAABB( g_nav_polys[ j ], aabbs[ j ] );
													totalSplices++;
													if ( g_nav_polys[ j ].entity_id == ENTITYNUM_NONE ) {
														s_nav_generation_diagnostics.world_splices++;
													} else {
														s_nav_generation_diagnostics.dynamic_splices++;
														s_nav_generation_diagnostics.splices_by_entity[ g_nav_polys[ j ].entity_id ]++;
													}
												} else if ( surfaces_are_coplanar && QM_Vector3DistanceSqrDP( pB_2d, projC_2d ) <= ( MAX_EDGE_PROJECTION_DISTANCE * MAX_EDGE_PROJECTION_DISTANCE ) ) {
													// Snap vC to corner pB
													nav_poly_t candidate = g_nav_polys[ j ];
													candidate.vertices[ vj ] = ( pB );
													if ( !IsValidNavPolygon( candidate, Vector3DP( candidate.normal ) ) ) {
														continue;
													}
													g_nav_polys[ j ] = candidate;
													RecomputeNavPolygonCenter( g_nav_polys[ j ] );
													UpdateAABB( g_nav_polys[ j ], aabbs[ j ] );
													totalSplices++;
													if ( g_nav_polys[ j ].entity_id == ENTITYNUM_NONE ) {
														s_nav_generation_diagnostics.world_splices++;
													} else {
														s_nav_generation_diagnostics.dynamic_splices++;
														s_nav_generation_diagnostics.splices_by_entity[ g_nav_polys[ j ].entity_id ]++;
													}
												} else {
													nav_poly_t newPoly = g_nav_polys[ i ];
													if ( newPoly.num_vertices < MAX_WINDING_POINTS ) {
														for ( int32_t k = newPoly.num_vertices; k > e + 1; k-- ) {
															newPoly.vertices[ k ] = newPoly.vertices[ k - 1 ];
														}
														newPoly.vertices[ e + 1 ] = ( projC_3d );
														newPoly.num_vertices++;

														if ( !IsValidNavPolygon( newPoly, Vector3DP( newPoly.normal ) ) ) {
															continue;
														}

														/**
														*   A step/slope splice aligns the projected edge span only. Do not snap the
														*   neighboring vertex to this face's Z value, because that would flatten the
														*   receiving step and recreate the blocked-edge regression.
														**/
														if ( !surfaces_are_coplanar ) {
															g_nav_polys[ i ] = newPoly;
															RecomputeNavPolygonCenter( g_nav_polys[ i ] );
															UpdateAABB( g_nav_polys[ i ], aabbs[ i ] );

															poly_modified = true;
															totalSplices++;
															s_nav_generation_diagnostics.world_splices++;
															continue;
														}

														// Validate the neighboring polygon before committing either side of the splice.
														nav_poly_t candidate = g_nav_polys[ j ];
														candidate.vertices[ vj ] = ( projC_3d );
														if ( !IsValidNavPolygon( candidate, Vector3DP( candidate.normal ) ) ) {
															continue;
														}

														// Snap vC to perfectly align with the new spliced point in 2D to guarantee TwinLinking.
														g_nav_polys[ i ] = newPoly;
														RecomputeNavPolygonCenter( g_nav_polys[ i ] );
														UpdateAABB( g_nav_polys[ i ], aabbs[ i ] );
														g_nav_polys[ j ] = candidate;
														RecomputeNavPolygonCenter( g_nav_polys[ j ] );
														UpdateAABB( g_nav_polys[ j ], aabbs[ j ] );

														poly_modified = true;
														totalSplices++;
														if ( g_nav_polys[ j ].entity_id == ENTITYNUM_NONE ) {
															s_nav_generation_diagnostics.world_splices++;
														} else {
															s_nav_generation_diagnostics.dynamic_splices++;
															s_nav_generation_diagnostics.splices_by_entity[ g_nav_polys[ j ].entity_id ]++;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}

			if ( totalSplices > 10000 ) {
				gi.dprintf( "NavMesh WARNING: Aborting Edge Splicing due to infinite loop! (Splices > 10000)\n" );
				break;
			}

			if ( poly_modified && i > 0 ) {
				// Re-process the preceding polygon so newly inserted vertices can participate in another splice.
				i--;
			}
		}
	}

	if ( totalSplices > 0 ) {
		gi.dprintf( "NavMesh: Spliced %d T-Junction vertices.\n", totalSplices );
	}
	LogNavGenerationDiagnostics( "Splice" );
}

/**
* @brief Build the half-edge mesh from the extracted nav polygons.
* @note This is the second stage of generation and produces the pathfinding graph.
**/
void Nav_BuildHalfEdgeMesh() {
	/**
	*	Ensure these are cleared out before we start building the half-edge mesh.
	**/
	g_nav_vertices.clear();
	g_nav_halfedges.clear();
	g_nav_faces.clear();
	// The registry stores half-edge indices that become stale whenever a new mesh is rebuilt.
	g_nav_entity_edges.clear();

	g_nav_vertices.reserve( g_nav_polys.size() * 4 );
	g_nav_halfedges.reserve( g_nav_polys.size() * 4 );
	g_nav_faces.reserve( g_nav_polys.size() );
	// Cancel early if there are no polygons to process.
	if ( g_nav_polys.empty() ) {
		return;
	}

	/**
	*	Phase 1: Iteratively merge adjacent coplanar polygons (dissolves temporary BSP cut seams).
	*			Merging coplanar world floor pieces directly ensures large open terrain areas remain unified
	*			and continuous before dynamic door portal conformance and T-junction splicing.
	**/
	Nav_MergeCoplanarPolygons();
	Nav_SetGenerationProgress( 0.48f );

	/**
	*	Phase 2: Dissolve surviving narrow diagonal slivers into adjacent coplanar neighbors.
	**/
	Nav_DissolveSlivers();
	Nav_SetGenerationProgress( 0.53f );

	/**
	*	Phase 3: Simplify collinear perimeter vertices across all world and entity polygons to restore
	*			straight edges along door thresholds and merged floor boundaries.
	**/
	SimplifyAllNavPolygonsCollinearVertices();
	Nav_SetGenerationProgress( 0.55f );

	/**
	*	Phase 4: Conform dynamic/world doorway perimeter segments now that world floors are merged and clean.
	**/
	Nav_ResolveDynamicPortalTJunctions();
	Nav_SetGenerationProgress( 0.58f );

	/**
	*	Phase 5: Splice T-junctions so remaining adjacent coplanar edges receive matching split vertices.
	**/
	Nav_ResolveTJunctionsByEdgeSplicing();
	Nav_SetGenerationProgress( 0.63f );

	/**
	*	Phase 6: Retain spliced vertices from Phase 4 and Phase 5 intact so adjacent edges match 1-to-1 in 2D for twin linking.
	**/
	Nav_SetGenerationProgress( 0.65f );


	// Spatial hash grid to deduplicate vertices and prevent T-junctions.
	std::unordered_map<int64_t, std::vector<int32_t>> vertex_grid;
	int32_t first_pass_twin_links = 0;
	int32_t second_pass_twin_links = 0;
	int32_t deterministic_twin_links = 0;

	/**
	*	@brief	Hash a vertex position into the deduplication grid.
	*	@details	We enforce exact 3D matching (with a tiny floating point epsilon) to prevent
	*			"canonical vertex drift" where an entire staircase merges into a single vertex.
	**/
	auto GetVertexIndex = [&]( const Vector3DP &p ) -> int32_t {
		// Use a 4-unit grid to hash vertices, allowing for very close proximity without merging.
		static constexpr double GRID_SIZE = 4.0;
		int64_t cx = ( int64_t )std::floor( p.x / GRID_SIZE );
		int64_t cy = ( int64_t )std::floor( p.y / GRID_SIZE );
		int64_t cz = ( int64_t )std::floor( p.z / GRID_SIZE );

		// Check neighboring grid cells to find an existing vertex that is very close to p.
		for ( int64_t ox = -1; ox <= 1; ox++ ) {
			// Check neighboring cells in X, Y, Z directions.
			for ( int64_t oy = -1; oy <= 1; oy++ ) {
				// Check neighboring cells in Z direction.
				for ( int64_t oz = -1; oz <= 1; oz++ ) {
					// Compute a unique hash key for the neighboring cell.
					int64_t key = ( ( cx + ox ) * 73856093 ) ^ ( ( cy + oy ) * 19349663 ) ^ ( ( cz + oz ) * 83492791 );
					// Look up the cell in the vertex grid
					auto it = vertex_grid.find( key );
					// If the cell exists, check each vertex index in that cell.
					if ( it != vertex_grid.end() ) {
						// Compare the position of each vertex in the cell to p.
						for ( int32_t idx : it->second ) {
							// Calculate the distance for each axis between the existing vertex and p.
							double dx = g_nav_vertices[ idx ].x - p.x;
							double dy = g_nav_vertices[ idx ].y - p.y;
							double dz = g_nav_vertices[ idx ].z - p.z;
							// If the squared distance is less than 0.000001, consider it a match and return the index.
							if ( dx * dx + dy * dy + dz * dz < 0.000001 ) {
								return idx;
							}
						}
					}
				}
			}
		}

		// If no existing vertex is found, add p as a new vertex and return its index.
		int32_t new_idx = ( int32_t )g_nav_vertices.size();
		// Add the new vertex to the global vertex list.
		g_nav_vertices.push_back( p );

		// Generate a unique hash key for the new vertex's grid cell.
		int64_t key = ( cx * 73856093 ) ^ ( cy * 19349663 ) ^ ( cz * 83492791 );
		// Add the new vertex index to the appropriate cell in the vertex grid.
		vertex_grid[ key ].push_back( new_idx );

		// Return the index of the newly added vertex.
		return new_idx;
		};

	/**
	*	Build the half-edge mesh from the partitioned polygons:
	*		1) Deduplicate vertices and store in g_nav_vertices
	*		2) Create half-edges for each polygon edge, linking to vertices and faces
	*		3) Compute face clearance (center to nearest edge)
	*		4) Perform twin linking to connect adjacent edges across polygons
	**/
	for ( size_t i = 0; i < g_nav_polys.size(); i++ ) {
		// Update mesh building progress (mapping to 0.65f..0.70f).
		if ( !g_nav_polys.empty() ) {
			const float pct = 0.65f + 0.05f * ( static_cast< float >( i ) / static_cast< float >( g_nav_polys.size() ) );
			Nav_SetGenerationProgress( pct );
		}

		const nav_poly_t &poly = g_nav_polys[ i ];

		/**
		*	Create a new face for this polygon
		**/
		nav_face_t face = {};
		face.face_id = i;
		face.num_edges = poly.num_vertices;
		face.center = poly.center;
		face.normal = poly.normal;
		face.entity_id = poly.entity_id;
		face.transition_entity_id = poly.transition_entity_id;
		face.bsp_leaf_id = poly.bsp_leaf_id;
		face.brush_id = static_cast< uint32_t >( poly.poly_id ); // Back-pointer to originating brush / poly ID
		face.last_query_id = 0; // Initialize Ray ID mailbox tag
		face.first_edge_idx = g_nav_halfedges.size();

		/**
		*	Check for deduplicated vertices and store their indices for the half-edges.
		**/
		std::vector<int32_t> v_indices( poly.num_vertices );
		for ( int32_t v = 0; v < poly.num_vertices; v++ ) {
			v_indices[ v ] = GetVertexIndex( Vector3DP( poly.vertices[ v ] ) );
		}

		/**
		*	Compute clearance for this face (distance from center to nearest edge).
		**/
		double max_poly_radius = 0.0;
		for ( int32_t v = 0; v < poly.num_vertices; v++ ) {
			max_poly_radius = std::max<double>( max_poly_radius, QM_Vector3DistanceDP( poly.center, poly.vertices[ v ] ) );
		}
		double min_dist = max_poly_radius;
		for ( int32_t v = 0; v < poly.num_vertices; v++ ) {
			Vector3DP a = Vector3DP( poly.vertices[ v ] );
			Vector3DP b = Vector3DP( poly.vertices[ ( v + 1 ) % poly.num_vertices ] );

			Vector3DP edge = b - a;
			Vector3DP toCenter = Vector3DP( face.center ) - a;
			double edgeLenSq = QM_Vector3LengthSqrDP( edge );
			double dist = 0.0;
			if ( edgeLenSq > 0.0001 ) {
				double t = QM_Vector3DotProductDP( toCenter, edge ) / edgeLenSq;
				t = std::max<double>( 0.0, std::min<double>( 1.0, t ) );
				Vector3DP proj = QM_Vector3MultiplyAddDP( a, t, edge );
				dist = QM_Vector3DistanceDP( Vector3DP( face.center ), proj );
			} else {
				dist = QM_Vector3DistanceDP( Vector3DP( face.center ), a );
			}
			if ( dist < min_dist ) {
				min_dist = dist;
			}
		}
		face.clearance = min_dist;

		/**
		*	For each edge of the polygon, create a half-edge and link it to the face and its vertices.
		**/
		for ( int32_t v = 0; v < poly.num_vertices; v++ ) {
			int32_t curr_v = v_indices[ v ];

			nav_halfedge_t he = {};
			he.vertex_idx = curr_v;
			he.face_idx = face.face_id;
			he.twin_idx = -1; // Default to boundary
			he.edge_entity_id = ENTITYNUM_NONE;
			he.wall_offset = 16.0;

			he.next_idx = face.first_edge_idx + ( ( v + 1 ) % poly.num_vertices );

			g_nav_halfedges.push_back( he );
		}

		g_nav_faces.push_back( face );
	}

	/**
	*   Deterministic unresolved-edge arbitration helper.
	*   This targets world-world non-transition seams and claims pairs globally using
	*   stable sort keys to avoid hash-iteration-dependent results.
	**/
	auto RunDeterministicArbitration = [&]() {
		struct nav_deterministic_pair_t {
			int32_t edge_a = -1;
			int32_t edge_b = -1;
			double overlap_len = 0.0;
			double z_delta = 0.0;
		};
		std::vector<nav_deterministic_pair_t> deterministic_pairs = {};
		deterministic_pairs.reserve( 4096 );

		// Index unlinked halfedges in a 2D spatial grid (64-unit cells)
		std::unordered_map<int64_t, std::vector<int32_t>> arb_grid;
		static constexpr double ARB_GRID_SIZE = 64.0;
		for ( int32_t e = 0; e < ( int32_t )g_nav_halfedges.size(); e++ ) {
			if ( g_nav_halfedges[ e ].twin_idx != -1 ) continue;
			const Vector3DP v1 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ e ].vertex_idx ] );
			const Vector3DP v2 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ g_nav_halfedges[ e ].next_idx ].vertex_idx ] );
			const Vector3DP mid = ( v1 + v2 ) * 0.5;
			const int64_t cx = ( int64_t )std::floor( mid.x / ARB_GRID_SIZE );
			const int64_t cy = ( int64_t )std::floor( mid.y / ARB_GRID_SIZE );
			arb_grid[ ( cx * 73856093 ) ^ ( cy * 19349663 ) ].push_back( e );
		}

		for ( int32_t edge_a = 0; edge_a < ( int32_t )g_nav_halfedges.size(); edge_a++ ) {
			if ( g_nav_halfedges[ edge_a ].twin_idx != -1 ) {
				continue;
			}

			const nav_halfedge_t &half_edge_a = g_nav_halfedges[ edge_a ];
			const nav_poly_t &poly_a = g_nav_polys[ half_edge_a.face_idx ];
			if ( poly_a.entity_id != ENTITYNUM_NONE || poly_a.transition_entity_id != ENTITYNUM_NONE || poly_a.normal.z < NAV_MIN_WALKABLE_Z ) {
				continue;
			}

			const Vector3DP a1 = Vector3DP( g_nav_vertices[ half_edge_a.vertex_idx ] );
			const Vector3DP a2 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ half_edge_a.next_idx ].vertex_idx ] );
			Vector3DP direction_a = a2 - a1;
			direction_a.z = 0.0;
			const double length_a = QM_Vector3LengthDP( direction_a );
			if ( length_a < 0.1 ) {
				continue;
			}
			direction_a = direction_a * static_cast< double >( 1.0 / length_a );

			const Vector3DP mid_a = ( a1 + a2 ) * 0.5;
			const int64_t cx = ( int64_t )std::floor( mid_a.x / ARB_GRID_SIZE );
			const int64_t cy = ( int64_t )std::floor( mid_a.y / ARB_GRID_SIZE );

			for ( int64_t ox = -1; ox <= 1; ox++ ) {
				for ( int64_t oy = -1; oy <= 1; oy++ ) {
					const int64_t key = ( ( cx + ox ) * 73856093 ) ^ ( ( cy + oy ) * 19349663 );
					auto it = arb_grid.find( key );
					if ( it == arb_grid.end() ) continue;

					for ( int32_t edge_b : it->second ) {
						if ( edge_b <= edge_a ) continue;
						if ( g_nav_halfedges[ edge_b ].twin_idx != -1 ) continue;

						const nav_halfedge_t &half_edge_b = g_nav_halfedges[ edge_b ];
						if ( half_edge_a.face_idx == half_edge_b.face_idx ) continue;

						const nav_poly_t &poly_b = g_nav_polys[ half_edge_b.face_idx ];
						if ( poly_b.entity_id != ENTITYNUM_NONE || poly_b.transition_entity_id != ENTITYNUM_NONE || poly_b.normal.z < NAV_MIN_WALKABLE_Z ) {
							continue;
						}

						if ( !AreHalfEdgesInCompatibleNavigationDomains( edge_a, edge_b ) ) {
							continue;
						}

						const Vector3DP b1 = Vector3DP( g_nav_vertices[ half_edge_b.vertex_idx ] );
						const Vector3DP b2 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ half_edge_b.next_idx ].vertex_idx ] );
						Vector3DP direction_b = b2 - b1;
						direction_b.z = 0.0;
						const double length_b = QM_Vector3LengthDP( direction_b );
						if ( length_b < 0.1 ) {
							continue;
						}
						direction_b = direction_b * static_cast< double >( 1.0 / length_b );
						if ( QM_Vector3DotProductDP( direction_a, direction_b ) > -0.95 ) {
							continue;
						}

						static constexpr double MAX_DETERMINISTIC_LATERAL_SEPARATION = 1.0;
						const double lateral_b1 = std::fabs( direction_a.x * ( b1.y - a1.y ) - direction_a.y * ( b1.x - a1.x ) );
						const double lateral_b2 = std::fabs( direction_a.x * ( b2.y - a1.y ) - direction_a.y * ( b2.x - a1.x ) );
						if ( lateral_b1 > MAX_DETERMINISTIC_LATERAL_SEPARATION || lateral_b2 > MAX_DETERMINISTIC_LATERAL_SEPARATION ) {
							continue;
						}

						Vector3DP a1_to_b1 = b1 - a1;
						a1_to_b1.z = 0.0;
						const double projection_b1 = QM_Vector3DotProductDP( a1_to_b1, direction_a );
						Vector3DP closest_point = QM_Vector3MultiplyAddDP( a1, projection_b1, direction_a );
						closest_point.z = 0.0;
						Vector3DP b1_2d = b1;
						b1_2d.z = 0.0;
						if ( QM_Vector3DistanceSqrDP( b1_2d, closest_point ) > 1.0 ) {
							continue;
						}

						Vector3DP a1_to_b2 = b2 - a1;
						a1_to_b2.z = 0.0;
						const double projection_b2 = QM_Vector3DotProductDP( a1_to_b2, direction_a );
						const double min_projection = std::min( projection_b1, projection_b2 );
						const double max_projection = std::max( projection_b1, projection_b2 );
						const double overlap_start = std::max<double>( 0.0, min_projection );
						const double overlap_end = std::min<double>( length_a, max_projection );
						const double overlap_length = overlap_end - overlap_start;
						if ( overlap_length < 2.0 ) {
							continue;
						}

						static constexpr double MAX_Z_DIFF = NAV_MAX_STEP_SIZE;
						const double z_delta = std::abs( ( a1.z + a2.z ) * 0.5 - ( b1.z + b2.z ) * 0.5 );
						if ( z_delta > MAX_Z_DIFF ) {
							continue;
						}

						nav_deterministic_pair_t pair = {};
						pair.edge_a = edge_a;
						pair.edge_b = edge_b;
						pair.overlap_len = overlap_length;
						pair.z_delta = z_delta;
						deterministic_pairs.push_back( pair );
					}
				}
			}
		}

		std::sort( deterministic_pairs.begin(), deterministic_pairs.end(), []( const nav_deterministic_pair_t &lhs, const nav_deterministic_pair_t &rhs ) {
			if ( lhs.overlap_len != rhs.overlap_len ) {
				return lhs.overlap_len > rhs.overlap_len;
			}
			if ( lhs.z_delta != rhs.z_delta ) {
				return lhs.z_delta < rhs.z_delta;
			}
			if ( lhs.edge_a != rhs.edge_a ) {
				return lhs.edge_a < rhs.edge_a;
			}
			return lhs.edge_b < rhs.edge_b;
			} );

		for ( const nav_deterministic_pair_t &pair : deterministic_pairs ) {
			if ( pair.edge_a < 0 || pair.edge_b < 0 ) {
				continue;
			}
			if ( g_nav_halfedges[ pair.edge_a ].twin_idx != -1 || g_nav_halfedges[ pair.edge_b ].twin_idx != -1 ) {
				continue;
			}

			g_nav_halfedges[ pair.edge_a ].twin_idx = pair.edge_b;
			g_nav_halfedges[ pair.edge_b ].twin_idx = pair.edge_a;
			g_nav_halfedges[ pair.edge_a ].wall_offset = 0.0;
			g_nav_halfedges[ pair.edge_b ].wall_offset = 0.0;

			const double z_a = ( g_nav_vertices[ g_nav_halfedges[ pair.edge_a ].vertex_idx ].z + g_nav_vertices[ g_nav_halfedges[ g_nav_halfedges[ pair.edge_a ].next_idx ].vertex_idx ].z ) * 0.5;
			const double z_b = ( g_nav_vertices[ g_nav_halfedges[ pair.edge_b ].vertex_idx ].z + g_nav_vertices[ g_nav_halfedges[ g_nav_halfedges[ pair.edge_b ].next_idx ].vertex_idx ].z ) * 0.5;
			g_nav_halfedges[ pair.edge_a ].z_diff = z_b - z_a;
			g_nav_halfedges[ pair.edge_b ].z_diff = z_a - z_b;

			AssignNavEntityEdgeMetadata( pair.edge_a, pair.edge_b );
			deterministic_twin_links++;
		}
		};

	// Run deterministic arbitration first.
	RunDeterministicArbitration();

	/**
	*	Twin Linking using Z-tolerant 2D overlap check
	*	Connects stair steps that are physically separated by up to 18 units vertically.
	**/
	std::unordered_map<int64_t, std::vector<int32_t>> twin_grid;
	for ( size_t j = 0; j < g_nav_halfedges.size(); j++ ) {
		Vector3DP b1 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ j ].vertex_idx ] );
		Vector3DP b2 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ g_nav_halfedges[ j ].next_idx ].vertex_idx ] );
		static constexpr double GRID_SIZE = 16.0;
		int64_t cx1 = ( int64_t )std::floor( b1.x / GRID_SIZE );
		int64_t cy1 = ( int64_t )std::floor( b1.y / GRID_SIZE );
		int64_t cx = ( int64_t )std::floor( b2.x / GRID_SIZE );
		int64_t cy = ( int64_t )std::floor( b2.y / GRID_SIZE );
		int64_t key1 = ( cx1 * 73856093 ) ^ ( cy1 * 19349663 );
		int64_t key = ( cx * 73856093 ) ^ ( cy * 19349663 );
		twin_grid[ key1 ].push_back( ( int32_t )j );
		twin_grid[ key ].push_back( ( int32_t )j );
	}

	/**
	*	Iterate through all half-edges and find twins by checking grid cells.
	**/
	for ( uint64_t i = 0; i < g_nav_halfedges.size(); i++ ) {
		// Update primary twin linking progress (mapping to 0.75f..0.80f).
		if ( !g_nav_halfedges.empty() ) {
			const float pct = 0.75f + 0.05f * ( static_cast< float >( i ) / static_cast< float >( g_nav_halfedges.size() ) );
			Nav_SetGenerationProgress( pct );
		}

		if ( g_nav_halfedges[ i ].twin_idx != -1 ) {
			continue;
		}

		nav_halfedge_t &heA = g_nav_halfedges[ i ];
		Vector3DP a1 = Vector3DP( g_nav_vertices[ heA.vertex_idx ] );
		Vector3DP a2 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ heA.next_idx ].vertex_idx ] );
		Vector3DP world_mins = {};
		Vector3DP world_maxs = {};
		Nav_GetWorldModelBounds( &world_mins, &world_maxs );
		double world_span = Nav_GetWorldWindingExtent();
		double bestEndpointError = world_span;
		double bestZ = world_maxs.z + 1000.0;
		int64_t bestTwin = -1;

		static constexpr double GRID_SIZE = 16.0;
		int64_t cx = ( int64_t )std::floor( a1.x / GRID_SIZE );
		int64_t cy = ( int64_t )std::floor( a1.y / GRID_SIZE );

		for ( int64_t ox = -1; ox <= 1; ox++ ) {
			for ( int64_t oy = -1; oy <= 1; oy++ ) {
				int64_t key = ( ( cx + ox ) * 73856093 ) ^ ( ( cy + oy ) * 19349663 );
				auto it = twin_grid.find( key );
				if ( it == twin_grid.end() ) continue;

				for ( int32_t j : it->second ) {
					if ( i == ( size_t )j ) {
						continue;
					}
					if ( g_nav_halfedges[ j ].twin_idx != -1 ) {
						continue;
					}

					nav_halfedge_t &heB = g_nav_halfedges[ j ];
					if ( heA.face_idx == heB.face_idx ) {
						continue;
					}
					if ( !AreHalfEdgesInCompatibleNavigationDomains( static_cast< int32_t >( i ), j ) ) {
						continue;
					}
					Vector3DP b1 = Vector3DP( g_nav_vertices[ heB.vertex_idx ] );
					Vector3DP b2 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ heB.next_idx ].vertex_idx ] );

					Vector3DP edgeA2D = a2 - a1;
					edgeA2D.z = 0.0;
					Vector3DP edgeB2D = b2 - b1;
					edgeB2D.z = 0.0;
					const double edgeALen = QM_Vector3LengthDP( edgeA2D );
					const double edgeBLen = QM_Vector3LengthDP( edgeB2D );
					if ( edgeALen <= 0.1 || edgeBLen <= 0.1 ) {
						continue;
					}
					edgeA2D = edgeA2D * static_cast< double >( 1.0 / edgeALen );
					edgeB2D = edgeB2D * static_cast< double >( 1.0 / edgeBLen );
					if ( QM_Vector3DotProductDP( edgeA2D, edgeB2D ) > -0.95 ) {
						continue;
					}

					const double lateralB1 = std::fabs( edgeA2D.x * ( b1.y - a1.y ) - edgeA2D.y * ( b1.x - a1.x ) );
					const double lateralB2 = std::fabs( edgeA2D.x * ( b2.y - a1.y ) - edgeA2D.y * ( b2.x - a1.x ) );
					static constexpr double MAX_TWIN_LATERAL_SEPARATION = 1.0;
					if ( lateralB1 > MAX_TWIN_LATERAL_SEPARATION || lateralB2 > MAX_TWIN_LATERAL_SEPARATION ) {
						continue;
					}

					const double dx_rev_1 = a1.x - b2.x;
					const double dy_rev_1 = a1.y - b2.y;
					const double dz_rev_1 = std::abs( a1.z - b2.z );
					const double dx_rev_2 = a2.x - b1.x;
					const double dy_rev_2 = a2.y - b1.y;
					const double dz_rev_2 = std::abs( a2.z - b1.z );

					const double dx_same_1 = a1.x - b1.x;
					const double dy_same_1 = a1.y - b1.y;
					const double dz_same_1 = std::abs( a1.z - b1.z );
					const double dx_same_2 = a2.x - b2.x;
					const double dy_same_2 = a2.y - b2.y;
					const double dz_same_2 = std::abs( a2.z - b2.z );

					static constexpr double MAX_DIST_SQR = 1.0;
					static constexpr double MAX_Z_DIFF = NAV_MAX_STEP_SIZE;
					const bool reversed_endpoint_match =
						dx_rev_1 * dx_rev_1 + dy_rev_1 * dy_rev_1 < MAX_DIST_SQR && dz_rev_1 <= MAX_Z_DIFF &&
						dx_rev_2 * dx_rev_2 + dy_rev_2 * dy_rev_2 < MAX_DIST_SQR && dz_rev_2 <= MAX_Z_DIFF;
					const bool same_order_endpoint_match =
						dx_same_1 * dx_same_1 + dy_same_1 * dy_same_1 < MAX_DIST_SQR && dz_same_1 <= MAX_Z_DIFF &&
						dx_same_2 * dx_same_2 + dy_same_2 * dy_same_2 < MAX_DIST_SQR && dz_same_2 <= MAX_Z_DIFF;

					if ( reversed_endpoint_match || same_order_endpoint_match ) {
						const double reversed_endpoint_error =
							( dx_rev_1 * dx_rev_1 + dy_rev_1 * dy_rev_1 ) +
							( dx_rev_2 * dx_rev_2 + dy_rev_2 * dy_rev_2 );
						const double same_order_endpoint_error =
							( dx_same_1 * dx_same_1 + dy_same_1 * dy_same_1 ) +
							( dx_same_2 * dx_same_2 + dy_same_2 * dy_same_2 );
						const double endpoint_error =
							( reversed_endpoint_match && same_order_endpoint_match )
							? std::min( reversed_endpoint_error, same_order_endpoint_error )
							: ( reversed_endpoint_match ? reversed_endpoint_error : same_order_endpoint_error );
						const double total_z_reversed = dz_rev_1 + dz_rev_2;
						const double total_z_same_order = dz_same_1 + dz_same_2;
						const double totalZ =
							( reversed_endpoint_match && same_order_endpoint_match )
							? std::min( total_z_reversed, total_z_same_order )
							: ( reversed_endpoint_match ? total_z_reversed : total_z_same_order );

						if ( endpoint_error < bestEndpointError || ( endpoint_error == bestEndpointError && totalZ < bestZ ) ) {
							bestEndpointError = endpoint_error;
							bestZ = totalZ;
							bestTwin = static_cast< int32_t >( j );
						}
					}
				}
			}
		}

		if ( bestTwin != -1 ) {
			g_nav_halfedges[ i ].twin_idx = bestTwin;
			g_nav_halfedges[ bestTwin ].twin_idx = static_cast< int32_t >( i );

			g_nav_halfedges[ i ].wall_offset = 0.0;
			g_nav_halfedges[ bestTwin ].wall_offset = 0.0;

			first_pass_twin_links++;

			double z1 = ( g_nav_vertices[ g_nav_halfedges[ i ].vertex_idx ].z + g_nav_vertices[ g_nav_halfedges[ g_nav_halfedges[ i ].next_idx ].vertex_idx ].z ) * 0.5;
			double z2 = ( g_nav_vertices[ g_nav_halfedges[ bestTwin ].vertex_idx ].z + g_nav_vertices[ g_nav_halfedges[ g_nav_halfedges[ bestTwin ].next_idx ].vertex_idx ].z ) * 0.5;
			g_nav_halfedges[ i ].z_diff = z2 - z1;
			g_nav_halfedges[ bestTwin ].z_diff = z1 - z2;

			AssignNavEntityEdgeMetadata( static_cast< int32_t >( i ), bestTwin );
		}
	}

	RunDeterministicArbitration();

	/**
	* 	Secondary Twin Linking pass for T-Junctions and Overlaps
	**/
	std::unordered_map<int64_t, std::vector<int32_t>> overlap_grid;
	for ( size_t j = 0; j < g_nav_halfedges.size(); j++ ) {
		nav_halfedge_t &heB = g_nav_halfedges[ j ];
		Vector3DP b1 = Vector3DP( g_nav_vertices[ heB.vertex_idx ] );
		Vector3DP b2 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ heB.next_idx ].vertex_idx ] );
		Vector3DP center = ( b1 + b2 ) * 0.5;

		static constexpr double GRID_SIZE = 128.0;
		int64_t cx = ( int64_t )std::floor( center.x / GRID_SIZE );
		int64_t cy = ( int64_t )std::floor( center.y / GRID_SIZE );
		int64_t key = ( cx * 73856093 ) ^ ( cy * 19349663 );
		overlap_grid[ key ].push_back( ( int32_t )j );
	}

	for ( size_t i = 0; i < g_nav_halfedges.size(); i++ ) {
		// Update secondary twin linking progress (mapping to 0.80f..0.85f).
		if ( !g_nav_halfedges.empty() ) {
			const float pct = 0.80f + 0.05f * ( static_cast< float >( i ) / static_cast< float >( g_nav_halfedges.size() ) );
			Nav_SetGenerationProgress( pct );
		}

		if ( g_nav_halfedges[ i ].twin_idx != -1 ) {
			continue;
		}

		nav_halfedge_t &heA = g_nav_halfedges[ i ];
		Vector3DP a1 = Vector3DP( g_nav_vertices[ heA.vertex_idx ] );
		Vector3DP a2 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ heA.next_idx ].vertex_idx ] );

		Vector3DP dA = a2 - a1;
		dA.z = 0.0;
		double lenA = QM_Vector3LengthDP( dA );
		if ( lenA < 0.1 ) continue;
		Vector3DP dirA = dA * static_cast< double >( 1.0 / lenA );

		double bestOverlap = -1.0;
		int32_t bestTwin = -1;

		Vector3DP centerA = ( a1 + a2 ) * 0.5;
		static constexpr double GRID_SIZE = 128.0;
		int64_t cx = ( int64_t )std::floor( centerA.x / GRID_SIZE );
		int64_t cy = ( int64_t )std::floor( centerA.y / GRID_SIZE );

		for ( int64_t ox = -1; ox <= 1; ox++ ) {
			for ( int64_t oy = -1; oy <= 1; oy++ ) {
				int64_t key = ( ( cx + ox ) * 73856093 ) ^ ( ( cy + oy ) * 19349663 );
				auto it = overlap_grid.find( key );
				if ( it == overlap_grid.end() ) {
					continue;
				}
				for ( int32_t j : it->second ) {
					if ( i == ( size_t )j ) {
						continue;
					}
					if ( g_nav_halfedges[ j ].twin_idx != -1 ) {
						continue;
					}

					nav_halfedge_t &heB = g_nav_halfedges[ j ];
					if ( heA.face_idx == heB.face_idx ) {
						continue;
					}
					if ( !AreHalfEdgesInCompatibleNavigationDomains( static_cast< int32_t >( i ), j ) ) {
						continue;
					}
					Vector3DP b1 = Vector3DP( g_nav_vertices[ heB.vertex_idx ] );
					Vector3DP b2 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ heB.next_idx ].vertex_idx ] );

					Vector3DP dB = b2 - b1;
					dB.z = 0.0;
					double lenB = QM_Vector3LengthDP( dB );
					if ( lenB < 0.1 ) {
						continue;
					}
					Vector3DP dirB = dB * static_cast< double >( 1.0 / lenB );
					if ( std::fabs( QM_Vector3DotProductDP( dirA, dirB ) ) < 0.95 ) {
						continue;
					}

					const double lateralSeparationB1 = std::fabs(
						dirA.x * ( b1.y - a1.y ) - dirA.y * ( b1.x - a1.x ) );
					const double lateralSeparationB2 = std::fabs(
						dirA.x * ( b2.y - a1.y ) - dirA.y * ( b2.x - a1.x ) );
					static constexpr double MAX_LATERAL_SEPARATION = 1.0;
					if ( lateralSeparationB1 > MAX_LATERAL_SEPARATION || lateralSeparationB2 > MAX_LATERAL_SEPARATION ) {
						continue;
					}

					Vector3DP a1_to_b1 = b1 - a1;
					a1_to_b1.z = 0.0;
					double proj = QM_Vector3DotProductDP( a1_to_b1, dirA );
					Vector3DP closestPt = QM_Vector3MultiplyAddDP( a1, proj, dirA );
					closestPt.z = 0.0;
					Vector3DP b1_2d = b1; b1_2d.z = 0.0;
					if ( QM_Vector3DistanceSqrDP( b1_2d, closestPt ) > 1.0 ) {
						continue;
					}

					double u1 = proj;
					Vector3DP a1_to_b2 = b2 - a1;
					a1_to_b2.z = 0.0;
					double u2 = QM_Vector3DotProductDP( a1_to_b2, dirA );

					double minU = std::min( u1, u2 );
					double maxU = std::max( u1, u2 );
					double overlapStart = std::max<double>( 0.0, minU );
					double overlapEnd = std::min<double>( lenA, maxU );
					double overlapLen = overlapEnd - overlapStart;

					if ( overlapLen >= 2.0 ) {
						double dz = std::abs( ( a1.z + a2.z ) * 0.5 - ( b1.z + b2.z ) * 0.5 );
						static constexpr double MAX_Z_DIFF = NAV_MAX_STEP_SIZE;
						if ( dz <= MAX_Z_DIFF ) {
							if ( overlapLen > bestOverlap ) {
								bestOverlap = overlapLen;
								bestTwin = static_cast< int32_t >( j );
							}
						}
					}
				}
			}
		}

		if ( bestTwin != -1 ) {
			g_nav_halfedges[ i ].twin_idx = bestTwin;
			g_nav_halfedges[ bestTwin ].twin_idx = static_cast< int32_t >( i );

			g_nav_halfedges[ i ].wall_offset = 0.0;
			g_nav_halfedges[ bestTwin ].wall_offset = 0.0;

			second_pass_twin_links++;

			double z1 = ( g_nav_vertices[ g_nav_halfedges[ i ].vertex_idx ].z + g_nav_vertices[ g_nav_halfedges[ g_nav_halfedges[ i ].next_idx ].vertex_idx ].z ) * 0.5;
			double z2 = ( g_nav_vertices[ g_nav_halfedges[ bestTwin ].vertex_idx ].z + g_nav_vertices[ g_nav_halfedges[ g_nav_halfedges[ bestTwin ].next_idx ].vertex_idx ].z ) * 0.5;
			g_nav_halfedges[ i ].z_diff = z2 - z1;
			g_nav_halfedges[ bestTwin ].z_diff = z1 - z2;

			AssignNavEntityEdgeMetadata( static_cast< int32_t >( i ), bestTwin );
		}

	}

	// Preserve boundary edges that belong to transition-owned faces.
	for ( size_t edge_index = 0; edge_index < g_nav_halfedges.size(); ++edge_index ) {
		nav_halfedge_t &halfedge = g_nav_halfedges[ edge_index ];
		if ( halfedge.twin_idx != -1 ) {
			continue;
		}

		const nav_face_t &face = g_nav_faces[ halfedge.face_idx ];
		const int32_t face_entity_id = GetNavFaceOwningEntityId( face );
		if ( face_entity_id != ENTITYNUM_NONE ) {
			halfedge.edge_entity_id = face_entity_id;
			RegisterNavEntityEdge( face_entity_id, static_cast< int32_t >( edge_index ) );
			if ( IsDynamicTransitionEntityBlockingNavigation( face_entity_id ) ) {
				halfedge.flags |= NAV_EDGE_DISABLED;
			} else {
				halfedge.flags &= ~NAV_EDGE_DISABLED;
			}
		} else {
			halfedge.edge_entity_id = ENTITYNUM_NONE;
			halfedge.flags &= ~NAV_EDGE_DISABLED;
		}
	}

	/**
	*   Find natural BSP twin edges that lie beneath functional doors and assign them as dynamic portal edges.
	**/
	for ( int32_t i = 1; i < g_edict_pool.num_edicts; ++i ) {
		svg_base_edict_t *edict = g_edicts[ i ];
		if ( !edict || !SVG_Entity_IsActive( edict ) ) continue;

		bool isVerticalDoor = false;
		if ( edict->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
			isVerticalDoor = true;
		} else if ( edict->GetTypeInfo()->IsSubClassType<svg_func_door_rotating_t>() ) {
			if ( !( edict->spawnflags & ( svg_func_door_rotating_t::SPAWNFLAG_X_AXIS | svg_func_door_rotating_t::SPAWNFLAG_Y_AXIS ) ) ) {
				isVerticalDoor = true;
			}
		} else if ( edict->GetTypeInfo()->IsSubClassType<svg_func_rotating_t>() ) {
			isVerticalDoor = true;
		}

		if ( isVerticalDoor ) {
			const int32_t entity_id = edict->teammaster ? edict->teammaster->s.number : i;

			/**
			*	Skip fallback bounding-box tagging if this door entity already has its exact threshold transition
			*	edges registered from extracted 2D door footprints. This prevents world-world floor edges extending
			*	into solid wall frames from being falsely promoted to dynamic door portals.
			**/
			if ( static_cast< size_t >( entity_id ) < g_nav_entity_edges.size() && !g_nav_entity_edges[ entity_id ].empty() ) {
				continue;
			}

			Vector3DP absMin = Vector3DP( edict->absMin );
			Vector3DP absMax = Vector3DP( edict->absMax );
			absMin.x -= 4.0; absMin.y -= 4.0; absMin.z -= 8.0;
			absMax.x += 4.0; absMax.y += 4.0; absMax.z += 8.0;

			for ( size_t edge_index = 0; edge_index < g_nav_halfedges.size(); ++edge_index ) {
				nav_halfedge_t &halfedge = g_nav_halfedges[ edge_index ];
				if ( halfedge.twin_idx == -1 ) continue;
				if ( halfedge.edge_entity_id != ENTITYNUM_NONE ) continue;

				const Vector3DP v1 = Vector3DP( g_nav_vertices[ halfedge.vertex_idx ] );
				const Vector3DP v2 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ halfedge.next_idx ].vertex_idx ] );

				/**
				*	Verify that both endpoints lie strictly within the door's 3D bounding box.
				**/
				if ( v1.x >= absMin.x && v1.x <= absMax.x &&
					v1.y >= absMin.y && v1.y <= absMax.y &&
					v1.z >= absMin.z && v1.z <= absMax.z &&
					v2.x >= absMin.x && v2.x <= absMax.x &&
					v2.y >= absMin.y && v2.y <= absMax.y &&
					v2.z >= absMin.z && v2.z <= absMax.z ) {

					/**
					*	Reject longitudinal internal seams that slice through the door interior.
					*	Only transverse perimeter edges spanning the width of the doorway form valid portals.
					**/
					const double door_width_x = absMax.x - absMin.x;
					const double door_width_y = absMax.y - absMin.y;

					// Compute 2D horizontal direction vector of the candidate edge.
					Vector3DP edge_dir = v2 - v1;
					edge_dir.z = 0.0;
					const double edge_len = QM_Vector3LengthDP( edge_dir );
					if ( edge_len < 0.1 ) {
						continue;
					}
					edge_dir = edge_dir * ( 1.0 / edge_len );

					// For a door spanning along X, threshold edges must align with X (|edge_dir.x| >= 0.707).
					// For a door spanning along Y, threshold edges must align with Y (|edge_dir.y| >= 0.707).
					const bool is_transverse_threshold = ( door_width_x > door_width_y )
						? ( std::abs( edge_dir.x ) >= 0.707 )
						: ( std::abs( edge_dir.y ) >= 0.707 );

					// Skip internal longitudinal seams so they are never tagged as dynamic door portals.
					if ( !is_transverse_threshold ) {
						continue;
					}

					halfedge.edge_entity_id = entity_id;
					g_nav_halfedges[ halfedge.twin_idx ].edge_entity_id = entity_id;

					RegisterNavEntityEdge( entity_id, static_cast< int32_t >( edge_index ) );
					RegisterNavEntityEdge( entity_id, halfedge.twin_idx );

					if ( IsDynamicTransitionEntityBlockingNavigation( entity_id ) ) {
						halfedge.flags |= NAV_EDGE_DISABLED;
						g_nav_halfedges[ halfedge.twin_idx ].flags |= NAV_EDGE_DISABLED;
					}
				}
			}
		}
	}

	/**
	*	Emit topology diagnostics.
	**/
	auto Compute2DOverlapLen = [&]( const nav_halfedge_t &a, const nav_halfedge_t &b ) -> double {
		const Vector3DP a0 = Vector3DP( g_nav_vertices[ a.vertex_idx ] );
		const Vector3DP a1 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ a.next_idx ].vertex_idx ] );
		const Vector3DP b0 = Vector3DP( g_nav_vertices[ b.vertex_idx ] );
		const Vector3DP b1 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ b.next_idx ].vertex_idx ] );
		Vector3DP aDir = a1 - a0;
		aDir.z = 0.0;
		const double aLen = QM_Vector3LengthDP( aDir );
		if ( aLen <= 0.0001 ) {
			return 0.0;
		}
		aDir = aDir * static_cast< double >( 1.0 / aLen );
		Vector3DP a0b0 = b0 - a0;
		Vector3DP a0b1 = b1 - a0;
		a0b0.z = 0.0;
		a0b1.z = 0.0;
		const double u0 = static_cast< double >( QM_Vector3DotProductDP( a0b0, aDir ) );
		const double u1 = static_cast< double >( QM_Vector3DotProductDP( a0b1, aDir ) );
		const double bMin = std::min( u0, u1 );
		const double bMax = std::max( u0, u1 );
		const double overlapStart = std::max<double>( 0.0, bMin );
		const double overlapEnd = std::min<double>( aLen, bMax );
		return std::max<double>( 0.0, overlapEnd - overlapStart );
		};

	int32_t boundary_edges = 0;
	int32_t twinned_edges = 0;
	int32_t twin_reverse_mismatch = 0;
	int32_t tiny_overlap_twins = 0;
	int32_t short_boundary_edges = 0;
	for ( size_t i = 0; i < g_nav_halfedges.size(); i++ ) {
		const nav_halfedge_t &he = g_nav_halfedges[ i ];
		const Vector3DP e0 = Vector3DP( g_nav_vertices[ he.vertex_idx ] );
		const Vector3DP e1 = Vector3DP( g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ] );
		const double edgeLen2D = static_cast< double >( QM_Vector2Distance( static_cast< Vector2 >( e0 ), static_cast< Vector2 >( e1 ) ) );
		if ( he.twin_idx == -1 ) {
			boundary_edges++;
			if ( edgeLen2D < 4.0 ) {
				short_boundary_edges++;
			}
			continue;
		}
		twinned_edges++;
		if ( static_cast< size_t >( he.twin_idx ) >= g_nav_halfedges.size() || g_nav_halfedges[ he.twin_idx ].twin_idx != static_cast< int32_t >( i ) ) {
			twin_reverse_mismatch++;
			continue;
		}
		const double overlapLen = Compute2DOverlapLen( he, g_nav_halfedges[ he.twin_idx ] );
		if ( overlapLen < 2.0 ) {
			tiny_overlap_twins++;
		}
	}

	// Validate mesh ownership before KD construction changes face storage order.
	Nav_ValidateTopology( "HalfEdge" );

	gi.dprintf( "NavMesh Half-Edge Generation Completed. %d vertices, %d half-edges, %d faces.\n",
		g_nav_vertices.size(), g_nav_halfedges.size(), g_nav_faces.size() );
	gi.dprintf( "NavMesh Topology Diagnostics: deterministicLinks=%d firstPassLinks=%d secondPassLinks=%d twinnedEdges=%d boundaryEdges=%d shortBoundaryEdges=%d twinReverseMismatch=%d tinyOverlapTwins=%d\n",
		deterministic_twin_links, first_pass_twin_links, second_pass_twin_links, twinned_edges, boundary_edges, short_boundary_edges, twin_reverse_mismatch, tiny_overlap_twins );
	LogNavGenerationDiagnostics( "HalfEdge" );
	Nav_SetGenerationProgress( 0.90f );
}

/**
*	@brief	Validate half-edge ownership, twin symmetry, and KD-tree face coverage.
*	@param	stage	Human-readable stage label emitted with the bounded summary.
*	@return	True when all mesh and available KD-tree invariants hold.
*	@note	The check intentionally reports aggregate failures plus at most eight examples
*			so generation diagnostics remain useful without flooding the server console.
**/
bool Nav_ValidateTopology( const char *stage ) {
	/**
	*	Resolve a stable stage name for command and generation callers.
	**/
	const char *const stage_name = stage ? stage : "Unknown";
	int32_t failure_count = 0;
	int32_t reported_failures = 0;
	static constexpr int32_t MAX_REPORTED_FAILURES = 8;

	/**
	*	Record a bounded failure example while retaining the full aggregate count.
	**/
	auto ReportFailure = [&]( const char *const format, const int32_t first, const int32_t second ) {
		failure_count++;
		if ( reported_failures >= MAX_REPORTED_FAILURES ) {
			return;
		}

		reported_failures++;
		gi.dprintf( "NavMesh Validate [%s] ", stage_name );
		gi.dprintf( format, first, second );
		gi.dprintf( "\n" );
	};

	/**
	*	Validate every face span and every half-edge reachable from that face.
	**/
	for ( int32_t face_index = 0; face_index < static_cast< int32_t >( g_nav_faces.size() ); face_index++ ) {
		const nav_face_t &face = g_nav_faces[ face_index ];
		if ( face.face_id != face_index ) {
			ReportFailure( "face identity mismatch: face=%d face_id=%d", face_index, face.face_id );
		}
		if ( face.num_edges < 3 || face.first_edge_idx < 0 || face.first_edge_idx > static_cast< int32_t >( g_nav_halfedges.size() ) - face.num_edges ) {
			ReportFailure( "invalid face edge span: face=%d first_edge=%d", face_index, face.first_edge_idx );
			continue;
		}

		for ( int32_t edge_offset = 0; edge_offset < face.num_edges; edge_offset++ ) {
			const int32_t edge_index = face.first_edge_idx + edge_offset;
			const nav_halfedge_t &edge = g_nav_halfedges[ edge_index ];
			if ( edge.face_idx != face_index ) {
				ReportFailure( "edge owner mismatch: edge=%d owner=%d", edge_index, edge.face_idx );
			}
			if ( edge.vertex_idx < 0 || edge.vertex_idx >= static_cast< int32_t >( g_nav_vertices.size() ) ) {
				ReportFailure( "edge vertex out of range: edge=%d vertex=%d", edge_index, edge.vertex_idx );
			}
			if ( edge.next_idx < face.first_edge_idx || edge.next_idx >= face.first_edge_idx + face.num_edges ) {
				ReportFailure( "edge next escapes face loop: edge=%d next=%d", edge_index, edge.next_idx );
			}
			if ( edge.twin_idx == -1 ) {
				continue;
			}
			if ( edge.twin_idx < 0 || edge.twin_idx >= static_cast< int32_t >( g_nav_halfedges.size() ) ) {
				ReportFailure( "edge twin out of range: edge=%d twin=%d", edge_index, edge.twin_idx );
				continue;
			}
			const nav_halfedge_t &twin = g_nav_halfedges[ edge.twin_idx ];
			if ( twin.twin_idx != edge_index || twin.face_idx == face_index ) {
				ReportFailure( "edge twin mismatch: edge=%d twin=%d", edge_index, edge.twin_idx );
			}
		}
	}

	/**
	*	Validate KD children and ensure leaf spans cover every face exactly once.
	**/
	if ( !g_nav_nodes.empty() ) {
		std::vector<int32_t> leaf_coverage( g_nav_faces.size(), 0 );
		for ( int32_t node_index = 0; node_index < static_cast< int32_t >( g_nav_nodes.size() ); node_index++ ) {
			const nav_kdtree_node_t &node = g_nav_nodes[ node_index ];
			const bool is_leaf = node.left_child == -1 && node.right_child == -1;
			if ( !is_leaf ) {
				if ( node.left_child < 0 || node.right_child < 0 || node.left_child >= static_cast< int32_t >( g_nav_nodes.size() ) || node.right_child >= static_cast< int32_t >( g_nav_nodes.size() ) ) {
					ReportFailure( "invalid KD children: node=%d left=%d", node_index, node.left_child );
				}
				continue;
			}
			if ( node.first_face_id < 0 || node.num_faces <= 0 || node.first_face_id > static_cast< int32_t >( g_nav_faces.size() ) - node.num_faces ) {
				ReportFailure( "invalid KD leaf span: node=%d first_face=%d", node_index, node.first_face_id );
				continue;
			}
			for ( int32_t face_offset = 0; face_offset < node.num_faces; face_offset++ ) {
				leaf_coverage[ node.first_face_id + face_offset ]++;
			}
		}

		for ( int32_t face_index = 0; face_index < static_cast< int32_t >( leaf_coverage.size() ); face_index++ ) {
			if ( leaf_coverage[ face_index ] != 1 ) {
				ReportFailure( "KD leaf coverage mismatch: face=%d coverage=%d", face_index, leaf_coverage[ face_index ] );
			}
		}
	}

	gi.dprintf( "NavMesh Validate [%s]: faces=%d edges=%d vertices=%d nodes=%d failures=%d\n",
		stage_name,
		static_cast< int32_t >( g_nav_faces.size() ),
		static_cast< int32_t >( g_nav_halfedges.size() ),
		static_cast< int32_t >( g_nav_vertices.size() ),
		static_cast< int32_t >( g_nav_nodes.size() ),
		failure_count );
	return failure_count == 0;
}
