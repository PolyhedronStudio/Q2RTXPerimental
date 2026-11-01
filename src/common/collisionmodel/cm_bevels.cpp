#include "shared/shared.h"
#include "common/bsp.h"
#include "common/collisionmodel.h"
#include "common/zone.h"

#include <cmath>
#include <vector>

//! Maximum number of winding points that can be stored during clipping.
static constexpr int32_t MAX_WINDING_POINTS = 1024;

/**
*	@brief	Temporary convex polygon used while constructing and clipping bevel windings in double precision.
*	@note	`num_points` tracks how many entries in `points` are valid.
**/
struct winding_t {
	int32_t num_points = 0;
	Vector3DP points[ MAX_WINDING_POINTS ] = {};
};

/**
*	@brief	Temporary brush side descriptor used before final zone allocation.
**/
struct temp_bevel_brushside_t {
	int32_t plane_index = 0;
	mtexinfo_t *texinfo = nullptr;
};

/**
*	@brief	Build a large double-precision base winding lying on the supplied plane.
*	@param	p	Plane used to position and orient the winding.
*	@return	A large quad centered on the plane and oriented to span the plane's surface.
*	@note	The result is intentionally oversized so later clipping operations can trim it
*			down safely against other planes.
**/
static winding_t BaseWindingForPlane( const cm_plane_t *p ) {
	/**
	*	Choose a stable reference axis that is least aligned with the plane normal.
	**/
	// Initialize a 4-point quad winding.
	winding_t w;
	w.num_points = 4;

	// Track the dominant axis of the plane normal.
	int32_t max = -1;
	double maxv = -1.0;
	// Find the dominant normal axis so we can pick a safe up vector.
	for ( int32_t i = 0; i < 3; i++ ) {
		const double v = std::fabs( static_cast<double>( p->normal[ i ] ) );
		// Record the axis with the largest absolute normal component.
		if ( v > maxv ) {
			max = i;
			maxv = v;
		}
	}

	/**
	*	Build an orthonormal basis for the plane in double precision.
	**/
	// Initialize reference up vector.
	Vector3DP up( 0.0, 0.0, 0.0 );
	// Choose a reference axis non-parallel to the dominant normal component.
	if ( max == 2 ) {
		// If the plane is mostly vertical, use X as the reference axis.
		up.x = 1.0;
	} else {
		// Otherwise use Z as the reference axis.
		up.z = 1.0;
	}

	// Copy plane normal into a Vector3DP for double-precision basis construction.
	const Vector3DP p_normal = QM_Vector3NormalizeDP( Vector3DP(
		static_cast<double>( p->normal[ 0 ] ),
		static_cast<double>( p->normal[ 1 ] ),
		static_cast<double>( p->normal[ 2 ] )
	) );

	// Cross the reference axis with the plane normal to obtain a tangent direction.
	Vector3DP right = QM_Vector3CrossProductDP( up, p_normal );
	right = QM_Vector3NormalizeDP( right );

	// Recompute the plane-up vector so the basis remains orthogonal and normalized.
	up = QM_Vector3CrossProductDP( p_normal, right );
	up = QM_Vector3NormalizeDP( up );

	/**
	*	Project a center point onto the plane and expand it into a large quad.
	**/
	// Compute origin on the plane surface.
	const Vector3DP org = p_normal * static_cast<double>( p->dist );

	// Use a very large extent so the winding fully covers the plane before clipping.
	constexpr double big_number = 99999.0;
	const Vector3DP vright = right * big_number;
	const Vector3DP vup = up * big_number;

	// Emit the four corners of the oversized quad.
	w.points[ 0 ] = org - vright + vup;
	w.points[ 1 ] = org + vright + vup;
	w.points[ 2 ] = org + vright - vup;
	w.points[ 3 ] = org - vright - vup;
	return w;
}

