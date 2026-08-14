/********************************************************************
*
*
*	ServerGame: Navigation mesh Constructive Solid Geometry (CSG)
*				winding operations, coplanar merging, sliver dissolution,
*				and obstacle-aware spatial polygon partitioning.
*
*
********************************************************************/
#pragma once

#include "nav_core.h"
#include "nav_types.h"
#include "nav_containers.h"
#include "shared/cm/cm_model.h"
#include <vector>

/**
*	@brief	Maximum number of vertices permitted in a single CSG winding.
**/
constexpr int32_t MAX_WINDING_POINTS = 1024;

/**
*	@brief		Winding representation of a convex polygon used during CSG plane splitting and coplanar merging.
*	@details	Encapsulates a dynamic 3D double-precision vertex perimeter backed by `nav_vector_t<Vector3DP>`
*				under engine memory tag `TAG_SVGAME_NAVMESH`.
*	@note		Replaces static 24.5 KB stack allocation with dynamic engine-heap vector storage, reducing struct
*				footprint down to ~40 bytes per instance. Drastically eliminates stack overflow risks during deep
*				recursive BSP/CSG partitioning while dynamically supporting complex QBism BSP polygons (1024+ vertices).
**/
struct winding_t {
private:
	//! Heap-allocated backing vector for vertex coordinates.
	nav_vector_t<Vector3DP> _vertices;

public:
	//! Number of active vertices in the winding perimeter.
	int32_t num_points = 0;
	//! Maximum allocated vertex capacity.
	int32_t capacity = 0;
	//! Entity ID this polygon belongs to (e.g. for doors), or ENTITYNUM_NONE if world.
	int32_t entity_id = ENTITYNUM_NONE;
	//! Door entity that caused this winding to become a transition boundary, or ENTITYNUM_NONE for ordinary world geometry.
	int32_t transition_entity_id = ENTITYNUM_NONE;

	//! Active vertex storage pointer (points to _vertices.begin()).
	Vector3DP *points = nullptr;

	/**
	*	@brief	Default constructor initializing engine-heap storage.
	**/
	winding_t() {
		reserve( 16 );
	}


	/**
	*	@brief	Copy constructor.
	**/
	winding_t( const winding_t &other ) {
		CopyFrom( other );
	}

	/**
	*	@brief	Copy assignment operator.
	**/
	winding_t &operator=( const winding_t &other ) {
		if ( this != &other ) {
			CopyFrom( other );
		}
		return *this;
	}

	/**
	*	@brief	Move constructor.
	**/
	winding_t( winding_t &&other ) noexcept {
		MoveFrom( std::move( other ) );
	}

	/**
	*	@brief	Move assignment operator.
	**/
	winding_t &operator=( winding_t &&other ) noexcept {
		if ( this != &other ) {
			MoveFrom( std::move( other ) );
		}
		return *this;
	}

	/**
	*	@brief	Pre-allocate capacity for at least new_cap vertices on the engine heap.
	*	@param	new_cap	Minimum capacity to allocate.
	**/
	void reserve( int32_t new_cap ) {
		if ( new_cap <= 0 ) return;
		_vertices.reserve( static_cast< size_t >( new_cap ) );
		capacity = static_cast< int32_t >( _vertices.capacity() );
		points = _vertices.begin();
	}

	/**
	*	@brief	Ensure storage capacity is at least needed_cap.
	*	@param	needed_cap	Required capacity.
	**/
	inline void ensure_capacity( int32_t needed_cap ) {
		if ( needed_cap > capacity ) {
			int32_t target_cap = std::max( needed_cap, ( capacity > 0 ) ? capacity * 2 : 16 );
			reserve( target_cap );
		}
	}

	/**
	*	@brief	Append a vertex to the winding perimeter, growing capacity automatically.
	*	@param	p	3D double-precision vertex position.
	**/
	void push_back( const Vector3DP &p ) {
		ensure_capacity( num_points + 1 );
		_vertices.push_back( p );
		num_points = static_cast< int32_t >( _vertices.size() );
		capacity = static_cast< int32_t >( _vertices.capacity() );
		points = _vertices.begin();
	}

	/**
	*	@brief	Reset active vertex count.
	**/
	void clear() {
		_vertices.clear();
		num_points = 0;
		points = _vertices.begin();
	}

private:
	void CopyFrom( const winding_t &other ) {
		_vertices = other._vertices;
		num_points = other.num_points;
		capacity = static_cast< int32_t >( _vertices.capacity() );
		entity_id = other.entity_id;
		transition_entity_id = other.transition_entity_id;
		points = _vertices.begin();
	}

	void MoveFrom( winding_t &&other ) {
		_vertices = std::move( other._vertices );
		num_points = other.num_points;
		capacity = static_cast< int32_t >( _vertices.capacity() );
		entity_id = other.entity_id;
		transition_entity_id = other.transition_entity_id;
		points = _vertices.begin();

		other.num_points = 0;
		other.capacity = 0;
		other.points = nullptr;
	}
};



/**
*	@brief	Split a CSG winding along a spatial plane into front and back sub-windings.
*	@param	in			[in] Source winding to split.
*	@param	split		[in] Clipping plane definition.
*	@param	epsilon		[in] Thickness tolerance for on-plane classification.
*	@param	face_normal	[in] Normal vector of the source face for 2D/3D projection.
*	@param	front		[out] Fragment lying in front of the split plane.
*	@param	back		[out] Fragment lying behind the split plane.
**/
void SplitWinding( const winding_t *in, const cm_plane_t *split, double epsilon, const Vector3DP &face_normal, winding_t *front, winding_t *back );

