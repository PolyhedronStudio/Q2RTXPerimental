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
	*	Estimate worst-case storage so we can allocate once up-front.
	*	Each brush may contribute a large number of bevel planes, so we reserve a
	*	conservative fixed upper bound per brush to avoid repeated reallocations.
	**/
	int32_t max_new_sides = bsp->numbrushsides + ( bsp->numbrushes * 128 );
	int32_t max_new_planes = bsp->numplanes + ( bsp->numbrushes * 128 );

	// Allocate the generated brushside and plane arrays from the collision-model zone.
	cm->bevel_brushsides = static_cast<mbrushside_t *>( Z_TagMallocz( max_new_sides * sizeof( mbrushside_t ), TAG_CMODEL ) );
	cm->bevel_planes = static_cast<cm_plane_t *>( Z_TagMallocz( max_new_planes * sizeof( cm_plane_t ), TAG_CMODEL ) );

	// Preserve the original plane table as the base of the new plane array.
	memcpy( cm->bevel_planes, bsp->planes, bsp->numplanes * sizeof( cm_plane_t ) );
	int32_t num_planes = bsp->numplanes;

	// Track the next write position inside the generated brushside array.
	int32_t current_side = 0;

	/**
	*	Process each brush in turn.
	*	Non-solid brushes are copied through unchanged; solid brushes are analyzed
	*	to derive extra bevel planes that help collision handling remain stable.
	**/
	for ( int32_t i = 0; i < bsp->numbrushes; i++ ) {
		mbrush_t *b = &bsp->brushes[i];

		// Non-solid brushes do not need bevel generation, but their sides still need to be copied over.
		if ( !( b->contents & MASK_SOLID ) ) {
			// Point the brush at the next generated side range.
			b->firstbrushside = &cm->bevel_brushsides[current_side];

			// Copy the brush's existing sides into the generated side array unchanged.
			for ( int32_t j = 0; j < b->numsides; j++ ) {
				cm->bevel_brushsides[current_side + j] = bsp->brushsides[b->firstbrushside_idx + j];
			}

			// Advance past the copied non-solid brush sides.
			current_side += b->numsides;
			continue;
		}

		/**
		*	Build clipped windings for each brush side.
		*	These windings are later used to infer edge directions and candidate bevel planes.
		**/
		winding_t windings[128];
		bool valid_windings[128] = { false };

		// Clamp the working side count to our fixed local buffer size for safety.
		int32_t numsides = b->numsides;
		if ( numsides > 128 ) {
			numsides = 128;
		}

		// Generate a clipped polygon for each side of the brush.
		for ( int32_t j = 0; j < numsides; j++ ) {
			mbrushside_t *s = &bsp->brushsides[b->firstbrushside_idx + j];
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

				mbrushside_t *s2 = &bsp->brushsides[b->firstbrushside_idx + k];

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
					cm_plane_t *p = bsp->brushsides[b->firstbrushside_idx + s].plane;
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
		b->firstbrushside = &cm->bevel_brushsides[current_side];

		// Copy the brush's original sides first so the brush still retains its source planes.
		for ( int32_t j = 0; j < numsides; j++ ) {
			cm->bevel_brushsides[current_side] = bsp->brushsides[b->firstbrushside_idx + j];

			// Rebind the side to the new plane array using the original plane index.
			int32_t plane_idx = bsp->brushsides[b->firstbrushside_idx + j].plane - bsp->planes;
			cm->bevel_brushsides[current_side].plane = &cm->bevel_planes[plane_idx];
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

			cm->bevel_planes[num_planes] = new_brush_planes[j];
			cm->bevel_brushsides[current_side].plane = &cm->bevel_planes[num_planes];
			cm->bevel_brushsides[current_side].surfaceFlags = 0;
			cm->bevel_brushsides[current_side].material = cm->bevel_brushsides[current_side - numsides].material; // Copy from the first side.

			num_planes++;
			current_side++;
			b->numsides++;
		}
	}

	// Repoint the BSP tables so downstream collision queries use the generated bevel arrays.
	bsp->planes = cm->bevel_planes;
	bsp->brushsides = cm->bevel_brushsides;

	// Preserve original lump counts to avoid breaking code that still iterates the source indices.
}