/**
*	@brief	Clip a winding against a splitting plane in place using double precision.
*	@param	in				[in/out] Winding to clip and replace with the surviving portion.
*	@param	split_normal	Normalized inward split plane normal.
*	@param	split_dist		Signed inward split plane distance.
*	@param	epsilon			Distance threshold used to classify vertices as front, back, or on-plane.
*	@return	`false` when the winding is entirely in front of the plane; `true` when any
*			back-side or split portion remains and `in` has been updated accordingly.
*	@note	This routine preserves back-side vertices and inserts intersection points when an
*			edge crosses the split plane.
**/
static bool ChopWindingInPlace( winding_t *in, const Vector3DP &split_normal, const double split_dist, const double epsilon ) {
	/**
	*	Classify each vertex relative to the splitting plane.
	**/
	double dists[ MAX_WINDING_POINTS + 4 ];
	int32_t sides[ MAX_WINDING_POINTS + 4 ];
	int32_t counts[ 3 ] = { 0, 0, 0 };

	// Iterate over all winding vertices to compute signed plane distances.
	for ( int32_t i = 0; i < in->num_points; i++ ) {
		// Measure signed distance from the vertex to the splitting plane.
		const double dot = QM_Vector3DotProductDP( in->points[ i ], split_normal );
		dists[ i ] = dot - split_dist;

		// Classify the vertex so later passes can decide whether to keep or split edges.
		if ( dists[ i ] > epsilon ) {
			sides[ i ] = 1; // front
		} else if ( dists[ i ] < -epsilon ) {
			sides[ i ] = 2; // back
		} else {
			sides[ i ] = 0; // on
		}
		counts[ sides[ i ] ]++;
	}

	// Mirror the first vertex so the closing edge can be processed uniformly.
	sides[ in->num_points ] = sides[ 0 ];
	dists[ in->num_points ] = dists[ 0 ];

	/**
	*	Handle trivial accept/reject cases before doing any edge splitting work.
	**/
	// If no vertices lie on the back side, the winding is clipped away.
	if ( counts[ 2 ] == 0 ) {
		// All points are on the front side, so nothing survives this clip.
		return false;
	}
	// If no vertices lie on the front side, the entire winding survives intact.
	if ( counts[ 1 ] == 0 ) {
		// All points are on the back side, so the winding is fully retained.
		return true;
	}

	/**
	*	Build the clipped output winding by copying back-side vertices and inserting splits.
	**/
	winding_t out;
	out.num_points = 0;

	// Walk each edge of the polygon and emit surviving vertices and split intersections.
	for ( int32_t i = 0; i < in->num_points; i++ ) {
		const Vector3DP p1 = in->points[ i ];

		// Keep vertices that are not strictly in front of the split plane.
		if ( sides[ i ] != 1 ) {
			if ( out.num_points < MAX_WINDING_POINTS ) {
				out.points[ out.num_points++ ] = p1;
			}
		}

		// Skip edges that do not cross the split plane.
		if ( sides[ i ] == 0 || sides[ i ] == sides[ i + 1 ] ) {
			continue;
		}
		// Skip edges whose next vertex lies directly on the split plane.
		if ( sides[ i + 1 ] == 0 ) {
			continue;
		}

		// Compute the edge intersection point against the split plane.
		const Vector3DP p2 = in->points[ ( i + 1 ) % in->num_points ];
		const double dot = dists[ i ] / ( dists[ i ] - dists[ i + 1 ] );
		Vector3DP mid;

		// Interpolate each coordinate along the split edge.
		for ( int32_t j = 0; j < 3; j++ ) {
			// Preserve axis-aligned coordinates exactly when the split plane is axis aligned.
			if ( split_normal[ j ] == 1.0 ) {
				mid[ j ] = split_dist;
			} else if ( split_normal[ j ] == -1.0 ) {
				mid[ j ] = -split_dist;
			} else {
				mid[ j ] = p1[ j ] + dot * ( p2[ j ] - p1[ j ] );
			}
		}

		// Append the new intersection vertex to the clipped winding.
		if ( out.num_points < MAX_WINDING_POINTS ) {
			out.points[ out.num_points++ ] = mid;
		}
	}

	/**
	*	Commit the clipped result back to the caller.
	**/
	*in = out;
	return ( out.num_points >= 3 );
}

