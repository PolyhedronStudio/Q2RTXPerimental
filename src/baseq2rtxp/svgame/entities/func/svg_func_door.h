/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door'.
*
*
********************************************************************/
#pragma once


/**
*   Spawnflags:
**/
//! Door starts opened.
static constexpr int32_t DOOR_SPAWNFLAG_START_OPEN		= BIT( 0 );
//! Door starts in reversed(its opposite) state.
static constexpr int32_t DOOR_SPAWNFLAG_REVERSE			= BIT( 1 );
//! Door does damage when blocked, trying to 'crush' the entity blocking its path.
static constexpr int32_t DOOR_SPAWNFLAG_CRUSHER			= BIT( 2 );
//! Door won't be triggered by monsters.
static constexpr int32_t DOOR_SPAWNFLAG_NOMONSTER		= BIT( 3 );
//! Door does not automatically wait for returning to its initial(can be reversed) state.
static constexpr int32_t DOOR_SPAWNFLAG_TOGGLE			= BIT( 4 );
//! Door has animation.
static constexpr int32_t DOOR_SPAWNFLAG_ANIMATED		= BIT( 5 );
//! Door has animation.
static constexpr int32_t DOOR_SPAWNFLAG_ANIMATED_FAST	= BIT( 6 );
//! Door moves into X axis instead of Z.
static constexpr int32_t DOOR_SPAWNFLAG_X_AXIS			= BIT( 7 );
//! Door moves into Y axis instead of Z.
static constexpr int32_t DOOR_SPAWNFLAG_Y_AXIS			= BIT( 8 );



/**
*	@brief
**/
void door_use_areaportals( edict_t *self, const bool open );
/**
*	@brief
**/
void door_blocked( edict_t *self, edict_t *other );
/**
*	@brief
**/
void door_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
/**
*	@brief
**/
void door_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
/**
*	@brief
**/
void door_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );