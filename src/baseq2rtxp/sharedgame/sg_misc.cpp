/********************************************************************
*
*
*	SharedGame: Misc Functions:
*
*
********************************************************************/
#include "shared/shared.h"

#include "sg_shared.h"
#include "sg_misc.h"

//! Uncomment to enable debug print output of predictable events.
#define _DEBUG_PRINT_PREDICTABLE_EVENTS

//! String representatives.
const char *sg_player_state_event_strings[ PS_EV_MAX ] = {
	"None",			// PS_EV_NONE
	"Jump Up",		// PS_EV_JUMP_UP
	"Jump Land",	// PS_EV_JUMP_LAND
};

/**
*	@brief	Adds a player movement predictable event to the player move state.
**/
void SG_PMoveState_AddPredictableEvent( const uint8_t newEvent, const uint8_t eventParm, player_state_t *ps ) {
	// Event sequence index.
	const int32_t sequenceIndex = ps->eventSequence & ( MAX_PS_EVENTS - 1 );

	// Ensure it is within bounds.
	if ( newEvent < PS_EV_NONE || newEvent >= PS_EV_MAX ) {
		#ifdef CLGAME_INCLUDE
		SG_DPrintf( "CLGame PMoveState INVALID(Out of Bounds) Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, sg_player_state_event_strings[ newEvent ], eventParm );
		#else
		//SG_DPrintf( "SVGame PMoveState INVALID(Out of Bounds) Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, sg_player_state_event_strings[ newEvent ], eventParm );
		#endif
	}
	#ifndef CLGAME_INCLUDE
	// Add predicted player move state event.
	ps->events[ sequenceIndex ] = newEvent;
	ps->eventParms[ sequenceIndex ] = eventParm;
	ps->eventSequence++;

	// Debug Output.
	#ifdef _DEBUG_PRINT_PREDICTABLE_EVENTS
	{
		#ifdef CLGAME_INCLUDE
		SG_DPrintf( "CLGame PMoveState Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, sg_player_state_event_strings[ newEvent ], eventParm );
		#else
		SG_DPrintf( "SVGame PMoveState Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, sg_player_state_event_strings[ newEvent ], eventParm );
		#endif
	}
	#endif
	#endif
}