/**
*	@brief	Determine whether two windings share at least one vertex within a distance tolerance.
*	@param	a			First winding to compare.
*	@param	b			Second winding to compare.
*	@param	maxDistSq	Maximum squared distance between vertices to treat the windings as adjacent.
*	@return	True if any vertex pair is within `maxDistSq`.
**/
static bool CM_WindingsShareVertex( const winding_t &a, const winding_t &b, const double maxDistSq ) {
	/**
	*	Compare all vertex pairs between the two windings in double precision.
	**/
	// Iterate over vertices of the first winding.
	for ( int32_t i = 0; i < a.num_points; i++ ) {
		const Vector3DP &pa = a.points[ i ];
		// Iterate over vertices of the second winding.
		for ( int32_t j = 0; j < b.num_points; j++ ) {
			const double distSq = QM_Vector3LengthSqrDP( pa - b.points[ j ] );
			// Return true as soon as a shared corner/edge vertex is found.
			if ( distSq <= maxDistSq ) {
				return true;
			}
		}
	}
	return false;
}

/**
*	@brief	Attempt to emit a single directed bevel plane for a brush if that normal direction
*			is not already represented on the brush.
*	@param	candidate_normal	Outward normal candidate in double precision.
*	@param	original_sides		Authored brush sides of the brush.
*	@param	numsides			Number of authored brush sides.
*	@param	brush_verts			Surviving winding vertices of the brush in double precision.
*	@param	new_brush_planes	[in/out] Newly generated bevel planes for this brush.
*	@param	new_brush_normals	[in/out] Double-precision unit normals for already generated bevel planes.
*	@param	new_brush_texinfos	[in/out] Best-matching texinfo pointers for each generated bevel plane.
**/
static void CM_TryAddBrushBevelNormal(
	const Vector3DP &candidate_normal,
	const mbrushside_t *original_sides,
	const int32_t numsides,
	const std::vector<Vector3DP> &brush_verts,
	std::vector<cm_plane_t> &new_brush_planes,
	std::vector<Vector3DP> &new_brush_normals,
	std::vector<mtexinfo_t *> &new_brush_texinfos ) {
	/**
	*	Sanity check the candidate normal and brush vertex set.
	**/
	// Require valid brush vertices to compute the support plane distance.
	if ( brush_verts.empty() || numsides <= 0 ) {
		return;
	}

	// Ensure the candidate normal is non-degenerate and normalized in double precision.
	const double len = QM_Vector3LengthDP( candidate_normal );
	if ( len < 0.1 ) {
		return;
	}
	const Vector3DP normal = QM_Vector3NormalizeDP( candidate_normal );

	/**
	*	Check whether an authored brush side already has the same outward normal direction,
	*	while simultaneously tracking the closest-oriented authored side for texinfo inheritance.
	**/
	mtexinfo_t *best_texinfo = original_sides[ 0 ].texinfo;
	double best_side_dot = -2.0;

	// Compare against every authored side using signed dot product so opposite normals are not suppressed.
	for ( int32_t s = 0; s < numsides; s++ ) {
		const cm_plane_t *p = original_sides[ s ].plane;
		const Vector3DP authored_normal(
			static_cast<double>( p->normal[ 0 ] ),
			static_cast<double>( p->normal[ 1 ] ),
			static_cast<double>( p->normal[ 2 ] )
		);
		const double dot = QM_Vector3DotProductDP( authored_normal, normal );
		// Track the authored side whose normal best aligns with this bevel normal.
		if ( dot > best_side_dot ) {
			best_side_dot = dot;
			best_texinfo = original_sides[ s ].texinfo;
		}
		// Skip only when the same outward direction already exists on the brush.
		if ( dot > 0.999 ) {
			return;
		}
	}

	/**
	*	Check whether a bevel plane with this outward normal was already added for this brush.
	**/
	// Compare against already generated bevel planes using double-precision signed dot product.
	for ( const Vector3DP &existing_normal : new_brush_normals ) {
		const double dot = QM_Vector3DotProductDP( existing_normal, normal );
		// Skip duplicate or near-duplicate outward normals.
		if ( dot > 0.999 ) {
			return;
		}
	}

	/**
	*	Compute the tightest support distance enclosing all surviving brush vertices.
	**/
	double max_dist = -999999.0;
	// Project each brush vertex onto the candidate normal in double precision.
	for ( const Vector3DP &pt : brush_verts ) {
		const double dot = QM_Vector3DotProductDP( pt, normal );
		if ( dot > max_dist ) {
			max_dist = dot;
		}
	}

	/**
	*	Construct and append the new bevel plane and its inherited surface texinfo.
	**/
	cm_plane_t p = {};
	p.normal[ 0 ] = static_cast<float>( normal.x );
	p.normal[ 1 ] = static_cast<float>( normal.y );
	p.normal[ 2 ] = static_cast<float>( normal.z );
	p.dist = static_cast<float>( max_dist );
	p.type = PLANE_NON_AXIAL;
	p.signbits = 0;

	// Populate signbits for fast box corner offset lookups.
	for ( int32_t axis = 0; axis < 3; axis++ ) {
		if ( p.normal[ axis ] < 0.0f ) {
			p.signbits |= ( 1 << axis );
		}
	}

	// Store the generated bevel plane, its double-precision normal, and matching texinfo.
	new_brush_planes.push_back( p );
	new_brush_normals.push_back( normal );
	new_brush_texinfos.push_back( best_texinfo );
}

