/********************************************************************
*
*	ServerGame: Pure Actor Query Ownership and Retry Contract
*	Owner code supplies scalar lifecycle state; no live entity or engine import is read.
*
********************************************************************/
#pragma once
#include "nav_query_algorithm.h"

/**
*	@brief	Explicit actor request states separate outstanding work from physical arrival.
**/
enum class nav_actor_query_state_t : uint8_t { Idle, Pending, Ready, NoRoute, Stale, Cancelled, ResourceLimited };

/**
*	@brief	Owner-observed rejection cause affects accounting only, never actor lifecycle or retry behavior.
**/
enum class nav_actor_terminal_cause_t : uint8_t { Unspecified, CorridorDetached };

/**
*	@brief	Check scalar actor/publication ownership with narrowly selected goal/sequence strictness.
*	@param	current	Current owner-captured actor identity, never captured on a reader.
*	@param	reply	Original request identity retained with graph or installed route data.
*	@note	Only pending geometry following may relax goal; commit and callbacks remain strict.
**/
inline bool Nav_ActorIdentityMatches( const nav_query_identity_t &current, const nav_query_identity_t &reply,
	const bool check_sequence = true, const bool check_goal = true ) {
	// Slot, incarnation, behavior, order, epoch and geometry ownership are never optional.
	return current.entity >= 0 && current.entity == reply.entity && current.incarnation == reply.incarnation &&
		current.state == reply.state && current.crowd == reply.crowd && current.epoch == reply.epoch &&
		current.generation == reply.generation && current.topology == reply.topology &&
		( !check_sequence || current.sequence == reply.sequence ) && ( !check_goal || current.goal == reply.goal );
}

/**
*	@brief	Apply bounded retry after terminal failures even when behavior calls force every tick.
*	@return	True when this request must wait; matching Pending requests are handled before this gate.
*	@note	Forced first requests and successful-route refreshes preserve their existing semantics.
**/
inline bool Nav_ActorQueryShouldBackoff( const nav_actor_query_state_t state, const bool cooldown_elapsed,
	const bool force, const bool worker_mode ) {
	// Terminal worker failures cannot bypass the cooldown and generate an unbounded forced retry burst.
	const bool terminal = state == nav_actor_query_state_t::NoRoute || state == nav_actor_query_state_t::Stale ||
		state == nav_actor_query_state_t::Cancelled || state == nav_actor_query_state_t::ResourceLimited;
	return !cooldown_elapsed && ( !force || ( worker_mode && terminal ) );
}

/**
*	@brief	Prioritize actual serialized doorway admission while ordinary actor work retains its normal tier.
*	@return	Priority zero for a released member of its current serialized order; otherwise priority two.
*	@note	Default release flags and release flags belonging to another queue cannot authorize admission.
**/
inline uint32_t Nav_ActorQueryPriority( const bool has_group, const bool serialized_ingress, const bool ingress_released,
	const bool serialized_egress, const bool egress_released ) {
	// Admission is meaningful only inside the corresponding live group's serialized queue.
	const bool admitted = has_group && ( ( serialized_ingress && ingress_released ) || ( serialized_egress && egress_released ) );
	return admitted ? 0u : 2u;
}

