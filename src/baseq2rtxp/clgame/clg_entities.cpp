/********************************************************************
*
*
*	ClientGame: Entity Management.
*
*
********************************************************************/
#include "clg_local.h"

/**
*
*
*	Entity Utilities:
*
*
**/
#if USE_DEBUG
/**
*	@brief	For debugging problems when out - of - date entity origin is referenced
**/
void CLG_CheckEntityPresent( int32_t entityNumber, const char *what ) {
	server_frame_t *frame = &clgi.client->frame;

	if ( entityNumber == clgi.client->frame.clientNum + 1 ) {
		return; // player entity = current.
	}

	centity_t *e = &clg_entities[ entityNumber ]; //e = &cl_entities[entnum];
	if ( e && e->serverframe == frame->number ) {
		return; // current
	}

	if ( e && e->serverframe ) {
		Com_LPrintf( PRINT_DEVELOPER, "SERVER BUG: %s on entity %d last seen %d frames ago\n", what, entityNumber, frame->number - e->serverframe );
	} else {
		Com_LPrintf( PRINT_DEVELOPER, "SERVER BUG: %s on entity %d never seen before\n", what, entityNumber );
	}
}
#endif
/**
*	@return		The entity bound to the client's view. 
*	@remarks	(Can be the one we're chasing, instead of the player himself.)
**/
centity_t *CLG_Self( void ) {
	int32_t index = clgi.client->clientNum;

	if ( clgi.client->frame.ps.stats[ STAT_CHASE ] ) {
		index = clgi.client->frame.ps.stats[ STAT_CHASE ] - CS_PLAYERSKINS;
	}

	return &clg_entities[ index + 1 ];
}

/**
 * @return True if the specified entity is bound to the local client's view.
 */
const bool CLG_IsSelf( const centity_t *ent ) {
	//if ( ent == cgi.client->entity ) {
	//	return true;
	//}

	//if ( ( ent->current.effects & EF_CORPSE ) == 0 ) {

	//	if ( ent->current.model1 == MODELINDEX_PLAYER ) {

	//		if ( ent->current.client == cgi.client->client_num ) {
	//			return true;
	//		}

	//		const int16_t chase = cgi.client->frame.ps.stats[ STAT_CHASE ] - CS_CLIENTS;

	//		if ( ent->current.client == chase ) {
	//			return true;
	//		}
	//	}
	//}

	return false;
}