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
static constexpr int32_t DOOR_SPAWNFLAG_START_LOCKED	= BIT( 0 );
//! Door starts opened.
static constexpr int32_t DOOR_SPAWNFLAG_START_OPEN		= BIT( 1 );
//! Door starts in reversed(its opposite) state.
static constexpr int32_t DOOR_SPAWNFLAG_REVERSE			= BIT( 2 );
//! Door does damage when blocked, trying to 'crush' the entity blocking its path.
static constexpr int32_t DOOR_SPAWNFLAG_CRUSHER			= BIT( 3 );
//! Door won't be triggered by monsters.
static constexpr int32_t DOOR_SPAWNFLAG_NOMONSTER		= BIT( 4 );
//! Door fires targets when its touch area is triggered.
static constexpr int32_t DOOR_SPAWNFLAG_TOUCH_AREA_TRIGGERED = BIT( 5 );
//! Door fires targets when damaged.
static constexpr int32_t DOOR_SPAWNFLAG_DAMAGE_ACTIVATES= BIT( 6 );
//! Door does not automatically wait for returning to its initial(can be reversed) state.
static constexpr int32_t DOOR_SPAWNFLAG_TOGGLE			= BIT( 7 );
//! Door has animation.
static constexpr int32_t DOOR_SPAWNFLAG_ANIMATED		= BIT( 8 );
//! Door has animation.
static constexpr int32_t DOOR_SPAWNFLAG_ANIMATED_FAST	= BIT( 9 );
//! Door moves into X axis instead of Z.
static constexpr int32_t DOOR_SPAWNFLAG_X_AXIS			= BIT( 10 );
//! Door moves into Y axis instead of Z.
static constexpr int32_t DOOR_SPAWNFLAG_Y_AXIS			= BIT( 11 );
//! Door always pushes into the negated direction of what the player is facing.
static constexpr int32_t DOOR_SPAWNFLAG_BOTH_DIRECTIONS = BIT( 12 );


/**
*   For readability's sake:
**/
static constexpr svg_pushmove_state_t DOOR_STATE_OPENED = PUSHMOVE_STATE_TOP;
static constexpr svg_pushmove_state_t DOOR_STATE_CLOSED = PUSHMOVE_STATE_BOTTOM;
static constexpr svg_pushmove_state_t DOOR_STATE_MOVING_TO_OPENED_STATE = PUSHMOVE_STATE_MOVING_UP;
static constexpr svg_pushmove_state_t DOOR_STATE_MOVING_TO_CLOSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;



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
void door_touch( edict_t *self, edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
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