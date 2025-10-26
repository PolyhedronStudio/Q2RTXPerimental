/********************************************************************
*
*
*   Entity Events:
*
*
********************************************************************/
#pragma once



/**
*
*
*
*   Entity Event Bit Flags:
*
*
*
**/
//! The two bits at the top of the entityState->event field
//! will be incremented with each change in the event. So
//! that an identical event started twice in a row, can be 
//! distinguished from the first fired event. 
//!
//! To get the actual event value,off it with the value with ~EV_EVENT_BITS
static constexpr int32_t EV_EVENT_BIT1 = BIT( 8 ); // 0x00000100;// BIT( 0 ); // OLD: 0x00000100
static constexpr int32_t EV_EVENT_BIT2 = BIT( 9 ); // 0x00000200;// BIT( 1 ); // OLD: 0x00000200
static constexpr int32_t EV_EVENT_BITS = ( EV_EVENT_BIT1 | EV_EVENT_BIT2 );



/**
*
*	Reasoning from AI to use 300ms for EVENT_VALID_MSEC at 40hz tickrate:
*
*	For a 40hz tickrate, I would recommend keeping the 300ms value because :
*
*	1. The event validity window needs to account for :
*		-Network latency( typically 30 - 100ms )
*		- Packet loss recovery( 1 - 2 frames )
*		- Event execution and visualization time
*		- Client / server state synchronization
*
*		2. Even though your tickrate is higher( 40hz vs original 20hz ) :
*		- Events still need similar real - world time to play out( animations, sounds, effects )
*		- Network conditions haven't fundamentally changed
*		- Many events( like teleports, death animations, weapon switches ) take multiple frames

*		3. If you reduced it proportionally to 150ms( 6 frames at 40hz ) :
*		- Events might get cut off before completing
*		- Network jitter could cause more event drops
*		- Visual / audio synchronization could break
*		- Complex event chains might fail
*
*	Therefore, I recommend keeping `EVENT_VALID_MSEC` at 300ms even at 40hz.The higher tickrate
*	will give you better movement and hit detection, but event validity timing is more about
*	real - world considerations( network, animations, human perception ) than pure engine tickrate.
*
*	If you really want to optimize it, you could potentially reduce it to 250ms( 10 frames ),
*	but I wouldn't go lower than that without extensive testing of all event types under various network conditions.
**/
//! Time in milliseconds that an entity event will be considered valid.
static constexpr int32_t EVENT_VALID_MSEC = 300;



/**
*
*
*
*   Entity Events:
*
*
*
**/
/**
*	@description entity_state_t->event values
*				 Ertity events are for effects that take place reletive
*				 to an existing entities origin. Very network efficient.
*				 All muzzle flashes really should be converted to events...
**/
typedef enum sg_entity_events_e {
    /**
    *   Footstep Events:
    **/
    //! For players.
    EV_PLAYER_FOOTSTEP = EV_ENGINE_MAX,
	//! For other entities.
    EV_OTHER_FOOTSTEP,
	//! For ladder climbing entities.
    EV_FOOTSTEP_LADDER,

    /**
	*   Water (Footstep-)Splash Events:
    **/
	//! For when the feet touch water.
	EV_WATER_ENTER_FEET, // Also plays a possible footstep sound.
	//! For when deep diving in water.
	EV_WATER_ENTER_WAIST,
	//! For when the head goes underwater.
    EV_WATER_ENTER_HEAD,
	//! For when the feet leave water.
    EV_WATER_LEAVE_FEET,
	//! For when waist leaves water.
    EV_WATER_LEAVE_WAIST,
	//! For when head leaves water.
    EV_WATER_LEAVE_HEAD,

    /**
    *   Player/(Humanoid-)Monster Events:   
    **/
    //! For when jumping up.
    EV_JUMP_UP,
    //! Or Fall, FallShort, FallFar??
    EV_JUMP_LAND,

    //! "Short" Fell to the ground.
    EV_FALL_SHORT,
    //! "Short" fall to the ground.
    EV_FALL_MEDIUM,
    // "Hard Far" fall to the ground.
    EV_FALL_FAR,


    //
    // External Events:
    //
    //! Reload Weapon.
    EV_WEAPON_RELOAD,
    //! Weapon Primary Fire.
    EV_WEAPON_PRIMARY_FIRE,
    //! Weapon Secondary Fire.
    EV_WEAPON_SECONDARY_FIRE,

    //! Weapon Holster/Draw:
    EV_WEAPON_HOLSTER_AND_DRAW,

    // TODO: We really do wanna split weapon draw and holstering, but, alas, lack animation skills.
    //! Draw Weapon.
    EV_WEAPON_DRAW,
    //! Holster Weapon.
    EV_WEAPON_HOLSTER,
    
    //!
    //! The maximum predictable player events.
    //! 
    PS_EV_MAX,

    /**
    *   Teleport Effects:
    **/
    //! Player Login.
    EV_PLAYER_LOGIN,
    //! Player Logout.
    EV_PLAYER_LOGOUT,
    //! Player teleporting.
    EV_PLAYER_TELEPORT, // <Q2RTXP>: TODO: PS_EV_MAX? Or... just use these for PS_EV_...

	//! Other entity teleporting.
    EV_OTHER_TELEPORT,

    /**
    *   Item Events:
    **/
    EV_ITEM_RESPAWN,

	//! Maximum number of entity events, must be last.
    EV_GAME_MAX
} sg_entity_events_t;



/**
*
*
*
*   Entity Event String Names:
*
*
*
**/
static constexpr const char *sg_event_string_names[ /*EV_GAME_MAX*/ ] = {
    "EV_ENGINE_NONE",

    "EV_PLAYER_FOOTSTEP",
	"EV_OTHER_FOOTSTEP",
    "EV_FOOTSTEP_LADDER",

	"EV_WATER_ENTER_FEET",
	"EV_WATER_ENTER_WAIST",
	"EV_WATER_ENTER_HEAD",

	"EV_WATER_LEAVE_FEET",
	"EV_WATER_LEAVE_WAIST",
	"EV_WATER_LEAVE_HEAD",

    "EV_JUMP_UP",
    "EV_JUMP_LAND",

    "EV_FALL_SHORT",
    "EV_FALL_MEDIUM",
    "EV_FALL_FAR",


    "EV_WEAPON_RELOAD",
    "EV_WEAPON_PRIMARY_FIRE",
    "EV_WEAPON_SECONDARY_FIRE",
    "EV_WEAPON_HOLSTER_AND_DRAW",

    "EV_WEAPON_DRAW",
    "EV_WEAPON_HOLSTER",

    "PS_EV_MAX",

    "EV_PLAYER_LOGIN",
    "EV_PLAYER_LOGOUT",

    "EV_PLAYER_TELEPORT",
    "EV_OTHER_TELEPORT",

    "EV_ITEM_RESPAWN",
};

/**
*   @brief   Compile-time assert to ensure that the sg_event_string_names array size matches EV_GAME_MAX.
**/
static_assert( std::size( sg_event_string_names ) == EV_GAME_MAX, "sg_event_string_names array size does not match expected size(EV_GAME_MAX), make sure all string identifiers for each event exist and are validly indexed." );