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
static constexpr int32_t FUNC_DOOR_START_OPEN    = BIT( 0 );
static constexpr int32_t FUNC_DOOR_REVERSE       = BIT( 1 );
static constexpr int32_t FUNC_DOOR_CRUSHER       = BIT( 2 );
static constexpr int32_t FUNC_DOOR_NOMONSTER     = BIT( 3 );
static constexpr int32_t FUNC_DOOR_TOGGLE        = BIT( 4 );
static constexpr int32_t FUNC_DOOR_X_AXIS        = BIT( 5 );
static constexpr int32_t FUNC_DOOR_Y_AXIS        = BIT( 6 );



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