/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_explosive'.
*
*
********************************************************************/
#pragma once



/**
*   Spawnflags:
**/
//! Door starts opened.
static constexpr int32_t FUNC_EXPLOSIVE_SPAWNFLAG_SPAWN_ON_TRIGGER	= BIT( 0 );
//! Door starts opened.
static constexpr int32_t FUNC_EXPLOSIVE_SPAWNFLAG_RESPAWN_ON_TRIGGER= BIT( 1 );
//! Brush has animations.
static constexpr int32_t FUNC_EXPLOSIVE_SPAWNFLAG_ANIMATE_ALL		= BIT( 2 );
//! Brush has animations.
static constexpr int32_t FUNC_EXPLOSIVE_SPAWNFLAG_ANIMATE_ALL_FAST	= BIT( 3 );


/**
*   For readability's sake:
**/
//static constexpr svg_pushmove_state_t DOOR_STATE_OPENED = PUSHMOVE_STATE_TOP;
//static constexpr svg_pushmove_state_t DOOR_STATE_CLOSED = PUSHMOVE_STATE_BOTTOM;
//static constexpr svg_pushmove_state_t DOOR_STATE_MOVING_TO_OPENED_STATE = PUSHMOVE_STATE_MOVING_UP;
//static constexpr svg_pushmove_state_t DOOR_STATE_MOVING_TO_CLOSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;



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
*   @brief  Pain for door.
**/
void door_pain( edict_t *self, edict_t *other, float kick, int damage );
/**
*	@brief
**/
void door_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
/**
*	@brief
**/
void door_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
/**
*   @brief  Pain for door.
**/
void door_pain( edict_t *self, edict_t *other, float kick, int damage );
/**
*   @brief  Signal Receiving:
**/
void door_onsignalin( edict_t *self, edict_t *other, edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );