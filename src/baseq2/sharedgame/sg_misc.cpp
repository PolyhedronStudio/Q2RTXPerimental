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

/**
*	@brief	Adds a player movement predictable event to the player move state.
**/
//void SG_PMoveState_AddPredictableEvent( const uint8_t newEvent, const uint8_t eventParm, pmove_state_t *ps ) {
//	// Event sequence index.
//	const int32_t sequenceIndex = ps->eventSequence & ( MAX_PS_EVENTS - 1 );
//
//	#ifdef _DEBUG
//	{
//		//char buf[ 256 ];
//		#ifdef CLGAME_INCLUDE
//		SG_DPrintf( "CLGame PMoveState Event(sequenceIndex: %i): newEvent(%i), eventParm(%i)\n", sequenceIndex, newEvent, eventParm );
//		#else
//		SG_DPrintf( "SVGame PMoveState Event(sequenceIndex: %i): newEvent(%i), eventParm(%i)\n", sequenceIndex, newEvent, eventParm );
//		#endif
//		//}
//	}
//	#endif
//
//	// Add predicted player move state event.
//	ps->events[ sequenceIndex ] = newEvent;
//	ps->eventParms[ sequenceIndex ] = eventParm;
//	ps->eventSequence++;
//}