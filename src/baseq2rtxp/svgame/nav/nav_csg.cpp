/********************************************************************
*
*
*	ServerGame: Navigation mesh Constructive Solid Geometry (CSG)
*				winding operations, coplanar merging, sliver dissolution,
*				and obstacle-aware spatial polygon partitioning.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "nav_csg.h"
#include "nav_generate.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

/**
*	@brief	Recompute the 3D geometric centroid of a nav_poly_t polygon from its vertex loop.
*	@param	poly	[in,out] Polygon structure whose center point will be updated.
*	@note	Averaging all vertex coordinates yields the exact 3D center used for bounding box calculations
*			and spatial distance comparisons during navigation queries.
**/
void RecomputeNavPolygonCenter( nav_poly_t &poly ) {
	/**
	*	Sanity check: ensure polygon has valid vertices before computing average position.
	**/
	// Check if the polygon contains no vertices to prevent division by zero.
	if ( poly.num_vertices <= 0 ) {
		// Reset center to zero vector for degenerate or empty polygons to avoid uninitialized memory.
		poly.center = { 0.0, 0.0, 0.0 };
		return;
	}

	/**
	*	Sum vertex coordinates across all polygon vertices.
	**/
	// Initialize accumulator vector to zero.
	Vector3DP sum = { 0.0, 0.0, 0.0 };
	// Loop over all vertices in the polygon loop.
	for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
		// Accumulate 3D vertex position into sum.
		sum = sum + Vector3DP( poly.vertices[ i ] );
	}

	/**
	*	Divide accumulated sum by total vertex count to compute average position centroid.
	**/
	// Compute reciprocal scaling factor for averaging.
	const double scale = 1.0 / static_cast< double >( poly.num_vertices );
	// Assign average coordinate position to polygon center.
	poly.center = sum * scale;
}

