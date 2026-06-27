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
*	@brief	Generate bevel windings for brush collision.
*	@param	cm	Collision model context that would receive generated bevel data.
*	@note	This implementation is intentionally disabled because adding bevel planes to
*			every brush edge can create visible bumps at seams between adjacent flat brushes.
*			Analytical sweeps against BSP hulls remain sufficient without these bevels.
**/
void CM_GenerateBrushBevels( cm_t *cm ) {
	(void)cm;

	// Intentionally disabled: bevel planes on every brush edge can create seams between
	// adjacent flat floor brushes. The current collision sweep path remains sufficient
	// without rounded-bottom capsule support on disjoint brushes.
}