/**
*	@brief	Compare complete native policy values without byte comparisons, padding or live callbacks.
*	@note	The owner normalizes hull/radius/shape before comparison; graph cost is captured separately.
**/
template<typename Policy>
bool Nav_ActorNativePolicyMatches( const Policy &accepted, const Policy &current ) {
	// Every native movement/steering limit must match the route's validated physical contract.
	return accepted.agent_radius == current.agent_radius && accepted.agent_mins == current.agent_mins &&
		accepted.agent_maxs == current.agent_maxs && accepted.trace_shape == current.trace_shape &&
		accepted.min_step_normal == current.min_step_normal && accepted.min_step_height == current.min_step_height &&
		accepted.max_step_height == current.max_step_height && accepted.max_drop_height == current.max_drop_height &&
		accepted.enable_max_drop_height_cap == current.enable_max_drop_height_cap && accepted.max_drop_height_cap == current.max_drop_height_cap &&
		accepted.min_portal_width == current.min_portal_width && accepted.waypoint_radius == current.waypoint_radius &&
		accepted.rebuild_goal_dist_2d == current.rebuild_goal_dist_2d && accepted.rebuild_goal_dist_3d == current.rebuild_goal_dist_3d &&
		accepted.rebuild_interval == current.rebuild_interval && accepted.allow_small_obstruction_jump == current.allow_small_obstruction_jump &&
		accepted.max_obstruction_jump_height == current.max_obstruction_jump_height && accepted.step_clearance == current.step_clearance &&
		accepted.ignore_visibility == current.ignore_visibility && accepted.ignore_infront == current.ignore_infront &&
		accepted.allow_gap_jumping == current.allow_gap_jumping && accepted.max_jump_distance == current.max_jump_distance &&
		accepted.min_gap_width == current.min_gap_width && accepted.ignore_disabled_edges == current.ignore_disabled_edges &&
		accepted.allow_entity_deflection == current.allow_entity_deflection;
}

/**
*	@brief	Reject a pending graph computed for obsolete tactical cost preferences.
*	@note	Cost mutation without an explicit AI reset still cannot silently relabel the solved request.
**/
inline bool Nav_ActorCostPolicyMatches( const nav_query_cost_policy_t &accepted, const nav_query_cost_policy_t &current ) {
	return accepted.corridor_multiplier == current.corridor_multiplier && accepted.stair_multiplier == current.stair_multiplier &&
		accepted.stair_min == current.stair_min && accepted.stair_max == current.stair_max;
}

/**
*	@brief	Require exact live request ownership and native/tactical policies for attachment accounting.
*	@note	A changed goal or reused incarnation cannot be counted as a harmless moving-origin detachment.
**/
template<typename Policy>
bool Nav_ActorPhysicalRequestMatches( const nav_query_identity_t &current, const nav_query_identity_t &pending,
	const Policy &current_policy, const Policy &pending_policy,
	const nav_query_cost_policy_t &current_cost, const nav_query_cost_policy_t &pending_cost ) {
	// Every ownership and physical/tactical input must still belong to the rejected request.
	return Nav_ActorIdentityMatches( current, pending ) && Nav_ActorNativePolicyMatches( pending_policy, current_policy ) &&
		Nav_ActorCostPolicyMatches( pending_cost, current_cost );
}

/**
*	@brief	Distinguish a superseded goal from obsolete ownership or physical/tactical policy.
*	@note	This is accounting only; it never authorizes an obsolete reply or relaxes native commit checks.
**/
template<typename Policy>
bool Nav_ActorGoalOnlySuperseded( const nav_query_identity_t &current, const nav_query_identity_t &pending,
	const Policy &current_policy, const Policy &pending_policy,
	const nav_query_cost_policy_t &current_cost, const nav_query_cost_policy_t &pending_cost ) {
	// Only the goal may differ; replacement lifetimes, sequences and policy changes remain stale failures.
	return current.goal != pending.goal && Nav_ActorIdentityMatches( current, pending, true, false ) &&
		Nav_ActorNativePolicyMatches( pending_policy, current_policy ) && Nav_ActorCostPolicyMatches( pending_cost, current_cost );
}

/**
*	@brief	Require a live, healthy actor's exact alive status before navigation or live think work.
*	@param	in_use	Whether the pool currently owns this incarnation as a live entity.
*	@param	health	Current actor health, not a copied request field.
*	@param	life_status	Current scalar lifecycle status.
*	@param	alive_status	Engine's actual alive enumerator, which may be zero rather than a bit.
*	@return	True only for a current healthy alive incarnation.
*	@note	A zero alive enumerator cannot be tested with bit-mask membership.
**/
inline bool Nav_ActorMayNavigate( const bool in_use, const int32_t health, const int32_t life_status, const int32_t alive_status ) {
	// Reject freed, dying and dead actors even when a direct lifecycle callback leaves positive health.
	return in_use && health > 0 && life_status == alive_status;
}
