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



/**
*
*
*	Events: WID: TODO: Should probably move to their own file sooner or later.
*
*
**/
//! String representatives.
const char *sg_player_state_event_strings[ PS_EV_MAX ] = {
	"None",			        // PS_EV_NONE
    "Weapon Reload",        // PS_EV_WEAPON_RELOAD
    "Weapon Primary Fire",  // PS_EV_WEAPON_PRIMARY_FIRE
    "Weapon Secondary Fire",// PS_EV_WEAPON_SECONDARY_FIRE
	"Jump Up",		        // PS_EV_JUMP_UP
	"Jump Land",	        // PS_EV_JUMP_LAND
};

/**
*	@brief	Adds a player movement predictable event to the player move state.
**/
void SG_PlayerState_AddPredictableEvent( const uint8_t newEvent, const uint8_t eventParm, player_state_t *playerState ) {
	// Event sequence index.
	const int32_t sequenceIndex = playerState->eventSequence & ( MAX_PS_EVENTS - 1 );

	// Ensure it is within bounds.
	if ( newEvent < PS_EV_NONE || newEvent >= PS_EV_MAX ) {
		#ifdef CLGAME_INCLUDE
		SG_DPrintf( "CLGame PMoveState INVALID(Out of Bounds) Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, sg_player_state_event_strings[ newEvent ], eventParm );
		#else
		//SG_DPrintf( "SVGame PMoveState INVALID(Out of Bounds) Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, sg_player_state_event_strings[ newEvent ], eventParm );
		#endif
	}
	//#ifndef CLGAME_INCLUDE
	// Add predicted player move state event.
    //playerState->events[ sequenceIndex ] = ( ( playerState->events[ sequenceIndex ] & GUN_ANIMATION_TOGGLE_BIT ) ^ GUN_ANIMATION_TOGGLE_BIT )
    //    | newEvent;
    playerState->events[ sequenceIndex ] = newEvent;
	playerState->eventParms[ sequenceIndex ] = eventParm;
	playerState->eventSequence++;

	// Debug Output.
	#ifdef _DEBUG_PRINT_PREDICTABLE_EVENTS
	{
        // Ensure string is within bounds.
        if ( newEvent < q_countof( sg_player_state_event_strings ) ) {
            #ifdef CLGAME_INCLUDE
            SG_DPrintf( "CLGame PMoveState Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, sg_player_state_event_strings[ newEvent ], eventParm );
            #else
            SG_DPrintf( "SVGame PMoveState Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, sg_player_state_event_strings[ newEvent ], eventParm );
            #endif
        } else {
            #ifdef CLGAME_INCLUDE
            SG_DPrintf( "CLGame PMoveState Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, "Out of Bounds for sg_player_state_event_strings", eventParm);
            #else
            SG_DPrintf( "SVGame PMoveState Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, "Out of Bounds for sg_player_state_event_strings", eventParm);
            #endif
        }
	}
	#endif
	//#endif
}



/**
*
*
*	(Client-)Player: WID: TODO: Move to their own file sometime.
*
*
**/
/**
*   @brief  Returns a string stating the determined 'Base' animation, and sets the FrameTime value for frameTime pointer.
**/
const std::string SG_Player_GetClientBaseAnimation( const player_state_t *oldPlayerState, const player_state_t *playerState, double *frameTime ) {
    // Base animation string we'll end up seeking in SKM data.
    std::string baseAnimStr = "";
    // Weapon to append to animation string.
    std::string baseAnimWeaponStr = "_rifle";
    // Default frameTime to apply to animation.
    *frameTime = BASE_FRAMETIME;

    if ( playerState->animation.isIdle ) {
        baseAnimStr = "idle";
        if ( playerState->animation.isCrouched ) {
            baseAnimStr += "_crouch";
        }
    } else {
        if ( playerState->animation.isCrouched ) {
            baseAnimStr = "crouch";
            *frameTime -= 5.f;
        } else if ( playerState->animation.isWalking ) {
            baseAnimStr = "walk";
            *frameTime *= 0.5;
        } else {
            baseAnimStr = "run";
            *frameTime -= 5.f;
        }
    }

    // Directions:
    //if ( ( moveFlags & ANIM_MOVE_CROUCH ) || ( moveFlags & ANIM_MOVE_RUN ) || ( moveFlags & ANIM_MOVE_WALK ) ) {
    if ( !playerState->animation.isIdle ) {
        // Get move direction for animation.
        const int32_t animationMoveDirection = playerState->animation.moveDirection;

        // Append to the baseAnimStr the name of the directional animation.
        // Forward:
        if ( animationMoveDirection == PM_MOVEDIRECTION_FORWARD ) {
            baseAnimStr += "_forward";
            // Forward Left:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_FORWARD_LEFT ) {
            baseAnimStr += "_forward_left";
            // Forward Right:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_FORWARD_RIGHT ) {
            baseAnimStr += "_forward_right";
            // Backward:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_BACKWARD ) {
            baseAnimStr += "_backward";
            // Backward Left:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_BACKWARD_LEFT ) {
            baseAnimStr += "_backward_left";
            // Backward Right:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_BACKWARD_RIGHT ) {
            baseAnimStr += "_backward_right";
            // Left:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_LEFT ) {
            baseAnimStr += "_left";
            // Right:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_RIGHT ) {
            baseAnimStr += "_right";
        }
        #if 1
        // Only apply Z translation to the animations if NOT 'In-Air'.
        if ( !playerState->animation.inAir ) {
            // Jump?
            if ( oldPlayerState->animation.inAir ) {
                //jumpAnimStr = "jump_down";
            // Only translate Z axis for rootmotion rendering.
            } else {
                //rootMotionFlags |= SKM_POSE_TRANSLATE_Z;
            }
        } else {
            if ( !oldPlayerState->animation.inAir ) {
                //jumpAnimStr = "jump_up";
            } else {
                if ( !playerState->animation.isCrouched ) {
                    baseAnimStr = "jump_air";
                    //baseAnimWeaponStr = "";
                }
            }
        }
        #endif
        // We're idling:
    } else {
        baseAnimStr = "idle";
        // Possible Crouched:
        if ( playerState->animation.isCrouched ) {
            baseAnimStr += "_crouch";
        } else {
            baseAnimStr += "_stand";
        }
    }

    // Add the weapon type stringname to the end.
    baseAnimStr += baseAnimWeaponStr;

    // Return.
    return baseAnimStr;
}