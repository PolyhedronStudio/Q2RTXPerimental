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
static constexpr int32_t EV_EVENT_BIT1 = BIT( 0 ); // OLD: 0x00000100
static constexpr int32_t EV_EVENT_BIT2 = BIT( 1 ); // OLD: 0x00000200
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
    *   Teleport Effects:
    **/
    //! Player teleporting.
    EV_PLAYER_TELEPORT = EV_ENGINE_MAX + 1,
	//! Other entity teleporting.
    EV_OTHER_TELEPORT,

    /**
	*   Falling Events:
    **/
    EV_FALL,
    EV_FALLSHORT,
    EV_FALLFAR,

    /**
	*   Footstep Events:
    **/
    EV_FOOTSTEP,
    EV_FOOTSTEP_LADDER,

    /**
    *   Item Events:
    **/
    EV_ITEM_RESPAWN,

	//! Maximum number of entity events, must be last.
    EV_GAME_MAX
} sg_entity_events_t;

