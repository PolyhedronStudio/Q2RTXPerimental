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
// We only need these in C++ anyway..
// <Q2RTX>: WID: TODO: Make sure this works fine with C code if needed.
#ifdef __cplusplus
    //! The two bits at the top of the entityState->event field
    //! will be incremented with each change in the event. So
    //! that an identical event started twice in a row, can be 
    //! distinguished from the first fired event. 
    //!
    //! To get the actual event value,off it with the value with ~EV_EVENT_BITS
    static constexpr int32_t EV_EVENT_BIT1 = BIT( 8 ); // 256 // 0x00000100;// BIT( 0 ); // OLD: 0x00000100
    static constexpr int32_t EV_EVENT_BIT2 = BIT( 9 ); // 512 // 0x00000200;// BIT( 1 ); // OLD: 0x00000200
    static constexpr int32_t EV_EVENT_BITS = ( EV_EVENT_BIT1 | EV_EVENT_BIT2 );
    /**
    *   @return The actual event ID value by offing the given event value with EV_EVENT_BITS.
    **/
    static inline const int32_t EV_GetEntityEventValue( const int32_t eventValue ) {
        return ( eventValue & ~EV_EVENT_BITS );
    }

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
    /**
    *   <Q2RTXP>: We can change this value as we want, because we still have an acknowledgement system
    *             in-place for the frame we send the event in, so the client will always get the event
    *             even if there is some packet loss.
    *
    *             However, to keep things feeling responsive, we can reduce this value to 150ms at 40hz tickrate,
    *             which should still be sufficient for our network conditions while improving responsiveness.
    *
    *             (It remains at 6 frames duration at 40hz tickrate using 150ms.)
    **/
    //! Time in milliseconds that an entity event will be considered valid.
    static constexpr int32_t EVENT_VALID_MSEC = 150; // Original: 300ms at 20hz tickrate.
#endif // #ifdef __cplusplus__




/**
*	@description entity_state_t->event values
*				 ertity events are for effects that take place reletive
*				 to an existing entities origin.  Very network efficient.
*				 All muzzle flashes really should be converted to events...
**/
typedef enum engine_entity_event_e {
    // Engine Specific:
    EV_NONE = 0,
    //! Game can start entities from this index.
    EV_ENGINE_MAX = EV_NONE + 1,
} engine_entity_events_t;
typedef uint32_t entity_event_t;