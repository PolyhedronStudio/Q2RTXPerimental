#pragma once

#include "shared/math/qm_vector3_dp.h"
#include <array>
#include <cstdint>

/**
*	@brief	Combinable waypoint obligations; one retained point may satisfy several constraints.
**/
enum svg_nav_waypoint_flags_t : uint32_t {
	NAV_WAYPOINT_FUNNEL = 1u << 0,
	NAV_WAYPOINT_START = 1u << 1,
	NAV_WAYPOINT_CORNER_ENTRY = 1u << 2,
	NAV_WAYPOINT_CORNER_APEX = 1u << 3,
	NAV_WAYPOINT_CORNER_EXIT = 1u << 4,
	NAV_WAYPOINT_DOOR_PORTAL = 1u << 5,
	NAV_WAYPOINT_STEP_APPROACH = 1u << 6,
	NAV_WAYPOINT_STEP_RISER = 1u << 7,
	NAV_WAYPOINT_STEP_LANDING = 1u << 8,
	NAV_WAYPOINT_FINAL_GOAL = 1u << 9,
	NAV_WAYPOINT_MANDATORY = 1u << 10,
	//! Explicit authored fixture provenance, never mistaken for a generated portal or corner cache ID.
	NAV_WAYPOINT_FIXTURE = 1u << 11
};

/**
*	@brief	Owned provenance for a portal or corner obligation merged into a waypoint.
**/
struct svg_nav_waypoint_constraint_t {
	//! Semantic obligations originating at this source geometry.
	uint32_t flags = NAV_WAYPOINT_FUNNEL;
	//! Half-edge index for a source portal, valid only at the route's topology revision.
	int32_t portal = -1;
	//! Owned-cache corner index for corner entry/apex/exit provenance.
	int32_t corner = -1;
};

/**
*	@brief	Authoritative feet-space waypoint and all retained semantic obligations.
*	@note	Inline provenance avoids an allocation per point; overflow is a defined route failure.
**/
struct svg_nav_waypoint_t {
	//! Exact feet-space position; start and requested final goal are never silently substituted.
	Vector3DP feet = {};
	//! Supporting navigation face, valid at the owning route's mesh/topology revision.
	int32_t face = -1;
	//! Union of all semantic flags; legacy forced drawing is derived from this field.
	uint32_t flags = NAV_WAYPOINT_FUNNEL;
	//! Stable constraint ordinal inside the accepted route revision.
	uint32_t constraint_id = 0;
	//! Unchanged physical XY radius required by incoming/outgoing closed links.
	double required_clearance = 0.0;
	//! Incoming/outgoing horizontal unit directions, zero for a degenerate neighbor link.
	Vector3DP incoming = {}, outgoing = {};
	//! All distinct source obligations coalesced at this retained position.
	std::array<svg_nav_waypoint_constraint_t, 8> constraints = {};
	//! Number of valid inline constraints; no obligation may be dropped on overflow.
	uint32_t constraint_count = 0;
	//! Callback ownership for this point in the accepted route revision.
	bool callback_fired = false;
};

/**
*	@brief	Derive the legacy forced visualization and mandatory steering contract from semantic data.
*	@param	waypoint	Authoritative waypoint record.
*	@return	True when the waypoint has an obligation that cannot be removed by lookahead.
**/
inline bool Nav_WaypointIsMandatory( const svg_nav_waypoint_t &waypoint ) {
	// Semantic obligation ownership replaces the former parallel Boolean contract.
	return ( waypoint.flags & NAV_WAYPOINT_MANDATORY ) != 0;
}

/**
*	@brief	Compare complete semantic waypoint ownership without relying on struct padding.
*	@return	True only for identical feet, support, obligations, directions and callback state.
**/
inline bool Nav_WaypointRecordsEqual( const svg_nav_waypoint_t &a, const svg_nav_waypoint_t &b ) {
	const auto pointEqual = []( const Vector3DP &x, const Vector3DP &y ) {
		return x.x == y.x && x.y == y.y && x.z == y.z;
	};
	// Every retained obligation is compared, not only the visible waypoint position.
	if ( !pointEqual( a.feet, b.feet ) || a.face != b.face || a.flags != b.flags ||
		 a.constraint_id != b.constraint_id || a.required_clearance != b.required_clearance ||
		 !pointEqual( a.incoming, b.incoming ) || !pointEqual( a.outgoing, b.outgoing ) ||
		 a.constraint_count > a.constraints.size() || b.constraint_count > b.constraints.size() ||
		 a.constraint_count != b.constraint_count || a.callback_fired != b.callback_fired ) {
		return false;
	}
	// Compare valid inline sources only; unused storage cannot manufacture an obligation.
	for ( uint32_t index = 0; index < a.constraint_count; ++index ) {
		const auto &x = a.constraints[ index ];
		const auto &y = b.constraints[ index ];
		if ( x.flags != y.flags || x.portal != y.portal || x.corner != y.corner ) {
			return false;
		}
	}
	return true;
}