/**
*	@brief	Generate bevel planes and side copies for solid brushes in the cached BSP.
*	@details	Allocates replacement plane/brushside arrays, copies the original BSP plane
*			data, then scans each solid brush to derive additional bevel planes from axial directions,
*			edge cross products, and adjacent-face bisectors. Non-solid brushes are copied through
*			unchanged. At the end, the BSP pointers are repointed at the generated arrays so
*			downstream collision code sees the augmented bevel data.
*	@param	cm	Collision model whose cached BSP brushes should be beveled.
*	@note	This function intentionally does not rewrite the original BSP lump counts.
*			Other systems still iterate those original indices, so only the backing pointers
*			are updated here.
**/
void CM_GenerateBrushBevels( cm_t *cm ) {
	/**
	*	Sanity check: we need a valid collision model and cached BSP before generating bevel data.
	**/
	if ( !cm || !cm->cache ) {
		return;
	}

	bsp_t *bsp = cm->cache;

	/**
	*	Reuse bevel tables already attached to this cached BSP. Multiple collision-model
	*	users can share one BSP cache entry, so generation must occur only once.
	**/
	if ( bsp->bevel_planes != nullptr || bsp->bevel_brushsides != nullptr ) {
		Q_assert( bsp->bevel_planes != nullptr && bsp->bevel_brushsides != nullptr );
		bsp->planes = bsp->bevel_planes;
		bsp->brushsides = bsp->bevel_brushsides;
		return;
	}

	/**
	*	Retain the authored brush ranges before runtime bevel sides replace them.
	**/
	// Save each brush's original firstbrushside and numsides pointers.
	for ( int32_t i = 0; i < bsp->numbrushes; i++ ) {
		mbrush_t *brush = &bsp->brushes[ i ];
		if ( brush->authored_firstbrushside == nullptr ) {
			brush->authored_firstbrushside = brush->firstbrushside;
			brush->authored_numsides = brush->numsides;
		}
	}
	// Save the authored planes pointer on the BSP.
	if ( bsp->authored_planes == nullptr ) {
		bsp->authored_planes = bsp->planes;
	}

	/**
	*	Prepare dynamic staging buffers so bevel planes and brushsides are never truncated.
	**/
	std::vector<cm_plane_t> staged_planes;
	staged_planes.reserve( static_cast<size_t>( bsp->numplanes ) + static_cast<size_t>( bsp->numbrushes ) * 32 );
	staged_planes.insert( staged_planes.end(), bsp->authored_planes, bsp->authored_planes + bsp->numplanes );

	std::vector<temp_bevel_brushside_t> staged_sides;
	staged_sides.reserve( static_cast<size_t>( bsp->numbrushsides ) + static_cast<size_t>( bsp->numbrushes ) * 32 );

	std::vector<int32_t> brush_first_side( static_cast<size_t>( bsp->numbrushes ), 0 );
	std::vector<int32_t> brush_num_sides( static_cast<size_t>( bsp->numbrushes ), 0 );

	std::vector<winding_t> windings;
	std::vector<bool> valid_windings;
	std::vector<Vector3DP> brush_verts;
	std::vector<Vector3DP> edges;
	std::vector<cm_plane_t> new_brush_planes;
	std::vector<Vector3DP> new_brush_normals;
	std::vector<mtexinfo_t *> new_brush_texinfos;

	/**
	*	Process each brush in turn.
	*	Non-solid brushes are copied through unchanged; solid brushes are analyzed
	*	to derive extra bevel planes that keep box and capsule collision stable.
	**/
	for ( int32_t i = 0; i < bsp->numbrushes; i++ ) {
		mbrush_t *b = &bsp->brushes[ i ];
		const mbrushside_t *original_sides = b->authored_firstbrushside;
		const int32_t original_num_sides = b->authored_numsides;

		// Non-solid brushes do not need bevel generation, but their authored sides still need to be copied over.
		if ( !( b->contents & CONTENTS_SOLID ) || original_num_sides <= 0 ) {
			brush_first_side[ i ] = static_cast<int32_t>( staged_sides.size() );
			brush_num_sides[ i ] = original_num_sides;

			// Copy the brush's authored sides into the staged side array.
			for ( int32_t j = 0; j < original_num_sides; j++ ) {
				temp_bevel_brushside_t side_desc;
				side_desc.plane_index = static_cast<int32_t>( original_sides[ j ].plane - bsp->authored_planes );
				side_desc.texinfo = original_sides[ j ].texinfo;
				staged_sides.push_back( side_desc );
			}
			continue;
		}

		/**
		*	Build clipped windings for each brush side in double precision.
		*	These windings are later used to infer brush vertices, edge directions,
		*	and adjacent-face normal bisectors.
		**/
		const int32_t numsides = original_num_sides;
		windings.resize( static_cast<size_t>( numsides ) );
		valid_windings.assign( static_cast<size_t>( numsides ), false );

		// Generate a clipped polygon for each authored side of the brush.
		for ( int32_t j = 0; j < numsides; j++ ) {
			const mbrushside_t *s = &original_sides[ j ];
			windings[ j ] = BaseWindingForPlane( s->plane );
			valid_windings[ j ] = true;

			const Vector3DP s_normal(
				static_cast<double>( s->plane->normal[ 0 ] ),
				static_cast<double>( s->plane->normal[ 1 ] ),
				static_cast<double>( s->plane->normal[ 2 ] )
			);

			/**
			*	Clip each side's winding against every other side of the same brush.
			*	This leaves only the polygon portion that is actually on the brush boundary.
			**/
			for ( int32_t k = 0; k < numsides && valid_windings[ j ]; k++ ) {
				if ( j == k ) {
					continue;
				}

				const mbrushside_t *s2 = &original_sides[ k ];
				const Vector3DP s2_normal(
					static_cast<double>( s2->plane->normal[ 0 ] ),
					static_cast<double>( s2->plane->normal[ 1 ] ),
					static_cast<double>( s2->plane->normal[ 2 ] )
				);

				// Guard against duplicate or parallel planes in the authored brush sides.
				const double plane_dot = QM_Vector3DotProductDP( s_normal, s2_normal );
				if ( plane_dot > 0.9999 ) {
					const double dist_diff = static_cast<double>( s->plane->dist ) - static_cast<double>( s2->plane->dist );
					if ( std::fabs( dist_diff ) <= 0.01 ) {
						// Coplanar duplicate side: keep the first index and discard later duplicates.
						if ( j > k ) {
							valid_windings[ j ] = false;
							break;
						}
						continue;
					} else if ( dist_diff < -0.01 ) {
						// Side j is strictly inside parallel side k, so side k cannot clip side j.
						continue;
					} else {
						// Side j is strictly outside parallel side k, so side j is redundant.
						valid_windings[ j ] = false;
						break;
					}
				}

				// Convert the opposing side into an inward-facing split plane in double precision.
				const Vector3DP split_normal = -s2_normal;
				const double split_dist = -static_cast<double>( s2->plane->dist );

				// Discard the winding if it cannot survive clipping against a neighboring face.
				if ( !ChopWindingInPlace( &windings[ j ], split_normal, split_dist, 0.1 ) ) {
					valid_windings[ j ] = false;
				}
			}
		}

		/**
		*	Collect unique surviving brush vertices and check whether the brush is purely axial.
		*	Purely axial brushes keep only axial planes so they continue to take the fast analytical
		*	sweep path (`CM_SweepShapeVsAxialBox`).
		**/
		brush_verts.clear();
		for ( int32_t j = 0; j < numsides; j++ ) {
			if ( !valid_windings[ j ] ) {
				continue;
			}
			const winding_t &w = windings[ j ];
			// Deduplicate vertices within a small spatial tolerance.
			for ( int32_t w_idx = 0; w_idx < w.num_points; w_idx++ ) {
				const Vector3DP &pt = w.points[ w_idx ];
				bool exists = false;
				for ( const Vector3DP &existing : brush_verts ) {
					if ( QM_Vector3LengthSqrDP( pt - existing ) <= 0.01 ) {
						exists = true;
						break;
					}
				}
				if ( !exists ) {
					brush_verts.push_back( pt );
				}
			}
		}

		// Determine if every authored side of this brush is axis-aligned.
		bool is_axial_brush = true;
		for ( int32_t j = 0; j < numsides; j++ ) {
			const cm_plane_t *p = original_sides[ j ].plane;
			const double nx = std::fabs( static_cast<double>( p->normal[ 0 ] ) );
			const double ny = std::fabs( static_cast<double>( p->normal[ 1 ] ) );
			const double nz = std::fabs( static_cast<double>( p->normal[ 2 ] ) );
			const bool is_axial_plane =
				( nx > 0.9999 && ny < 0.0001 && nz < 0.0001 ) ||
				( ny > 0.9999 && nx < 0.0001 && nz < 0.0001 ) ||
				( nz > 0.9999 && nx < 0.0001 && ny < 0.0001 );
			if ( !is_axial_plane ) {
				is_axial_brush = false;
				break;
			}
		}

		new_brush_planes.clear();
		new_brush_normals.clear();
		new_brush_texinfos.clear();

		/**
		*	Always ensure all 6 axial bounding planes exist on the brush.
		*	Testing each signed direction independently guarantees that an existing `-N` side
		*	never suppresses a needed `+N` bevel on the opposite side of the brush.
		**/
		static constexpr Vector3DP axial_normals[ 6 ] = {
			Vector3DP( 1.0, 0.0, 0.0 ),
			Vector3DP( -1.0, 0.0, 0.0 ),
			Vector3DP( 0.0, 1.0, 0.0 ),
			Vector3DP( 0.0, -1.0, 0.0 ),
			Vector3DP( 0.0, 0.0, 1.0 ),
			Vector3DP( 0.0, 0.0, -1.0 )
		};
		for ( int32_t a = 0; a < 6; a++ ) {
			CM_TryAddBrushBevelNormal( axial_normals[ a ], original_sides, numsides, brush_verts, new_brush_planes, new_brush_normals, new_brush_texinfos );
		}

		/**
		*	For non-axial (angled/curved) brushes, generate edge-cross bevels, 45-degree XY
		*	diagonal bevels, and adjacent-face normal bisectors so rounded shapes (capsules,
		*	cylinders, spheres) do not encounter sharp miter spikes at angled seams.
		**/
		if ( !is_axial_brush && !brush_verts.empty() ) {
			// Extract unique edge directions from all surviving windings in double precision.
			edges.clear();
			for ( int32_t j = 0; j < numsides; j++ ) {
				if ( !valid_windings[ j ] ) {
					continue;
				}

				const winding_t &w = windings[ j ];
				for ( int32_t k = 0; k < w.num_points; k++ ) {
					const Vector3DP &p1 = w.points[ k ];
					const Vector3DP &p2 = w.points[ ( k + 1 ) % w.num_points ];
					Vector3DP dir = p2 - p1;
					const double edge_len = QM_Vector3LengthDP( dir );
					// Skip degenerate micro-edges.
					if ( edge_len < 0.2 ) {
						continue;
					}
					dir = QM_Vector3NormalizeDP( dir );

					// Avoid storing duplicate or opposite edge directions.
					bool found = false;
					for ( const Vector3DP &existing_edge : edges ) {
						const double dot = QM_Vector3DotProductDP( existing_edge, dir );
						if ( std::fabs( dot ) > 0.999 ) {
							found = true;
							break;
						}
					}

					if ( !found ) {
						edges.push_back( dir );
					}
				}
			}

			// Cross every unique brush edge with the three world axes (and test both signs independently).
			static constexpr Vector3DP axial_axes[ 3 ] = {
				Vector3DP( 1.0, 0.0, 0.0 ),
				Vector3DP( 0.0, 1.0, 0.0 ),
				Vector3DP( 0.0, 0.0, 1.0 )
			};
			for ( const Vector3DP &edge_dir : edges ) {
				for ( int32_t a = 0; a < 3; a++ ) {
					const Vector3DP cross = QM_Vector3CrossProductDP( edge_dir, axial_axes[ a ] );
					const double cross_len = QM_Vector3LengthDP( cross );
					if ( cross_len < 0.1 ) {
						continue;
					}
					const Vector3DP unit_cross = QM_Vector3NormalizeDP( cross );
					CM_TryAddBrushBevelNormal( unit_cross, original_sides, numsides, brush_verts, new_brush_planes, new_brush_normals, new_brush_texinfos );
					CM_TryAddBrushBevelNormal( -unit_cross, original_sides, numsides, brush_verts, new_brush_planes, new_brush_normals, new_brush_texinfos );
				}
			}

			// Add the 4 horizontal 45-degree diagonal normals to cap acute XY corners.
			constexpr double inv_sqrt2 = 0.70710678118654752440;
			static constexpr Vector3DP diag_xy_normals[ 4 ] = {
				Vector3DP( inv_sqrt2, inv_sqrt2, 0.0 ),
				Vector3DP( inv_sqrt2, -inv_sqrt2, 0.0 ),
				Vector3DP( -inv_sqrt2, inv_sqrt2, 0.0 ),
				Vector3DP( -inv_sqrt2, -inv_sqrt2, 0.0 )
			};
			for ( int32_t d = 0; d < 4; d++ ) {
				CM_TryAddBrushBevelNormal( diag_xy_normals[ d ], original_sides, numsides, brush_verts, new_brush_planes, new_brush_normals, new_brush_texinfos );
			}

			// For sloped faces, add their horizontal XY normal projection to bevel horizontal wedge tips.
			for ( int32_t j = 0; j < numsides; j++ ) {
				if ( !valid_windings[ j ] ) {
					continue;
				}
				const cm_plane_t *p = original_sides[ j ].plane;
				const Vector3DP n_horiz( static_cast<double>( p->normal[ 0 ] ), static_cast<double>( p->normal[ 1 ] ), 0.0 );
				const double horiz_len = QM_Vector3LengthDP( n_horiz );
				if ( std::fabs( static_cast<double>( p->normal[ 2 ] ) ) > 0.01 && horiz_len > 0.1 ) {
					CM_TryAddBrushBevelNormal( QM_Vector3NormalizeDP( n_horiz ), original_sides, numsides, brush_verts, new_brush_planes, new_brush_normals, new_brush_texinfos );
				}
			}

			// Generate normal bisectors (and sub-bisectors for sharper corners) between adjacent brush faces.
			constexpr double shared_vertex_dist_sq = 1.0;
			for ( int32_t j = 0; j < numsides; j++ ) {
				if ( !valid_windings[ j ] ) {
					continue;
				}
				const Vector3DP nj = QM_Vector3NormalizeDP( Vector3DP(
					static_cast<double>( original_sides[ j ].plane->normal[ 0 ] ),
					static_cast<double>( original_sides[ j ].plane->normal[ 1 ] ),
					static_cast<double>( original_sides[ j ].plane->normal[ 2 ] )
				) );

				for ( int32_t k = j + 1; k < numsides; k++ ) {
					if ( !valid_windings[ k ] ) {
						continue;
					}
					// Only bevel corners/edges between faces that actually touch on the brush.
					if ( !CM_WindingsShareVertex( windings[ j ], windings[ k ], shared_vertex_dist_sq ) ) {
						continue;
					}

					const Vector3DP nk = QM_Vector3NormalizeDP( Vector3DP(
						static_cast<double>( original_sides[ k ].plane->normal[ 0 ] ),
						static_cast<double>( original_sides[ k ].plane->normal[ 1 ] ),
						static_cast<double>( original_sides[ k ].plane->normal[ 2 ] )
					) );
					const double face_dot = QM_Vector3DotProductDP( nj, nk );
					// Skip nearly coplanar faces.
					if ( face_dot > 0.995 ) {
						continue;
					}

					const Vector3DP sum = nj + nk;
					if ( QM_Vector3LengthDP( sum ) > 0.1 ) {
						const Vector3DP bisector = QM_Vector3NormalizeDP( sum );
						CM_TryAddBrushBevelNormal( bisector, original_sides, numsides, brush_verts, new_brush_planes, new_brush_normals, new_brush_texinfos );

						// For corners sharper than 45 degrees, add secondary sub-bisectors to keep capsule radial error minimal.
						if ( face_dot < inv_sqrt2 ) {
							const Vector3DP sub1 = nj + bisector;
							if ( QM_Vector3LengthDP( sub1 ) > 0.1 ) {
								CM_TryAddBrushBevelNormal( QM_Vector3NormalizeDP( sub1 ), original_sides, numsides, brush_verts, new_brush_planes, new_brush_normals, new_brush_texinfos );
							}
							const Vector3DP sub2 = nk + bisector;
							if ( QM_Vector3LengthDP( sub2 ) > 0.1 ) {
								CM_TryAddBrushBevelNormal( QM_Vector3NormalizeDP( sub2 ), original_sides, numsides, brush_verts, new_brush_planes, new_brush_normals, new_brush_texinfos );
							}
						}

						// Also add the horizontal XY projection of tilted bisectors for lateral capsule sweeps.
						const Vector3DP bisector_xy( bisector.x, bisector.y, 0.0 );
						if ( std::fabs( bisector.z ) > 0.01 && QM_Vector3LengthDP( bisector_xy ) > 0.1 ) {
							CM_TryAddBrushBevelNormal( QM_Vector3NormalizeDP( bisector_xy ), original_sides, numsides, brush_verts, new_brush_planes, new_brush_normals, new_brush_texinfos );
						}
					}
				}
			}
		}

		/**
		*	Stage the authored brush sides followed by the newly generated bevel sides.
		**/
		brush_first_side[ i ] = static_cast<int32_t>( staged_sides.size() );
		brush_num_sides[ i ] = numsides + static_cast<int32_t>( new_brush_planes.size() );

		// Copy the brush's authored sides first so the brush retains its source planes in front.
		for ( int32_t j = 0; j < numsides; j++ ) {
			temp_bevel_brushside_t side_desc;
			side_desc.plane_index = static_cast<int32_t>( original_sides[ j ].plane - bsp->authored_planes );
			side_desc.texinfo = original_sides[ j ].texinfo;
			staged_sides.push_back( side_desc );
		}

		// Append the generated bevel sides to the staged arrays.
		for ( size_t j = 0; j < new_brush_planes.size(); j++ ) {
			const int32_t new_plane_idx = static_cast<int32_t>( staged_planes.size() );
			staged_planes.push_back( new_brush_planes[ j ] );

			temp_bevel_brushside_t side_desc;
			side_desc.plane_index = new_plane_idx;
			side_desc.texinfo = new_brush_texinfos[ j ];
			staged_sides.push_back( side_desc );
		}
	}

	/**
	*	Allocate exact-sized bevel arrays in the collision model zone and commit staged data.
	**/
	const size_t total_planes = staged_planes.size();
	const size_t total_sides = staged_sides.size();

	bsp->bevel_planes = static_cast<cm_plane_t *>( Z_TagMallocz( total_planes * sizeof( cm_plane_t ), TAG_CMODEL ) );
	bsp->bevel_brushsides = static_cast<mbrushside_t *>( Z_TagMallocz( total_sides * sizeof( mbrushside_t ), TAG_CMODEL ) );

	// Copy all authored and bevel planes into the zone buffer.
	memcpy( bsp->bevel_planes, staged_planes.data(), total_planes * sizeof( cm_plane_t ) );

	// Populate the final brushsides with resolved plane pointers.
	for ( size_t s = 0; s < total_sides; s++ ) {
		bsp->bevel_brushsides[ s ].plane = &bsp->bevel_planes[ staged_sides[ s ].plane_index ];
		bsp->bevel_brushsides[ s ].texinfo = staged_sides[ s ].texinfo;
	}

	// Update each brush's side range to point into the newly allocated brushside table.
	for ( int32_t i = 0; i < bsp->numbrushes; i++ ) {
		mbrush_t *b = &bsp->brushes[ i ];
		b->firstbrushside = &bsp->bevel_brushsides[ brush_first_side[ i ] ];
		b->numsides = brush_num_sides[ i ];
	}

	// Repoint the BSP tables so downstream collision queries use the generated bevel arrays.
	bsp->planes = bsp->bevel_planes;
	bsp->brushsides = bsp->bevel_brushsides;
}
