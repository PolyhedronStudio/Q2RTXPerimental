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
#include "svgame/entities/func/svg_func_wall.h"
#include "svgame/entities/func/svg_func_areaportal.h"



/**
*	Global navmesh data containers.
**/
//! Global navmesh polygon data (temporary during build).
nav_vector_t<nav_poly_t> g_nav_polys;

//! Half-edge mesh global vertices data
std::vector<Vector3> g_nav_vertices;
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

static constexpr int32_t KDTREE_MAX_BIN = 1024;

//! Node result for negative space during BSP tree construction.
static constexpr int32_t NODE_NEGATIVE = -1;

//! Maximum number of points in a winding (polygon) for navmesh generation.
static constexpr int32_t MAX_WINDING_POINTS = 1024;

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
    s_nav_generation_diagnostics = {};
}

/**
* @brief Print a bounded summary of the generation diagnostics for a named stage.
* @param stage Stage label to include in the console output.
**/
static void LogNavGenerationDiagnostics( const char *stage ) {
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

    for ( const auto &entry : s_nav_generation_diagnostics.transition_portals_by_entity ) {
        const int32_t entity_id = entry.first;
        if ( entity_id <= 0 || entity_id >= static_cast<int32_t>( g_nav_entity_edges.size() ) ) {
            continue;
        }

        int32_t printed_portals = 0;
        const std::vector<int32_t> &entity_edges = g_nav_entity_edges[ entity_id ];
        for ( const int32_t edge_index : entity_edges ) {
            if ( printed_portals >= 8 || edge_index < 0 || edge_index >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
                continue;
            }

            const nav_halfedge_t &edge = g_nav_halfedges[ edge_index ];
            if ( edge.edge_entity_id != entity_id || edge.twin_idx < 0 || edge.twin_idx >= static_cast<int32_t>( g_nav_halfedges.size() ) || edge_index > edge.twin_idx ) {
                continue;
            }

            const nav_halfedge_t &twin = g_nav_halfedges[ edge.twin_idx ];
            const Vector3 edge_start = g_nav_vertices[ edge.vertex_idx ];
            const Vector3 edge_end = g_nav_vertices[ g_nav_halfedges[ edge.next_idx ].vertex_idx ];
            const Vector3 twin_start = g_nav_vertices[ twin.vertex_idx ];
            const Vector3 twin_end = g_nav_vertices[ g_nav_halfedges[ twin.next_idx ].vertex_idx ];
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
    Vector3 offset = {};
    //! Runtime Euler orientation applied after local BSP plane construction.
    Vector3 angles = {};
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

    const char *leaf_name = texture_name;
    for ( const char *character = texture_name; *character != '\0'; character++ ) {
        // Support BSPs authored with either slash convention.
        if ( *character == '/' || *character == '\\' ) {
            leaf_name = character + 1;
        }
    }

    static constexpr char ORIGIN_NAME[] = "origin";
    for ( size_t index = 0; index < sizeof( ORIGIN_NAME ) - 1; index++ ) {
        if ( leaf_name[ index ] == '\0' || std::tolower( static_cast<unsigned char>( leaf_name[ index ] ) ) != ORIGIN_NAME[ index ] ) {
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
    if ( !brush || brush->numsides <= 0 || !brush->firstbrushside ) {
        return false;
    }

	bool foundOrigin = false;

    for ( int32_t i = 0; i < brush->numsides; i++ ) {
        const mbrushside_t *side = &brush->firstbrushside[ i ];
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
* @brief Determine whether all vertices of a navigation polygon lie on its expected plane.
* @param poly Polygon to validate.
* @param expected_normal Surface normal that defines the polygon plane.
* @return True when the polygon has a usable normal and every vertex is within the planar tolerance.
**/
static bool IsNavPolygonCoplanar( const nav_poly_t &poly, const Vector3 &expected_normal ) {
    /**
    * Reject an invalid plane normal before using it to classify vertex distances.
    **/
    const float normal_length_sqr = QM_Vector3LengthSqr( expected_normal );
    if ( normal_length_sqr <= 0.000001f ) {
        return false;
    }

    /**
    * Normalize the signed distance calculation so the tolerance is independent of normal magnitude.
    **/
    const float normal_length = std::sqrt( normal_length_sqr );
    const Vector3 origin = poly.vertices[ 0 ];
    for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
        // Reject candidate vertices that no longer lie on the source surface plane.
        const float plane_error = std::fabs( QM_Vector3DotProduct( poly.vertices[ i ] - origin, expected_normal ) ) / normal_length;
        if ( plane_error > 0.25f ) {
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
static bool IsNavPolygonConvex( const nav_poly_t &poly, const Vector3 &expected_normal ) {
    /**
    * Reject impossible vertex counts before reading the polygon loop.
    **/
    if ( poly.num_vertices < 3 || poly.num_vertices > MAX_WINDING_POINTS ) {
        return false;
    }

    /**
    * Check every edge and turn so a splice cannot create a duplicate vertex or self-intersection.
    **/
    float winding_sign = 0.0f;
    for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
        const Vector3 &a = poly.vertices[ i ];
        const Vector3 &b = poly.vertices[ ( i + 1 ) % poly.num_vertices ];
        const Vector3 &c = poly.vertices[ ( i + 2 ) % poly.num_vertices ];
        if ( QM_Vector3DistanceSqr( a, b ) <= 0.01f ) {
            return false;
        }

        const float turn = QM_Vector3DotProduct( QM_Vector3CrossProduct( b - a, c - b ), expected_normal );
        if ( std::fabs( turn ) <= 0.01f ) {
            continue;
        }
        if ( winding_sign == 0.0f ) {
            winding_sign = turn;
        } else if ( turn * winding_sign < 0.0f ) {
            return false;
        }
    }

    return winding_sign != 0.0f;
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
    const float first_normal_length_sqr = QM_Vector3LengthSqr( first.normal );
    const float second_normal_length_sqr = QM_Vector3LengthSqr( second.normal );
    if ( first_normal_length_sqr <= 0.000001f || second_normal_length_sqr <= 0.000001f ) {
        return false;
    }

    const float normal_alignment = QM_Vector3DotProduct( first.normal, second.normal ) /
        std::sqrt( first_normal_length_sqr * second_normal_length_sqr );
    if ( normal_alignment < 0.999f ) {
        return false;
    }

    /**
    * Compare one vertex against the first polygon plane to reject different-height surfaces.
    **/
    const float plane_distance = std::fabs( QM_Vector3DotProduct( second.vertices[ 0 ] - first.vertices[ 0 ], first.normal ) ) /
        std::sqrt( first_normal_length_sqr );
    return plane_distance <= 0.25f;
}

/**
* @brief Determine whether a navigation polygon is safe to use after a topology mutation.
* @param poly Polygon to validate.
* @param expected_normal Surface normal that defines the polygon plane.
* @return True when the polygon is planar, convex, and has no degenerate edges.
* @note This rejects topology repairs that would turn a valid convex surface into a skewed or self-intersecting face.
**/
static bool IsValidNavPolygon( const nav_poly_t &poly, const Vector3 &expected_normal ) {
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
* @brief Recompute the cached center of a navigation polygon.
* @param poly Polygon whose vertices define the new center.
**/
static void RecomputeNavPolygonCenter( nav_poly_t &poly ) {
    /**
    * Average all vertices after a successful topology mutation so spatial queries use current geometry.
    **/
    Vector3 center = {};
    for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
        center = center + poly.vertices[ i ];
    }
    poly.center = center / static_cast<float>( poly.num_vertices );
}

/**
*	@brief	 A winding_t represents a polygon defined by a set of points, used during the navmesh generation process.
**/
struct winding_t {
	//! The number of points in the winding (polygon).
	int32_t num_points = 0;
	//! The array of points that define the winding (polygon). The maximum number of points is defined by MAX_WINDING_POINTS.
	Vector3 points[ MAX_WINDING_POINTS ] = {};
	//! Entity ID this polygon belongs to (e.g. for doors), or ENTITYNUM_NONE if world.
	int32_t entity_id = ENTITYNUM_NONE;
    //! Door entity that caused this winding to become a transition boundary, or ENTITYNUM_NONE for ordinary world geometry.
    int32_t transition_entity_id = ENTITYNUM_NONE;
};



/**
*
*
*
*	Navigation Mesh Core:
* 
* 
* 
**/
/**
*	@brief	 Generate the navmesh asynchronously.
**/
/**
* @brief Generate the navmesh asynchronously from the console command path.
* @note This only submits the worker job; the actual extraction runs off-thread.
**/
void Nav_GenerateCommand() {
    /**
	*	Start the async generation.
	**/
    Nav_StartAsyncGeneration();
}

/**
*	@brief	Deallocate all navmesh data and clear the global containers.
**/
/**
* @brief Clear all global navmesh containers and release the current build state.
**/
void Nav_Clear() {
	/**
	*	Just clear all navmesh data containers.
	**/
	g_nav_polys.clear();
	g_nav_vertices.clear();
	g_nav_halfedges.clear();
	g_nav_faces.clear();
    g_nav_entity_edges.clear();
	g_nav_nodes.clear();
	g_nav_leaf_links.clear();
	g_nav_leaf_poly_ids.clear();

	// We've cleared all navmesh data, so we can also clear any async generation state if needed.
	gi.dprintf( "NavMesh memory cleared.\n" );
}

/**
*	@brief	Print the current standalone nav generation status to the server console.
*	@note	This keeps the nav system self-contained so the command path does not need nav2/nav3 helpers.
**/
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
        "NavMesh status: generating=%d progress=%.2f%% elapsed=%u ms remaining=%u ms\n",
        progress.is_generating ? 1 : 0,
        progress.progress_pct * 100.0f,
        progress.time_taken_ms,
        progress.estimated_time_left_ms );
}



/**
*
*
*
*	Tools for Winding (Polygon) Manipulation:
*
*
*
**/
/**
* @brief Construct a large base winding for a collision plane.
* @param p Plane to expand into a temporary polygon.
* @return Four-point winding centered on the plane.
**/
static winding_t BaseWindingForPlane( const cm_plane_t *p ) {
    winding_t w;
    w.num_points = 4;
    
    int32_t max = -1;
    float maxv = -1;
    for (int32_t i=0; i<3; i++) {
        float v = fabs(p->normal[i]);
        if (v > maxv) {
            max = i;
            maxv = v;
        }
    }
    
    Vector3 up(0, 0, 0);
    if (max == 2) {
        up.x = 1;
    } else {
        up.z = 1;
    }
    
    Vector3 p_normal(p->normal[0], p->normal[1], p->normal[2]);
    Vector3 right = QM_Vector3CrossProduct(up, p_normal);
    const double lengthA = QM_Vector3NormalizeLength(right);
    up = QM_Vector3CrossProduct(p_normal, right);
	const double lengthB = QM_Vector3NormalizeLength(up);
    
    Vector3 org = p_normal * p->dist;
    
    float BIGNUMBER = 99999.0f;
    Vector3 vright = right * BIGNUMBER;
    Vector3 vup = up * BIGNUMBER;
    
    w.points[0] = org - vright + vup;
    w.points[1] = org + vright + vup;
    w.points[2] = org + vright - vup;
    w.points[3] = org - vright - vup;
    return w;
}

/**
* @brief Clip a winding against one plane in place.
* @param in Polygon to clip.
* @param split Plane to clip against.
* @param epsilon Tolerance used to classify points near the plane.
* @return True when the winding survives the clip.
**/
static bool ChopWindingInPlace( winding_t *in, const cm_plane_t *split, float epsilon ) {
    float dists[MAX_WINDING_POINTS + 4];
    int32_t sides[MAX_WINDING_POINTS + 4];
    int32_t counts[3] = {0, 0, 0};
    
    for (int32_t i=0; i<in->num_points; i++) {
        float dot = in->points[i].x * split->normal[0] + 
                    in->points[i].y * split->normal[1] + 
                    in->points[i].z * split->normal[2];
        dists[i] = dot - split->dist;
        if (dists[i] > epsilon) {
            sides[i] = 1; // front
        } else if (dists[i] < -epsilon) {
            sides[i] = 2; // back
        } else {
            sides[i] = 0; // on
        }
        counts[sides[i]]++;
    }
    sides[in->num_points] = sides[0];
    dists[in->num_points] = dists[0];
    
    if (counts[2] == 0) return false; // all front
    if (counts[1] == 0) return true; // all back
    
    winding_t out;
    out.num_points = 0;
    
    for (int32_t i=0; i<in->num_points; i++) {
        Vector3 p1 = in->points[i];
        if (sides[i] != 1) { // back or on
            out.points[out.num_points++] = p1;
        }
        if (sides[i] == 0 || sides[i] == sides[i+1]) {
            continue;
        }
        if (sides[i+1] == 0) {
            continue;
        }
        // split
        Vector3 p2 = in->points[(i+1)%in->num_points];
        float dot = dists[i] / (dists[i] - dists[i+1]);
        Vector3 mid;
        for (int32_t j=0; j<3; j++) {
            if (split->normal[j] == 1) mid[j] = split->dist;
            else if (split->normal[j] == -1) mid[j] = -split->dist;
            else mid[j] = p1[j] + dot * (p2[j] - p1[j]);
        }
        out.points[out.num_points++] = mid;
    }
    
    *in = out;
    return true;
}

/**
*	@brief Splits a winding by a plane into two separate windings.
**/
/**
* @brief Split a winding into front and back fragments against a plane.
* @param in Source polygon.
* @param split Plane used to split the polygon.
* @param epsilon Tolerance used for plane-side classification.
* @param poly_normal Original polygon normal used to classify coplanar fragments.
* @param front Output front fragment.
* @param back Output back fragment.
**/
static void SplitWinding( const winding_t *in, const cm_plane_t *split, float epsilon, const Vector3 &poly_normal, winding_t *front, winding_t *back ) {
    float dists[MAX_WINDING_POINTS + 4];
    int32_t sides[MAX_WINDING_POINTS + 4];
    int32_t counts[3] = {0, 0, 0};
    
    for (int32_t i=0; i<in->num_points; i++) {
        float dot = in->points[i].x * split->normal[0] + 
                    in->points[i].y * split->normal[1] + 
                    in->points[i].z * split->normal[2];
        dists[i] = dot - split->dist;
        if (dists[i] > epsilon) {
            sides[i] = 1; // front
        } else if (dists[i] < -epsilon) {
            sides[i] = 2; // back
        } else {
            sides[i] = 0; // on
        }
        counts[sides[i]]++;
    }
    sides[in->num_points] = sides[0];
    dists[in->num_points] = dists[0];
    
    front->num_points = 0;
    back->num_points = 0;
    front->entity_id = in->entity_id;
    back->entity_id = in->entity_id;
    front->transition_entity_id = in->transition_entity_id;
    back->transition_entity_id = in->transition_entity_id;

    if (counts[2] == 0 && counts[1] == 0) { // all on
        // Coplanar! If normals point in same direction, it's on the front of the brush. 
        // If normals are opposite, it's resting underneath the brush, so it goes to back (inside).
        Vector3 split_normal(split->normal[0], split->normal[1], split->normal[2]);
        if (QM_Vector3DotProduct(poly_normal, split_normal) > 0.0f) {
            *front = *in;
        } else {
            *back = *in;
        }
        return;
    }

    if (counts[2] == 0) { // all front (and on)
        *front = *in;
        return;
    }
    if (counts[1] == 0) { // all back (and on)
        *back = *in;
        return;
    }
    
    for (int32_t i=0; i<in->num_points; i++) {
        Vector3 p1 = in->points[i];
        
        if (sides[i] == 0) {
            front->points[front->num_points++] = p1;
            back->points[back->num_points++] = p1;
            continue;
        }
        
        if (sides[i] == 1) {
            front->points[front->num_points++] = p1;
        } else {
            back->points[back->num_points++] = p1;
        }
        
        if (sides[i+1] == 0 || sides[i+1] == sides[i]) {
            continue;
        }
        
        // split
        Vector3 p2 = in->points[(i+1)%in->num_points];
        float dot = dists[i] / (dists[i] - dists[i+1]);
        Vector3 mid;
        for (int32_t j=0; j<3; j++) {
            if (split->normal[j] == 1) mid[j] = split->dist;
            else if (split->normal[j] == -1) mid[j] = -split->dist;
            else mid[j] = p1[j] + dot * (p2[j] - p1[j]);
        }
        
        front->points[front->num_points++] = mid;
        back->points[back->num_points++] = mid;
    }
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
        const svg_func_door_t *door = static_cast<const svg_func_door_t *>( entity );
        return door->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_CLOSED ||
            door->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_MOVING_TO_CLOSED_STATE;
    }

    if ( entity->GetTypeInfo()->IsSubClassType<svg_func_areaportal_t>() ) {
        const svg_func_areaportal_t *areaportal = static_cast<const svg_func_areaportal_t *>( entity );
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
* @brief Register boundary edges that belong to a transition-owned face.
* @param face_idx Face whose perimeter may border a door transition strip.
* @note These edges are not portals and have no twin, but they still need the owning
*       entity ID so door callbacks can mark them blocked alongside the true portals.
**/
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
    if ( edge_a < 0 || edge_b < 0 || edge_a >= static_cast<int32_t>( g_nav_halfedges.size() ) || edge_b >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
        return false;
    }

    const int32_t face_a = g_nav_halfedges[ edge_a ].face_idx;
    const int32_t face_b = g_nav_halfedges[ edge_b ].face_idx;
    if ( face_a < 0 || face_b < 0 || face_a >= static_cast<int32_t>( g_nav_faces.size() ) || face_b >= static_cast<int32_t>( g_nav_faces.size() ) || face_a == face_b ) {
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

    // Allow doors and their linked areaportals to connect natively.
    // (Areaportals are now ignored entirely during navmesh generation, so this hack is no longer needed.)

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
    if ( brush_instance.angles.x != 0.0f || brush_instance.angles.y != 0.0f || brush_instance.angles.z != 0.0f ) {
        vec3_t angles = { brush_instance.angles.x, brush_instance.angles.y, brush_instance.angles.z };
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
    transformed_plane.dist += QM_Vector3DotProduct( Vector3( transformed_plane.normal[ 0 ], transformed_plane.normal[ 1 ], transformed_plane.normal[ 2 ] ), brush_instance.offset );
    return transformed_plane;
}

/**
* @brief Computes the maximum Z height of a brush by physically building its geometry.
* @param b The brush to analyze.
 * @param brush_instance World-space transform applied to the brush's planes.
* @return The maximum Z height of the brush, or 999999.0f if the brush could not be constructed.
**/
static float GetBrushMaxZ( const mbrush_t *b, const nav_brush_ownership_t &brush_instance ) {
	float max_z = -999999.0f;
	
    // Try to build a face for every plane of the brush
	for ( int32_t i = 0; i < b->numsides; i++ ) {
        mbrushside_t* side = &b->firstbrushside[i];
        cm_plane_t p = GetTransformedPlane( side->plane, brush_instance );
        
        winding_t w = BaseWindingForPlane( &p );
        bool valid = true;
        
        for ( int32_t j = 0; j < b->numsides && valid; j++ ) {
            if ( i == j ) continue;
            cm_plane_t clip = GetTransformedPlane( b->firstbrushside[ j ].plane, brush_instance );
            if ( !ChopWindingInPlace( &w, &clip, 0.1f ) ) {
                valid = false;
            }
        }
        
        // If this face is valid, check its vertices for the maximum Z coordinate.
        if ( valid ) {
            for ( int32_t pt = 0; pt < w.num_points; pt++ ) {
                if ( w.points[pt].z > max_z ) {
                    max_z = w.points[pt].z;
                }
            }
        }
	}
    
    // If the brush geometry could not be constructed for some reason, return a high Z value to treat it as a wall.
	return (max_z == -999999.0f) ? 999999.0f : max_z;
}

/**
* @brief Computes which planes of a brush are actually part of its physical surface (not redundant).
**/
static std::vector<bool> GetBrushActivePlanes( const mbrush_t *b, const nav_brush_ownership_t &brush_instance ) {
    std::vector<bool> active(b->numsides, false);
    for ( int32_t i = 0; i < b->numsides; i++ ) {
        cm_plane_t p = GetTransformedPlane( b->firstbrushside[ i ].plane, brush_instance );
        winding_t w = BaseWindingForPlane( &p );
        bool valid = true;
        for ( int32_t j = 0; j < b->numsides && valid; j++ ) {
            if ( i == j ) continue;
            cm_plane_t clip = GetTransformedPlane( b->firstbrushside[ j ].plane, brush_instance );
            if ( !ChopWindingInPlace( &w, &clip, 0.1f ) ) {
                valid = false;
            }
        }
        if ( valid && w.num_points >= 3 ) {
            active[i] = true;
        }
    }
    return active;
}

/**
*	@brief	Determine whether every constructed boundary vertex of one convex brush lies in another convex brush.
*	@param	candidate	Brush whose complete volume may be contained.
*	@param	candidate_offset	World-space translation for the candidate brush.
*	@param	container	Brush that may enclose the candidate.
*	@param	container_offset	World-space translation for the enclosing brush.
*	@return	True when the candidate is fully enclosed by the container.
*	@note	Partial overlap is deliberately not containment. Only a complete enclosure can be
*			removed from a compound door union without changing its visible outer boundary.
**/
static bool IsBrushFullyContainedByBrush( const mbrush_t *candidate, const nav_brush_ownership_t &candidate_instance, const mbrush_t *container, const nav_brush_ownership_t &container_instance ) {
    /**
    *	Reject invalid brush data before constructing convex boundary windings.
    **/
    if ( candidate == nullptr || container == nullptr || candidate->numsides <= 0 || container->numsides <= 0 ) {
        return false;
    }

    /**
    *	Build only physical brush faces. Redundant BSP planes cannot contribute a volume
    *	vertex and would otherwise make containment classification implementation-defined.
    **/
    const std::vector<bool> candidate_plane_active = GetBrushActivePlanes( candidate, candidate_instance );
    const std::vector<bool> container_plane_active = GetBrushActivePlanes( container, container_instance );
    bool found_candidate_vertex = false;

    /**
    *	Every vertex on every active candidate face must remain behind every active
    *	container plane. A point in front of one outward-facing plane is outside the
    *	container, proving that this candidate adds visible compound-door geometry.
    **/
    for ( int32_t candidate_side_index = 0; candidate_side_index < candidate->numsides; candidate_side_index++ ) {
        if ( !candidate_plane_active[ candidate_side_index ] ) {
            continue;
        }

        const cm_plane_t candidate_plane = GetTransformedPlane( candidate->firstbrushside[ candidate_side_index ].plane, candidate_instance );
        winding_t candidate_face = BaseWindingForPlane( &candidate_plane );
        bool candidate_face_valid = true;
        for ( int32_t clip_side_index = 0; clip_side_index < candidate->numsides && candidate_face_valid; clip_side_index++ ) {
            if ( candidate_side_index == clip_side_index ) {
                continue;
            }

            const cm_plane_t candidate_clip_plane = GetTransformedPlane( candidate->firstbrushside[ clip_side_index ].plane, candidate_instance );
            candidate_face_valid = ChopWindingInPlace( &candidate_face, &candidate_clip_plane, 0.1f );
        }

        // Ignore an invalid generated face; valid convex brushes expose at least one usable boundary face.
        if ( !candidate_face_valid || candidate_face.num_points < 3 ) {
            continue;
        }

        for ( int32_t point_index = 0; point_index < candidate_face.num_points; point_index++ ) {
            found_candidate_vertex = true;
            const Vector3 &candidate_vertex = candidate_face.points[ point_index ];
            for ( int32_t container_side_index = 0; container_side_index < container->numsides; container_side_index++ ) {
                if ( !container_plane_active[ container_side_index ] ) {
                    continue;
                }

                const cm_plane_t container_plane = GetTransformedPlane( container->firstbrushside[ container_side_index ].plane, container_instance );
                const Vector3 container_normal( container_plane.normal[ 0 ], container_plane.normal[ 1 ], container_plane.normal[ 2 ] );
                // Keep a shared numerical tolerance for face-boundary contact while rejecting real protrusions.
                if ( QM_Vector3DotProduct( candidate_vertex, container_normal ) - container_plane.dist > 0.1f ) {
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
* @param offset The world offset to apply to the brush's planes.
* @param plane_active Precomputed boolean array of which planes are non-redundant.
* @return True when the fragment can be kept without further clipping.
**/
static bool IsFragmentCompletelyOutsideBrush( const winding_t *frag, const mbrush_t *b, const nav_brush_ownership_t &brush_instance, const std::vector<bool> &plane_active, const float expand = 0.0f ) {
    for (int32_t j = 0; j < b->numsides; j++) {
        if (!plane_active[j]) continue;

        mbrushside_t* side = &b->firstbrushside[j];
        cm_plane_t p = GetTransformedPlane( side->plane, brush_instance );
        p.dist += expand;
        
        // If all points of the fragment are strictly in front of this plane,
        // the entire fragment is outside the convex brush.
        bool completely_outside = true;
        for (int32_t i = 0; i < frag->num_points; i++) {
            float d = QM_Vector3DotProduct(frag->points[i], Vector3(p.normal[0], p.normal[1], p.normal[2])) - p.dist;
            // 0.1f tolerance to account for floating point inaccuracies
            if (d <= 0.1f) {
                completely_outside = false;
                break;
            }
        }
        if (completely_outside) {
            return true;
        }
    }
    return false;
}

/**
*	@brief Subtracts a brush from a list of polygon fragments, replacing them with the resulting smaller fragments.
**/
/**
* @brief Subtract one blocking brush from a set of polygon fragments.
* @param fragments Fragments that will be clipped in place.
* @param b Blocking brush to subtract.
 * @param brush_instance World-space transform applied to the brush's planes.
* @param poly_normal Original polygon normal used for coplanar tests.
**/
static void SubtractBrushFromWindings( std::vector<winding_t> &fragments, const mbrush_t *b, const nav_brush_ownership_t &brush_instance, const Vector3 &poly_normal ) {
	std::vector<winding_t> next_fragments;
    std::vector<bool> plane_active = GetBrushActivePlanes( b, brush_instance );
	
	for ( const winding_t& frag : fragments ) {
		// Optimization: If the fragment is entirely in front of any plane of the brush, it is completely outside the brush.
		// We can skip splitting it entirely, saving massive amounts of fragmentation!
        if ( IsFragmentCompletelyOutsideBrush( &frag, b, brush_instance, plane_active, 0.0f ) ) {
			next_fragments.push_back(frag);
			continue;
		}

		winding_t inside_part = frag;
		bool entirely_inside = true;
		
		// Slicing against every plane of the brush
		for ( int32_t j = 0; j < b->numsides; j++ ) {
			if (!plane_active[j]) continue;
			
			mbrushside_t* side = &b->firstbrushside[j];
            cm_plane_t p = GetTransformedPlane( side->plane, brush_instance );
			
			winding_t front = {};
			winding_t back = {};
			SplitWinding(&inside_part, &p, 0.1f, poly_normal, &front, &back);
			
			// Anything in front of an outward-facing plane is strictly OUTSIDE the brush. We save it.
			if (front.num_points >= 3) {
				next_fragments.push_back(front);
				entirely_inside = false;
			}
			
			// Anything behind the plane might still be inside the brush, so we keep chopping it.
			inside_part = back;
			if (inside_part.num_points < 3) {
				entirely_inside = false;
				break; // It's no longer inside the brush
			}
		}
		
		// If inside_part still has points after checking all planes, it is completely inside the brush.
		// Since this is a subtraction, we simply discard it! (Do not add it to next_fragments).
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
static float GetFragmentsInsideBrushArea( const std::vector<winding_t> &fragments, const mbrush_t *b, const nav_brush_ownership_t &brush_instance ) {
    /**
    * 	Reject malformed brush input before constructing measurement fragments.
    **/
    if ( b == nullptr || b->numsides <= 0 ) {
        return 0.0f;
    }

    const std::vector<bool> plane_active = GetBrushActivePlanes( b, brush_instance );
    float total_area = 0.0f;
    for ( const winding_t &fragment : fragments ) {
        /**
        * 	Clip a local copy to the brush interior. The front side of each outward
        * 	plane lies outside, while the back side remains a volume candidate.
        **/
        winding_t inside_fragment = fragment;
        for ( int32_t side_index = 0; side_index < b->numsides; side_index++ ) {
            if ( !plane_active[ side_index ] || inside_fragment.num_points < 3 ) {
                continue;
            }

            const cm_plane_t plane = GetTransformedPlane( b->firstbrushside[ side_index ].plane, brush_instance );
            winding_t front = {};
            winding_t back = {};
            SplitWinding( &inside_fragment, &plane, 0.1f, Vector3( 0.0f, 0.0f, 1.0f ), &front, &back );
            inside_fragment = back;
        }

        /**
        * 	Accumulate a triangle fan only for a valid surviving interior polygon.
        **/
        for ( int32_t vertex_index = 2; vertex_index < inside_fragment.num_points; vertex_index++ ) {
            const Vector3 cross = QM_Vector3CrossProduct( inside_fragment.points[ vertex_index - 1 ] - inside_fragment.points[ 0 ], inside_fragment.points[ vertex_index ] - inside_fragment.points[ 0 ] );
            total_area += 0.5f * QM_Vector3Length( cross );
        }
    }

    return total_area;
}

/**
*	@brief Attempts to merge two coplanar convex polygons. If successful, stores the merged strictly-convex polygon in out.
**/
/**
* @brief Attempt to merge two coplanar convex windings.
* @param w1 First winding.
* @param w2 Second winding.
* @param normal Plane normal shared by both windings.
* @param out Output merged winding.
* @return True when a valid merged polygon could be produced.
**/
static bool TryMergeWindings(const winding_t* w1, const winding_t* w2, const Vector3& normal, winding_t* out) {
    if (!w1 || !w2 || !out) return false;
    
                // Do not merge polygons that belong to different entities!
                // This preserves the special door boundary edges for the nav graph.
    if (w1->entity_id != w2->entity_id) {
        return false;
    }
    int32_t match_i1 = -1, match_i2 = -1;
    for (int32_t i1 = 0; i1 < w1->num_points; i1++) {
        Vector3 a1 = w1->points[i1];
        Vector3 b1 = w1->points[(i1 + 1) % w1->num_points];

        for (int32_t i2 = 0; i2 < w2->num_points; i2++) {
            Vector3 a2 = w2->points[i2];
            Vector3 b2 = w2->points[(i2 + 1) % w2->num_points];

                // Match b2 == a1 and a2 == b1 (reverse order).
                if ( QM_Vector3DistanceSqr( a1, b2 ) < 0.1f && QM_Vector3DistanceSqr( b1, a2 ) < 0.1f ) {
                match_i1 = i1;
                match_i2 = i2;
                break;
            }
        }
        if (match_i1 != -1) break;
    }

    if (match_i1 == -1) return false;

    // 2. Build merged polygon
    out->num_points = 0;
    int32_t n1 = w1->num_points;
    int32_t n2 = w2->num_points;

    for (int32_t j = 0; j < n1; j++) {
        out->points[out->num_points++] = w1->points[(match_i1 + 1 + j) % n1];
    }
    for (int32_t j = 0; j < n2 - 2; j++) {
        out->points[out->num_points++] = w2->points[(match_i2 + 2 + j) % n2];
    }

    // 3. Check convexity and simplify
    winding_t simple = *out;
    for (int32_t j = 0; j < simple.num_points; j++) {
        Vector3 p1 = simple.points[j];
        Vector3 p2 = simple.points[(j + 1) % simple.num_points];
        Vector3 p3 = simple.points[(j + 2) % simple.num_points];
        Vector3 dir1 = QM_Vector3Subtract(p2, p1);
        Vector3 dir2 = QM_Vector3Subtract(p3, p2);
        
        Vector3 cross = QM_Vector3CrossProduct(dir1, dir2);
        float dot = QM_Vector3DotProduct(cross, normal);

        if (dot < -0.01f) {
            return false; // Concave, cannot merge
        }
        if (std::abs(dot) < 0.01f) {
            // Collinear points, remove the middle one
            for (int32_t k = j + 1; k < simple.num_points - 1; k++) {
                simple.points[k] = simple.points[k + 1];
            }
            simple.num_points--;
            j--;
        }
    }
    
    if (simple.num_points < 3 || simple.num_points > MAX_WINDING_POINTS) return false; // Too complex or invalid

    *out = simple;
    return true;
}

/**
* @brief Determine whether a spatial-partition fragment has enough area to become a nav polygon.
* @param winding Candidate fragment produced by an axis-aligned split.
* @return True when the fragment is a non-degenerate polygon rather than a numerical sliver.
* @note Rejecting slivers here prevents partition edges from fanning into door corners and preserves the unsplit source polygon.
**/
static bool IsUsablePartitionFragment( const winding_t &winding ) {
    /**
    * Reject fragments that cannot form a valid polygon before calculating their area.
    **/
    if ( winding.num_points < 3 || winding.num_points > MAX_WINDING_POINTS ) {
        return false;
    }

    /**
    * Measure polygon area and horizontal extent to identify narrow numerical remnants.
    **/
    float area = 0.0f;
    float min_x = winding.points[ 0 ].x;
    float min_y = winding.points[ 0 ].y;
    float max_x = min_x;
    float max_y = min_y;
    for ( int32_t i = 0; i < winding.num_points; i++ ) {
        const Vector3 &point = winding.points[ i ];
        min_x = std::min( min_x, point.x );
        min_y = std::min( min_y, point.y );
        max_x = std::max( max_x, point.x );
        max_y = std::max( max_y, point.y );

        if ( i >= 2 ) {
            const Vector3 first_edge = winding.points[ i - 1 ] - winding.points[ 0 ];
            const Vector3 second_edge = point - winding.points[ 0 ];
            area += 0.5f * QM_Vector3Length( QM_Vector3CrossProduct( first_edge, second_edge ) );
        }
    }

    const float longest_extent = std::max( max_x - min_x, max_y - min_y );
    return area >= 1.0f && ( longest_extent <= 0.001f || ( area / longest_extent ) >= 2.0f );
}

/**
* @brief Checks whether the given node pointer securely falls within the loaded bsp nodes or leafs boundaries.
**/
static bool IsNodeValid(const bsp_t* bsp, const mnode_t* node) {
    if (!node || !bsp) return false;
    
    // Check if it safely falls within the bsp->nodes array.
    if (node >= bsp->nodes && node < bsp->nodes + bsp->numnodes) {
        return true;
    }
    
    // Check if it safely falls within the bsp->leafs array.
    const mleaf_t* leaf = reinterpret_cast<const mleaf_t*>(node);
    if (leaf >= bsp->leafs && leaf < bsp->leafs + bsp->numleafs) {
        return true;
    }
    
    // Likely uninitialized memory, or synthetic hulls. We don't process these here.
    return false;
}

/**
* @brief Recursively traverse a BSP tree node and collect all brush indices that belong to it.
**/
static void CollectModelBrushes( bsp_t *bsp, mnode_t *node, const int32_t model_num, const int32_t instance_id, const int32_t entity_id, const Vector3 &offset, const Vector3 &angles, std::vector<nav_brush_ownership_t> &brush_instances, std::vector<bool> &seen_brushes ) {
    /**
    *   Sanity check to prevent out-of-bounds pointer reads if a map happens to have corrupted inline trees
    *   or synthetic engine hulls that point outside normal geometry pools.
    **/
    if (!IsNodeValid(bsp, node)) {
        return;
    }
    
    // If it's a leaf, process its brushes
    if (node->plane == nullptr) {
        mleaf_t* leaf = (mleaf_t*)node;
        
        // For each leaf, process its brushes
        for (int32_t i = 0; i < leaf->numleafbrushes; i++) {
            mbrush_t* b = leaf->firstleafbrush[i];
            int32_t brush_num = b - bsp->brushes;
            
            if (brush_num >= 0 && brush_num < bsp->numbrushes) {
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
 * @note The split uses the exact translated brush planes so the generated transition edge remains seated on the door geometry.
**/
static void SplitWindingsByEntityBrush( std::vector<winding_t> &fragments, const mbrush_t *b, const nav_brush_ownership_t &brush_instance, const float expand = 0.0f ) {
    std::vector<winding_t> next_fragments;
    std::vector<bool> plane_active = GetBrushActivePlanes( b, brush_instance );
    
    for (const winding_t& frag : fragments) {
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
        
        for (int32_t j = 0; j < b->numsides; j++) {
            if (!plane_active[j]) continue;
            
            mbrushside_t* side = &b->firstbrushside[j];
            cm_plane_t plane = GetTransformedPlane( side->plane, brush_instance );
            // Keep the split on the authored brush plane; expanding it creates a visible offset from the closed door.
            plane.dist += expand;
            
            winding_t front = {};
            winding_t back = {};
            SplitWinding(&inside_part, &plane, 0.1f, Vector3(0,0,1) /* not used */, &front, &back);
            
            if (front.num_points >= 3) {
                /**
                *   Keep the outer world strip as ordinary world geometry.
                *   It borders the door volume, but it is not itself door-owned and must
                *   not be promoted into transition metadata or runtime edge ownership.
                **/
                next_fragments.push_back(front);
            }

            inside_part = back; // The back part is inside this plane, keep checking it against other planes
        }

        // Whatever is left in inside_part after checking all planes is completely inside the brush!
        if (inside_part.num_points >= 3) {
            inside_part.entity_id = brush_instance.entity_id;
            inside_part.transition_entity_id = ENTITYNUM_NONE;
            next_fragments.push_back(inside_part);
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
    if ( static_cast<size_t>( entity_id ) >= g_nav_entity_edges.size() ) {
        g_nav_entity_edges.resize( static_cast<size_t>( entity_id ) + 1 );
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
*	@brief Extract walkable polygons from the current map's collision model and store them in g_nav_polys.
**/
/**
* @brief Extract walkable polygons from the current map collision model.
* @note This is the first stage of navmesh generation and fills g_nav_polys.
**/
void Nav_DoExtractionWork() {
	// Get the collision model from the game interface
    cm_t* cm = gi.GetCollisionModel();
	// Safety check: Ensure the collision model and its cache are valid before proceeding.
	if ( !cm || !cm->cache ) {
        gi.dprintf("Nav_DoExtractionWork: No collision model or cache!\n");
		return;
	}
	// Get the BSP data from the collision model's cache
    bsp_t* bsp = cm->cache;

	// Now we can safely clear the global navmesh polygon container before starting the extraction process.
    ResetNavGenerationDiagnostics();
    g_nav_polys.clear();

    
    // Parse the runtime edicts to find active bmodels while retaining separate ownership for every model instance.
    std::vector<nav_brush_ownership_t> brush_instances;
    brush_instances.reserve( static_cast<size_t>( bsp->numbrushes ) );
    int32_t next_instance_id = 1;
    
    // World geometry is always active, and has no offset.
    if (bsp->models != nullptr && bsp->nummodels > 0 && bsp->models[0].headnode != nullptr) {
        std::vector<bool> seen_world_brushes( static_cast<size_t>( bsp->numbrushes ), false );
        CollectModelBrushes( bsp, bsp->models[ 0 ].headnode, 0, 0, ENTITYNUM_NONE, Vector3( 0.0f, 0.0f, 0.0f ), Vector3( 0.0f, 0.0f, 0.0f ), brush_instances, seen_world_brushes );
    }
    
    for (int32_t i = 1; i < g_edict_pool.num_edicts; i++) {
        svg_base_edict_t* edict = g_edicts[i];
        if ( !SVG_Entity_IsActive( edict ) ) {
            continue;
        }

        if ( edict->model.ptr != nullptr && edict->model.size() >= 2 && edict->model[ 0 ] == '*' ) {
            if ( edict->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ||
                 edict->GetTypeInfo()->IsSubClassType<svg_func_door_rotating_t>() ) {
                // Ignore physical door geometry so it doesn't carve a 3D footprint into the floor,
                // which would result in multiple redundant portals. We will tag the natural BSP edge later.
                continue;
            }
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
                    // Inline BSP planes are authored in local coordinates; position them at the current runtime origin.
                    const Vector3 offset = edict->currentOrigin;
                    
                    int32_t assigned_ent_id = ENTITYNUM_NONE;
                    if (edict->GetTypeInfo()->IsSubClassType<svg_func_wall_t>()) {
                        assigned_ent_id = i;
                    }
                    
                    std::vector<bool> seen_model_brushes( static_cast<size_t>( bsp->numbrushes ), false );
                    CollectModelBrushes( bsp, bsp->models[ model_num ].headnode, model_num, next_instance_id++, assigned_ent_id, offset, edict->currentAngles, brush_instances, seen_model_brushes );
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
        const char *first_texture_name = "<none>";
        for ( int32_t side_index = 0; side_index < dynamic_brush->numsides; side_index++ ) {
            const mbrushside_t *side = &dynamic_brush->firstbrushside[ side_index ];
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
            static_cast<uint32_t>( dynamic_brush->contents ),
            dynamic_brush->numsides,
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

		// Discard brushes that belong to the sky
		bool is_sky_brush = false;
		for ( int32_t j = 0; j < b->numsides; j++ ) {
			mbrushside_t *side = &b->firstbrushside[ j ];
			if ( side->texinfo && ( side->texinfo->c.flags & CM_SURFACE_FLAG_SKY ) ) {
				is_sky_brush = true;
				break;
			}
		}
		if ( is_sky_brush ) {
			continue;
		}

		//! Iterate through each side of the brush to find walkable surfaces
		for ( int32_t j = 0; j < b->numsides; j++ ) {
			// Get the current brush side
			mbrushside_t *side = &b->firstbrushside[ j ];
			
			// Discard sides that belong to sky surfaces (we don't want to walk on the skybox).
			if ( side->texinfo && ( side->texinfo->c.flags & CM_SURFACE_FLAG_SKY ) ) {
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
			//! Clip the winding against all other sides of the brush to ensure it fits within the brush's volume
			for ( int32_t k = 0; k < b->numsides && valid; k++ ) {
				// Skip clipping against the same side
				if ( j == k ) {
					continue;
				}
				// Get the brush side to clip against
				mbrushside_t *clip = &b->firstbrushside[ k ];
                cm_plane_t clip_plane = GetTransformedPlane( clip->plane, brush_instance );
				// Clip the winding in place against the plane. If it fails, mark the winding as invalid.
				if ( !ChopWindingInPlace( &w, &clip_plane, 0.1f ) ) {
					valid = false;
				}
			}
			// If the winding is still valid and has at least 3 points, create a nav_poly_t and add it to the global navmesh polygon container.
			if ( !valid || w.num_points < 3 ) {
				continue;
			}
            w.entity_id = brush_instance.entity_id;

			std::vector<winding_t> fragments;
				fragments.push_back(w);

                /**
                *   Calculate the aggregate polygon area currently available on this source
                *   floor. This is used only by the bounded ordered-clipping audit below.
                **/
				// Calculate the highest Z of the floor polygon we are currently subtracting from
				float floor_max_z = -999999.0f;
				for ( int32_t p = 0; p < w.num_points; p++ ) {
					if ( w.points[ p ].z > floor_max_z ) {
						floor_max_z = w.points[ p ].z;
					}
				}

				// Subtract all other solid brushes from this walkable surface
				Vector3 normal(shifted_plane.normal[0], shifted_plane.normal[1], shifted_plane.normal[2]);
                for ( size_t other_instance_index = 0; other_instance_index < brush_instances.size(); other_instance_index++ ) {
                    if ( other_instance_index == instance_brush_index ) {
                        continue; // Skip the source brush instance itself.
                    }

                    const nav_brush_ownership_t &other_instance = brush_instances[ other_instance_index ];
                    // Only compatible model ownership domains may alter this source polygon.
                    if ( !AreBrushesCompatibleForClipping( brush_instance, other_instance ) ) {
                        continue;
                    }

                    mbrush_t *other_b = &bsp->brushes[ other_instance.brush_num ];
					if ( !( other_b->contents & ( CONTENTS_SOLID | CONTENTS_DETAIL | CONTENTS_MONSTERCLIP ) ) ) {
						continue; // Only subtract blocking geometry
					}
					
                    // Determine whether the candidate brush is a low stair-like step.
                    // Low steps must still carve the surrounding floor, but the first tread
                    // should not be fully swallowed or turned into a blocked boundary strip.
                    const float other_max_z = GetBrushMaxZ( other_b, other_instance );
                    const bool isLowAscendingStep = other_max_z > floor_max_z && other_max_z <= floor_max_z + NAV_MAX_STEP_HEIGHT;
                    if ( isLowAscendingStep ) {
                        if ( other_max_z - floor_max_z <= NAV_MAX_STEP_HEIGHT * 0.25f ) {
                            continue;
                        }
                    }
					
                    const int32_t other_ent_id = other_instance.entity_id;
                    if ( IsDynamicTransitionBrush( other_instance ) ) {
                        /**
                        *   A fully enclosed same-door brush cannot contribute an outer
                        *   compound-door boundary. Skip it before fragment clipping so it
                        *   cannot manufacture an internal portal or duplicate seam.
                        **/
                        if ( IsContainedDynamicTransitionBrush( bsp, brush_instances, other_instance_index ) ) {
                            s_nav_generation_diagnostics.contained_dynamic_clip_skips++;
                            continue;
                        }

                        // A dynamic model may exist elsewhere in the map; only split this floor when its current fragment reaches the model volume.
                        const std::vector<bool> other_plane_active = GetBrushActivePlanes( other_b, other_instance );
                        bool intersectsFragment = false;
                        for ( const winding_t &fragment : fragments ) {
                            if ( !IsFragmentCompletelyOutsideBrush( &fragment, other_b, other_instance, other_plane_active, 4.0f ) ) {
                                intersectsFragment = true;
                                break;
                            }
                        }
                        if ( !intersectsFragment ) {
                            continue;
                        }

                        // This is a door brush! We DO NOT subtract it to block movement.
                        // Instead, we split the fragments with the standard safety expansion, and any fragment
                        // that falls INSIDE the door gets its entity_id updated!
                        SplitWindingsByEntityBrush( fragments, other_b, other_instance, 0.0f );
                    } else {
                        /**
                        *   Measure the actual remaining floor area inside every dynamic
                        *   transition brush before this static subtraction. Unlike aggregate
                        *   floor-area logging, this isolates loss of the door footprint.
                        **/
                        float dynamic_overlap_before = 0.0f;
                        int32_t overlap_entity_id = ENTITYNUM_NONE;
                        for ( const nav_brush_ownership_t &pending_dynamic_instance : brush_instances ) {
                            if ( !IsDynamicTransitionBrush( pending_dynamic_instance ) ) {
                                continue;
                            }

                            const mbrush_t *pending_dynamic_brush = &bsp->brushes[ pending_dynamic_instance.brush_num ];
                            const float candidate_overlap = GetFragmentsInsideBrushArea( fragments, pending_dynamic_brush, pending_dynamic_instance );
                            if ( candidate_overlap > dynamic_overlap_before ) {
                                dynamic_overlap_before = candidate_overlap;
                                overlap_entity_id = pending_dynamic_instance.entity_id;
                            }
                            if ( dynamic_overlap_before > 0.01f ) {
                                break;
                            }
                        }

                        // Normal solid obstacle subtraction.
                        SubtractBrushFromWindings( fragments, other_b, other_instance, normal );

                        /**
                        *   Report only a static blocker that removes actual dynamic overlap.
                        *   This names the geometry that changes the candidate door footprint,
                        *   rather than unrelated reductions elsewhere on a large floor.
                        **/
                        if ( dynamic_overlap_before > 0.01f && printed_pending_dynamic_blockers < 16 ) {
                            const mbrush_t *overlap_dynamic_brush = nullptr;
                            nav_brush_ownership_t overlap_dynamic_instance = {};
                            for ( const nav_brush_ownership_t &pending_dynamic_instance : brush_instances ) {
                                if ( pending_dynamic_instance.entity_id == overlap_entity_id && IsDynamicTransitionBrush( pending_dynamic_instance ) ) {
                                    overlap_dynamic_brush = &bsp->brushes[ pending_dynamic_instance.brush_num ];
                                    overlap_dynamic_instance = pending_dynamic_instance;
                                    break;
                                }
                            }

                            const float dynamic_overlap_after = GetFragmentsInsideBrushArea( fragments, overlap_dynamic_brush, overlap_dynamic_instance );
                            if ( dynamic_overlap_after < dynamic_overlap_before - 0.01f ) {
                                /**
                                *   Attribute the overlap loss to concrete compiled brush
                                *   geometry. Bounds are constructed from shifted planes so
                                *   they use the same world-space convention as extraction.
                                **/
                                Vector3 blocker_mins( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
                                Vector3 blocker_maxs( std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() );
                                bool has_blocker_bounds = false;
                                for ( int32_t blocker_side_index = 0; blocker_side_index < other_b->numsides; blocker_side_index++ ) {
                                    const cm_plane_t blocker_plane = GetTransformedPlane( other_b->firstbrushside[ blocker_side_index ].plane, other_instance );
                                    winding_t blocker_winding = BaseWindingForPlane( &blocker_plane );
                                    bool blocker_face_valid = true;
                                    for ( int32_t blocker_clip_index = 0; blocker_clip_index < other_b->numsides && blocker_face_valid; blocker_clip_index++ ) {
                                        if ( blocker_clip_index == blocker_side_index ) {
                                            continue;
                                        }

                                        const cm_plane_t blocker_clip_plane = GetTransformedPlane( other_b->firstbrushside[ blocker_clip_index ].plane, other_instance );
                                        blocker_face_valid = ChopWindingInPlace( &blocker_winding, &blocker_clip_plane, 0.1f );
                                    }

                                    if ( !blocker_face_valid ) {
                                        continue;
                                    }

                                    for ( int32_t blocker_vertex_index = 0; blocker_vertex_index < blocker_winding.num_points; blocker_vertex_index++ ) {
                                        const Vector3 &blocker_vertex = blocker_winding.points[ blocker_vertex_index ];
                                        blocker_mins.x = std::min( blocker_mins.x, blocker_vertex.x );
                                        blocker_mins.y = std::min( blocker_mins.y, blocker_vertex.y );
                                        blocker_mins.z = std::min( blocker_mins.z, blocker_vertex.z );
                                        blocker_maxs.x = std::max( blocker_maxs.x, blocker_vertex.x );
                                        blocker_maxs.y = std::max( blocker_maxs.y, blocker_vertex.y );
                                        blocker_maxs.z = std::max( blocker_maxs.z, blocker_vertex.z );
                                        has_blocker_bounds = true;
                                    }
                                }

                                const char *blocker_texture = "<none>";
                                for ( int32_t blocker_side_index = 0; blocker_side_index < other_b->numsides; blocker_side_index++ ) {
                                    const mbrushside_t *blocker_side = &other_b->firstbrushside[ blocker_side_index ];
                                    if ( blocker_side->texinfo != nullptr && blocker_side->texinfo->name[ 0 ] != '\0' ) {
                                        blocker_texture = blocker_side->texinfo->name;
                                        break;
                                    }
                                }

                                gi.dprintf( "NavMesh DynamicOverlapBlocker [sourceBrush=%d side=%d entity=%d blockerBrush=%d contents=0x%08x overlap=%.1f->%.1f bounds=(%.1f %.1f %.1f)->(%.1f %.1f %.1f) maxZ=%.1f texture=%s]\n",
                                    brush_num,
                                    j,
                                    overlap_entity_id,
                                    other_instance.brush_num,
                                    static_cast<uint32_t>( other_b->contents ),
                                    dynamic_overlap_before,
                                    dynamic_overlap_after,
                                    blocker_mins.x,
                                    blocker_mins.y,
                                    blocker_mins.z,
                                    blocker_maxs.x,
                                    blocker_maxs.y,
                                    blocker_maxs.z,
                                    has_blocker_bounds ? blocker_maxs.z : 0.0f,
                                    blocker_texture );
                                printed_pending_dynamic_blockers++;
                            }
                        }
                    }
					
					if ( fragments.empty() ) {
						break; // Entire surface was swallowed by other brushes
					}
				}
				// Merge fragments to reduce unnecessary fragmentation and keep polygon counts low
#if 1
				bool merged = true;
				while (merged) {
					merged = false;
					for (size_t f1 = 0; f1 < fragments.size() && !merged; f1++) {
						for (size_t f2 = f1 + 1; f2 < fragments.size(); f2++) {
							winding_t merged_w;
							if (TryMergeWindings(&fragments[f1], &fragments[f2], normal, &merged_w)) {
								// Replace f1 with merged, remove f2
								fragments[f1] = merged_w;
								fragments.erase(fragments.begin() + f2);
								merged = true; // Restart the scan because indices have changed
								break;
							}
						}
					}
				}
#endif
				
				// Add surviving fragments
				for ( const winding_t& frag : fragments ) {
					// --- SLIVER PRUNING ---
					// Calculate polygon area and AABB to prune slivers (very long and thin polygons).
					if ( frag.num_points >= 3 ) {
						float area = 0.0f;
						Vector3 mins = frag.points[ 0 ];
						Vector3 maxs = frag.points[ 0 ];
						for ( int32_t v = 0; v < frag.num_points; v++ ) {
							Vector3 p = frag.points[ v ];
							mins.x = std::min( mins.x, p.x );
							mins.y = std::min( mins.y, p.y );
							mins.z = std::min( mins.z, p.z );
							maxs.x = std::max( maxs.x, p.x );
							maxs.y = std::max( maxs.y, p.y );
							maxs.z = std::max( maxs.z, p.z );
							
							if ( v >= 2 ) {
								Vector3 cross = QM_Vector3CrossProduct( frag.points[ v - 1 ] - frag.points[ 0 ], p - frag.points[ 0 ] );
								area += 0.5f * QM_Vector3Length( cross );
							}
						}
						
						float dx = maxs.x - mins.x;
						float dy = maxs.y - mins.y;
						float longest = std::max( dx, dy );
						
						// If the average width (area / longest edge) is less than 2 units, it's a useless sliver!
						// This perfectly culls floating-point errors without destroying valid narrow pathways or creating false insets.
						if ( area < 1.0f || ( longest > 0.001f && ( area / longest ) < 2.0f ) ) {
							sliver_pruned_fragments++;
							continue; // Prune sliver
						}
					}
					// ----------------------

					// Create a new nav_poly_t and populate its fields.
					nav_poly_t poly = {};
					// Assign a unique polygon ID based on the current size of the global navmesh polygon container.
					poly.poly_id = g_nav_polys.size();
					// Clamp the number of vertices to 8, as nav_poly_t supports a maximum of 8 vertices.
					poly.num_vertices = std::min( frag.num_points, MAX_WINDING_POINTS );
					// Calculate the center of the polygon by averaging its vertices.
					Vector3 center( 0, 0, 0 );
					// Copy the vertices from the winding to the nav_poly_t and accumulate the center.
					for ( int32_t v = 0; v < poly.num_vertices; v++ ) {
						// Copy the vertex position from the winding to the polygon.
						poly.vertices[ v ] = frag.points[ v ];
						// Accumulate the vertex positions to compute the center.
						center = center + poly.vertices[ v ];
					}
					// Average the accumulated vertex positions to find the center of the polygon.
					poly.center = center / static_cast<float>( poly.num_vertices );
					// Store the normal of the polygon plane, derived from the original brush side.
					poly.normal = normal;
                    poly.entity_id = frag.entity_id;
                    poly.transition_entity_id = frag.transition_entity_id;
					// Save the ID of the BSP leaf that contributed this polygon (used for leaf-local pathfinding lookups).
					poly.bsp_leaf_id = 0;

                    /**
                    *   Emit a small, extraction-stage-only footprint inventory for dynamic
                    *   interiors and their preserved world boundary fragments. The record
                    *   distinguishes an incomplete brush/floor intersection from a later
                    *   partition or half-edge linking loss without logging every world face.
                    **/
                    if ( ( poly.entity_id != ENTITYNUM_NONE || poly.transition_entity_id != ENTITYNUM_NONE ) && printed_dynamic_footprints < 16 ) {
                        float footprint_area = 0.0f;
                        Vector3 footprint_mins = poly.vertices[ 0 ];
                        Vector3 footprint_maxs = poly.vertices[ 0 ];
                        for ( int32_t footprint_vertex_index = 0; footprint_vertex_index < poly.num_vertices; footprint_vertex_index++ ) {
                            const Vector3 &footprint_vertex = poly.vertices[ footprint_vertex_index ];
                            footprint_mins.x = std::min( footprint_mins.x, footprint_vertex.x );
                            footprint_mins.y = std::min( footprint_mins.y, footprint_vertex.y );
                            footprint_mins.z = std::min( footprint_mins.z, footprint_vertex.z );
                            footprint_maxs.x = std::max( footprint_maxs.x, footprint_vertex.x );
                            footprint_maxs.y = std::max( footprint_maxs.y, footprint_vertex.y );
                            footprint_maxs.z = std::max( footprint_maxs.z, footprint_vertex.z );
                            if ( footprint_vertex_index >= 2 ) {
                                const Vector3 footprint_cross = QM_Vector3CrossProduct( poly.vertices[ footprint_vertex_index - 1 ] - poly.vertices[ 0 ], footprint_vertex - poly.vertices[ 0 ] );
                                footprint_area += 0.5f * QM_Vector3Length( footprint_cross );
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

					// Submit the polygon to the global navmesh polygon container.
					g_nav_polys.push_back( poly );

                    // Track how much geometry each ownership class contributes before later partitioning and splicing.
                    s_nav_generation_diagnostics.extracted_polys++;
                    if ( poly.entity_id != ENTITYNUM_NONE ) {
                        s_nav_generation_diagnostics.extracted_dynamic_polys++;
                        s_nav_generation_diagnostics.extracted_polys_by_entity[ poly.entity_id ]++;
                        s_nav_generation_diagnostics.extracted_dynamic_entities = static_cast<int32_t>( s_nav_generation_diagnostics.extracted_polys_by_entity.size() );
                    }
				}
			}
		}

	// Print a summary of the extraction process to the server console for debugging and verification.
    gi.dprintf("Nav_DoExtractionWork: Checked %d brushes. Found %d solid/detail brushes, %d playerclip-only brushes, %d walkable sides. Extracted %d polys, pruned %d sliver fragments.\n",
        bsp->numbrushes, solid_brushes, playerclip_brushes, walkable_sides, (int)g_nav_polys.size(), sliver_pruned_fragments);
    LogNavGenerationDiagnostics( "Extraction" );
}

/**
*	@brief Calculate the surface area of an axis-aligned bounding box defined by mins and maxs.	
*	@param mins The minimum corner of the AABB.
*	@param maxs The maximum corner of the AABB.
**/
/**
* @brief Compute the surface area of an axis-aligned bounding box.
* @param mins Minimum corner.
* @param maxs Maximum corner.
* @return AABB surface area.
**/
static double SurfaceArea( const Vector3 &mins, const Vector3 &maxs ) {
    Vector3 ext = maxs - mins;
    return 2.0 * (double)( ( double )ext.x * ( double )ext.y + ( double )ext.y * ( double )ext.z + ( double )ext.x * ( double )ext.z);
}

/**
*	@brief Recursively build a KD-tree node for the navmesh faces.
*	@param firstFaceIdx	The index of the first face in g_nav_faces that belongs to this node.
*	@param faceCount The number of faces in this node.
*	@param depth The current depth in the tree, used for termination conditions.
*	@return The index of the created node in g_nav_nodes, or -1 on failure.
**/
/**
* @brief Recursively build one KD-tree node for a face span.
* @param firstFaceIdx First face index in the current span.
* @param faceCount Number of faces in the current span.
* @param depth Current tree depth.
* @return Created node index, or -1 on failure.
**/
static int32_t BuildKDNode( int32_t firstFaceIdx, int32_t faceCount, int32_t depth ) {
	if ( faceCount == 0 ) return -1;
	if ( g_nav_nodes.size() >= MAX_NAV_KDTREE_NODES ) {
		gi.dprintf( "WARNING: MAX_NAV_KDTREE_NODES reached!\n" );
		return -1;
	}

	int32_t nodeIdx = static_cast< int32_t >( g_nav_nodes.size() );
	nav_kdtree_node_t node = {};
	node.left_child = -1;
	node.right_child = -1;
	node.first_face_id = -1;      // Not a leaf yet.
	node.num_faces = 0;
	node.bsp_leaf_id = -1;        // Will be filled for leaf nodes.
	g_nav_nodes.push_back( node );

	Vector3 nodeMins( 99999, 99999, 99999 );
	Vector3 nodeMaxs( -99999, -99999, -99999 );
	for ( int32_t i = 0; i < faceCount; ++i ) {
		const nav_face_t &face = g_nav_faces[ firstFaceIdx + i ];
        for (int32_t e = 0; e < face.num_edges; ++e) {
            const Vector3& v = g_nav_vertices[g_nav_halfedges[face.first_edge_idx + e].vertex_idx];
			for ( int32_t k = 0; k < 3; ++k ) {
				if ( v[ k ] < nodeMins[ k ] ) nodeMins[ k ] = v[ k ];
				if ( v[ k ] > nodeMaxs[ k ] ) nodeMaxs[ k ] = v[ k ];
			}
		}
	}
	g_nav_nodes[ nodeIdx ].mins = nodeMins;
	g_nav_nodes[ nodeIdx ].maxs = nodeMaxs;

	const int32_t MAX_FACES_PER_LEAF = 32;
	if ( faceCount <= MAX_FACES_PER_LEAF ) {
		g_nav_nodes[ nodeIdx ].first_face_id = firstFaceIdx;
		g_nav_nodes[ nodeIdx ].num_faces = faceCount;
		g_nav_nodes[ nodeIdx ].bsp_leaf_id = g_nav_faces[ firstFaceIdx ].bsp_leaf_id;
		return nodeIdx;
	}

	const int32_t NUM_BINS = KDTREE_MAX_BIN;
	double bestCost = 9.999e9;
	int32_t    bestAxis = -1;
	double bestSplitPos = 0.0;

	Vector3 extents = nodeMaxs - nodeMins;
	double parentSA = SurfaceArea( nodeMins, nodeMaxs );
	if ( parentSA == 0.0 ) parentSA = 1.0;

	for ( int32_t axis = 0; axis < 3; ++axis ) {
		if ( extents[ axis ] < 0.1f ) continue;

		struct Bin {
			Vector3 mins{ 99999, 99999, 99999 };
			Vector3 maxs{ -99999, -99999, -99999 };
			int32_t     count = 0;
			void Expand( const nav_face_t &f ) {
                for (int32_t e = 0; e < f.num_edges; ++e) {
                    const Vector3& v = g_nav_vertices[g_nav_halfedges[f.first_edge_idx + e].vertex_idx];
					for ( int32_t k = 0; k < 3; ++k ) {
						if ( v[ k ] < mins[ k ] ) mins[ k ] = v[ k ];
						if ( v[ k ] > maxs[ k ] ) maxs[ k ] = v[ k ];
					}
				}
				++count;
			}
		};
		// Use vector for bins to avoid massive stack allocation with 1024 bins
		std::vector<Bin> bins(NUM_BINS);

		for ( int32_t i = 0; i < faceCount; ++i ) {
			const nav_face_t &face = g_nav_faces[ firstFaceIdx + i ];
			double c = face.center[ axis ];
			int32_t b = static_cast< int32_t >( NUM_BINS * ( c - nodeMins[ axis ] ) / extents[ axis ] );
			if ( b < 0 ) b = 0;
			if ( b >= NUM_BINS ) b = NUM_BINS - 1;
			bins[ b ].Expand( face );
		}

		for ( int32_t i = 0; i < NUM_BINS - 1; ++i ) {
			Vector3 leftMins( 99999, 99999, 99999 );
			Vector3 leftMaxs( -99999, -99999, -99999 );
			int32_t leftCount = 0;
			for ( int32_t j = 0; j <= i; ++j ) {
				if ( bins[ j ].count == 0 ) continue;
				for ( int32_t k = 0; k < 3; ++k ) {
					if ( bins[ j ].mins[ k ] < leftMins[ k ] ) leftMins[ k ] = bins[ j ].mins[ k ];
					if ( bins[ j ].maxs[ k ] > leftMaxs[ k ] ) leftMaxs[ k ] = bins[ j ].maxs[ k ];
				}
				leftCount += bins[ j ].count;
			}

			Vector3 rightMins( 99999, 99999, 99999 );
			Vector3 rightMaxs( -99999, -99999, -99999 );
			int32_t rightCount = 0;
			for ( int32_t j = i + 1; j < NUM_BINS; ++j ) {
				if ( bins[ j ].count == 0 ) continue;
				for ( int32_t k = 0; k < 3; ++k ) {
					if ( bins[ j ].mins[ k ] < rightMins[ k ] ) rightMins[ k ] = bins[ j ].mins[ k ];
					if ( bins[ j ].maxs[ k ] > rightMaxs[ k ] ) rightMaxs[ k ] = bins[ j ].maxs[ k ];
				}
				rightCount += bins[ j ].count;
			}

			if ( leftCount == 0 || rightCount == 0 ) continue;

			double leftSA = SurfaceArea( leftMins, leftMaxs );
			double rightSA = SurfaceArea( rightMins, rightMaxs );
			double cost = 1.0 + ( leftSA / parentSA ) * leftCount + ( rightSA / parentSA ) * rightCount;

			if ( cost < bestCost ) {
				bestCost = cost;
				bestAxis = axis;
				bestSplitPos = nodeMins[ axis ] + extents[ axis ] * ( i + 1 ) / static_cast< double >( NUM_BINS );
			}
		}
	}

	int32_t leftCount = 0;
	int32_t rightCount = 0;

	if ( bestAxis == -1 ) {
		// Fallback: Median split along longest axis if SAH failed to find any split
		bestAxis = 0;
		if ( extents.y > extents.x ) bestAxis = 1;
		if ( extents.z > extents[ bestAxis ] ) bestAxis = 2;

		std::nth_element( g_nav_faces.begin() + firstFaceIdx,
						  g_nav_faces.begin() + firstFaceIdx + faceCount / 2,
						  g_nav_faces.begin() + firstFaceIdx + faceCount,
						  [ bestAxis ] ( const nav_face_t &a, const nav_face_t &b ) {
							  return a.center[ bestAxis ] < b.center[ bestAxis ];
						  } );

		leftCount = faceCount / 2;
		rightCount = faceCount - leftCount;
	} else {
		// Normal SAH split
		auto it = std::partition( g_nav_faces.begin() + firstFaceIdx,
								g_nav_faces.begin() + firstFaceIdx + faceCount,
								[ bestAxis, bestSplitPos ] ( const nav_face_t &f ) {
									return f.center[ bestAxis ] <= bestSplitPos;
								} 
		);
		leftCount = static_cast< int32_t >( it - ( g_nav_faces.begin() + firstFaceIdx ) );
		rightCount = faceCount - leftCount;

		// Edge case: if partition put everything on one side despite SAH, force median split
		if ( leftCount == 0 || rightCount == 0 ) {
			std::nth_element( g_nav_faces.begin() + firstFaceIdx,
							  g_nav_faces.begin() + firstFaceIdx + faceCount / 2,
							  g_nav_faces.begin() + firstFaceIdx + faceCount,
							  [ bestAxis ] ( const nav_face_t &a, const nav_face_t &b ) {
								  return a.center[ bestAxis ] < b.center[ bestAxis ];
							  } );
			leftCount = faceCount / 2;
			rightCount = faceCount - leftCount;
		}
	}

	g_nav_nodes[ nodeIdx ].split_axis = bestAxis;

	int32_t left = BuildKDNode( firstFaceIdx, leftCount, depth + 1 );
	int32_t right = BuildKDNode( firstFaceIdx + leftCount, rightCount, depth + 1 );

	g_nav_nodes[ nodeIdx ].left_child = left;
	g_nav_nodes[ nodeIdx ].right_child = right;

	return nodeIdx;
}

/**
*	@brief Recursively partitions a list of polygons into a grid using spatial split planes.
**/
template <typename PolyContainer>
/**
* @brief Recursively partition polygons using axis-aligned split planes.
* @param polys Polygon container to split.
* @param mins Current region minimum bounds.
* @param maxs Current region maximum bounds.
* @param depth Current recursion depth.
**/
/**
* @brief Recursively partition polygons using axis-aligned split planes.
* @param polys Polygon container to split.
* @param mins Current region minimum bounds.
* @param maxs Current region maximum bounds.
* @param depth Current recursion depth.
**/
static void PartitionPolygonsRecursive( PolyContainer &polys, const Vector3 &mins, const Vector3 &maxs, int32_t depth ) {
    if (polys.empty()) return;

    static int32_t recurse_count = 0;
    if (depth == 0) recurse_count = 0;
    recurse_count++;
    if (recurse_count % 10000 == 0) {
        gi.dprintf("PartitionPolygonsRecursive: count=%d, depth=%d, polys=%d, extents=(%.1f, %.1f, %.1f)\n", 
            recurse_count, depth, (int)polys.size(), maxs.x - mins.x, maxs.y - mins.y, maxs.z - mins.z);
    }
    
    Vector3 extents = maxs - mins;
    
    // Pick the longest axis to split (only X or Y for 2.5D NavMesh!)
    int32_t primary_axis = (extents.y > extents.x) ? 1 : 0;
    
    // "Obstacle-Aware KD-Tree Splitting"
    // Find the polygon vertex coordinate along the split axis that is closest to the geometric center.
    // This perfectly aligns the split plane with the edges of crates/stairs.
    int32_t split_axis = -1;
    float split_dist = 0.0f;
    
    // Try the primary (longest) axis first
    std::vector<float> candidates;
    for ( const auto &poly : polys ) {
        // Dynamic door fragments already carry their authored transition boundaries and must not seed world subdivision planes.
            if ( poly.entity_id != ENTITYNUM_NONE || poly.transition_entity_id != ENTITYNUM_NONE ) {
            continue;
        }
        for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
            float v = poly.vertices[i][primary_axis];
            // Enforce a KD-Node size limit to prevent tiny sliver polygons, but allow splitting stairs!
            if (v > mins[primary_axis] + 1.0f && v < maxs[primary_axis] - 1.0f) {
                candidates.push_back(v);
            }
        }
    }
    
    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end());
        split_dist = candidates[candidates.size() / 2];
        split_axis = primary_axis;
    } else {
        // Try secondary axis
        int32_t secondary_axis = 1 - primary_axis;
        for ( const auto &poly : polys ) {
            // Keep dynamic door geometry out of the fallback candidate set for the same reason as the primary axis.
            if ( poly.entity_id != ENTITYNUM_NONE || poly.transition_entity_id != ENTITYNUM_NONE ) {
                continue;
            }
            for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
                float v = poly.vertices[i][secondary_axis];
                if (v > mins[secondary_axis] + 1.0f && v < maxs[secondary_axis] - 1.0f) {
                    candidates.push_back(v);
                }
            }
        }
        if (!candidates.empty()) {
            std::sort(candidates.begin(), candidates.end());
            split_dist = candidates[candidates.size() / 2];
            split_axis = secondary_axis;
        }
    }
    
    // If still no internal vertex, the region is perfectly empty of obstacles. 
    if (split_axis == -1) {
        // Enforce a maximum cell size to ensure a "few more extra and proper splits" 
        // to prevent gigantic polygons, but only if they are truly massive.
        if (extents.x > 256.0f || extents.y > 256.0f) {
            split_axis = primary_axis;
            split_dist = mins[split_axis] + extents[split_axis] * 0.5f;
        } else {
            return; // Cell is empty and reasonably sized, stop subdividing.
        }
    }
    
    cm_plane_t plane = {};
    plane.normal[split_axis] = 1.0f;
    plane.dist = split_dist;
    
    std::vector<nav_poly_t> front_polys;
    std::vector<nav_poly_t> back_polys;
    
    for ( const auto &poly : polys ) {
        /**
        * Split each polygon independently and preserve it when the candidate plane only creates a sliver.
        **/
        // Preserve dynamic door union interiors; their boundaries are runtime transition geometry, not subdivision hints.
            if ( poly.entity_id != ENTITYNUM_NONE || poly.transition_entity_id != ENTITYNUM_NONE ) {
            if ( poly.center[ split_axis ] >= split_dist ) {
                front_polys.push_back( poly );
            } else {
                back_polys.push_back( poly );
            }
                if ( poly.entity_id != ENTITYNUM_NONE ) {
                    s_nav_generation_diagnostics.partition_dynamic_preserved++;
                } else {
                    s_nav_generation_diagnostics.partition_transition_preserved++;
                }
            continue;
        }

        winding_t w = {};
        w.num_points = poly.num_vertices;
        for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
            w.points[ i ] = poly.vertices[ i ];
        }

        winding_t front = {};
        winding_t back = {};

        // Split the winding using the current axis-aligned plane.
        SplitWinding( &w, &plane, 0.1f, poly.normal, &front, &back );

        /**
        * Commit only complete, meaningful partitions; never discard the source polygon on a failed split.
        **/
        if ( IsUsablePartitionFragment( front ) && IsUsablePartitionFragment( back ) ) {
                s_nav_generation_diagnostics.partition_accepted_splits++;
            nav_poly_t front_poly = poly;
            front_poly.num_vertices = front.num_points;
            for ( int32_t i = 0; i < front.num_points; i++ ) {
                front_poly.vertices[ i ] = front.points[ i ];
            }
            RecomputeNavPolygonCenter( front_poly );
            front_polys.push_back( front_poly );

            nav_poly_t back_poly = poly;
            back_poly.num_vertices = back.num_points;
            for ( int32_t i = 0; i < back.num_points; i++ ) {
                back_poly.vertices[ i ] = back.points[ i ];
            }
            RecomputeNavPolygonCenter( back_poly );
            back_polys.push_back( back_poly );
        } else {
                s_nav_generation_diagnostics.partition_rejected_splits++;
            // Keep failed or boundary-only splits in the child region containing the polygon center.
            if ( poly.center[ split_axis ] >= split_dist ) {
                front_polys.push_back( poly );
            } else {
                back_polys.push_back( poly );
            }
        }
    }
    
    // Update bounding boxes for children
    Vector3 front_mins = mins;
    Vector3 front_maxs = maxs;
    front_mins[split_axis] = split_dist;
    
    Vector3 back_mins = mins;
    Vector3 back_maxs = maxs;
    back_maxs[split_axis] = split_dist;
    
    PartitionPolygonsRecursive(front_polys, front_mins, front_maxs, depth + 1);
    PartitionPolygonsRecursive(back_polys, back_mins, back_maxs, depth + 1);
    
    // Reconstruct the polys array from the leaves
    polys.clear();
    for (const auto& p : front_polys) polys.push_back(p);
    for (const auto& p : back_polys) polys.push_back(p);
}

/**
*	@brief	KD-Tree driven spatial partitioning of the NavMesh polygons.
*			Creates detailed sub-polygon areas to allow more organic AI movement paths,
*			while ensuring zero T-Junctions by splitting adjacent polys with infinite planes.
**/
/**
* @brief Spatially partition the extracted nav polygons.
* @note This prepares the data for half-edge construction and later twin linking.
**/
static void Nav_PartitionPolygons() {
    if (g_nav_polys.empty()) return;

    // Record the number of polygons entering spatial partitioning so later stages can explain any fan-out.
    s_nav_generation_diagnostics.partition_input_polys = static_cast<int32_t>( g_nav_polys.size() );
    
    Vector3 mins(99999, 99999, 99999);
    Vector3 maxs(-99999, -99999, -99999);
    
    for (const auto& p : g_nav_polys) {
        for (int32_t i = 0; i < p.num_vertices; i++) {
            mins.x = std::min(mins.x, p.vertices[i].x);
            mins.y = std::min(mins.y, p.vertices[i].y);
            mins.z = std::min(mins.z, p.vertices[i].z);
            maxs.x = std::max(maxs.x, p.vertices[i].x);
            maxs.y = std::max(maxs.y, p.vertices[i].y);
            maxs.z = std::max(maxs.z, p.vertices[i].z);
        }
    }
    
    int32_t before_count = (int)g_nav_polys.size();
    PartitionPolygonsRecursive(g_nav_polys, mins, maxs, 0);
    gi.dprintf("NavMesh: Spatial Partitioning expanded %d polys into %d grid cells.\n", before_count, (int)g_nav_polys.size());
    LogNavGenerationDiagnostics( "Partition" );
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
static bool InsertDynamicPortalConformanceVertex( nav_poly_t &target, const int32_t edge_index, const Vector3 &source_vertex ) {
    /**
    *	Ensure that there is capacity for one additional collinear perimeter vertex.
    **/
    if ( target.num_vertices < 3 || target.num_vertices >= MAX_WINDING_POINTS || edge_index < 0 || edge_index >= target.num_vertices ) {
        return false;
    }

    const Vector3 edge_start = target.vertices[ edge_index ];
    const Vector3 edge_end = target.vertices[ ( edge_index + 1 ) % target.num_vertices ];
    Vector3 edge_direction_2d = edge_end - edge_start;
    edge_direction_2d.z = 0.0f;
    const float edge_length_sqr_2d = QM_Vector3LengthSqr( edge_direction_2d );
    if ( edge_length_sqr_2d <= 0.001f ) {
        return false;
    }

    /**
    *	Require a strict interior projection on the target segment. Endpoints are already
    *	conformal and must not be duplicated into a zero-length half-edge.
    **/
    Vector3 start_to_source_2d = source_vertex - edge_start;
    start_to_source_2d.z = 0.0f;
    const float edge_fraction = QM_Vector3DotProduct( start_to_source_2d, edge_direction_2d ) / edge_length_sqr_2d;
    if ( edge_fraction <= 0.001f || edge_fraction >= 0.999f ) {
        return false;
    }

    const Vector3 projected_vertex = QM_Vector3MultiplyAdd( edge_start, edge_fraction, edge_end - edge_start );
    Vector3 projected_vertex_2d = projected_vertex;
    projected_vertex_2d.z = 0.0f;
    Vector3 source_vertex_2d = source_vertex;
    source_vertex_2d.z = 0.0f;
    // A portal conformance vertex must already lie on the same projected boundary line.
    if ( QM_Vector3DistanceSqr( source_vertex_2d, projected_vertex_2d ) > ( 0.25f * 0.25f ) ) {
        return false;
    }

    // Door footprints and their adjoining world floors must share one walkable elevation.
    if ( std::fabs( source_vertex.z - projected_vertex.z ) > 0.25f ) {
        return false;
    }

    /**
    *	Insert only the target-plane projection. The source polygon remains untouched,
    *	which prevents portal conformance from producing the triangular geometry observed
    *	when cross-domain T-junction repair moved vertices between polygons.
    **/
    nav_poly_t candidate = target;
    for ( int32_t vertex_index = candidate.num_vertices; vertex_index > edge_index + 1; vertex_index-- ) {
        candidate.vertices[ vertex_index ] = candidate.vertices[ vertex_index - 1 ];
    }
    candidate.vertices[ edge_index + 1 ] = projected_vertex;
    candidate.num_vertices++;
    if ( !IsValidNavPolygon( candidate, candidate.normal ) ) {
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
                    const Vector3 source_vertex = g_nav_polys[ source_index ].vertices[ source_vertex_index ];
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
*	@brief	Resolves micro-gaps and T-Junctions by splicing missing vertices directly into the edges of adjacent polygons.
*			This perfectly guarantees the 1-to-1 Twin Half-Edge constraint without destroying the faces.
**/
/**
*	@brief	Resolve T-junctions by splicing missing vertices into adjacent polygons.
*	@note	This keeps the later half-edge mesh conformal.
**/
static void Nav_ResolveTJunctionsByEdgeSplicing() {
    if (g_nav_polys.empty()) return;

    struct PolyAABB {
        float min_x, min_y, min_z, max_x, max_y, max_z;
    };

    // Strict coplanar snapping tolerance used when both polygons share one geometric plane.
    static constexpr float MAX_PLANAR_POINT_DISTANCE = 0.25f;
    // Maximum vertical separation eligible for projected step/slope edge subdivision.
    static constexpr float MAX_PROJECTED_STEP_SPLICE_HEIGHT = NAV_MAX_STEP_SIZE + 4.0f;
    // Include walkable step-height neighbors in the spatial index so their 2D seam vertices can conform.
    static constexpr float SPATIAL_INDEX_Z_PADDING = MAX_PROJECTED_STEP_SPLICE_HEIGHT;
    std::vector<PolyAABB> aabbs(g_nav_polys.size());
    auto UpdateAABB = [](const nav_poly_t& p, PolyAABB& aabb) {
        float min_x = 99999, min_y = 99999, min_z = 99999, max_x = -99999, max_y = -99999, max_z = -99999;
        for (int32_t k = 0; k < p.num_vertices; k++) {
            min_x = std::min(min_x, p.vertices[k].x);
            min_y = std::min(min_y, p.vertices[k].y);
            min_z = std::min(min_z, p.vertices[k].z);
            max_x = std::max(max_x, p.vertices[k].x);
            max_y = std::max(max_y, p.vertices[k].y);
            max_z = std::max(max_z, p.vertices[k].z);
        }
        aabb = {min_x - 4.0f, min_y - 4.0f, min_z - SPATIAL_INDEX_Z_PADDING, max_x + 4.0f, max_y + 4.0f, max_z + SPATIAL_INDEX_Z_PADDING };
    };

    struct GridCell {
        std::vector<int32_t> polys;
    };
    std::unordered_map<int64_t, GridCell> grid;
    static constexpr float cell_size = 256.0f;

    for (size_t i = 0; i < g_nav_polys.size(); i++) {
        UpdateAABB(g_nav_polys[i], aabbs[i]);
        int32_t min_cx = (int)std::floor(aabbs[i].min_x / cell_size);
        int32_t max_cx = (int)std::floor(aabbs[i].max_x / cell_size);
        int32_t min_cy = (int)std::floor(aabbs[i].min_y / cell_size);
        int32_t max_cy = (int)std::floor(aabbs[i].max_y / cell_size);
        int32_t min_cz = (int)std::floor(aabbs[i].min_z / cell_size);
        int32_t max_cz = (int)std::floor(aabbs[i].max_z / cell_size);
        for (int32_t cx = min_cx; cx <= max_cx; cx++) {
            for (int32_t cy = min_cy; cy <= max_cy; cy++) {
                for (int32_t cz = min_cz; cz <= max_cz; cz++) {
                    int64_t key = (cx * 73856093) ^ (cy * 19349663) ^ (cz * 83492791);
                    grid[key].polys.push_back((int32_t)i);
                }
            }
        }
    }

    int32_t totalSplices = 0;

    for (size_t i = 0; i < g_nav_polys.size(); i++) {
        // Dynamic union interiors are authored portal boundaries and must not be reshaped by world T-junction repair.
        if ( g_nav_polys[i].entity_id != ENTITYNUM_NONE || g_nav_polys[i].transition_entity_id != ENTITYNUM_NONE ) {
            continue;
        }
        bool poly_modified = false;
        
        for (int32_t e = 0; e < g_nav_polys[i].num_vertices && !poly_modified; e++) {
            Vector3 pA = g_nav_polys[i].vertices[e];
            Vector3 pB = g_nav_polys[i].vertices[(e + 1) % g_nav_polys[i].num_vertices];
            
            Vector3 edgeVec = pB - pA;
            float edgeLenSqr = QM_Vector3LengthSqr(edgeVec);
            
            if (edgeLenSqr < 1.0f) continue;
            
            Vector3 pA_2d = { pA.x, pA.y, 0.0f };
            Vector3 pB_2d = { pB.x, pB.y, 0.0f };
            Vector3 edgeVec_2d = pB_2d - pA_2d;
            float edgeLenSqr_2d = QM_Vector3LengthSqr(edgeVec_2d);
            
            if (edgeLenSqr_2d < 0.001f) continue;
            
            int32_t min_cx = (int)std::floor(aabbs[i].min_x / cell_size);
            int32_t max_cx = (int)std::floor(aabbs[i].max_x / cell_size);
            int32_t min_cy = (int)std::floor(aabbs[i].min_y / cell_size);
            int32_t max_cy = (int)std::floor(aabbs[i].max_y / cell_size);
            int32_t min_cz = (int)std::floor(aabbs[i].min_z / cell_size);
            int32_t max_cz = (int)std::floor(aabbs[i].max_z / cell_size);
            
            std::vector<int32_t> checked_j; // Simple deduplication for cell overlap
            
            for (int32_t cx = min_cx; cx <= max_cx && !poly_modified; cx++) {
                for (int32_t cy = min_cy; cy <= max_cy && !poly_modified; cy++) {
                    for (int32_t cz = min_cz; cz <= max_cz && !poly_modified; cz++) {
                        int64_t key = (cx * 73856093) ^ (cy * 19349663) ^ (cz * 83492791);
                        auto it = grid.find(key);
                        if (it == grid.end()) continue;

                        for (int32_t j : it->second.polys) {
                            if (poly_modified) break;
                            if (i == j) continue;
                    // Never splice a dynamic union interior into a world polygon edge.
                    if ( g_nav_polys[ j ].entity_id != ENTITYNUM_NONE || g_nav_polys[ j ].transition_entity_id != ENTITYNUM_NONE ) continue;
                            
                            bool already_checked = false;
                            for (int32_t c : checked_j) {
                                if (c == j) { already_checked = true; break; }
                            }
                            if (already_checked) continue;
                            checked_j.push_back(j);
                            
                            if (aabbs[j].max_x < aabbs[i].min_x || aabbs[j].min_x > aabbs[i].max_x ||
                                aabbs[j].max_y < aabbs[i].min_y || aabbs[j].min_y > aabbs[i].max_y ||
                                aabbs[j].max_z < aabbs[i].min_z || aabbs[j].min_z > aabbs[i].max_z) {
                                continue;
                            }

                            // Do not splice across dynamic boundaries or between unrelated face ownership domains.
                            if ( g_nav_polys[ i ].entity_id != g_nav_polys[ j ].entity_id ) {
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
                                const float normal_dot = QM_Vector3DotProduct( g_nav_polys[ i ].normal, g_nav_polys[ j ].normal );
                                if ( normal_dot < 0.0f ) {
                                    continue;
                                }
                            }

                            for (int32_t vj = 0; vj < g_nav_polys[j].num_vertices && !poly_modified; vj++) {
                                Vector3 vC = g_nav_polys[j].vertices[vj];
                                Vector3 vC_2d = { vC.x, vC.y, 0.0f };
                                
                                Vector3 toC_2d = vC_2d - pA_2d;
                                float t = QM_Vector3DotProduct(toC_2d, edgeVec_2d) / edgeLenSqr_2d;
                                
                                if (t > 0.0f && t < 1.0f) {
                                    Vector3 projC_2d = QM_Vector3MultiplyAdd( pA_2d, t, edgeVec_2d );
                                    
                                    if (QM_Vector3DistanceSqr(vC_2d, projC_2d) < 16.0f) { 
                                        Vector3 projC_3d = QM_Vector3MultiplyAdd( pA, t, edgeVec );
                                        float dz = std::abs(vC.z - projC_3d.z);
                                        
                                        // Non-coplanar walkable seams may differ by one step height, but must retain their individual surface elevations.
                                        const float max_point_distance = surfaces_are_coplanar ? MAX_PLANAR_POINT_DISTANCE : MAX_PROJECTED_STEP_SPLICE_HEIGHT;
                                        if ( dz <= max_point_distance ) {
                                            if ( surfaces_are_coplanar && QM_Vector3DistanceSqr(pA_2d, projC_2d) <= 1.0f) {
                                                // Snap vC to corner pA
                                                nav_poly_t candidate = g_nav_polys[ j ];
                                                candidate.vertices[ vj ] = pA;
                                                if ( !IsValidNavPolygon( candidate, candidate.normal ) ) {
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
                                            } else if ( surfaces_are_coplanar && QM_Vector3DistanceSqr(pB_2d, projC_2d) <= 1.0f) {
                                                // Snap vC to corner pB
                                                nav_poly_t candidate = g_nav_polys[ j ];
                                                candidate.vertices[ vj ] = pB;
                                                if ( !IsValidNavPolygon( candidate, candidate.normal ) ) {
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
                                                nav_poly_t newPoly = g_nav_polys[i];
                                                if (newPoly.num_vertices < MAX_WINDING_POINTS) {
                                                    for (int32_t k = newPoly.num_vertices; k > e + 1; k--) {
                                                        newPoly.vertices[k] = newPoly.vertices[k - 1];
                                                    }
                                                    newPoly.vertices[e + 1] = projC_3d;
                                                    newPoly.num_vertices++;

                                                    if ( !IsValidNavPolygon( newPoly, newPoly.normal ) ) {
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
                                                    candidate.vertices[ vj ] = projC_3d;
                                                    if ( !IsValidNavPolygon( candidate, candidate.normal ) ) {
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
            gi.dprintf("NavMesh WARNING: Aborting Edge Splicing due to infinite loop! (Splices > 10000)\n");
            break;
        }
        
        if ( poly_modified && i > 0 ) {
            // Re-process the preceding polygon so newly inserted vertices can participate in another splice.
            i--;
        }
    }
    
    if (totalSplices > 0) {
        gi.dprintf("NavMesh: Spliced %d T-Junction vertices.\n", totalSplices);
    }
    LogNavGenerationDiagnostics( "Splice" );
}

#if 0
/**
*	@brief Triangulates all polygons in the navmesh by connecting their center (meridian mid-point) to their outer vertices.
*	@note This guarantees all faces are strictly planar triangles, eliminating precision issues and non-convex splicing artifacts.
**/
static void Nav_TriangulatePolygons() {
    std::vector<nav_poly_t> new_polys;
    new_polys.reserve(g_nav_polys.size() * 4); // Roughly 4 triangles per quad

    for (const auto& poly : g_nav_polys) {
        if (poly.num_vertices < 3) continue;

        // The "meridian mid-point" is the calculated center
        Vector3 meridian = poly.center;

        for (int32_t i = 0; i < poly.num_vertices; i++) {
            Vector3 v1 = poly.vertices[i];
            Vector3 v2 = poly.vertices[(i + 1) % poly.num_vertices];

            nav_poly_t tri = {};
            tri.poly_id = new_polys.size();
            tri.num_vertices = 3;
            tri.vertices[0] = v1;
            tri.vertices[1] = v2;
            tri.vertices[2] = meridian;
            
            // The center of this triangle
            tri.center = (v1 + v2 + meridian) / 3.0f;
            tri.normal = poly.normal;
            tri.bsp_leaf_id = poly.bsp_leaf_id;

            new_polys.push_back(tri);
        }
    }

    // Transfer triangulated polygons into the engine's nav_vector container.
    g_nav_polys.clear();
    for (const auto &tri : new_polys) {
        g_nav_polys.push_back(tri);
    }
    gi.dprintf("NavMesh: Triangulated into %zu faces.\n", g_nav_polys.size());
}
#endif

/**
*	@brief	Builds the half-edge mesh from the extracted and partitioned polygons.
**/
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
	// Cancel early if there are no polygons to process.
	if ( g_nav_polys.empty() ) {
		return;
	}

	/**
	*	Partition the massive extracted polys into a structured sub-polygon grid.
	**/
	Nav_PartitionPolygons();

    // Conform dynamic/world doorway perimeter segments before generic world T-junction repair.
    Nav_ResolveDynamicPortalTJunctions();

	/**
	*	No prune pass needed here.
	*	Since we enabled full sloped CSG subtraction in Nav_DoExtractionWork,
	*	overlapping brushes are perfectly clipped and there are no buried phantom polygons.
	**/

	// Resolve any remaining T-junctions by splicing (mostly non-grid aligned or dynamic cuts).
	Nav_ResolveTJunctionsByEdgeSplicing();

	/**
	*	Triangulation is disabled because the KD-tree sub-polygons are naturally planar N-gons.
	*	Triangulating them creates unnecessary extra geometry and internal spokes that confuse pathing.
	*	Nav_TriangulatePolygons();
	**/

	// This is a spatial hash grid to deduplicate vertices and prevent T-junctions.
	std::unordered_map<int64_t, std::vector<int32_t>> vertex_grid;
	int32_t first_pass_twin_links = 0;
	int32_t second_pass_twin_links = 0;
    int32_t deterministic_twin_links = 0;

	/**
	*	@brief	Hash a vertex position into the deduplication grid.
	*	@details	We enforce exact 3D matching (with a tiny floating point epsilon) to prevent
	*			"canonical vertex drift" where an entire staircase merges into a single vertex.
	**/
	auto GetVertexIndex = [&]( const Vector3 &p ) -> int32_t {
		// Use a 4-unit grid to hash vertices, allowing for very close proximity without merging.
		static constexpr float GRID_SIZE = 4.0f;
		int64_t cx = (int64_t)std::floor( p.x / GRID_SIZE );
		int64_t cy = (int64_t)std::floor( p.y / GRID_SIZE );
		int64_t cz = (int64_t)std::floor( p.z / GRID_SIZE );

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
							float dx = g_nav_vertices[ idx ].x - p.x;
							float dy = g_nav_vertices[ idx ].y - p.y;
							float dz = g_nav_vertices[ idx ].z - p.z;
							// If the squared distance is less than 0.01, consider it a match and return the index.
							if ( dx * dx + dy * dy + dz * dz < 0.01f ) {
								return idx;
							}
						}
					}
				}
			}
		}

		// If no existing vertex is found, add p as a new vertex and return its index.
		int32_t new_idx = (int32_t)g_nav_vertices.size();
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
	*	Now we build the half-edge mesh from the partitioned polygons.
	*	To do this we:
	*		- 1:	Deduplicate vertices and store them in g_nav_vertices
	*		- 2:	Create half-edges for each polygon edge, linking them to their vertices and faces
	*		- 3:	Compute the clearance for each face (distance from center to nearest edge)
	*		- 4:	Store the half-edges and faces in g_nav_halfedges and g_nav_faces
	*		- 5:	After all half-edges are created, we perform twin linking to connect adjacent edges across polygons
	*				This ensures a fully conformal half-edge graph for pathfinding.
	**/
    for (size_t i = 0; i < g_nav_polys.size(); i++) {
		// For each polygon, we create a nav_face_t and its associated half-edges.
        const nav_poly_t& poly = g_nav_polys[i];
        
		/**
		*	Create a new face for this polygon
		**/
		// Initialize a new face structure
        nav_face_t face = {};
		// Assign the face ID to the current index
        face.face_id = i;
        face.num_edges = poly.num_vertices;
        face.center = poly.center;
        face.normal = poly.normal;
        face.entity_id = poly.entity_id;
        face.transition_entity_id = poly.transition_entity_id;
        face.bsp_leaf_id = poly.bsp_leaf_id;
		// Store the index of the first half-edge for this face
        face.first_edge_idx = g_nav_halfedges.size();

		/**
		*	Check for deduplicated vertices and store their indices for the half-edges.
		**/
		// Deduplicated vertice storage for the polygon's vertices
        std::vector<int32_t> v_indices(poly.num_vertices);
		// For each vertex in the polygon, get its index in the global vertex list
        for (int32_t v = 0; v < poly.num_vertices; v++) {
			// Deduplicate the vertex and get its index
            v_indices[v] = GetVertexIndex(poly.vertices[v]);
        }

        /**
		*	Now we compute the clearance for this face, which is the distance from the center to the nearest edge.
		**/
        float min_dist = 99999.0f;
		// For each edge of the polygon, compute the distance from the face center to the edge
        for (int32_t v = 0; v < poly.num_vertices; v++) {
			// Get the two vertices that define the edge.
            Vector3 a = poly.vertices[v];
            Vector3 b = poly.vertices[(v + 1) % poly.num_vertices];
            
            // distance from center to line segment a-b
            Vector3 edge = b - a;
            Vector3 toCenter = face.center - a;
			// Compute the squared length of the edge to avoid unnecessary square root calculations.
            float edgeLenSq = QM_Vector3LengthSqr(edge);
			// Default distance is 0.0f, will be updated if edge length is significant.
            float dist = 0.0f;
			// If the edge length is significant, compute the projection of the center onto the edge.
            if (edgeLenSq > 0.0001f) {
				// Compute the projection factor t of the center onto the edge, clamped between 0 and 1.
                float t = QM_Vector3DotProduct(toCenter, edge) / edgeLenSq;
				// Clamp t to the range [0, 1] to ensure the projection lies on the edge segment.
                t = std::max(0.0f, std::min(1.0f, t));
				// Compute the projected point on the edge using the clamped t value.
                Vector3 proj = QM_Vector3MultiplyAdd( a, t, edge );
				// Compute the distance from the face center to the projected point on the edge.
                dist = QM_Vector3Distance(face.center, proj);
			// If the edge length is too small, fall back to the distance from the center to one of the edge vertices.
            } else {
				// Edge is too small, use distance to vertex a
                dist = QM_Vector3Distance(face.center, a);
            }
			// Update the minimum distance if the current distance is smaller.
			if ( dist < min_dist ) {
				min_dist = dist;
			}
        }
		// Last but not least, store the computed clearance in the face structure.
        face.clearance = min_dist;

		/**
		*	For each edge of the polygon, create a half-edge and link it to the face and its vertices.
		**/
        for (int32_t v = 0; v < poly.num_vertices; v++) {
			// Get the current and next vertex indices for the half-edge
            int32_t curr_v = v_indices[v];

			// Create a new half-edge structure
            nav_halfedge_t he = {};
            he.vertex_idx = curr_v;
            he.face_idx = face.face_id;
            he.twin_idx = -1; // Default to boundary
            // Face ownership is diagnostic metadata; edge ownership is assigned only after twin adjacency is known.
            he.edge_entity_id = ENTITYNUM_NONE;
			he.wall_offset = 16.0f; // Metadata for runtime inspection: boundary edges were expanded by 16.0f during CSG.

			// The next half-edge index is the next edge in the polygon, wrapping around to the first edge.
            he.next_idx = face.first_edge_idx + ((v + 1) % poly.num_vertices);
            
			// Add the half-edge to the global half-edge list
                g_nav_halfedges.push_back(he);
        }

        g_nav_faces.push_back(face);
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
            float overlap_len = 0.0f;
            float z_delta = 0.0f;
        };
        std::vector<nav_deterministic_pair_t> deterministic_pairs = {};
        deterministic_pairs.reserve( 4096 );

        for ( int32_t edge_a = 0; edge_a < ( int32_t )g_nav_halfedges.size(); edge_a++ ) {
            if ( g_nav_halfedges[ edge_a ].twin_idx != -1 ) {
                continue;
            }

            const nav_halfedge_t &half_edge_a = g_nav_halfedges[ edge_a ];
            const nav_poly_t &poly_a = g_nav_polys[ half_edge_a.face_idx ];
            // Deterministic arbitration permits partial overlaps, which is safe only for ordinary world seams.
            // Dynamic portals must use the stricter endpoint/overlap passes below so a door cannot claim wall fragments.
            if ( poly_a.entity_id != ENTITYNUM_NONE || poly_a.transition_entity_id != ENTITYNUM_NONE || poly_a.normal.z < NAV_MIN_WALKABLE_Z ) {
                continue;
            }

            const Vector3 a1 = g_nav_vertices[ half_edge_a.vertex_idx ];
            const Vector3 a2 = g_nav_vertices[ g_nav_halfedges[ half_edge_a.next_idx ].vertex_idx ];
            Vector3 direction_a = a2 - a1;
            direction_a.z = 0.0f;
            const float length_a = QM_Vector3Length( direction_a );
            if ( length_a < 0.1f ) {
                continue;
            }
            direction_a = direction_a * ( 1.0f / length_a );

            for ( int32_t edge_b = edge_a + 1; edge_b < ( int32_t )g_nav_halfedges.size(); edge_b++ ) {
                if ( g_nav_halfedges[ edge_b ].twin_idx != -1 ) {
                    continue;
                }

                const nav_halfedge_t &half_edge_b = g_nav_halfedges[ edge_b ];
                if ( half_edge_a.face_idx == half_edge_b.face_idx ) {
                    continue;
                }

                const nav_poly_t &poly_b = g_nav_polys[ half_edge_b.face_idx ];
                if ( poly_b.entity_id != ENTITYNUM_NONE || poly_b.transition_entity_id != ENTITYNUM_NONE || poly_b.normal.z < NAV_MIN_WALKABLE_Z ) {
                    continue;
                }

                // Apply the same ownership-domain policy used by every remaining twin-link pass.
                if ( !AreHalfEdgesInCompatibleNavigationDomains( edge_a, edge_b ) ) {
                    continue;
                }

                const Vector3 b1 = g_nav_vertices[ half_edge_b.vertex_idx ];
                const Vector3 b2 = g_nav_vertices[ g_nav_halfedges[ half_edge_b.next_idx ].vertex_idx ];
                Vector3 direction_b = b2 - b1;
                direction_b.z = 0.0f;
                const float length_b = QM_Vector3Length( direction_b );
                if ( length_b < 0.1f ) {
                    continue;
                }
                direction_b = direction_b * ( 1.0f / length_b );
                // Accept near-collinear candidates regardless of winding order; overlap and
                // lateral tests below determine whether this is a valid shared seam.
                if ( std::fabs( QM_Vector3DotProduct( direction_a, direction_b ) ) < 0.95f ) {
                    continue;
                }

                static constexpr float MAX_DETERMINISTIC_LATERAL_SEPARATION = 4.0f;
                const float lateral_b1 = std::fabs( direction_a.x * ( b1.y - a1.y ) - direction_a.y * ( b1.x - a1.x ) );
                const float lateral_b2 = std::fabs( direction_a.x * ( b2.y - a1.y ) - direction_a.y * ( b2.x - a1.x ) );
                if ( lateral_b1 > MAX_DETERMINISTIC_LATERAL_SEPARATION || lateral_b2 > MAX_DETERMINISTIC_LATERAL_SEPARATION ) {
                    continue;
                }

                Vector3 a1_to_b1 = b1 - a1;
                a1_to_b1.z = 0.0f;
                const float projection_b1 = QM_Vector3DotProduct( a1_to_b1, direction_a );
                Vector3 closest_point = QM_Vector3MultiplyAdd( a1, projection_b1, direction_a );
                closest_point.z = 0.0f;
                Vector3 b1_2d = b1;
                b1_2d.z = 0.0f;
                if ( QM_Vector3DistanceSqr( b1_2d, closest_point ) > 64.0f ) {
                    continue;
                }

                Vector3 a1_to_b2 = b2 - a1;
                a1_to_b2.z = 0.0f;
                const float projection_b2 = QM_Vector3DotProduct( a1_to_b2, direction_a );
                const float min_projection = std::min( projection_b1, projection_b2 );
                const float max_projection = std::max( projection_b1, projection_b2 );
                const float overlap_start = std::max( 0.0f, min_projection );
                const float overlap_end = std::min( length_a, max_projection );
                const float overlap_length = overlap_end - overlap_start;
                if ( overlap_length < 2.0f ) {
                    continue;
                }

                static constexpr float MAX_Z_DIFF = NAV_MAX_STEP_SIZE + 4.0f;
                const float z_delta = std::abs( ( a1.z + a2.z ) * 0.5f - ( b1.z + b2.z ) * 0.5f );
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
            g_nav_halfedges[ pair.edge_a ].wall_offset = 0.0f;
            g_nav_halfedges[ pair.edge_b ].wall_offset = 0.0f;

            const float z_a = ( g_nav_vertices[ g_nav_halfedges[ pair.edge_a ].vertex_idx ].z + g_nav_vertices[ g_nav_halfedges[ g_nav_halfedges[ pair.edge_a ].next_idx ].vertex_idx ].z ) * 0.5f;
            const float z_b = ( g_nav_vertices[ g_nav_halfedges[ pair.edge_b ].vertex_idx ].z + g_nav_vertices[ g_nav_halfedges[ g_nav_halfedges[ pair.edge_b ].next_idx ].vertex_idx ].z ) * 0.5f;
            g_nav_halfedges[ pair.edge_a ].z_diff = z_b - z_a;
            g_nav_halfedges[ pair.edge_b ].z_diff = z_a - z_b;

            AssignNavEntityEdgeMetadata( pair.edge_a, pair.edge_b );
            deterministic_twin_links++;
        }
    };

    // Run deterministic arbitration first so greedy local matching cannot consume globally better seam pairs.
    RunDeterministicArbitration();
    
    /**
	*	Twin Linking using Z-tolerant 2D overlap check
    *	This connects stair steps that are physically separated by up to 18 units vertically,
    *	ensuring the half-edge graph remains fully conformal across varying height terrain.
	**/
    // The twin linking is done in two passes. The first pass links edges that are very close in 2D and have a small Z difference.
    // The second pass links edges that overlap in 2D but have a strict Z-tolerance to prevent linking catwalks to floors below them.
    std::unordered_map<int64_t, std::vector<int32_t>> twin_grid;
    // Build a spatial hash grid of half-edges using both edge endpoints so winding-order
    // differences (same-order vs reversed-order seams) still land in nearby lookup buckets.
	for (size_t j = 0; j < g_nav_halfedges.size(); j++) {
        // Get both half-edge endpoints.
        Vector3 b1 = g_nav_vertices[g_nav_halfedges[j].vertex_idx];
		Vector3 b2 = g_nav_vertices[g_nav_halfedges[g_nav_halfedges[j].next_idx].vertex_idx];
        // Compute grid cell coordinates for both endpoint positions, using a 16-unit grid size.
		static constexpr float GRID_SIZE = 16.0f;
        int64_t cx1 = (int64_t)std::floor(b1.x / GRID_SIZE );
        int64_t cy1 = (int64_t)std::floor(b1.y / GRID_SIZE );
        int64_t cx = (int64_t)std::floor(b2.x / GRID_SIZE );
        int64_t cy = (int64_t)std::floor(b2.y / GRID_SIZE );
        int64_t key1 = (cx1 * 73856093) ^ (cy1 * 19349663);
        int64_t key = (cx * 73856093) ^ (cy * 19349663);
        // Store the half-edge index in both endpoint cells for later twin linking.
        twin_grid[key1].push_back((int32_t)j);
        twin_grid[key].push_back((int32_t)j);
    }
    /**
	*	Now we iterate through all half-edges and attempt to find their twins by checking neighboring grid cells for potential matches.
	*	A half-edge is considered a twin if it is very close in 2D and has a small Z difference, allowing for slight vertical offsets.
	**/
    for (uint64_t i = 0; i < g_nav_halfedges.size(); i++) {
		// Skip half-edges that already have a twin assigned.
		if ( g_nav_halfedges[ i ].twin_idx != -1 ) {
			continue;
		}

		// Get the current half-edge and its vertices.
        nav_halfedge_t& heA = g_nav_halfedges[i];
        Vector3 a1 = g_nav_vertices[heA.vertex_idx];
        Vector3 a2 = g_nav_vertices[g_nav_halfedges[heA.next_idx].vertex_idx];
        // Initialize variables to track the best twin candidate using endpoint fit, then Z tie-break.
        float bestEndpointError = 99999.0f;
        float bestZ = 99999.0f;
        int64_t bestTwin = -1;

		// Compute the grid cell coordinates for the first vertex of the half-edge, using a 16-unit grid size.
		static constexpr float GRID_SIZE = 16.0f;
		int64_t cx = ( int64_t )std::floor( a1.x / GRID_SIZE );
        int64_t cy = (int64_t)std::floor(a1.y / GRID_SIZE );
        
		// Check neighboring grid cells in a 3x3 area to find potential twin half-edges.
        for (int64_t ox = -1; ox <= 1; ox++) {
            for (int64_t oy = -1; oy <= 1; oy++) {
				// Compute the hash key for the neighboring grid cell to look for potential twin half-edges.
                int64_t key = ((cx + ox) * 73856093) ^ ((cy + oy) * 19349663);
                auto it = twin_grid.find(key);
                if (it == twin_grid.end()) continue;
                
                for (int32_t j : it->second) {
					if ( i == (size_t)j ) {
						continue; // Don't twin with self!
					}
					if ( g_nav_halfedges[ j ].twin_idx != -1 ) {
						continue;
					}

					// Get the candidate half-edge and its vertices for comparison.
                    nav_halfedge_t& heB = g_nav_halfedges[j];
                    if ( heA.face_idx == heB.face_idx ) {
                        continue;
                    }
                    // Reject cross-mover pairs before geometry scoring can claim this edge.
                    if ( !AreHalfEdgesInCompatibleNavigationDomains( static_cast<int32_t>( i ), j ) ) {
                        continue;
                    }
					// Get the vertices of the candidate half-edge.
                    Vector3 b1 = g_nav_vertices[heB.vertex_idx];
                    Vector3 b2 = g_nav_vertices[g_nav_halfedges[heB.next_idx].vertex_idx];

                    /**
                    * Endpoint proximity alone is not sufficient for a portal.  Adjacent
                    * stair risers can have reversed endpoints within the T-junction
                    * tolerance while being laterally separated from the actual shared
                    * edge.  Require opposing, collinear spans before accepting a twin.
                    **/
                    Vector3 edgeA2D = a2 - a1;
                    edgeA2D.z = 0.0f;
                    Vector3 edgeB2D = b2 - b1;
                    edgeB2D.z = 0.0f;
                    const float edgeALen = QM_Vector3Length( edgeA2D );
                    const float edgeBLen = QM_Vector3Length( edgeB2D );
                    if ( edgeALen <= 0.1f || edgeBLen <= 0.1f ) {
                        continue;
                    }
                    edgeA2D = edgeA2D * ( 1.0f / edgeALen );
                    edgeB2D = edgeB2D * ( 1.0f / edgeBLen );
                    // Accept near-collinear candidates regardless of winding order; endpoint checks below
                    // determine whether reversed or same-order pairing is geometrically valid.
                    if ( std::fabs( QM_Vector3DotProduct( edgeA2D, edgeB2D ) ) < 0.95f ) {
                        continue;
                    }

                    const float lateralB1 = std::fabs( edgeA2D.x * ( b1.y - a1.y ) - edgeA2D.y * ( b1.x - a1.x ) );
                    const float lateralB2 = std::fabs( edgeA2D.x * ( b2.y - a1.y ) - edgeA2D.y * ( b2.x - a1.x ) );
                    static constexpr float MAX_TWIN_LATERAL_SEPARATION = 4.0f;
                    if ( lateralB1 > MAX_TWIN_LATERAL_SEPARATION || lateralB2 > MAX_TWIN_LATERAL_SEPARATION ) {
                        continue;
                    }

                    // Evaluate both endpoint pairings so same-order seams from clip-fragment winding
                    // differences are not rejected as false boundaries.
                    const float dx_rev_1 = a1.x - b2.x;
                    const float dy_rev_1 = a1.y - b2.y;
                    const float dz_rev_1 = std::abs(a1.z - b2.z);
                    const float dx_rev_2 = a2.x - b1.x;
                    const float dy_rev_2 = a2.y - b1.y;
                    const float dz_rev_2 = std::abs(a2.z - b1.z);

                    const float dx_same_1 = a1.x - b1.x;
                    const float dy_same_1 = a1.y - b1.y;
                    const float dz_same_1 = std::abs(a1.z - b1.z);
                    const float dx_same_2 = a2.x - b2.x;
                    const float dy_same_2 = a2.y - b2.y;
                    const float dz_same_2 = std::abs(a2.z - b2.z);

                    // Tolerate up to a 4 unit horizontal gap (16.0f squared) to handle BSP T-junctions.
                    static constexpr float MAX_DIST_SQR = 16.0f; // 4 units squared
                    static constexpr float MAX_Z_DIFF = NAV_MAX_STEP_SIZE + 4.0f; // Keep twin linking aligned with default step + clearance policy.
                    const bool reversed_endpoint_match =
                        dx_rev_1 * dx_rev_1 + dy_rev_1 * dy_rev_1 < MAX_DIST_SQR && dz_rev_1 <= MAX_Z_DIFF &&
                        dx_rev_2 * dx_rev_2 + dy_rev_2 * dy_rev_2 < MAX_DIST_SQR && dz_rev_2 <= MAX_Z_DIFF;
                    const bool same_order_endpoint_match =
                        dx_same_1 * dx_same_1 + dy_same_1 * dy_same_1 < MAX_DIST_SQR && dz_same_1 <= MAX_Z_DIFF &&
                        dx_same_2 * dx_same_2 + dy_same_2 * dy_same_2 < MAX_DIST_SQR && dz_same_2 <= MAX_Z_DIFF;

                    if ( reversed_endpoint_match || same_order_endpoint_match ) {
                        // If there are multiple overlapping edges, pick the one with the smallest endpoint
                        // error first, then the best vertical match as a tie-breaker.
                        const float reversed_endpoint_error =
                            ( dx_rev_1 * dx_rev_1 + dy_rev_1 * dy_rev_1 ) +
                            ( dx_rev_2 * dx_rev_2 + dy_rev_2 * dy_rev_2 );
                        const float same_order_endpoint_error =
                            ( dx_same_1 * dx_same_1 + dy_same_1 * dy_same_1 ) +
                            ( dx_same_2 * dx_same_2 + dy_same_2 * dy_same_2 );
                        const float endpoint_error =
                            ( reversed_endpoint_match && same_order_endpoint_match )
                                ? std::min( reversed_endpoint_error, same_order_endpoint_error )
                                : ( reversed_endpoint_match ? reversed_endpoint_error : same_order_endpoint_error );
                        const float total_z_reversed = dz_rev_1 + dz_rev_2;
                        const float total_z_same_order = dz_same_1 + dz_same_2;
                        const float totalZ =
                            ( reversed_endpoint_match && same_order_endpoint_match )
                                ? std::min( total_z_reversed, total_z_same_order )
                                : ( reversed_endpoint_match ? total_z_reversed : total_z_same_order );

                        if ( endpoint_error < bestEndpointError || ( endpoint_error == bestEndpointError && totalZ < bestZ ) ) {
                            bestEndpointError = endpoint_error;
                            bestZ = totalZ;
                            bestTwin = static_cast<int32_t>(j);
                        }
                    }
                }
            }
        }

		// If a suitable twin half-edge was found, link them together and compute the Z difference for pathfinding.
        if (bestTwin != -1) {
			// Link the twin half-edges together by setting their twin indices.
            g_nav_halfedges[i].twin_idx = bestTwin;
            g_nav_halfedges[bestTwin].twin_idx = static_cast<int32_t>(i);
			
			// Clear wall offset metadata since this is now an internal connected edge
			g_nav_halfedges[i].wall_offset = 0.0f;
			g_nav_halfedges[bestTwin].wall_offset = 0.0f;

			first_pass_twin_links++;
            
            // Use the actual Z height of the shared edge vertices, not the face centers!
            // Face centers are wildly inaccurate for long slopes or ramps.
            float z1 = (g_nav_vertices[g_nav_halfedges[i].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[i].next_idx].vertex_idx].z) * 0.5f;
            float z2 = (g_nav_vertices[g_nav_halfedges[bestTwin].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[bestTwin].next_idx].vertex_idx].z) * 0.5f;
            g_nav_halfedges[i].z_diff = z2 - z1;
            g_nav_halfedges[bestTwin].z_diff = z1 - z2;
            
            // Assign door metadata only when this portal separates one dynamic fragment from world geometry.
            AssignNavEntityEdgeMetadata( static_cast<int32_t>( i ), bestTwin );
        }
    }

    // Run a second deterministic sweep after first-pass linking to claim any seams that
    // became reachable once nearby greedy links were committed.
    RunDeterministicArbitration();

    /**
    * 	Secondary Twin Linking pass for T - Junctions and Overlaps
    *	Only links edges that overlap in 2D but have a strict Z-tolerance (<= 18.0f)
    *	to prevent accidentally linking catwalks to floors below them.
    **/
	// Build a spatial hash grid of half-edges based on their center position to facilitate efficient twin linking for overlapping edges.
    std::unordered_map<int64_t, std::vector<int32_t>> overlap_grid;
	// For each half-edge, compute its center point and store it in the overlap grid for later twin linking.
    for (size_t j = 0; j < g_nav_halfedges.size(); j++) {
		// Get the half-edge and its vertices to compute the center point.
        nav_halfedge_t& heB = g_nav_halfedges[j];
		// Compute the center point of the half-edge by averaging its two vertices.
        Vector3 b1 = g_nav_vertices[heB.vertex_idx];
        Vector3 b2 = g_nav_vertices[g_nav_halfedges[heB.next_idx].vertex_idx];
		// Compute the center point of the half-edge in 3D space.
        Vector3 center = ( b1 + b2 ) * 0.5f;
        
		// Compute the grid cell coordinates for the center point, using a 128-unit grid size to group nearby edges together.
		static constexpr float GRID_SIZE = 128.0f;
		int64_t cx = (int64_t)std::floor(center.x / GRID_SIZE );
        int64_t cy = (int64_t)std::floor(center.y / GRID_SIZE );
		// Compute a unique hash key for the grid cell based on its coordinates.
        int64_t key = (cx * 73856093) ^ (cy * 19349663);
		// Store the half-edge index in the corresponding grid cell for later twin linking of overlapping edges.
        overlap_grid[key].push_back((int32_t)j);
    }
    
	/**
	*	Now we iterate through all half-edges and attempt to find their twins by checking neighboring grid cells for potential matches.
	**/
	for (size_t i = 0; i < g_nav_halfedges.size(); i++) {
		if ( g_nav_halfedges[ i ].twin_idx != -1 ) {
			continue;
		}

		// Get the current half-edge and its vertices.
        nav_halfedge_t& heA = g_nav_halfedges[i];
		// Get the two vertices of the half-edge to compute its direction and length.
        Vector3 a1 = g_nav_vertices[heA.vertex_idx];
		// Get the next vertex of the half-edge to compute its direction and length.
        Vector3 a2 = g_nav_vertices[g_nav_halfedges[heA.next_idx].vertex_idx];

		// Compute the direction vector of the half-edge in 2D (ignoring Z) and its length.
        Vector3 dA = a2 - a1;
        dA.z = 0.0f;
        float lenA = QM_Vector3Length(dA);
        if (lenA < 0.1f) continue;
        Vector3 dirA = dA * ( 1.0f / lenA );

        // Initialize variables to track the best overlapping twin candidate based on overlap length.
        float bestOverlap = -1.0f;
        int32_t bestTwin = -1;

		// Compute the center point of the half-edge to use for spatial hashing in the overlap grid.
        Vector3 centerA = ( a1 + a2 ) * 0.5f;
		static constexpr float GRID_SIZE = 128.0f;
        int64_t cx = (int64_t)std::floor(centerA.x / GRID_SIZE );
        int64_t cy = (int64_t)std::floor(centerA.y / GRID_SIZE );
        
        for (int64_t ox = -1; ox <= 1; ox++) {
            for (int64_t oy = -1; oy <= 1; oy++) {
				// Compute the hash key for the neighboring grid cell to look for potential overlapping half-edges.
                int64_t key = ((cx + ox) * 73856093) ^ ((cy + oy) * 19349663);
                auto it = overlap_grid.find(key);
				// If the grid cell has no half-edges, skip to the next cell.
				if ( it == overlap_grid.end() ) {
					continue;
				}
				// For each half-edge in the neighboring grid cell, check if it overlaps with the current half-edge in 2D and has a small Z difference.
                for (int32_t j : it->second) {
					// Skip self and already linked half-edges.
					if ( i == j ) {
						continue;
					}
					// Skip half-edges that already have a twin assigned.
					if ( g_nav_halfedges[ j ].twin_idx != -1 ) {
						continue;
					}

					// Get the candidate half-edge and its vertices for comparison.
                    nav_halfedge_t& heB = g_nav_halfedges[j];
                    if ( heA.face_idx == heB.face_idx ) {
                        continue;
                    }
                    // Reject cross-mover pairs before overlap arbitration can claim this edge.
                    if ( !AreHalfEdgesInCompatibleNavigationDomains( static_cast<int32_t>( i ), j ) ) {
                        continue;
                    }
                    Vector3 b1 = g_nav_vertices[heB.vertex_idx];
                    Vector3 b2 = g_nav_vertices[g_nav_halfedges[heB.next_idx].vertex_idx];

					// Compute the direction vector of the candidate half-edge in 2D (ignoring Z) and its length.
                    Vector3 dB = b2 - b1;
                    dB.z = 0.0f;
                    float lenB = QM_Vector3Length(dB);
					// Skip candidate half-edges that are too short to be considered for twin linking.
					if ( lenB < 0.1f ) {
						continue;
					}
                    Vector3 dirB = dB * ( 1.0f / lenB );
                    // Accept near-collinear candidates regardless of winding order. This is
                    // required for split-detail seams where winding direction can diverge.
                    if ( std::fabs( QM_Vector3DotProduct( dirA, dirB ) ) < 0.95f ) {
						continue;
					}

                    // Endpoint projection alone is insufficient: two parallel stair
                    // edges on adjacent risers can project onto one another while
                    // remaining several units apart.  Such a false twin creates a
                    // traversable-looking L-turn portal that the capsule cannot cross.
                    const float lateralSeparation = std::fabs(
                        dirA.x * ( b1.y - a1.y ) - dirA.y * ( b1.x - a1.x ) );
                    static constexpr float MAX_LATERAL_SEPARATION = 4.0f;
                    if ( lateralSeparation > MAX_LATERAL_SEPARATION ) {
                        continue;
                    }

					// Project the candidate half-edge's first vertex onto the current half-edge's direction to find the closest point and check for overlap.
                    Vector3 a1_to_b1 = b1 - a1;
                    a1_to_b1.z = 0.0f;
                    float proj = QM_Vector3DotProduct(a1_to_b1, dirA);
                    Vector3 closestPt = QM_Vector3MultiplyAdd( a1, proj, dirA );
                    closestPt.z = 0.0f;
                    Vector3 b1_2d = b1; b1_2d.z = 0.0f;
					// If the closest point is more than 4 units away from b1 in 2D, skip this candidate half-edge.
                    if ( QM_Vector3DistanceSqr( b1_2d, closestPt ) > 64.0f ) {
						continue;
					}

					// Compute the projection of the candidate half-edge's second vertex onto the current half-edge's direction to find the overlap range.
                    float u1 = proj;
                    Vector3 a1_to_b2 = b2 - a1;
                    a1_to_b2.z = 0.0f;
                    float u2 = QM_Vector3DotProduct(a1_to_b2, dirA);

					// Compute the overlap range between the two half-edges in 2D and check if it exceeds 1.0f units.
                    float minU = std::min(u1, u2);
                    float maxU = std::max(u1, u2);
                    float overlapStart = std::max(0.0f, minU);
                    float overlapEnd = std::min(lenA, maxU);
                    float overlapLen = overlapEnd - overlapStart;

					// Require a minimally usable overlap so we do not link paper-thin portals that path traversal later rejects.
                    if (overlapLen >= 2.0f) {
                        float dz = std::abs((a1.z + a2.z) * 0.5f - (b1.z + b2.z) * 0.5f);
                        // STRICT Z tolerance to prevent vertical scrambling!
						static constexpr float MAX_Z_DIFF = NAV_MAX_STEP_SIZE + 4.0f; // Keep twin linking aligned with default step + clearance policy.
                        if ( dz <= MAX_Z_DIFF ) {
                            if (overlapLen > bestOverlap) {
                                bestOverlap = overlapLen;
                                bestTwin = static_cast<int32_t>(j);
                            }
                        }
                    }
                }
            }
        }

        if (bestTwin != -1) {
            g_nav_halfedges[i].twin_idx = bestTwin;
            g_nav_halfedges[bestTwin].twin_idx = static_cast<int32_t>(i);
			
			// Clear wall offset metadata since this is now an internal connected edge
			g_nav_halfedges[i].wall_offset = 0.0f;
			g_nav_halfedges[bestTwin].wall_offset = 0.0f;

			second_pass_twin_links++;
            
            // Use the actual Z height of the shared edge vertices, not the face centers!
            float z1 = (g_nav_vertices[g_nav_halfedges[i].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[i].next_idx].vertex_idx].z) * 0.5f;
            float z2 = (g_nav_vertices[g_nav_halfedges[bestTwin].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[bestTwin].next_idx].vertex_idx].z) * 0.5f;
            g_nav_halfedges[i].z_diff = z2 - z1;
            g_nav_halfedges[bestTwin].z_diff = z1 - z2;

            // Apply the same strict dynamic-to-world transition rule to overlap-linked portals.
            AssignNavEntityEdgeMetadata( static_cast<int32_t>( i ), bestTwin );
        }

    }

    // Preserve boundary edges that belong to transition-owned faces so the debug overlay
    // can show door open/closed state directly on the navmesh itself.
    for ( size_t edge_index = 0; edge_index < g_nav_halfedges.size(); ++edge_index ) {
        nav_halfedge_t &halfedge = g_nav_halfedges[ edge_index ];
        if ( halfedge.twin_idx != -1 ) {
            continue;
        }

        const nav_face_t &face = g_nav_faces[ halfedge.face_idx ];
        const int32_t face_entity_id = GetNavFaceOwningEntityId( face );
        if ( face_entity_id != ENTITYNUM_NONE ) {
            halfedge.edge_entity_id = face_entity_id;
            RegisterNavEntityEdge( face_entity_id, static_cast<int32_t>( edge_index ) );
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
    *   This provides a single clean edge for doors without extracting their 3D physical bounds.
    **/
    for ( int32_t i = 1; i < g_edict_pool.num_edicts; ++i ) {
        svg_base_edict_t *edict = g_edicts[ i ];
        if ( !edict || !SVG_Entity_IsActive( edict ) ) continue;

        if ( edict->GetTypeInfo()->IsSubClassType<svg_func_door_t>() || edict->GetTypeInfo()->IsSubClassType<svg_func_door_rotating_t>() ) {
            const int32_t entity_id = edict->teammaster ? edict->teammaster->s.number : i;
            
            // Expand the bounding box slightly to capture the underlying floor edge,
            // taking into account that vertical rotating doors might have angled boxes.
            Vector3 absMin = edict->absMin;
            Vector3 absMax = edict->absMax;
            absMin.x -= 4.0f; absMin.y -= 4.0f; absMin.z -= 8.0f;
            absMax.x += 4.0f; absMax.y += 4.0f; absMax.z += 8.0f;
            
            for ( size_t edge_index = 0; edge_index < g_nav_halfedges.size(); ++edge_index ) {
                nav_halfedge_t &halfedge = g_nav_halfedges[ edge_index ];
                if ( halfedge.twin_idx == -1 ) continue;
                if ( halfedge.edge_entity_id != ENTITYNUM_NONE ) continue;
                
                const Vector3 &v1 = g_nav_vertices[ halfedge.vertex_idx ];
                const Vector3 &v2 = g_nav_vertices[ g_nav_halfedges[ halfedge.next_idx ].vertex_idx ];
                
                if ( v1.x >= absMin.x && v1.x <= absMax.x &&
                     v1.y >= absMin.y && v1.y <= absMax.y &&
                     v1.z >= absMin.z && v1.z <= absMax.z &&
                     v2.x >= absMin.x && v2.x <= absMax.x &&
                     v2.y >= absMin.y && v2.y <= absMax.y &&
                     v2.z >= absMin.z && v2.z <= absMax.z ) {
                    
                    halfedge.edge_entity_id = entity_id;
                    g_nav_halfedges[ halfedge.twin_idx ].edge_entity_id = entity_id;
                    
                    RegisterNavEntityEdge( entity_id, static_cast<int32_t>( edge_index ) );
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
	*	Emit topology diagnostics so we can detect fragmented regions and weak links.
	**/
	auto Compute2DOverlapLen = [&]( const nav_halfedge_t &a, const nav_halfedge_t &b ) -> float {
		const Vector3 a0 = g_nav_vertices[ a.vertex_idx ];
		const Vector3 a1 = g_nav_vertices[ g_nav_halfedges[ a.next_idx ].vertex_idx ];
		const Vector3 b0 = g_nav_vertices[ b.vertex_idx ];
		const Vector3 b1 = g_nav_vertices[ g_nav_halfedges[ b.next_idx ].vertex_idx ];
        Vector3 aDir = a1 - a0;
		aDir.z = 0.0f;
		const float aLen = QM_Vector3Length( aDir );
		if ( aLen <= 0.0001f ) {
			return 0.0f;
		}
        aDir = aDir * ( 1.0f / aLen );
        Vector3 a0b0 = b0 - a0;
        Vector3 a0b1 = b1 - a0;
		a0b0.z = 0.0f;
		a0b1.z = 0.0f;
		const float u0 = static_cast<float>( QM_Vector3DotProduct( a0b0, aDir ) );
		const float u1 = static_cast<float>( QM_Vector3DotProduct( a0b1, aDir ) );
		const float bMin = std::min( u0, u1 );
		const float bMax = std::max( u0, u1 );
		const float overlapStart = std::max( 0.0f, bMin );
		const float overlapEnd = std::min( aLen, bMax );
		return std::max( 0.0f, overlapEnd - overlapStart );
	};

	int32_t boundary_edges = 0;
	int32_t twinned_edges = 0;
	int32_t twin_reverse_mismatch = 0;
	int32_t tiny_overlap_twins = 0;
	int32_t short_boundary_edges = 0;
	for ( size_t i = 0; i < g_nav_halfedges.size(); i++ ) {
		const nav_halfedge_t &he = g_nav_halfedges[ i ];
		const Vector3 e0 = g_nav_vertices[ he.vertex_idx ];
		const Vector3 e1 = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];
		const float edgeLen2D = static_cast<float>( QM_Vector2Distance( e0, e1 ) );
		if ( he.twin_idx == -1 ) {
			boundary_edges++;
			if ( edgeLen2D < 4.0f ) {
				short_boundary_edges++;
			}
			continue;
		}
		twinned_edges++;
		if ( static_cast<size_t>( he.twin_idx ) >= g_nav_halfedges.size() || g_nav_halfedges[ he.twin_idx ].twin_idx != static_cast<int32_t>( i ) ) {
			twin_reverse_mismatch++;
			continue;
		}
		const float overlapLen = Compute2DOverlapLen( he, g_nav_halfedges[ he.twin_idx ] );
		if ( overlapLen < 2.0f ) {
			tiny_overlap_twins++;
		}
	}

    gi.dprintf("NavMesh Half-Edge Generation Completed. %d vertices, %d half-edges, %d faces.\n", 
               g_nav_vertices.size(), g_nav_halfedges.size(), g_nav_faces.size());
    gi.dprintf("NavMesh Topology Diagnostics: deterministicLinks=%d firstPassLinks=%d secondPassLinks=%d twinnedEdges=%d boundaryEdges=%d shortBoundaryEdges=%d twinReverseMismatch=%d tinyOverlapTwins=%d\n",
        deterministic_twin_links, first_pass_twin_links, second_pass_twin_links, twinned_edges, boundary_edges, short_boundary_edges, twin_reverse_mismatch, tiny_overlap_twins );
    LogNavGenerationDiagnostics( "HalfEdge" );
}

/**
*	@brief	Wrapper function to build the KD-Tree for the navmesh faces.
**/
/**
* @brief Build the KD-tree used for spatial nav queries.
* @note This is the final generation stage after the half-edge mesh is ready.
**/
void Nav_BuildKDTree() {
    g_nav_nodes.clear();
	if ( g_nav_faces.empty() ) return;
	
    BuildKDNode(0, g_nav_faces.size(), 0);

	for (int32_t i = 0; i < g_nav_faces.size(); i++) {
		g_nav_faces[i].face_id = i;
        
		/**
        *	`CRITICAL FIX`: `BuildKDNode` calls `std::partition`, which physically shuffles `g_nav_faces`.
        *	We `MUST` update the back-pointers in the half-edges so they point to the NEW shuffled face index!
        *	Otherwise, Nav_FindPath's A* will jump to random, completely unconnected faces!
		**/
        for (int32_t e = 0; e < g_nav_faces[i].num_edges; ++e) {
            g_nav_halfedges[g_nav_faces[i].first_edge_idx + e].face_idx = i;
        }
	}

    gi.dprintf("NavMesh KD-Tree Generation Completed. %d nodes created.\n", g_nav_nodes.size());
}
 
 
