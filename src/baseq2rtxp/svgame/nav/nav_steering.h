#pragma once

#include "svgame/monsters/svg_mmove.h"
#include "svgame/nav/nav_path.h"
#include <array>

/**
*	@brief	Owned reason for one shared initial/runtime waypoint release decision.
**/
enum class svg_nav_release_reason_t : uint8_t {
	clear, progress_pending, invalid_index, stale_route, unsupported_hull, budget_exhausted,
	elevation_pending, geometric_rejection, native_rejection
};

/**
*	@brief	Immutable evidence from the shared waypoint release contract.
**/
struct svg_nav_release_t {
	//! Decision defaults to a non-authorizing progress state.
	svg_nav_release_reason_t reason = svg_nav_release_reason_t::progress_pending;
	//! Proposed waypoint and route/goal identities when the decision was made.
	uint32_t index = 0;
	uint64_t route = 0, goal = 0;
	//! Incoming/outgoing alignment and signed distance past the incoming approach plane.
	double turn_dot = 0.0, plane_distance = 0.0;
	//! Unchanged-radius boundary witness and native continuation result.
	nav_segment_clearance_t geometric;
	svg_nav_movement_result_t native;
};

/**
*	@brief	One named candidate after speed, constraint cap and reservation projection.
**/
struct svg_nav_steering_candidate_t {
	//! Exact proposed/projected velocity and feet-space endpoint.
	Vector3DP proposed = {}, projected = {}, endpoint = {};
	//! Owned boundary and physical results; unexecuted phases keep invalid_input.
	nav_segment_clearance_t geometric;
	svg_nav_movement_result_t native;
	//! Whether all gates accepted this exact projected candidate.
	bool accepted = false;
};

/**
*	@brief	Bounded causal steering evidence for one grounded navigation update.
*	@note	No pointers or per-tick heap work; all rejected candidates remain distinct from the final command.
**/
struct svg_nav_steering_t {
	//! Waypoint identity before steering, and named direction stages in evaluation order.
	uint32_t old_index = 0;
	Vector3DP raw = {}, lookahead = {}, transit = {}, monotonic = {}, deadband = {}, separation = {};
	//! Independent scalar contributors to actual command speed.
	double turn_scale = 1.0, squad_scale = 1.0, follow_scale = 1.0, catchup_scale = 1.0, fixture_scale = 1.0;
	//! Actual yaw before movement, constraint distance and final velocity.
	double yaw_before = 0.0, constraint_distance = 0.0;
	//! Exact velocity committed after all candidate and reservation validation.
	Vector3DP final_velocity = {};

	//! At most four complete projected candidate evaluations.
	std::array<svg_nav_steering_candidate_t, 4> candidates = {};
	uint32_t candidate_count = 0;
};