/**
*	@brief	Chop a CSG winding in-place against a clipping plane, retaining only the front fragment.
*	@param	in			[in,out] Pointer to the winding to modify.
*	@param	split		[in] Clipping plane definition.
*	@param	epsilon		[in] Thickness tolerance for on-plane classification.
*	@param	face_normal	[in] Normal vector of the source face.
**/
void ChopWindingInPlace( winding_t **in, const cm_plane_t *split, double epsilon, const Vector3DP &face_normal );

/**
*	@brief	Attempt to merge two coplanar polygon windings along a shared anti-parallel edge segment.
*	@param	w1			[in] First polygon winding.
*	@param	w2			[in] Second polygon winding.
*	@param	normal		[in] Surface normal of the coplanar plane.
*	@param	out			[out] Resulting merged convex polygon winding.
*	@return	True when a valid convex merge was successfully formed; false otherwise.
**/
bool TryMergeWindings( const winding_t &w1, const winding_t &w2, const Vector3DP &normal, winding_t *out );

/**
*	@brief	Recompute the centroid of a nav_poly_t polygon from its vertex loop.
*	@param	poly	[in,out] Polygon to update.
**/
void RecomputeNavPolygonCenter( nav_poly_t &poly );

/**
*	@brief	Determine whether two navigation polygons occupy the same surface plane.
*	@param	first	First polygon to compare.
*	@param	second	Second polygon to compare.
*	@return	True when the normals agree and the second polygon is coplanar with the first.
**/
bool AreNavPolygonsCoplanar( const nav_poly_t &first, const nav_poly_t &second );


/**
*	@brief	Ensure 3D double-precision winding vertices are strictly ordered Counter-Clockwise (CCW) in 2D.
*	@param	w	[in,out] Winding structure to sanitize.
*	@return	True if winding is non-degenerate CCW polygon; false if degenerate.
**/
inline bool EnsureWindingCCW( winding_t &w ) {
	if ( w.num_points < 3 || !w.points ) return false;

	// Calculate 2D signed area using the shoelace formula.
	double signed_area = 0.0;
	for ( int32_t i = 0; i < w.num_points; i++ ) {
		int32_t next = ( i + 1 ) % w.num_points;
		signed_area += ( w.points[ i ].x * w.points[ next ].y ) - ( w.points[ next ].x * w.points[ i ].y );
	}

	// Reject degenerate zero-area windings.
	if ( std::abs( signed_area ) < 0.01 ) {
		return false;
	}

	// If signed area is negative, winding is Clockwise (CW); reverse vertices to enforce CCW.
	if ( signed_area < 0.0 ) {
		for ( int32_t i = 0; i < w.num_points / 2; i++ ) {
			std::swap( w.points[ i ], w.points[ w.num_points - 1 - i ] );
		}
	}

	return true;
}

/**
*	@brief	Ensure navigation polygon perimeter vertices are strictly ordered Counter-Clockwise (CCW) in 2D.
*	@param	poly	[in,out] Navigation polygon to sanitize.
*	@return	True if polygon is non-degenerate CCW polygon; false if degenerate.
**/
inline bool EnsureNavPolygonCCW( nav_poly_t &poly ) {
	if ( poly.num_vertices < 3 ) return false;

	// Calculate 2D signed area using the shoelace formula.
	double signed_area = 0.0;
	for ( int32_t i = 0; i < poly.num_vertices; i++ ) {
		int32_t next = ( i + 1 ) % poly.num_vertices;
		signed_area += ( poly.vertices[ i ].x * poly.vertices[ next ].y ) - ( poly.vertices[ next ].x * poly.vertices[ i ].y );
	}

	// Reject degenerate zero-area polygons.
	if ( std::abs( signed_area ) < 0.01 ) {
		return false;
	}

	// If signed area is negative, winding is Clockwise (CW); reverse vertices to enforce CCW.
	if ( signed_area < 0.0 ) {
		std::reverse( poly.vertices, poly.vertices + poly.num_vertices );
	}

	return true;
}

/**
*	@brief	Evaluate whether a partition fragment winding is non-degenerate and usable.
*	@param	w	[in] Winding to test.
*	@return	True if the winding has at least 3 vertices and non-zero 2D surface area.
**/
bool IsUsablePartitionFragment( const winding_t &w );


/**
*	@brief	Simplify redundant collinear perimeter vertices on a single navigation polygon.
*	@param	poly	[in,out] Navigation polygon whose perimeter vertices will be simplified.
*	@return	True if any redundant collinear vertex was removed.
*	@note	Removes intermediate vertices where the 2D turn angle is near zero,
*			restoring straight edges across door thresholds and merged floor boundaries.
**/
bool SimplifyNavPolygonCollinearVertices( nav_poly_t &poly );

/**
*	@brief	Simplify redundant collinear perimeter vertices across all extracted navigation polygons.
**/
void SimplifyAllNavPolygonsCollinearVertices();

/**
*	@brief	Merge adjacent coplanar polygons in g_nav_polys to form maximal convex walk surfaces.
**/
void Nav_MergeCoplanarPolygons();

/**
*	@brief	Dissolve razor-thin sliver polygons and absorb them into adjacent floor surfaces.
**/
void Nav_DissolveSlivers();

/**
*	@brief	Recursively partition raw floor polygons using localized, obstacle-aware split planes.
**/
void Nav_PartitionPolygons();