/**
* @brief Determine whether two navigation polygons occupy the same surface plane.
* @param first First polygon to compare.
* @param second Second polygon to compare.
* @return True when the normals agree and the second polygon is coplanar with the first.
* @note This prevents projected 2D overlaps from joining legitimate step transitions.
**/
bool AreNavPolygonsCoplanar( const nav_poly_t &first, const nav_poly_t &second ) {
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
*	@brief	Split a CSG winding along a spatial plane into front and back sub-windings.
*	@param	in			[in] Source winding structure to be split.
*	@param	split		[in] Clipping plane definition containing normal vector and plane distance.
*	@param	epsilon		[in] Thick-plane thickness tolerance for on-plane classification.
*	@param	face_normal	[in] Unit normal vector of the source face for projection math.
*	@param	front		[out] Output winding fragment lying in front of the split plane.
*	@param	back		[out] Output winding fragment lying behind the split plane.
*	@note	Uses standard Sutherland-Hodgman clipping math extended for double-precision 3D Quake geometry.
**/
void SplitWinding( const winding_t *in, const cm_plane_t *split, double epsilon, const Vector3DP &face_normal, winding_t *front, winding_t *back ) {
	/**
	*	Sanity checks: validate caller-provided pointers.
	**/
	// Check if any mandatory input or output pointers are null.
	if ( !in || !split || !front || !back ) {
		// Return immediately if any pointer is invalid to prevent crash.
		return;
	}

	/**
	*	Reset output winding vertex counts and pre-allocate capacity.
	**/
	front->clear();
	front->reserve( in->num_points + 4 );
	back->clear();
	back->reserve( in->num_points + 4 );

	/**
	*	Propagate entity ownership metadata from source winding to output fragments.
	**/
	// Preserve entity ID on front fragment so dynamic door boundaries remain linked.
	front->entity_id = in->entity_id;
	// Preserve transition entity ID on front fragment.
	front->transition_entity_id = in->transition_entity_id;
	// Preserve entity ID on back fragment.
	back->entity_id = in->entity_id;
	// Preserve transition entity ID on back fragment.
	back->transition_entity_id = in->transition_entity_id;

	/**
	*	Per-vertex distance evaluation and side classification buffer setup.
	**/
	// Signed distance buffer for each vertex relative to the clipping plane.
	nav_vector_t<double> dists;
	dists.resize( static_cast< size_t >( in->num_points + 4 ) );

	// Side classification buffer (0 = FRONT, 1 = BACK, 2 = ON PLANE).
	nav_vector_t<int32_t> sides;
	sides.resize( static_cast< size_t >( in->num_points + 4 ) );

	// Side count histogram counters.
	int32_t counts[ 3 ] = {}; // 0 = FRONT, 1 = BACK, 2 = ON PLANE

	// Convert split plane normal array to double-precision 3D vector.
	Vector3DP split_normal = { split->normal[ 0 ], split->normal[ 1 ], split->normal[ 2 ] };

	/**
	*	Classify every vertex against the split plane using signed dot product distance.
	**/
	// Loop over all vertices in the input winding.
	for ( int32_t i = 0; i < in->num_points; i++ ) {
		// Calculate signed distance from plane: dot(p, N) - d.
		double dot = QM_Vector3DotProductDP( in->points[ i ], split_normal ) - split->dist;
		// Store exact signed distance in dists array.
		dists[ i ] = dot;

		// Classify vertex side relative to epsilon thickness threshold.
		if ( dot > epsilon ) {
			// Vertex lies strictly in front of plane.
			sides[ i ] = 0; // SIDE_FRONT
		} else if ( dot < -epsilon ) {
			// Vertex lies strictly behind plane.
			sides[ i ] = 1; // SIDE_BACK
		} else {
			// Vertex lies within epsilon thick-plane tolerance.
			sides[ i ] = 2; // SIDE_ON
		}
		// Increment count for the classified side.
		counts[ sides[ i ] ]++;
	}

	/**
	*	Wrap distance and side arrays to simplify edge loop calculations.
	**/
	// Wrap first vertex distance to end of buffer for closed polygon iteration.
	dists[ in->num_points ] = dists[ 0 ];
	// Wrap first vertex side classification to end of buffer.
	sides[ in->num_points ] = sides[ 0 ];

	/**
	*	Fast early-out pass: handle un-split windings that lie entirely on one side.
	**/
	// If no points lie in front of the plane, the entire winding lies on or behind the plane.
	if ( counts[ 0 ] == 0 ) {
		// Pass input winding intact to back output fragment.
		*back = *in;
		return;
	}
	// If no points lie behind the plane, the entire winding lies on or in front of the plane.
	if ( counts[ 1 ] == 0 ) {
		// Pass input winding intact to front output fragment.
		*front = *in;
		return;
	}

	/**
	*	Sutherland-Hodgman clipping pass: iterate over all polygon edges and slice crossing segments.
	**/
	// Loop over all edge segments of the input polygon.
	for ( int32_t i = 0; i < in->num_points; i++ ) {
		// Fetch start vertex p1 of current edge segment.
		Vector3DP p1 = in->points[ i ];

		// If start vertex lies directly on the plane, emit to both front and back output fragments.
		if ( sides[ i ] == 2 ) {
			front->push_back( p1 );
			back->push_back( p1 );
			continue;
		}

		// Emit vertex p1 to front output fragment if classified as SIDE_FRONT.
		if ( sides[ i ] == 0 ) {
			front->push_back( p1 );
		}
		// Emit vertex p1 to back output fragment if classified as SIDE_BACK.
		if ( sides[ i ] == 1 ) {
			back->push_back( p1 );
		}

		// Skip intersection calculation if next vertex is on-plane or on the same side.
		if ( sides[ i + 1 ] == 2 || sides[ i + 1 ] == sides[ i ] ) {
			continue;
		}

		/**
		*	Calculate exact 3D intersection point where edge segment (p1 -> p2) crosses split plane.
		**/
		// Fetch end vertex p2 of current edge segment.
		Vector3DP p2 = in->points[ ( i + 1 ) % in->num_points ];
		// Fetch signed distance of p1.
		double d1 = dists[ i ];
		// Fetch signed distance of p2.
		double d2 = dists[ i + 1 ];
		// Interpolation fraction t = d1 / (d1 - d2).
		double dot = d1 / ( d1 - d2 );

		// Compute 3D midpoint position via linear interpolation: mid = p1 + (p2 - p1) * t.
		Vector3DP mid = p1 + ( p2 - p1 ) * dot;

		// Emit intersection point to both front and back fragments to form shared split edge.
		front->push_back( mid );
		back->push_back( mid );
	}

}

/**
*	@brief	Chop a CSG winding in-place against a clipping plane, retaining only the front fragment.
*	@param	in			[in,out] Double pointer to the winding structure to modify.
*	@param	split		[in] Clipping plane definition.
*	@param	epsilon		[in] Thickness tolerance for on-plane classification.
*	@param	face_normal	[in] Normal vector of the source face.
*	@note	Used during BSP tree carving to discard back fragments that fall inside solid geometry.
**/
void ChopWindingInPlace( winding_t **in, const cm_plane_t *split, double epsilon, const Vector3DP &face_normal ) {
	// Sanity check: return immediately if pointer or target winding is null.
	if ( !in || !*in ) return;

	// Temporary output buffers for split operation.
	winding_t front = {};
	winding_t back = {};

	// Perform plane split operation.
	SplitWinding( *in, split, epsilon, face_normal, &front, &back );

	// Overwrite input winding with front fragment (discarding back fragment).
	**in = front;
}

/**
*	@brief	Attempt to merge two coplanar 2D/3D polygon windings into a single convex winding.
*	@param	w1		[in] First candidate winding to merge.
*	@param	w2		[in] Second candidate winding to merge.
*	@param	normal	[in] Common plane unit normal vector of the coplanar polygons.
*	@param	out		[out] Output winding structure to store the combined convex polygon.
*	@return	True if the two windings share a valid edge segment and form a strictly convex polygon; false otherwise.
*	@note	Splices all collinear intermediate vertices along shared edges using projection distance sorting.
*			Evaluates convexity using 2D normalized cross products to prevent edge-length scaling artifacts.
**/
bool TryMergeWindings( const winding_t &w1, const winding_t &w2, const Vector3DP &normal, winding_t *out ) {
	/**
	*	Sanity checks: ensure valid pointers and matching entity ownership.
	**/
	// Check if caller provided a valid output pointer.
	if ( !out ) {
		// Return false if output pointer is null.
		return false;
	}

	// Do not merge polygons that belong to different entities!
	// This preserves dynamic door boundary edges for the nav graph.
	if ( w1.entity_id != w2.entity_id ) {
		// Return false if entity IDs do not match.
		return false;
	}

	/**
	*	Copy input windings and ensure both are strictly Counter-Clockwise (CCW).
	**/
	winding_t copy1 = w1;
	winding_t copy2 = w2;
	if ( !EnsureWindingCCW( copy1 ) || !EnsureWindingCCW( copy2 ) ) {
		return false;
	}

	/**
	*	Structure to hold collinear splice candidate vertices sorted by edge projection distance.
	**/
	struct SpliceCand {
		//! Projection scalar distance along the host edge segment.
		double proj = 0.0;
		//! 3D vertex position to splice into the host winding.
		Vector3DP point = {};
	};

	/**
	*	Splice collinear vertices from copy2 onto copy1's edge segments.
	*	Collects all candidate vertices from copy2 projecting inside each copy1 edge,
	*	sorts them by projection distance, and inserts them in sequence.
	**/
	// Loop over all edge segments of copy1.
	for ( int32_t i1 = 0; i1 < copy1.num_points; i1++ ) {
		// Start point of copy1 edge i1.
		Vector3DP a1 = copy1.points[ i1 ];
		// End point of copy1 edge i1.
		Vector3DP b1 = copy1.points[ ( i1 + 1 ) % copy1.num_points ];
		// Compute edge direction vector.
		Vector3DP dir1 = b1 - a1;
		// Project direction onto horizontal 2D plane to ignore vertical step noise.
		dir1.z = 0.0;
		// Measure original edge length in 2D.
		const double origLen1 = std::sqrt( dir1.x * dir1.x + dir1.y * dir1.y );
		// Skip degenerate zero-length edge segments.
		if ( origLen1 < 0.001 ) {
			continue;
		}
		// Normalize edge 2D direction vector.
		dir1 = dir1 * ( 1.0 / origLen1 );

		// Container for candidate collinear vertices from copy2 projecting onto edge i1.
		std::vector<SpliceCand> cands;

		// Iterate over all vertices of copy2 to find points lying on copy1 edge i1.
		for ( int32_t i2 = 0; i2 < copy2.num_points; i2++ ) {
			// Candidate vertex from copy2.
			Vector3DP p2 = copy2.points[ i2 ];
			// Vector from edge start a1 to candidate vertex p2.
			Vector3DP a1_to_p2 = p2 - a1;
			// Project onto horizontal 2D plane.
			a1_to_p2.z = 0.0;
			// Compute 2D projection distance along normalized edge direction.
			double proj = QM_Vector3DotProductDP( a1_to_p2, dir1 );

			// Check if projection falls strictly inside the interior of edge i1.
			if ( proj > 0.01 && proj < origLen1 - 0.01 ) {
				// Compute closest point on edge line segment.
				Vector3DP closest = a1 + dir1 * proj;
				// Compute 2D squared distance from vertex to edge segment.
				double dist2d_sqr = ( p2.x - closest.x ) * ( p2.x - closest.x ) + ( p2.y - closest.y ) * ( p2.y - closest.y );
				// If vertex lies within 0.5 units in 2D and 1.0 unit in Z, register as splice candidate.
				if ( dist2d_sqr < 0.25 && std::abs( p2.z - closest.z ) < 1.0 ) {
					cands.push_back( { proj, p2 } );
				}
			}
		}

		// If candidate vertices were found, sort by projection distance and insert into copy1.
		if ( !cands.empty() ) {
			// Sort candidates in ascending order along the edge segment.
			std::sort( cands.begin(), cands.end(), []( const SpliceCand &a, const SpliceCand &b ) {
				return a.proj < b.proj;
			} );

			// Insert sorted candidates into copy1 after index i1.
			for ( const auto &cand : cands ) {
				// Shift existing points forward to make room for inserted vertex.
				copy1.ensure_capacity( copy1.num_points + 1 );
				for ( int32_t k = copy1.num_points; k > i1 + 1; k-- ) {
					copy1.points[ k ] = copy1.points[ k - 1 ];
				}
				// Assign inserted vertex position.
				copy1.points[ i1 + 1 ] = cand.point;
				// Increment winding point count.
				copy1.num_points++;
				// Advance edge index past the newly inserted vertex.
				i1++;
			}
		}
	}

	/**
	*	Splice collinear vertices from copy1 onto copy2's edge segments.
	*	Symmetrically inserts T-junction vertices from copy1 onto copy2 edges.
	**/
	// Loop over all edge segments of copy2.
	for ( int32_t i2 = 0; i2 < copy2.num_points; i2++ ) {
		// Start point of copy2 edge i2.
		Vector3DP a2 = copy2.points[ i2 ];
		// End point of copy2 edge i2.
		Vector3DP b2 = copy2.points[ ( i2 + 1 ) % copy2.num_points ];
		// Compute edge direction vector.
		Vector3DP dir2 = b2 - a2;
		// Project direction onto horizontal 2D plane.
		dir2.z = 0.0;
		// Measure original edge length in 2D.
		const double origLen2 = std::sqrt( dir2.x * dir2.x + dir2.y * dir2.y );
		// Skip degenerate zero-length edge segments.
		if ( origLen2 < 0.001 ) {
			continue;
		}
		// Normalize edge 2D direction vector.
		dir2 = dir2 * ( 1.0 / origLen2 );

		// Container for candidate collinear vertices from copy1 projecting onto edge i2.
		std::vector<SpliceCand> cands;

		// Iterate over all vertices of copy1 to find points lying on copy2 edge i2.
		for ( int32_t i1 = 0; i1 < copy1.num_points; i1++ ) {
			// Candidate vertex from copy1.
			Vector3DP p1 = copy1.points[ i1 ];
			// Vector from edge start a2 to candidate vertex p1.
			Vector3DP a2_to_p1 = p1 - a2;
			// Project onto horizontal 2D plane.
			a2_to_p1.z = 0.0;
			// Compute 2D projection distance along normalized edge direction.
			double proj = QM_Vector3DotProductDP( a2_to_p1, dir2 );

			// Check if projection falls strictly inside the interior of edge i2.
			if ( proj > 0.01 && proj < origLen2 - 0.01 ) {
				// Compute closest point on edge line segment.
				Vector3DP closest = a2 + dir2 * proj;
				// Compute 2D squared distance from vertex to edge segment.
				double dist2d_sqr = ( p1.x - closest.x ) * ( p1.x - closest.x ) + ( p1.y - closest.y ) * ( p1.y - closest.y );
				// If vertex lies within 0.5 units in 2D and 1.0 unit in Z, register as splice candidate.
				if ( dist2d_sqr < 0.25 && std::abs( p1.z - closest.z ) < 1.0 ) {
					cands.push_back( { proj, p1 } );
				}
			}
		}

		// If candidate vertices were found, sort by projection distance and insert into copy2.
		if ( !cands.empty() ) {
			// Sort candidates in ascending order along the edge segment.
			std::sort( cands.begin(), cands.end(), []( const SpliceCand &a, const SpliceCand &b ) {
				return a.proj < b.proj;
			} );

			// Insert sorted candidates into copy2 after index i2.
			for ( const auto &cand : cands ) {
				copy2.ensure_capacity( copy2.num_points + 1 );
				// Shift existing points forward to make room for inserted vertex.
				for ( int32_t k = copy2.num_points; k > i2 + 1; k-- ) {
					copy2.points[ k ] = copy2.points[ k - 1 ];
				}
				// Assign inserted vertex position.
				copy2.points[ i2 + 1 ] = cand.point;
				// Increment winding point count.
				copy2.num_points++;
				// Advance edge index past the newly inserted vertex.
				i2++;
			}
		}
	}

	/**
	*	Match anti-parallel shared edge segment between the spliced windings.
	**/
	// Indices of matching edge endpoints.
	int32_t match_i1 = -1, match_i2 = -1;

	// Loop over all edge segments of copy1.
	for ( int32_t i1 = 0; i1 < copy1.num_points; i1++ ) {
		Vector3DP a1 = copy1.points[ i1 ];
		Vector3DP b1 = copy1.points[ ( i1 + 1 ) % copy1.num_points ];

		// Loop over all edge segments of copy2.
		for ( int32_t i2 = 0; i2 < copy2.num_points; i2++ ) {
			Vector3DP a2 = copy2.points[ i2 ];
			Vector3DP b2 = copy2.points[ ( i2 + 1 ) % copy2.num_points ];

			// Compute squared 2D distances between opposite endpoints (b2 == a1 and a2 == b1).
			double dist_a1_b2_2d = ( a1.x - b2.x ) * ( a1.x - b2.x ) + ( a1.y - b2.y ) * ( a1.y - b2.y );
			double dist_b1_a2_2d = ( b1.x - a2.x ) * ( b1.x - a2.x ) + ( b1.y - a2.y ) * ( b1.y - a2.y );

			// Match anti-parallel shared edge in 2D with 1.0 unit Z tolerance.
			if ( dist_a1_b2_2d < 0.25 && dist_b1_a2_2d < 0.25 &&
				 std::abs( a1.z - b2.z ) < 1.0 && std::abs( b1.z - a2.z ) < 1.0 ) {
				match_i1 = i1;
				match_i2 = i2;
				break;
			}
		}
		// If match found, stop searching.
		if ( match_i1 != -1 ) {
			break;
		}
	}

	// Fallback: check if copy2 has matching parallel edge (indicating reversed orientation).
	if ( match_i1 == -1 ) {
		for ( int32_t i1 = 0; i1 < copy1.num_points; i1++ ) {
			Vector3DP a1 = copy1.points[ i1 ];
			Vector3DP b1 = copy1.points[ ( i1 + 1 ) % copy1.num_points ];

			for ( int32_t i2 = 0; i2 < copy2.num_points; i2++ ) {
				Vector3DP a2 = copy2.points[ i2 ];
				Vector3DP b2 = copy2.points[ ( i2 + 1 ) % copy2.num_points ];

				double dist_a1_a2_2d = ( a1.x - a2.x ) * ( a1.x - a2.x ) + ( a1.y - a2.y ) * ( a1.y - a2.y );
				double dist_b1_b2_2d = ( b1.x - b2.x ) * ( b1.x - b2.x ) + ( b1.y - b2.y ) * ( b1.y - b2.y );

				if ( dist_a1_a2_2d < 0.25 && dist_b1_b2_2d < 0.25 &&
					 std::abs( a1.z - a2.z ) < 1.0 && std::abs( b1.z - b2.z ) < 1.0 ) {
					// Reverse copy2 to enforce anti-parallel alignment.
					for ( int32_t k = 0; k < copy2.num_points / 2; k++ ) {
						std::swap( copy2.points[ k ], copy2.points[ copy2.num_points - 1 - k ] );
					}
					// Re-evaluate anti-parallel edge matching after reversal.
					for ( int32_t r2 = 0; r2 < copy2.num_points; r2++ ) {
						Vector3DP ra2 = copy2.points[ r2 ];
						Vector3DP rb2 = copy2.points[ ( r2 + 1 ) % copy2.num_points ];
						double d_a1_rb2 = ( a1.x - rb2.x ) * ( a1.x - rb2.x ) + ( a1.y - rb2.y ) * ( a1.y - rb2.y );
						double d_b1_ra2 = ( b1.x - ra2.x ) * ( b1.x - ra2.x ) + ( b1.y - ra2.y ) * ( b1.y - ra2.y );
						if ( d_a1_rb2 < 0.25 && d_b1_ra2 < 0.25 &&
							 std::abs( a1.z - rb2.z ) < 1.0 && std::abs( b1.z - ra2.z ) < 1.0 ) {
							match_i1 = i1;
							match_i2 = r2;
							break;
						}
					}
					break;
				}
			}
			if ( match_i1 != -1 ) {
				break;
			}
		}
	}

	// If no shared edge segment was found between windings, return false.
	if ( match_i1 == -1 ) {
		return false;
	}

	/**
	*	Construct merged polygon winding by stitching perimeter vertices.
	**/
	// Reset output winding point count and pre-allocate capacity.
	out->clear();
	out->entity_id = w1.entity_id;
	out->transition_entity_id = w1.transition_entity_id;
	// Store total point counts of spliced windings.
	int32_t n1 = copy1.num_points;
	int32_t n2 = copy2.num_points;
	out->reserve( n1 + n2 );

	// Append non-shared vertices from copy1.
	for ( int32_t j = 0; j < n1; j++ ) {
		out->push_back( Vector3DP( copy1.points[ ( match_i1 + 1 + j ) % n1 ] ) );
	}
	// Append non-shared vertices from copy2.
	for ( int32_t j = 0; j < n2 - 2; j++ ) {
		out->push_back( Vector3DP( copy2.points[ ( match_i2 + 2 + j ) % n2 ] ) );
	}

	/**
	*	Check convexity and simplify collinear vertices using normalized 2D edge directions.
	**/
	// Copy merged winding for in-place simplification loop.
	winding_t simple = *out;

	// Evaluate vertex turn angles around the perimeter.
	for ( int32_t j = 0; j < simple.num_points; j++ ) {
		Vector3DP p1 = simple.points[ j ];
		Vector3DP p2 = simple.points[ ( j + 1 ) % simple.num_points ];
		Vector3DP p3 = simple.points[ ( j + 2 ) % simple.num_points ];

		// Compute edge direction vectors.
		Vector3DP dir1 = p2 - p1;
		Vector3DP dir2 = p3 - p2;
		// Project onto horizontal 2D plane.
		dir1.z = 0.0;
		dir2.z = 0.0;

		// Calculate 2D edge lengths.
		const double len1 = std::sqrt( dir1.x * dir1.x + dir1.y * dir1.y );
		const double len2 = std::sqrt( dir2.x * dir2.x + dir2.y * dir2.y );

		// Remove degenerate zero-length edge vertices.
		if ( len1 < 0.001 || len2 < 0.001 ) {
			int32_t remove_idx = ( j + 1 ) % simple.num_points;
			for ( int32_t k = remove_idx; k < simple.num_points - 1; k++ ) {
				simple.points[ k ] = simple.points[ k + 1 ];
			}
			simple.num_points--;
			j = -1;
			continue;
		}

		// Normalize edge direction vectors.
		dir1 = dir1 * ( 1.0 / len1 );
		dir2 = dir2 * ( 1.0 / len2 );

		// Compute 2D cross product of normalized edge direction vectors.
		const double cross2d = dir1.x * dir2.y - dir1.y * dir2.x;

		// Angular tolerances (~0.3 degrees) independent of physical edge lengths.
		constexpr double CONCAVE_TOLERANCE = -0.005;
		constexpr double COLLINEAR_TOLERANCE = 0.005;

		// Reject concavity: for CCW 2.5D polygons, left turns must be positive (>= -0.005).
		if ( cross2d < CONCAVE_TOLERANCE ) {
			return false;
		}
		// Simplify collinear points: if turn angle is near zero, remove intermediate vertex p2.
		if ( std::abs( cross2d ) <= COLLINEAR_TOLERANCE ) {
			int32_t remove_idx = ( j + 1 ) % simple.num_points;
			for ( int32_t k = remove_idx; k < simple.num_points - 1; k++ ) {
				simple.points[ k ] = Vector3DP( simple.points[ k + 1 ] );
			}
			simple.num_points--;
			// Restart loop to cleanly re-evaluate the modified polygon perimeter.
			j = -1;
		}
	}

	// Validate final vertex count bounds and CCW winding order.
	if ( !EnsureWindingCCW( simple ) || simple.num_points < 3 || simple.num_points > MAX_WINDING_POINTS ) {
		return false;
	}

	*out = simple;
	return true;
}


/**
*	@brief	Evaluate whether a partition fragment winding is non-degenerate and usable.
*	@param	winding	[in] Candidate fragment produced by an axis-aligned split.
*	@return	True when the fragment is a non-degenerate polygon rather than a numerical sliver.
*	@note	Rejecting micro-slivers here prevents partition edges from fanning into door corners
*			and preserves the unsplit source polygon.
**/
bool IsUsablePartitionFragment( const winding_t &winding ) {
	/**
	*	Reject fragments that cannot form a valid polygon before calculating area.
	**/
	// Check if vertex count is out of bounds.
	if ( winding.num_points < 3 || winding.num_points > MAX_WINDING_POINTS ) {
		return false;
	}

	/**
	*	Measure polygon surface area and horizontal extent bounds.
	**/
	double area = 0.0;
	double min_x = winding.points[ 0 ].x;
	double min_y = winding.points[ 0 ].y;
	double max_x = min_x;
	double max_y = min_y;

	// Loop over winding vertices to accumulate surface area and bounding extent.
	for ( int32_t i = 0; i < winding.num_points; i++ ) {
		const Vector3DP &point = winding.points[ i ];
		min_x = std::min<double>( min_x, point.x );
		min_y = std::min<double>( min_y, point.y );
		max_x = std::max<double>( max_x, point.x );
		max_y = std::max<double>( max_y, point.y );

		// Triangulate polygon relative to first vertex to compute total surface area.
		if ( i >= 2 ) {
			const Vector3DP first_edge = winding.points[ i - 1 ] - winding.points[ 0 ];
			const Vector3DP second_edge = point - winding.points[ 0 ];
			area += 0.5 * QM_Vector3LengthDP( QM_Vector3CrossProductDP( first_edge, second_edge ) );
		}
	}

	// Calculate horizontal extents along X and Y.
	const double width_x = max_x - min_x;
	const double width_y = max_y - min_y;
	const double min_extent = std::min<double>( width_x, width_y );

	// Reject fragments narrower than 4.0 units or with surface area less than 16.0 sq units.
	if ( min_extent < 4.0 || area < 16.0 ) {
		return false;
	}

	return true;
}

/**
*	@brief	Merge adjacent coplanar polygons in g_nav_polys to form maximal convex walk surfaces.
*	@details	Iteratively scans all polygon pairs in g_nav_polys and combines adjacent coplanar
*			polygons sharing edge segments into single convex polygons. This eliminates redundant CSG seams.
**/
void Nav_MergeCoplanarPolygons() {
	/**
	*	Sanity check: ensure at least 2 polygons exist to perform merging.
	**/
	if ( g_nav_polys.empty() ) {
		return;
	}

	// Print starting message for diagnostics.
	gi.dprintf( "Nav_MergeCoplanarPolygons: starting with %d polygons...\n", static_cast< int32_t >( g_nav_polys.size() ) );

	// Loop control flags.
	bool merged_any = true;
	int32_t pass_count = 0;

	/**
	*	Iteratively merge polygon pairs until no further combinations are possible.
	**/
	while ( merged_any ) {
		merged_any = false;
		pass_count++;

		// Outer loop: iterate over all polygons.
		for ( size_t i = 0; i < g_nav_polys.size(); i++ ) {
			// Skip deleted polygons marked with 0 vertices.
			if ( g_nav_polys[ i ].num_vertices == 0 ) continue;

			// Inner loop: compare against candidate polygon j.
			for ( size_t j = i + 1; j < g_nav_polys.size(); j++ ) {
				// Skip deleted candidate polygons.
				if ( g_nav_polys[ j ].num_vertices == 0 ) continue;
				// Do not merge polygons belonging to different entities!
				if ( g_nav_polys[ i ].entity_id != g_nav_polys[ j ].entity_id ) continue;

				// Plane check: ensure polygons lie on the exact same spatial plane and normal.
				if ( !AreNavPolygonsCoplanar( g_nav_polys[ i ], g_nav_polys[ j ] ) ) continue;

				// Convert polygon i to winding structure.
				winding_t w1 = {};
				w1.entity_id = g_nav_polys[ i ].entity_id;
				w1.transition_entity_id = g_nav_polys[ i ].transition_entity_id;
				w1.reserve( g_nav_polys[ i ].num_vertices );
				for ( int32_t k = 0; k < g_nav_polys[ i ].num_vertices; k++ ) {
					w1.push_back( Vector3DP( g_nav_polys[ i ].vertices[ k ] ) );
				}

				// Convert polygon j to winding structure.
				winding_t w2 = {};
				w2.entity_id = g_nav_polys[ j ].entity_id;
				w2.transition_entity_id = g_nav_polys[ j ].transition_entity_id;
				w2.reserve( g_nav_polys[ j ].num_vertices );
				for ( int32_t k = 0; k < g_nav_polys[ j ].num_vertices; k++ ) {
					w2.push_back( Vector3DP( g_nav_polys[ j ].vertices[ k ] ) );
				}


				// Attempt convex merge of the two windings.
				winding_t merged_w = {};
				if ( TryMergeWindings( w1, w2, Vector3DP( g_nav_polys[ i ].normal ), &merged_w ) ) {
					// Enforce CCW winding order on merged winding.
					EnsureWindingCCW( merged_w );
					// Update polygon i with merged winding vertices.
					g_nav_polys[ i ].num_vertices = merged_w.num_points;
					for ( int32_t k = 0; k < merged_w.num_points; k++ ) {
						g_nav_polys[ i ].vertices[ k ] = Vector3DP( merged_w.points[ k ] );
					}
					// Ensure polygon i remains valid CCW.
					EnsureNavPolygonCCW( g_nav_polys[ i ] );
					// Recompute polygon i geometric centroid.
					RecomputeNavPolygonCenter( g_nav_polys[ i ] );


					// Mark candidate polygon j as deleted (0 vertices).
					g_nav_polys[ j ].num_vertices = 0;
					// Set flag to trigger another pass.
					merged_any = true;
				}
			}
		}
	}

	/**
	*	Compact g_nav_polys vector to remove deleted polygons.
	**/
	std::vector<nav_poly_t> clean_polys;
	clean_polys.reserve( g_nav_polys.size() );
	for ( size_t k = 0; k < g_nav_polys.size(); ++k ) {
		if ( g_nav_polys[ k ].num_vertices >= 3 ) {
			clean_polys.push_back( g_nav_polys[ k ] );
		}
	}

	// Print summary diagnostic log.
	gi.dprintf( "Nav_MergeCoplanarPolygons: finished after %" PRId32 " passes, reduced from %d to %d polygons.\n",
		pass_count, static_cast< int32_t >( g_nav_polys.size() ), static_cast< int32_t >( clean_polys.size() ) );

	// Commit compacted polygon vector.
	g_nav_polys.clear();
	for ( const auto &poly : clean_polys ) {
		g_nav_polys.push_back( poly );
	}
}

/**
*	@brief	Dissolve razor-thin sliver polygons and absorb them into adjacent floor surfaces.
*	@details	Filters out narrow diagonal remnants and microscopic fragment polygons that fall below
*			minimum physical dimensions to prevent pathfinder navigation glitches.
**/
void Nav_DissolveSlivers() {
	/**
	*	Sanity check: ensure polygon list is non-empty.
	**/
	if ( g_nav_polys.empty() ) {
		return;
	}

	// Log start of sliver dissolution pass.
	gi.dprintf( "Nav_DissolveSlivers: evaluating %d polygons...\n", static_cast< int32_t >( g_nav_polys.size() ) );

	// Temporary vector to store clean surviving polygons.
	std::vector<nav_poly_t> clean_polys;
	clean_polys.reserve( g_nav_polys.size() );

	// Counter for dissolved slivers.
	int32_t dissolved_count = 0;

	// Loop over all extracted polygons.
	for ( size_t k = 0; k < g_nav_polys.size(); ++k ) {
		const auto &poly = g_nav_polys[ k ];
		// Reject degenerate polygons with fewer than 3 vertices.
		if ( poly.num_vertices < 3 ) continue;

		// Calculate 2D horizontal bounding box and surface area using nav_aabb_t.
		nav_aabb_t box;
		box.Clear();
		double area = 0.0;

		for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
			const Vector3DP &p = poly.vertices[ i ];
			box.AddPoint( p );

			if ( i >= 2 ) {
				Vector3DP e1 = Vector3DP( poly.vertices[ i - 1 ] ) - Vector3DP( poly.vertices[ 0 ] );
				Vector3DP e2 = Vector3DP( p ) - Vector3DP( poly.vertices[ 0 ] );
				area += 0.5 * QM_Vector3LengthDP( QM_Vector3CrossProductDP( e1, e2 ) );
			}
		}

		// Calculate horizontal extents along X and Y.
		double extent_x = box.maxs.x - box.mins.x;
		double extent_y = box.maxs.y - box.mins.y;
		double min_extent = std::min<double>( extent_x, extent_y );

		// Dissolve micro slivers (< 4.0 units extent or < 16.0 sq units area).
		if ( min_extent < 4.0 || area < 16.0 ) {
			dissolved_count++;
			continue;
		}


		// Keep usable polygon.
		clean_polys.push_back( poly );
	}

	// Log completion summary.
	gi.dprintf( "Nav_DissolveSlivers: dissolved %" PRId32 " slivers, remaining polygons: %d\n",
		dissolved_count, static_cast< int32_t >( clean_polys.size() ) );

	// Commit clean polygon collection.
	g_nav_polys.clear();
	for ( const auto &poly : clean_polys ) {
		g_nav_polys.push_back( poly );
	}
}

/**
*	@brief	Recursively partition polygons using obstacle-aware, localized axis-aligned split planes.
*	@tparam	PolyContainer	Container type holding nav_poly_t structures.
*	@param	polys		[in,out] Polygon collection to partition.
*	@param	mins		[in] Current bounding box minimum extents.
*	@param	maxs		[in] Current bounding box maximum extents.
*	@param	depth		[in] Current recursion depth level.
*	@note	Restricts candidate split plane coordinates to true axis-aligned edge segments.
*			Tracks candidate obstacle perpendicular extents (cand_min_perp, cand_max_perp)
*			and bypasses splitting distant non-overlapping open floor polygons.
**/
template <typename PolyContainer>
static void PartitionPolygonsRecursive( PolyContainer &polys, const Vector3DP &mins, const Vector3DP &maxs, int32_t depth ) {
	/**
	*	Sanity check: return immediately if polygon container is empty.
	**/
	if ( polys.empty() ) {
		return;
	}

	// Recursion diagnostic logging counter.
	static int32_t recurse_count = 0;
	if ( depth == 0 ) {
		recurse_count = 0;
	}
	recurse_count++;
	if ( recurse_count % 10000 == 0 ) {
		gi.dprintf( "PartitionPolygonsRecursive: count=%" PRId32 ", depth=%" PRId32 ", polys=%d, extents=(%.1f, %.1f, %.1f)\n",
			recurse_count, depth, static_cast< int32_t >( polys.size() ), maxs.x - mins.x, maxs.y - mins.y, maxs.z - mins.z );
	}

	// Compute bounding box spatial extents.
	Vector3DP extents = maxs - mins;
	// Pick the longest horizontal axis to split (0 for X, 1 for Y in 2.5D NavMesh).
	int32_t primary_axis = ( extents.y > extents.x ) ? 1 : 0;

	// Structure to track split candidates along active axis with perpendicular obstacle bounds.
	struct SplitCandidate {
		double dist = 0.0;
		double min_perp = 0.0;
		double max_perp = 0.0;
	};

	// Initialize split plane parameters.
	int32_t split_axis = -1;
	double split_dist = 0.0;
	double cand_min_perp = -1e9;
	double cand_max_perp = 1e9;

	// Perpendicular axis relative to primary split axis.
	const int32_t primary_perp_axis = 1 - primary_axis;
	std::vector<SplitCandidate> candidates;

	/**
	*	Gather candidate split coordinates along the primary axis.
	**/
	auto IntersectsDynamicDoorInterior = [&]( const int32_t axis, const double dist ) -> bool {
		for ( const auto &poly : polys ) {
			if ( poly.entity_id != ENTITYNUM_NONE || poly.transition_entity_id != ENTITYNUM_NONE ) {
				double p_min = poly.vertices[ 0 ][ axis ];
				double p_max = p_min;
				for ( int32_t k = 1; k < poly.num_vertices; k++ ) {
					p_min = std::min<double>( p_min, poly.vertices[ k ][ axis ] );
					p_max = std::max<double>( p_max, poly.vertices[ k ][ axis ] );
				}
				if ( dist > p_min + 0.5 && dist < p_max - 0.5 ) {
					return true;
				}
			}
		}
		return false;
	};

	/**
	*	Only seed split planes from true axis-aligned edge segments (std::abs(v1[axis] - v2[axis]) < 0.1)
	*	to prevent rotated/diagonal geometry from spraying arbitrary planes across open ground.
	**/
	for ( const auto &poly : polys ) {
		// Skip dynamic door transition fragments; their authored boundaries are runtime hints.
		if ( poly.entity_id != ENTITYNUM_NONE || poly.transition_entity_id != ENTITYNUM_NONE ) {
			continue;
		}

		// Calculate polygon extent along the perpendicular axis.
		double p_min = poly.vertices[ 0 ][ primary_perp_axis ];
		double p_max = p_min;
		for ( int32_t k = 1; k < poly.num_vertices; k++ ) {
			p_min = std::min<double>( p_min, poly.vertices[ k ][ primary_perp_axis ] );
			p_max = std::max<double>( p_max, poly.vertices[ k ][ primary_perp_axis ] );
		}

		for ( int32_t k = 0; k < poly.num_vertices; k++ ) {
			int32_t k_next = ( k + 1 ) % poly.num_vertices;
			const Vector3DP &v1 = poly.vertices[ k ];
			const Vector3DP &v2 = poly.vertices[ k_next ];

			// If the edge segment is strictly axis-aligned along the primary axis, record candidate split.
			if ( std::abs( v1[ primary_axis ] - v2[ primary_axis ] ) < 0.1 ) {
				const double candDist = v1[ primary_axis ];
				if ( !IntersectsDynamicDoorInterior( primary_axis, candDist ) ) {
					SplitCandidate cand = {};
					cand.dist = candDist;
					cand.min_perp = p_min;
					cand.max_perp = p_max;
					candidates.push_back( cand );
				}
			}
		}
	}

	// Sort candidate splits along primary axis coordinate.
	if ( !candidates.empty() ) {
		std::sort( candidates.begin(), candidates.end(), []( const SplitCandidate &a, const SplitCandidate &b ) {
			return a.dist < b.dist;
		} );
		size_t mid_idx = candidates.size() / 2;
		const auto &chosen = candidates[ mid_idx ];
		split_dist = chosen.dist;
		split_axis = primary_axis;
		cand_min_perp = chosen.min_perp;
		cand_max_perp = chosen.max_perp;
	} else {
		// Fallback: evaluate secondary perpendicular axis.
		const int32_t secondary_axis = primary_perp_axis;
		const int32_t secondary_perp_axis = primary_axis;

		for ( const auto &poly : polys ) {
			if ( poly.entity_id != ENTITYNUM_NONE || poly.transition_entity_id != ENTITYNUM_NONE ) {
				continue;
			}

			double p_min = poly.vertices[ 0 ][ secondary_perp_axis ];
			double p_max = p_min;
			for ( int32_t k = 1; k < poly.num_vertices; k++ ) {
				p_min = std::min<double>( p_min, poly.vertices[ k ][ secondary_perp_axis ] );
				p_max = std::max<double>( p_max, poly.vertices[ k ][ secondary_perp_axis ] );
			}

			for ( int32_t k = 0; k < poly.num_vertices; k++ ) {
				int32_t k_next = ( k + 1 ) % poly.num_vertices;
				const Vector3DP &v1 = poly.vertices[ k ];
				const Vector3DP &v2 = poly.vertices[ k_next ];

				if ( std::abs( v1[ secondary_axis ] - v2[ secondary_axis ] ) < 0.1 ) {
					const double candDist = v1[ secondary_axis ];
					if ( !IntersectsDynamicDoorInterior( secondary_axis, candDist ) ) {
						SplitCandidate cand = {};
						cand.dist = candDist;
						cand.min_perp = p_min;
						cand.max_perp = p_max;
						candidates.push_back( cand );
					}
				}
			}
		}

		if ( !candidates.empty() ) {
			std::sort( candidates.begin(), candidates.end(), []( const SplitCandidate &a, const SplitCandidate &b ) {
				return a.dist < b.dist;
			} );
			size_t mid_idx = candidates.size() / 2;
			const auto &chosen = candidates[ mid_idx ];
			split_dist = chosen.dist;
			split_axis = secondary_axis;
			cand_min_perp = chosen.min_perp;
			cand_max_perp = chosen.max_perp;
		}
	}

	// Midpoint fallback for cells exceeding max extents.
	if ( split_axis == -1 ) {
		if ( extents.x > 512.0 || extents.y > 512.0 ) {
			const double candidateMidpoint = mins[ primary_axis ] + extents[ primary_axis ] * 0.5;
			if ( !IntersectsDynamicDoorInterior( primary_axis, candidateMidpoint ) ) {
				split_axis = primary_axis;
				split_dist = candidateMidpoint;

				double local_min_perp = 1e9;
				double local_max_perp = -1e9;
				const int32_t active_perp = 1 - split_axis;
				for ( const auto &poly : polys ) {
					double p_min_ax = poly.vertices[ 0 ][ split_axis ];
					double p_max_ax = p_min_ax;
					for ( int32_t i = 1; i < poly.num_vertices; i++ ) {
						p_min_ax = std::min<double>( p_min_ax, poly.vertices[ i ][ split_axis ] );
						p_max_ax = std::max<double>( p_max_ax, poly.vertices[ i ][ split_axis ] );
					}
					if ( p_min_ax <= split_dist && p_max_ax >= split_dist ) {
						for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
							local_min_perp = std::min<double>( local_min_perp, poly.vertices[ i ][ active_perp ] );
							local_max_perp = std::max<double>( local_max_perp, poly.vertices[ i ][ active_perp ] );
						}
					}
				}
				if ( local_min_perp < local_max_perp ) {
					cand_min_perp = local_min_perp;
					cand_max_perp = local_max_perp;
				}
			}
		}
	}

	if ( split_axis == -1 ) {
		return;
	}

	// Base case: if depth limit reached, stop recursion.
	if ( depth >= 10 ) {
		return;
	}

	// Construct split plane definition.
	cm_plane_t plane = {};
	plane.normal[ split_axis ] = 1.0;
	plane.dist = split_dist;

	// Child partition polygon collections.
	PolyContainer front_polys;
	PolyContainer back_polys;

	const int32_t active_perp_axis = 1 - split_axis;

	/**
	*	Partition each polygon into child regions cleanly along the split plane.
	**/
	for ( const auto &poly : polys ) {
		// Preserve dynamic door transition fragments in child region containing polygon center.
		if ( poly.entity_id != ENTITYNUM_NONE || poly.transition_entity_id != ENTITYNUM_NONE ) {
			if ( poly.center[ split_axis ] >= split_dist ) {
				front_polys.push_back( poly );
			} else {
				back_polys.push_back( poly );
			}
			continue;
		}

		// Convert nav_poly_t to SBO winding_t for CSG plane splitting.
		winding_t w = {};
		w.entity_id = poly.entity_id;
		w.transition_entity_id = poly.transition_entity_id;
		w.reserve( poly.num_vertices );
		for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
			w.push_back( Vector3DP( poly.vertices[ i ] ) );
		}



		winding_t front_w = {};
		winding_t back_w = {};

		// Perform CSG plane split.
		SplitWinding( &w, &plane, 0.1, Vector3DP( poly.normal ), &front_w, &back_w );

		// Commit split fragments if both front and back fragments are usable non-sliver polygons.
		if ( IsUsablePartitionFragment( front_w ) && IsUsablePartitionFragment( back_w ) ) {
			front_polys.push_back( poly );
			nav_poly_t &front_poly = front_polys.back();
			front_poly.num_vertices = front_w.num_points;
			for ( int32_t i = 0; i < front_w.num_points; i++ ) {
				front_poly.vertices[ i ] = Vector3DP( front_w.points[ i ] );
			}
			RecomputeNavPolygonCenter( front_poly );

			back_polys.push_back( poly );
			nav_poly_t &back_poly = back_polys.back();
			back_poly.num_vertices = back_w.num_points;
			for ( int32_t i = 0; i < back_w.num_points; i++ ) {
				back_poly.vertices[ i ] = Vector3DP( back_w.points[ i ] );
			}
			RecomputeNavPolygonCenter( back_poly );
		} else {
			// Keep failed or boundary-only splits in child region containing polygon center.
			if ( poly.center[ split_axis ] >= split_dist ) {
				front_polys.push_back( poly );
			} else {
				back_polys.push_back( poly );
			}
		}
	}

	// Update child region bounding box extents.
	Vector3DP left_maxs = maxs;
	left_maxs[ split_axis ] = split_dist;

	Vector3DP right_mins = mins;
	right_mins[ split_axis ] = split_dist;

	// Recurse into child regions.
	PartitionPolygonsRecursive( front_polys, right_mins, maxs, depth + 1 );
	PartitionPolygonsRecursive( back_polys, mins, left_maxs, depth + 1 );

	// Reconstruct polys container from child partition leaves.
	polys.clear();
	polys.reserve( front_polys.size() + back_polys.size() );
	for ( const auto &poly : front_polys ) {
		polys.push_back( poly );
	}
	for ( const auto &poly : back_polys ) {
		polys.push_back( poly );
	}
}

