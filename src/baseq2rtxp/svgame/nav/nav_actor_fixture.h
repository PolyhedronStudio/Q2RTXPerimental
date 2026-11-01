#pragma once
#include <cstdint>

/**
*	@brief	Apply bounded opt-in stimuli to the compiled debug monster's real behavior dispatcher.
*	@note	Requires nav_actor_fixture_enable and an explicitly validated native fixture profile.
**/
void Nav_Actor_FixtureCommand( void );

/**
*	@brief	Advance one opt-in 64-actor benchmark stimulus on the actual 40Hz owner tick.
*	@note	Inactive outside explicitly requested fixtures; never computes a route or modifies worker inputs off-thread.
**/
void Nav_ActorBenchmarkTickMainThread( void );
/**
*	@brief	Seal bounded benchmark phases after the measured tick and report outside its timing interval.
**/
void Nav_ActorBenchmarkEndFrameMainThread( void );

struct svg_monster_base_t;
/**
*	@brief	Count a successful native route installation for an exact opt-in benchmark lifetime.
*	@param	actor	Live owner-thread actor whose complete native route was just installed.
*	@note	Inactive outside the benchmark; entity number, spawn count and incarnation must all match.
**/
void Nav_ActorBenchmarkRecordCommitMainThread( const svg_monster_base_t *actor );

/**
*	@brief	Attribute native benchmark rejection sites without changing request disposition.
*	@param	site	Policy zero, tactical cost one, corridor attachment two, reply identity three.
*	@note	Fixed counters report after the tick; inactive in ordinary gameplay.
**/
void Nav_ActorBenchmarkRecordRejectionMainThread( const uint32_t site );
