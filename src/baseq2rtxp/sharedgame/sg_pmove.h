/********************************************************************
*
*
*	SharedGame: Player Movement
*
*
********************************************************************/
#pragma once


/**
*	
*
*	PMove Constants:
* 
* 
**/
/**
*	(Predictable-)Player State Events:
**/
//! Enumerator event numbers, also act as string indices.
typedef enum {
	PS_EV_NONE = 0,

	//
	// External Events:
	//
	//! Reload Weapon.
	PS_EV_WEAPON_RELOAD,
	//! Weapon Primary Fire.
	PS_EV_WEAPON_PRIMARY_FIRE,
	//! Weapon Secondary Fire.
	PS_EV_WEAPON_SECONDARY_FIRE,
	
	//! Weapon Holster/Draw:
	PS_EV_WEAPON_HOLSTER_AND_DRAW,
	// TODO: We really do wanna split weapon draw and holstering, but, alas, lack animation skills.
	//! Draw Weapon.
	PS_EV_WEAPON_DRAW,
	//! Holster Weapon.
	PS_EV_WEAPON_HOLSTER,

	//
	// Non External Events:
	// 
	//! For when jumping up.
	PS_EV_JUMP_UP,
	//! Or Fall, FallShort, FallFar??
	PS_EV_JUMP_LAND,

	//! Maximum.
	PS_EV_MAX,
} sg_player_state_event_t;
//! String representatives.
extern const char *sg_player_state_event_strings[ PS_EV_MAX ];


/**
*	@brief	Default player movement parameter constants.
**/
typedef struct default_pmoveParams_s {
	//! Stop speed.
	static constexpr float pm_stop_speed	= 100.f;
	//! Server determined maximum speed.
	static constexpr float pm_max_speed		= 300.f;
	//! Velocity that is set for jumping. (Determines the height we aim for.)
	static constexpr float pm_jump_height	= 270.f;

	//! General up/down movespeed for on a ladder.
	static constexpr float pm_ladder_speed			= 200.f;
	//! Maximum 'strafe' side move speed while on a ladder.
	static constexpr float pm_ladder_sidemove_speed = 150.f;
	//! Ladder modulation scalar for when being in-water and climbing a ladder.
	static constexpr float pm_ladder_mod			= 0.5f;

	//! Speed for when ducked and crawling on-ground.
	static constexpr float pm_duck_speed	= 100.f;
	//! Speed for when moving in water(swimming).
	static constexpr float pm_water_speed	= 400.f;
	//! Speed for when flying.
	static constexpr float pm_fly_speed		= 400.f;

	//! General acceleration.
	static constexpr float pm_accelerate		= 10.f;
	//! General water acceleration.
	static constexpr float pm_water_accelerate	= 10.f;

	//! General friction.
	static constexpr float pm_friction = 6.f;
	//! General water friction.
	static constexpr float pm_water_friction = 1.f;
} default_pmoveParams_t;

/**
*	Default BoundingBoxes:
**/
//! For when player is standing straight up.
static constexpr Vector3 PM_BBOX_STANDUP_MINS	= { -16.f, -16.f, -36.f };
static constexpr Vector3 PM_BBOX_STANDUP_MAXS	= { 16.f, 16.f, 36.f };
static constexpr float   PM_VIEWHEIGHT_STANDUP	= 30.f;
//! For when player is crouching.
static constexpr Vector3 PM_BBOX_DUCKED_MINS	= { -16.f, -16.f, -36.f };
static constexpr Vector3 PM_BBOX_DUCKED_MAXS	= { 16.f, 16.f, 8.f };
static constexpr float   PM_VIEWHEIGHT_DUCKED	= 4.f;
//! For when player is gibbed out.
static constexpr Vector3 PM_BBOX_GIBBED_MINS	= { -16.f, -16.f, 0.f };
static constexpr Vector3 PM_BBOX_GIBBED_MAXS	= { 16.f, 16.f, 24.f };
static constexpr float   PM_VIEWHEIGHT_GIBBED	= 8.f;

/**
*	StepHeight:
**/
//! Minimal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float PM_MIN_STEP_SIZE = 4.f;
//! Maximal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float PM_MAX_STEP_SIZE = 18.f;

/**
*	Slide Move Results:
**/
//! Succesfully performed the move.
static constexpr int32_t PM_SLIDEMOVEFLAG_MOVED = BIT( 0 );
//! It was blocked at some point, doesn't mean it didn't slide along the blocking object.
static constexpr int32_t PM_SLIDEMOVEFLAG_BLOCKED = BIT( 1 );
//! It is trapped.
static constexpr int32_t PM_SLIDEMOVEFLAG_TRAPPED = BIT( 2 );
//! Blocked by a literal wall.
static constexpr int32_t PM_SLIDEMOVEFLAG_WALL_BLOCKED = BIT( 3 );
//! Touched at least a single plane along the way.
static constexpr int32_t PM_SLIDEMOVEFLAG_PLANE_TOUCHED = BIT( 4 );



/**
*
*
*	PMove Functions:
*
*
**/
/**
*	@brief	Shard Game Player Movement code implementation:
**/
void SG_PlayerMove( pmove_t *pmove, pmoveParams_t *params );
/**
*	@brief	Useful to in the future, configure the player move parameters to game specific scenarions.
**/
void SG_ConfigurePlayerMoveParameters( pmoveParams_t *pmp );