/**
*	@brief	Recursively partition raw floor polygons using localized, obstacle-aware split planes.
*	@details	Top-level entry point for polygon spatial partitioning.
**/
void Nav_PartitionPolygons() {
	// Sanity check: early out if global polygon vector is empty.
	if ( g_nav_polys.empty() ) {
		return;
	}

	// Log start of polygon partitioning pass.
	gi.dprintf( "Nav_PartitionPolygons: partitioning %d polygons...\n", static_cast< int32_t >( g_nav_polys.size() ) );

	// Compute tight 3D bounding box for all extracted polygons.
	Vector3DP mins = Vector3DP( g_nav_polys[ 0 ].vertices[ 0 ] );
	Vector3DP maxs = mins;

	for ( const auto &poly : g_nav_polys ) {
		for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
			const Vector3DP &v = poly.vertices[ i ];
			for ( int32_t k = 0; k < 3; k++ ) {
				mins[ k ] = std::min<double>( mins[ k ], v[ k ] );
				maxs[ k ] = std::max<double>( maxs[ k ], v[ k ] );
			}
		}
	}

	// Execute recursive spatial partitioning.
	PartitionPolygonsRecursive( g_nav_polys, mins, maxs, 0 );

	// Log completion summary.
	gi.dprintf( "Nav_PartitionPolygons: finished, resulting in %d partitioned polygons.\n", static_cast< int32_t >( g_nav_polys.size() ) );
}
