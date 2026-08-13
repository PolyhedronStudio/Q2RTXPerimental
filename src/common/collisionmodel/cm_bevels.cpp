#include "shared/shared.h"
#include "common/bsp.h"
#include "common/collisionmodel.h"
#include "common/zone.h"

// Maximum number of winding points that can be stored during clipping.
static constexpr int32_t MAX_WINDING_POINTS = 1024;

/**
*	@brief	Temporary convex polygon used while constructing and clipping bevel windings.
*	@note	`num_points` tracks how many entries in `points` are valid.
**/
struct winding_t {
	int32_t num_points = 0;
	Vector3 points[ MAX_WINDING_POINTS ] = {};
};

/**
*	@brief	Build a large base winding lying on the supplied plane.
*	@param	p	Plane used to position and orient the winding.
*	@return	A large quad centered on the plane and oriented to span the plane's surface.
*	@note	The result is intentionally oversized so later clipping operations can trim it
*			down safely against other planes.
**/
static winding_t BaseWindingForPlane( const cm_plane_t *p ) {
	/**
	*	Choose a stable reference axis that is least aligned with the plane normal.
	**/
	winding_t w;
	w.num_points = 4;

	int32_t max = -1;
	float maxv = -1.0f;
	for ( int32_t i = 0; i < 3; i++ ) {
		// Find the dominant normal axis so we can pick a safe up vector.
		float v = fabs( p->normal[ i ] );
		if ( v > maxv ) {
			max = i;
			maxv = v;
		}
	}

	/**
	*	Build an orthonormal basis for the plane.
	**/
	Vector3 up( 0.0f, 0.0f, 0.0f );
	if ( max == 2 ) {
		// If the plane is mostly vertical, use X as the reference axis.
		up.x = 1.0f;
	} else {
		// Otherwise use Z as the reference axis.
		up.z = 1.0f;
	}

	Vector3 p_normal( p->normal[ 0 ], p->normal[ 1 ], p->normal[ 2 ] );

	// Cross the reference axis with the plane normal to obtain a tangent direction.
	Vector3 right = QM_Vector3CrossProduct( up, p_normal );
	right = QM_Vector3Normalize( right );

	// Recompute the plane-up vector so the basis remains orthogonal and normalized.
	up = QM_Vector3CrossProduct( p_normal, right );
	up = QM_Vector3Normalize( up );

	/**
	*	Project a center point onto the plane and expand it into a large quad.
	**/
	Vector3 org = p_normal * p->dist;

	// Use a very large extent so the winding fully covers the plane before clipping.
	const float big_number = 99999.0f;
	Vector3 vright = right * big_number;
	Vector3 vup = up * big_number;

	// Emit the four corners of the oversized quad.
	w.points[ 0 ] = org - vright + vup;
	w.points[ 1 ] = org + vright + vup;
	w.points[ 2 ] = org + vright - vup;
	w.points[ 3 ] = org - vright - vup;
	return w;
}

