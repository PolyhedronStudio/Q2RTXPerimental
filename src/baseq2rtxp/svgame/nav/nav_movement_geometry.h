#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

//! Reporting/endpoint tolerance; this never reduces the physical hull radius.
inline constexpr double NAV_MOVEMENT_ENDPOINT_TOLERANCE = 0.125;

/**
*	@brief	Check whether bounds match the centered circular XY profile used by native traces.
*	@param	mins	BBox minima relative to bbox origin.
*	@param	maxs	BBox maxima relative to bbox origin.
*	@return	True only for finite symmetric square XY bounds and positive vertical extent.
*	@note	Asymmetric Z is supported by the existing center conversion; asymmetric XY is rejected.
**/
template <typename Bounds>
inline bool Nav_IsSupportedNativeHull( const Bounds &mins, const Bounds &maxs ) {
	/**
	*	Do not approximate asymmetric/non-square bounds with a smaller native trace radius.
	**/
	return std::isfinite( mins.x ) && std::isfinite( mins.y ) && std::isfinite( mins.z ) &&
		std::isfinite( maxs.x ) && std::isfinite( maxs.y ) && std::isfinite( maxs.z ) &&
		mins.x < 0.0 && mins.x == mins.y && maxs.x == maxs.y && mins.x == -maxs.x && maxs.z > mins.z;
}

/**
*	@brief	Require full XY completion rather than accepting a probe's partial-progress Boolean.
*	@param	expected	Requested bbox origin.
*	@param	actual	Returned bbox origin.
*	@return	True if finite XY completion error is within the frozen reporting tolerance.
*	@note	Elevation and walkable support must be checked separately.
**/
template <typename Point>
inline bool Nav_HasFullHorizontalProgress( const Point &expected, const Point &actual ) {
	/**
	*	Measure exact endpoint error without changing radius or gameplay arrival thresholds.
	**/
	const double dx = expected.x - actual.x;
	const double dy = expected.y - actual.y;
	return std::isfinite( dx ) && std::isfinite( dy ) && std::isfinite( actual.z ) &&
		dx * dx + dy * dy <= NAV_MOVEMENT_ENDPOINT_TOLERANCE * NAV_MOVEMENT_ENDPOINT_TOLERANCE;
}

/**
*	@brief	Bound inward error when a native double position is stored in float entity coordinates.
*	@param	point	Proposed endpoint near the reserved stop plane.
*	@param	normal	Normalized inward portal normal.
*	@return	Conservative outward margin in world units, or infinity for invalid storage coordinates.
*	@note	Two local float spacings cover rounding/binade changes; this never shrinks a hull or relaxes the stop plane.
**/
template <typename Point, typename Normal>
inline double Nav_ReservationStorageMargin( const Point &point, const Normal &normal ) {
	/**
	*	Use actual storage spacing rather than a fixed gameplay epsilon; invalid inputs cannot authorize a crossing.
	**/
	if ( !std::isfinite( point.x ) || !std::isfinite( point.y ) || !std::isfinite( point.z ) ||
		!std::isfinite( normal.x ) || !std::isfinite( normal.y ) || !std::isfinite( normal.z ) ) {
		return std::numeric_limits<double>::infinity();
	}
	const auto spacing = []( const double value ) {
		const float stored = static_cast<float>( value );
		// Unrepresentable entity coordinates cannot have a finite rounding guarantee.
		if ( !std::isfinite( stored ) ) {
			return std::numeric_limits<double>::infinity();
		}
		const float above = std::nextafter( stored, std::numeric_limits<float>::infinity() );
		const float below = std::nextafter( stored, -std::numeric_limits<float>::infinity() );
		return std::max( static_cast<double>( above ) - stored, static_cast<double>( stored ) - below );
	};
	/**
	*	Project coordinate rounding onto the physical inward normal, retaining a small arithmetic floor near zero.
	**/
	const double spacingX = spacing( point.x ), spacingY = spacing( point.y ), spacingZ = spacing( point.z );
	// Check each axis before weighting; a zero normal must not hide an unrepresentable coordinate through 0 * infinity.
	if ( !std::isfinite( spacingX ) || !std::isfinite( spacingY ) || !std::isfinite( spacingZ ) ) {
		return std::numeric_limits<double>::infinity();
	}
	const double weightedSpacing = std::fabs( normal.x ) * spacingX +
		std::fabs( normal.y ) * spacingY + std::fabs( normal.z ) * spacingZ;
	return std::max( static_cast<double>( std::numeric_limits<float>::epsilon() ), 2.0 * weightedSpacing );
}

