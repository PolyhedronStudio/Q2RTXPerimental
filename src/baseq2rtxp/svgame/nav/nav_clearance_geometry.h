#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

/**
*	@brief	Measure point-to-closed-segment squared distance without changing endpoints.
*	@param	p	Point with double-precision x/y coordinates.
*	@param	a	First segment endpoint.
*	@param	b	Second segment endpoint.
*	@return	Squared XY distance, including degenerate segments.
*	@note	This pure helper does not access mesh, collision or engine state.
**/
template <typename Point>
inline double Nav_ClearancePointDistanceSqr( const Point &p, const Point &a, const Point &b ) {
	/**
	*	Project onto the finite segment; a zero-length segment is a point.
	**/
	const double dx = b.x - a.x;
	const double dy = b.y - a.y;
	const double lengthSqr = dx * dx + dy * dy;
	const double t = lengthSqr > 0.0 ? std::clamp( ( ( p.x - a.x ) * dx + ( p.y - a.y ) * dy ) / lengthSqr, 0.0, 1.0 ) : 0.0;
	const double ex = p.x - ( a.x + t * dx );
	const double ey = p.y - ( a.y + t * dy );
	return ex * ex + ey * ey;
}

/**
*	@brief	Measure minimum squared distance between two closed XY segments.
*	@param	p	Travel start.
*	@param	q	Travel end.
*	@param	a	Wall start.
*	@param	b	Wall end.
*	@return	Zero for intersections/contact; otherwise minimum squared distance.
*	@note	Endpoint distances handle collinear overlap and zero-length segments.
**/
template <typename Point>
inline double Nav_ClearanceSegmentDistanceSqr( const Point &p, const Point &q, const Point &a, const Point &b ) {
	/**
	*	Detect interior crossings before minimizing endpoint-to-segment distances.
	**/
	const double ap = ( b.x - a.x ) * ( p.y - a.y ) - ( b.y - a.y ) * ( p.x - a.x );
	const double aq = ( b.x - a.x ) * ( q.y - a.y ) - ( b.y - a.y ) * ( q.x - a.x );
	const double pa = ( q.x - p.x ) * ( a.y - p.y ) - ( q.y - p.y ) * ( a.x - p.x );
	const double pb = ( q.x - p.x ) * ( b.y - p.y ) - ( q.y - p.y ) * ( b.x - p.x );
	// Opposite orientations on both segments prove an interior intersection.
	if ( ( ( ap < 0.0 && aq > 0.0 ) || ( ap > 0.0 && aq < 0.0 ) ) &&
		 ( ( pa < 0.0 && pb > 0.0 ) || ( pa > 0.0 && pb < 0.0 ) ) ) {
		return 0.0;
	}

	/**
	*	Include all endpoints: long walls and stationary disks have no exemption.
	**/
	return std::min( { Nav_ClearancePointDistanceSqr( p, a, b ), Nav_ClearancePointDistanceSqr( q, a, b ),
		Nav_ClearancePointDistanceSqr( a, p, q ), Nav_ClearancePointDistanceSqr( b, p, q ) } );
}

/**
*	@brief	Conservatively reject a segment that cannot improve an existing nearest witness.
*	@param	p	Travel start.
*	@param	q	Travel end.
*	@param	a	Boundary start.
*	@param	b	Boundary end.
*	@param	minimumSqr	Current exact squared distance.
*	@return	True when exact distance evaluation remains necessary.
*	@note	Box distance is a lower bound; rounding headroom and extreme-input fallback preserve ties and failure checks.
**/
template <typename Point>
inline bool Nav_ClearanceSegmentMayImprove( const Point &p, const Point &q, const Point &a, const Point &b, const double minimumSqr ) {
	/**
	*	No finite witness or extreme arithmetic means the exact evaluator must run.
	**/
	// Outside this bounded world-coordinate domain, retain the original arithmetic and failure checks.
	if ( !std::isfinite( minimumSqr ) || p.x < -32768.0 || p.x > 32768.0 || p.y < -32768.0 || p.y > 32768.0 ||
		q.x < -32768.0 || q.x > 32768.0 || q.y < -32768.0 || q.y > 32768.0 ||
		a.x < -32768.0 || a.x > 32768.0 || a.y < -32768.0 || a.y > 32768.0 ||
		b.x < -32768.0 || b.x > 32768.0 || b.y < -32768.0 || b.y > 32768.0 ) {
		return true;
	}
	/**
	*	Use scalar bounds to avoid initializer-list/function overhead in unoptimized gameplay builds.
	**/
	const double pMinX = p.x < q.x ? p.x : q.x, pMaxX = p.x > q.x ? p.x : q.x;
	const double pMinY = p.y < q.y ? p.y : q.y, pMaxY = p.y > q.y ? p.y : q.y;
	const double aMinX = a.x < b.x ? a.x : b.x, aMaxX = a.x > b.x ? a.x : b.x;
	const double aMinY = a.y < b.y ? a.y : b.y, aMaxY = a.y > b.y ? a.y : b.y;
	const double dx = aMinX > pMaxX ? aMinX - pMaxX : ( pMinX > aMaxX ? pMinX - aMaxX : 0.0 );
	const double dy = aMinY > pMaxY ? aMinY - pMaxY : ( pMinY > aMaxY ? pMinY - aMaxY : 0.0 );
	// The fixed cushion exceeds 128 double epsilons times the squared maximum supported coordinate.
	return dx * dx + dy * dy <= minimumSqr + 0.0001;
}

/**
*	@brief	Compute safe radial standoff from both inward wall-normal projections.
*	@param	radius	Unchanged physical planar radius.
*	@param	margin	Nonnegative optional safety margin.
*	@param	projectionA	Unit bisector dot first unit inward normal.
*	@param	projectionB	Unit bisector dot second unit inward normal.
*	@return	Required distance, or infinity when the bisector has no safe common side.
*	@note	A radial radius alone underestimates perpendicular clearance at corners.
**/
inline double Nav_RequiredBisectorDistance( const double radius, const double margin,
	const double projectionA, const double projectionB ) {
	// Invalid/nonpositive normal projections cannot prove a free-space standoff.
	if ( !std::isfinite( radius ) || radius <= 0.0 || !std::isfinite( margin ) || margin < 0.0 ||
		 !std::isfinite( projectionA ) || !std::isfinite( projectionB ) || projectionA <= 0.000001 || projectionB <= 0.000001 ) {
		return std::numeric_limits<double>::infinity();
	}
	return ( radius + margin ) / std::min( projectionA, projectionB );
}
