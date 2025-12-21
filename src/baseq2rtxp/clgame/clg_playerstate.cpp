/********************************************************************
*
*
*	ClientGame: Entity Management.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_entities.h"
#include "sharedgame/sg_entity_events.h"
#include "clgame/clg_playerstate.h"
#include "clgame/clg_predict.h"

//// Needed for EAX spatialization to work:
//#include "clgame/clg_local_entities.h"
//#include "clgame/local_entities/clg_local_entity_classes.h"
//#include "clgame/local_entities/clg_local_env_sound.h"

#include "sharedgame/pmove/sg_pmove.h"
#include "sharedgame/sg_entities.h"


/**
*
*
*   Debugging:
*
*
**/
//! Debugging output for player state duplication.
//#define DEBUG_PLAYERSTATE_DUPLICATION
/**
*   @brief  Debugging IDs for player state duplication reasons.
**/
static constexpr int32_t PS_DUP_DEBUG_OLDFRAME_INVALID = BIT( 0 );
static constexpr int32_t PS_DUP_DEBUG_OLDFRAME_NOT_LASTFRAME_NUMBER = BIT( 1 );
static constexpr int32_t PS_DUP_DEBUG_ORIGIN_OFFSET_LARGER_THAN_256 = BIT( 2 );
static constexpr int32_t PS_DUP_DEBUG_EVENT_TELEPORT = BIT( 3 );
static constexpr int32_t PS_DUP_DEBUG_TIME_TELEPORT = BIT( 4 );
static constexpr int32_t PS_DUP_DEBUG_CLIENTNUM_MISMATCH = BIT( 5 );
static constexpr int32_t PS_DUP_DEBUG_NOLERP = BIT( 6 );



/**
*
*
*
*   Player State Transitioning(Duplication/Lerping etc):
*
*
*
**/
/**
*   @brief  Duplicates old player state, into playerstate. Used as a utility function.
*   @param ps Pointer to the state that we want to copy data into.
*   @param ops Pointer to the state's data source.
*   @param debugID Debugging ID for what caused the duplication.
**/
static void duplicate_player_state( player_state_t *ps, player_state_t *ops, const int32_t debugID ) {
    *ops = *ps;

    // Debugging output for player state duplication.
    #ifdef DEBUG_PLAYERSTATE_DUPLICATION
    if ( debugID == PS_DUP_DEBUG_OLDFRAME_INVALID ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_OLDFRAME_INVALID\n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_OLDFRAME_NOT_LASTFRAME_NUMBER ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_OLDFRAME_NOT_LASTFRAME_NUMBER \n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_ORIGIN_OFFSET_LARGER_THAN_256 ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_ORIGIN_OFFSET_LARGER_THAN_256 \n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_EVENT_TELEPORT ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_EVENT_TELEPORT \n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_TIME_TELEPORT ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_TIME_TELEPORT \n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_CLIENTNUM_MISMATCH ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_CLIENTNUM_MISMATCH \n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_NOLERP ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_NOLERP \n", cl.frame.number );
    } else {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) No player state duplicating. \n", cl.frame.number );
    }
    #endif
}
/**
*   @brief  Handles player state transition ON the given client entity
*           (thus can be predicted player entity, or the frame's playerstate client number matching entitiy) 
*           between old and new server frames. 
*           Duplicates old state into current state in case of no lerping conditions being met.
*   @param clent Pointer to the client entity on which the playerstates of each frame are to be processed.
*   @note   This is used for demo playerback, cl_nopredict = 0, as well as in the prediction codepath.
**/
void CLG_PlayerState_Transition( centity_t *clent, server_frame_t *oldframe, server_frame_t *frame, const int32_t framediv ) {
    // Find player states to interpolate between
    player_state_t *ps = &frame->ps;
    player_state_t *ops = &oldframe->ps;

    // If the player entity was within the range of lastFrameNumber and frame->number,
    // and had any teleport events going on, duplicate the player state into the old player state,
    // to prevent it from lerping afar distance.
    const int32_t entityEvent = EV_GetEntityEventValue( clent->current.event );

	// Calculate last frame number.
    const int32_t lastFrameNumber = frame->number - framediv;

    // Used for testing whether we teleported too far away.
    const Vector3 originDifference = QM_Vector3Fabs( ops->pmove.origin - ps->pmove.origin );
	const double fabsOriginDifference = std::max( std::max( originDifference.x, originDifference.y ), originDifference.z );
    
	// Did we duplicate state, or not? (If so, prevent firing events again).
	bool duplicatedState = false;
    
    // No lerping if previous frame was dropped or invalid.
    if ( !oldframe->valid ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_OLDFRAME_INVALID );
        duplicatedState = true;
        //return;
    // Duplicate state in case the stored old frame number does not match to the expected last frame number.
    }
    else if ( oldframe->number != lastFrameNumber ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_OLDFRAME_NOT_LASTFRAME_NUMBER );
        duplicatedState = true;
        //return;
    // No lerping if POV number changed.
    } else if ( oldframe->ps.clientNumber != frame->ps.clientNumber ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_CLIENTNUM_MISMATCH );
        duplicatedState = true;
        //return;
    // No lerping in case of the enabled developer option.
    }
    else if ( cl_nolerp->integer == 1 ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_NOLERP );
        duplicatedState = true;
        //return;
    // No lerping if player entity was teleported (origin check).
    } else if ( fabsOriginDifference > 256.0 ) {
    //} else if ( fabsf( ops->pmove.origin.x - ps->pmove.origin.x ) > 256 ||
    //    fabsf( ops->pmove.origin.y - ps->pmove.origin.y ) > 256 ||
    //    fabsf( ops->pmove.origin.z - ps->pmove.origin.z ) > 256 ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_ORIGIN_OFFSET_LARGER_THAN_256 );
        duplicatedState = true;
        //return;
    // No lerping if player entity was teleported (event check).
    } else if ( clent->serverframe > lastFrameNumber && clent->serverframe <= frame->number &&
        ( entityEvent == EV_PLAYER_TELEPORT || entityEvent == EV_OTHER_TELEPORT ) ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_EVENT_TELEPORT );
        duplicatedState = true;
        //return;
    // No lerping if teleport bit was flipped.
    } else if ( ( ops->pmove.pm_flags ^ ps->pmove.pm_flags ) & PMF_TIME_TELEPORT ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_TIME_TELEPORT );
        duplicatedState = true;
        //return;
    // No lerping if teleport bit was flipped.
    }
    //else if ( ( ops->rdflags ^ ps->rdflags ) & RDF_TELEPORT_BIT ) {
    //    duplicate_player_state( ps, ops );
    //    return;
    //}

	// Make sure to check for playerstate events changes.
    if ( !duplicatedState ) {
        CLG_CheckPlayerstateEvents( ops, ps );
    }
}