/**
*	@brief	Clip a winding against a plane in place.
*	@param	in		[in/out] Winding to clip and replace with the surviving portion.
*	@param	split	Plane used to split the winding.
*	@param	epsilon	Distance threshold used to classify vertices as front, back, or on-plane.
*	@return	`false` when the winding is entirely in front of the plane; `true` when any
*			back-side or split portion remains and `in` has been updated accordingly.
*	@note	This routine preserves back-side vertices and inserts intersection points when an
*			edge crosses the split plane.
**/
static bool ChopWindingInPlace( winding_t *in, const cm_plane_t *split, float epsilon ) {
	/**
	*	Classify each vertex relative to the splitting plane.
	**/
	float dists[ MAX_WINDING_POINTS + 4 ];
	int32_t sides[ MAX_WINDING_POINTS + 4 ];
	int32_t counts[ 3 ] = { 0, 0, 0 };

	for ( int32_t i = 0; i < in->num_points; i++ ) {
		// Measure signed distance from the vertex to the splitting plane.
		float dot = in->points[ i ].x * split->normal[ 0 ] +
			in->points[ i ].y * split->normal[ 1 ] +
			in->points[ i ].z * split->normal[ 2 ];
		dists[ i ] = dot - split->dist;

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
	if ( counts[ 2 ] == 0 ) {
		// All points are on the front side, so nothing survives this clip.
		return false;
	}
	if ( counts[ 1 ] == 0 ) {
		// All points are on the back side, so the winding is fully retained.
		return true;
	}

	/**
	*	Build the clipped output winding by copying back-side vertices and inserting splits.
	**/
	winding_t out;
	out.num_points = 0;

	for ( int32_t i = 0; i < in->num_points; i++ ) {
		Vector3 p1 = in->points[ i ];

		// Keep vertices that are not strictly in front of the split plane.
		if ( sides[ i ] != 1 ) {
			out.points[ out.num_points++ ] = p1;
		}

		// Skip edges that do not cross the split plane.
		if ( sides[ i ] == 0 || sides[ i ] == sides[ i + 1 ] ) {
			continue;
		}
		if ( sides[ i + 1 ] == 0 ) {
			continue;
		}

		// Compute the edge intersection point against the split plane.
		Vector3 p2 = in->points[ ( i + 1 ) % in->num_points ];
		float dot = dists[ i ] / ( dists[ i ] - dists[ i + 1 ] );
		Vector3 mid;

		for ( int32_t j = 0; j < 3; j++ ) {
			// Preserve axis-aligned coordinates exactly when the split plane is axis aligned.
			if ( split->normal[ j ] == 1 ) {
				mid[ j ] = split->dist;
			} else if ( split->normal[ j ] == -1 ) {
				mid[ j ] = -split->dist;
			} else {
				mid[ j ] = p1[ j ] + dot * ( p2[ j ] - p1[ j ] );
			}
		}

		// Append the new intersection vertex to the clipped winding.
		out.points[ out.num_points++ ] = mid;
	}

	/**
	*	Commit the clipped result back to the caller.
	**/
	*in = out;
	return true;
}

/**
*	@brief	Generate bevel planes and side copies for solid brushes in the cached BSP.
*	@details	Allocates replacement plane/brushside arrays, copies the original BSP plane
*			data, then scans each solid brush to derive additional bevel planes from edge
*			directions. Non-solid brushes are copied through unchanged. At the end, the BSP
*			pointers are repointed at the generated arrays so downstream collision code sees
*			the augmented bevel data.
*	@note	This function intentionally does not rewrite the original BSP lump counts.
*			Other systems still iterate those original indices, so only the backing pointers
*			are updated here.
**/
void CM_GenerateBrushBevels( cm_t *cm ) {
	// Sanity check: we need a valid collision model and cached BSP before generating bevel data.
	if ( !cm || !cm->cache ) {
		return;
	}

	bsp_t *bsp = cm->cache;

	/**
	* Reuse bevel tables already attached to this cached BSP. Multiple collision-model
	* users can share one BSP cache entry, so generation must occur only once.
	**/
	if ( bsp->bevel_planes != nullptr || bsp->bevel_brushsides != nullptr ) {
		Q_assert( bsp->bevel_planes != nullptr && bsp->bevel_brushsides != nullptr );
		bsp->planes = bsp->bevel_planes;
		bsp->brushsides = bsp->bevel_brushsides;
		return;
	}

	/**
	* Retain the authored brush ranges before runtime bevel sides replace them.
	**/
	for ( int32_t i = 0; i < bsp->numbrushes; i++ ) {
		mbrush_t *brush = &bsp->brushes[ i ];
		if ( brush->authored_firstbrushside == nullptr ) {
			brush->authored_firstbrushside = brush->firstbrushside;
			brush->authored_numsides = brush->numsides;
		}
	}
	if ( bsp->authored_planes == nullptr ) {
		bsp->authored_planes = bsp->planes;
	}

	/**
	*	Estimate worst-case storage so we can allocate once up-front.
	*	Each brush may contribute a large number of bevel planes, so we reserve a
	*	conservative fixed upper bound per brush to avoid repeated reallocations.
	**/
	int32_t max_new_sides = bsp->numbrushsides + ( bsp->numbrushes * 128 );
	int32_t max_new_planes = bsp->numplanes + ( bsp->numbrushes * 128 );

	// Allocate the generated brushside and plane arrays from the collision-model zone.
	bsp->bevel_brushsides = static_cast<mbrushside_t *>( Z_TagMallocz( max_new_sides * sizeof( mbrushside_t ), TAG_CMODEL ) );
	bsp->bevel_planes = static_cast<cm_plane_t *>( Z_TagMallocz( max_new_planes * sizeof( cm_plane_t ), TAG_CMODEL ) );

	// Allocate the large windings array on the heap to avoid a stack overflow.
	winding_t *windings = new winding_t[128];

	// Preserve the original plane table as the base of the new plane array.
	memcpy( bsp->bevel_planes, bsp->authored_planes, bsp->numplanes * sizeof( cm_plane_t ) );
	int32_t num_planes = bsp->numplanes;

	// Track the next write position inside the generated brushside array.
	int32_t current_side = 0;

	/**
	*	Process each brush in turn.
	*	Non-solid brushes are copied through unchanged; solid brushes are analyzed
	*	to derive extra bevel planes that help collision handling remain stable.
	**/
	for ( int32_t i = 0; i < bsp->numbrushes; i++ ) {
		mbrush_t *b = &bsp->brushes[ i ];
		mbrushside_t *original_sides = b->authored_firstbrushside;
		const int32_t original_num_sides = b->authored_numsides;

		// Non-solid brushes do not need bevel generation, but their authored sides still need to be copied over.
		if ( !( b->contents & CONTENTS_SOLID ) ) {
			// Point the brush at the next generated side range.
			b->firstbrushside = &bsp->bevel_brushsides[ current_side ];
			b->numsides = original_num_sides;

			// Copy the brush's authored sides into the generated side array.
			for ( int32_t j = 0; j < original_num_sides; j++ ) {
				bsp->bevel_brushsides[ current_side + j ] = original_sides[ j ];
				const int32_t plane_idx = static_cast< int32_t >( original_sides[ j ].plane - bsp->authored_planes );
				bsp->bevel_brushsides[ current_side + j ].plane = &bsp->bevel_planes[ plane_idx ];
			}

			// Advance past the copied non-solid brush sides.
			current_side += original_num_sides;
			continue;
		}

		/**
		*	Build clipped windings for each brush side.
		*	These windings are later used to infer edge directions and candidate bevel planes.
		**/
		bool valid_windings[128] = { false };

		// Clamp the authored working side count to our fixed local buffer size for safety.
		int32_t numsides = original_num_sides;
		if ( numsides > 128 ) {
			numsides = 128;
		}

		// Generate a clipped polygon for each authored side of the brush.
		for ( int32_t j = 0; j < numsides; j++ ) {
			mbrushside_t *s = &original_sides[ j ];
			windings[j] = BaseWindingForPlane( s->plane );
			valid_windings[j] = true;

			/**
			*	Clip each side's winding against every other side of the same brush.
			*	This leaves only the polygon portion that is actually inside the solid volume.
			**/
			for ( int32_t k = 0; k < numsides && valid_windings[j]; k++ ) {
				if ( j == k ) {
					continue;
				}

				mbrushside_t *s2 = &original_sides[ k ];

				// Convert the opposing side into an inward-facing split plane.
				cm_plane_t splitPlane;
				splitPlane.normal[0] = -s2->plane->normal[0];
				splitPlane.normal[1] = -s2->plane->normal[1];
				splitPlane.normal[2] = -s2->plane->normal[2];
				splitPlane.dist = -s2->plane->dist;

				// Discard the winding if it cannot survive clipping against a neighboring face.
				if ( !ChopWindingInPlace( &windings[j], &splitPlane, 0.1f ) ) {
					valid_windings[j] = false;
				}
			}
		}

		/**
		*	Extract unique edge directions from all surviving windings.
		*	These edge vectors form the basis for cross-product plane candidates.
		**/
		Vector3 edges[512];
		int32_t num_edges = 0;

		// Walk every surviving polygon edge and collect normalized directions.
		for ( int32_t j = 0; j < numsides; j++ ) {
			if ( !valid_windings[j] ) {
				continue;
			}

			winding_t *w = &windings[j];
			for ( int32_t k = 0; k < w->num_points; k++ ) {
				Vector3 p1 = w->points[k];
				Vector3 p2 = w->points[( k + 1 ) % w->num_points];
				Vector3 dir = p2 - p1;
				dir = QM_Vector3Normalize( dir );

				// Skip degenerate edges that do not provide meaningful bevel directions.
				if ( dir.x * dir.x + dir.y * dir.y + dir.z * dir.z < 0.9f ) {
					continue;
				}

				// Avoid storing duplicate edge directions, including reversed equivalents.
				bool found = false;
				for ( int32_t e = 0; e < num_edges; e++ ) {
					float dot = edges[e].x * dir.x + edges[e].y * dir.y + edges[e].z * dir.z;
					if ( fabs( dot ) > 0.999f ) {
						found = true;
						break;
					}
				}

				// Append the new edge direction if it is unique and we still have capacity.
				if ( !found && num_edges < 512 ) {
					edges[num_edges++] = dir;
				}
			}
		}

		/**
		*	Ensure the axial directions are represented as bevel candidates.
		*	This helps preserve collision stability for axis-aligned geometry.
		**/
		Vector3 axial_edges[3] = {
			Vector3( 1, 0, 0 ),
			Vector3( 0, 1, 0 ),
			Vector3( 0, 0, 1 )
		};

		// Add missing axial directions if they were not already present in the edge set.
		for ( int32_t a = 0; a < 3; a++ ) {
			bool found = false;
			for ( int32_t e = 0; e < num_edges; e++ ) {
				float dot = edges[e].x * axial_edges[a].x + edges[e].y * axial_edges[a].y + edges[e].z * axial_edges[a].z;
				if ( fabs( dot ) > 0.999f ) {
					found = true;
					break;
				}
			}

			// Append the axial direction when it is still absent.
			if ( !found && num_edges < 512 ) {
				edges[num_edges++] = axial_edges[a];
			}
		}

		/**
		*	Generate candidate bevel planes by crossing pairs of edge directions.
		*	Only planes that do not already exist on the brush are kept.
		**/
		cm_plane_t new_brush_planes[512];
		int32_t num_new_brush_planes = 0;

		// Cross every unique edge pair to produce a normal candidate.
		for ( int32_t j = 0; j < num_edges; j++ ) {
			for ( int32_t k = j + 1; k < num_edges; k++ ) {
				Vector3 normal = QM_Vector3CrossProduct( edges[j], edges[k] );
				float len = QM_Vector3Length( normal );
				if ( len < 0.1f ) {
					continue;
				}
				normal.x /= len;
				normal.y /= len;
				normal.z /= len;

				// Skip planes that already match an existing brush side normal.
				bool has_plane = false;
				for ( int32_t s = 0; s < numsides; s++ ) {
					cm_plane_t *p = original_sides[ s ].plane;
					float dot = p->normal[0] * normal.x + p->normal[1] * normal.y + p->normal[2] * normal.z;
					if ( fabs( dot ) > 0.999f ) {
						has_plane = true;
						break;
					}
				}

				// Also skip candidates that duplicate a bevel plane already built for this brush.
				for ( int32_t np = 0; np < num_new_brush_planes; np++ ) {
					cm_plane_t *p = &new_brush_planes[np];
					float dot = p->normal[0] * normal.x + p->normal[1] * normal.y + p->normal[2] * normal.z;
					if ( fabs( dot ) > 0.999f ) {
						has_plane = true;
						break;
					}
				}

				// Duplicate or near-duplicate normals are not useful, so discard them.
				if ( has_plane ) {
					continue;
				}

				/**
				*	Choose the plane distance so the bevel tightly encloses the brush.
				*	The outward-facing plane uses the maximum projection of all brush vertices,
				*	while the opposite-facing plane uses the minimum projection.
				**/
				float max_dist_pos = -99999.0f;
				float min_dist_neg = 99999.0f;
				bool has_points = false;

				// Scan all surviving winding points to find the brush extent along this normal.
				for ( int32_t s = 0; s < numsides; s++ ) {
					if ( !valid_windings[s] ) {
						continue;
					}

					winding_t *w = &windings[s];
					for ( int32_t w_idx = 0; w_idx < w->num_points; w_idx++ ) {
						Vector3 pt = w->points[w_idx];
						float dot = pt.x * normal.x + pt.y * normal.y + pt.z * normal.z;
						if ( dot > max_dist_pos ) {
							max_dist_pos = dot;
						}
						if ( dot < min_dist_neg ) {
							min_dist_neg = dot;
						}
						has_points = true;
					}
				}

				// If no valid points survived clipping, this plane cannot be formed.
				if ( !has_points ) {
					continue;
				}

				// Store both facing directions so the brush can be bounded symmetrically.
				if ( num_new_brush_planes < 512 ) {
					cm_plane_t p;
					p.normal[0] = normal.x;
					p.normal[1] = normal.y;
					p.normal[2] = normal.z;
					p.dist = max_dist_pos;
					p.type = 3; // generic
					p.signbits = 0;
					for ( int32_t s = 0; s < 3; s++ ) {
						if ( p.normal[s] < 0 ) {
							p.signbits |= ( 1 << s );
						}
					}
					new_brush_planes[num_new_brush_planes++] = p;
				}

				// Add the mirrored plane if capacity remains.
				if ( num_new_brush_planes < 512 ) {
					cm_plane_t p;
					p.normal[0] = -normal.x;
					p.normal[1] = -normal.y;
					p.normal[2] = -normal.z;
					p.dist = -min_dist_neg;
					p.type = 3; // generic
					p.signbits = 0;
					for ( int32_t s = 0; s < 3; s++ ) {
						if ( p.normal[s] < 0 ) {
							p.signbits |= ( 1 << s );
						}
					}
					new_brush_planes[num_new_brush_planes++] = p;
				}
			}
		}

		/**
		*	Commit the original brush sides into the generated array.
		*	Each side's plane pointer is remapped into the newly allocated plane buffer.
		**/
		b->firstbrushside = &bsp->bevel_brushsides[ current_side ];
		b->numsides = numsides;

		// Copy the brush's authored sides first so the brush retains its source geometry separately.
		for ( int32_t j = 0; j < numsides; j++ ) {
			bsp->bevel_brushsides[ current_side ] = original_sides[ j ];

			// Rebind the side to the new plane array using the authored plane index.
			const int32_t plane_idx = static_cast< int32_t >( original_sides[ j ].plane - bsp->authored_planes );
			bsp->bevel_brushsides[ current_side ].plane = &bsp->bevel_planes[ plane_idx ];
			current_side++;
		}

		/**
		*	Append any generated bevel sides to the brush.
		*	The newly inserted sides reference planes in the copied plane table and
		*	inherit material data from the first original side.
		**/
		for ( int32_t j = 0; j < num_new_brush_planes; j++ ) {
			if ( num_planes >= max_new_planes || current_side >= max_new_sides ) {
				break;
			}

			bsp->bevel_planes[ num_planes ] = new_brush_planes[ j ];
			bsp->bevel_brushsides[ current_side ].plane = &bsp->bevel_planes[ num_planes ];
			bsp->bevel_brushsides[ current_side ].texinfo = bsp->bevel_brushsides[ current_side - numsides ].texinfo; // Copy from the first authored side.

			num_planes++;
			current_side++;
			b->numsides++;
		}
	}

	// Repoint the BSP tables so downstream collision queries use the generated bevel arrays.
	bsp->planes = bsp->bevel_planes;
	bsp->brushsides = bsp->bevel_brushsides;

	delete[] windings;

	// Preserve original lump counts to avoid breaking code that still iterates the source indices.
}