/**
*	@brief	Terminal status of a complete, supported native-hull candidate.
**/
enum class svg_nav_movement_reason_t : uint8_t {
	clear,
	invalid_input,
	unsupported_hull,
	blocked,
	incomplete,
	unsupported_landing,
	elevation_policy
};

/**
*	@brief	Owned result of the engine-independent traversal decision.
**/
template <typename Point>
struct nav_native_traversal_t {
	//! Fail closed until the entire candidate and its floor support pass.
	svg_nav_movement_reason_t reason = svg_nav_movement_reason_t::invalid_input;
	//! Supported bbox-origin endpoint, never a borrowed scratch pointer.
	Point endpoint = {};
	//! Internal sweep count, including discarded step trials.
	uint32_t trace_count = 0;
};

/**
*	@brief	Evaluate complete direct/step/landing traversal using an injected native sweep.
*	@param	start	BBox-origin start.
*	@param	end	BBox-origin requested endpoint.
*	@param	maxStep	Maximum rise, including explicit zero-height policy.
*	@param	maxDrop	Maximum descent after any independent cap.
*	@param	minNormal	Minimum walkable landing normal Z.
*	@param	requireHeight	Require exact waypoint elevation as well as XY completion.
*	@param	sweep	Native trace callback with fraction/solid/endpoint/plane fields.
*	@return	Reason, supported endpoint and number of sweeps performed.
*	@note	At most five native traces, including a rejected direct landing before one step trial. Standalone fixtures use the identical production decision code.
**/
template <typename Point, typename Sweep>
inline nav_native_traversal_t<Point> Nav_TestNativeTraversal( const Point &start, const Point &end,
	const double maxStep, const double maxDrop, const double minNormal, const bool requireHeight, Sweep sweep ) {
	/**
	*	Reject invalid policy/coordinates before invoking collision callbacks.
	**/
	nav_native_traversal_t<Point> result;
	result.endpoint = start;
	// Invalid values must not become unchecked comparisons or collision destinations.
	if ( !std::isfinite( start.x ) || !std::isfinite( start.y ) || !std::isfinite( start.z ) ||
		 !std::isfinite( end.x ) || !std::isfinite( end.y ) || !std::isfinite( end.z ) ||
		 !std::isfinite( maxStep ) || maxStep < 0.0 || !std::isfinite( maxDrop ) || maxDrop < 0.0 ||
		 !std::isfinite( minNormal ) || minNormal < 0.0 || minNormal > 1.0 ) {
		return result;
	}
	const auto trace = [&]( const Point &from, const Point &to ) {
		++result.trace_count;
		return sweep( from, to );
	};
	const auto complete = []( const auto &hit ) {
		return !hit.startsolid && !hit.allsolid && std::isfinite( hit.fraction ) && hit.fraction == 1.0;
	};
	const auto validTrace = []( const auto &hit ) {
		return std::isfinite( hit.fraction ) && hit.fraction >= 0.0 && hit.fraction <= 1.0 &&
			std::isfinite( hit.endpos.x ) && std::isfinite( hit.endpos.y ) && std::isfinite( hit.endpos.z );
	};

	/**
	*	Direct traversal requires complete progress; a riser may use one bounded step trial.
	**/
	const auto direct = trace( start, end );
	// Do not pass corrupt trace endpoints to subsequent native callbacks.
	if ( !validTrace( direct ) ) {
		return result;
	}
	// An embedded starting hull needs explicit recovery and cannot count as clean traversal.
	if ( direct.startsolid || direct.allsolid ) {
		result.reason = svg_nav_movement_reason_t::blocked;
		return result;
	}
	/**
	*	Evaluate support separately: a clear horizontal sweep may still end inside a riser.
	*	Only this endpoint embedding permits retrying a raised sweep from the original safe start.
	**/
	bool retryEmbeddedLanding = false;
	bool usingRaisedTrial = false;
	const auto land = [&]( const Point &landingStart ) {
		Point floorEnd = landingStart;
		floorEnd.z = std::min( landingStart.z, start.z ) - maxDrop - NAV_MOVEMENT_ENDPOINT_TOLERANCE;
		const auto floor = trace( landingStart, floorEnd );
		// Invalid collision output cannot authorize another trace or a successful candidate.
		if ( !validTrace( floor ) ) {
			result.reason = svg_nav_movement_reason_t::invalid_input;
			return;
		}
		retryEmbeddedLanding = floor.startsolid && !floor.allsolid;
		// A floor must be walkable static world support, matching authoritative StepSlideMove; actors cannot become treads.
		if ( floor.startsolid || floor.allsolid || floor.fraction >= 1.0 ||
			 !std::isfinite( floor.plane.normal[ 2 ] ) || floor.plane.normal[ 2 ] < minNormal || floor.entityNumber > 0 ) {
			result.reason = svg_nav_movement_reason_t::unsupported_landing;
			return;
		}
		result.endpoint = Point( floor.endpos );
		// Full XY completion uses reporting precision, never the gameplay arrival radius.
		if ( !Nav_HasFullHorizontalProgress( end, result.endpoint ) ) {
			result.reason = svg_nav_movement_reason_t::incomplete;
			return;
		}
		const double rise = result.endpoint.z - start.z;
		// Complete sloped movement can climb a ramp; maxStep constrains discrete risers.
		const bool continuousRampRise = complete( direct ) && !usingRaisedTrial && floor.plane.normal[ 2 ] < 0.999;
		// Require allowed rise/descent and optional literal endpoint height for explicit queries.
		if ( ( rise > maxStep + NAV_MOVEMENT_ENDPOINT_TOLERANCE && !continuousRampRise ) ||
			 -rise > maxDrop + NAV_MOVEMENT_ENDPOINT_TOLERANCE ||
			 ( requireHeight && std::fabs( result.endpoint.z - end.z ) > NAV_MOVEMENT_ENDPOINT_TOLERANCE ) ) {
			result.reason = svg_nav_movement_reason_t::elevation_policy;
			return;
		}
		result.reason = svg_nav_movement_reason_t::clear;
	};
	// Normal supported flat/ramp motion remains a two-trace path.
	if ( complete( direct ) ) {
		land( Point( direct.endpos ) );
		// Missing/steep floors and invalid results are terminal; only endpoint embedding needs a step trial.
		if ( !retryEmbeddedLanding || maxStep == 0.0 ) {
			return result;
		}
	}

	/**
	*	Try one raised traversal from the original start, then require complete supported landing.
	**/
	usingRaisedTrial = true;
	Point raisedStart = start;
	raisedStart.z += maxStep;
	const auto up = trace( start, raisedStart );
	// Explicit zero-step policy and embedded starts cannot manufacture vertical clearance.
	if ( !validTrace( up ) || up.startsolid || up.allsolid || up.fraction <= 0.0 || maxStep == 0.0 ||
		 !Nav_HasFullHorizontalProgress( start, Point( up.endpos ) ) ) {
		result.reason = svg_nav_movement_reason_t::blocked;
		return result;
	}
	const double stepUp = static_cast<double>( up.endpos.z ) - start.z;
	// Invalid/negative overhead progress cannot define an elevated trial.
	if ( stepUp <= 0.0 || stepUp > maxStep + NAV_MOVEMENT_ENDPOINT_TOLERANCE ) {
		result.reason = svg_nav_movement_reason_t::incomplete;
		return result;
	}
	Point raisedEnd = end;
	raisedEnd.z += stepUp;
	const auto forward = trace( Point( up.endpos ), raisedEnd );
	// Greater partial progress remains failure; no slide recovery is simulated by validation.
	if ( !validTrace( forward ) || !complete( forward ) ) {
		result.reason = svg_nav_movement_reason_t::incomplete;
		return result;
	}
	land( Point( forward.endpos ) );
	return result;
}
