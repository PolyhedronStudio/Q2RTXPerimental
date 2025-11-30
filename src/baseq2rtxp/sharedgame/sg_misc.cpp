/********************************************************************
*
*
*	SharedGame: Misc Functions:
*
*
********************************************************************/
#include "shared/shared.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_entity_events.h"
#include "sharedgame/sg_misc.h"
#include "sharedgame/pmove/sg_pmove.h"

//! Uncomment to enable debug print output of predictable events.
//#define _DEBUG_PRINT_PREDICTABLE_EVENTS



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
    
    "FootStep",
    "FootStep[Ladder]",
    
    "Jump Up",		        // PS_EV_JUMP_UP
    "Jump Land",	        // PS_EV_JUMP_LAND
    
    "Fall Short",
    "Fall Medium",
    "Fall Far",
    
    "Weapon Reload",        // PS_EV_WEAPON_RELOAD
    "Weapon Primary Fire",  // PS_EV_WEAPON_PRIMARY_FIRE
    "Weapon Secondary Fire",// PS_EV_WEAPON_SECONDARY_FIRE
    "Weapon Holster and Draw",
    "Weapon Draw",
    "Weapon Holster"
};

/**
*	@brief	Adds a player movement predictable event to the player move state.
**/
void SG_PlayerState_AddPredictableEvent( const int32_t newEvent, const int32_t eventParm0, /*const int32_t eventParm1,*/ player_state_t *playerState ) {
	// Event sequence index.
	const int64_t sequenceIndex = playerState->eventSequence & ( MAX_PS_EVENTS - 1 );

	// Ensure it is within bounds.
	if ( newEvent < 0 || newEvent >= EV_GAME_MAX ) {
        if ( newEvent < q_countof( sg_event_string_names ) ) {
            SG_DPrintf( SG_GAME_MODULE_STR " PMoveState INVALID(Out of Bounds) Event(sequenceIndex: %i): newEvent(#%i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, sg_event_string_names[ newEvent ], eventParm0 );
        }
	}

	//#ifndef CLGAME_INCLUDE
	// Add predicted player move state event.
    //playerState->events[ sequenceIndex ] = ( ( playerState->events[ sequenceIndex ] & GUN_ANIMATION_TOGGLE_BIT ) ^ GUN_ANIMATION_TOGGLE_BIT )
    //    | newEvent;
    playerState->events[ sequenceIndex ] = newEvent;
	playerState->eventParms[ sequenceIndex ] = eventParm0;
    //playerState->eventParms[ sequenceIndex ] = eventParm1;
	playerState->eventSequence++;

	// Debug Output.
	#ifdef _DEBUG_PRINT_PREDICTABLE_EVENTS
	{
        // Ensure string is within bounds.
        if ( newEvent < q_countof( sg_event_string_names ) ) {
            SG_DPrintf( SG_GAME_MODULE_STR " PMoveState Event(sequenceIndex: % i) : newEvent(# % i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, sg_event_string_names[newEvent], eventParm);
        } else {
            SG_DPrintf( SG_GAME_MODULE_STR " PMoveState Event(sequenceIndex: % i) : newEvent(# % i, \"%s\"), eventParm(%i)\n", sequenceIndex, newEvent, "Out of Bounds for sg_event_string_names", eventParm);
        }
	}
	#endif
}



/**
*
*
*	(Client-)Player: WID: TODO: Move to their own file sometime.
*
*
**/
#define ONLY_FORWARD_AND_BACKWARD 1
/**
*   @brief  Returns a string stating the determined 'Base' animation, and sets the FrameTime value for frameTime pointer.
**/
const std::string SG_Player_GetClientBaseAnimation( const player_state_t *oldPlayerState, const player_state_t *playerState, double *frameTime ) {
    // Base animation string we'll end up seeking in SKM data.
    std::string baseAnimStr = "";
    // Weapon to append to animation string.
    std::string baseAnimWeaponStr = "_pistol";
    // Default frameTime to apply to animation.
    *frameTime = BASE_FRAMETIME;

    if ( playerState->animation.isIdle ) {
        baseAnimStr = "idle";
        if ( playerState->animation.isCrouched ) {
            baseAnimStr += "_crouch";
        } else {
            baseAnimStr += "_stand";
        }
    } else {
        if ( playerState->animation.isCrouched ) {
            baseAnimStr = "crouch";
            //*frameTime -= 5.f;
        } else if ( playerState->animation.isWalking ) {
            baseAnimStr = "walk";
            //*frameTime *= 0.5;
        } else {
            baseAnimStr = "run";//baseAnimStr = "run";
            //*frameTime -= 5.f;
        }
    }

    // Directions:
    //if ( ( moveFlags & ANIM_MOVE_CROUCH ) || ( moveFlags & ANIM_MOVE_RUN ) || ( moveFlags & ANIM_MOVE_WALK ) ) {
    if ( !playerState->animation.isIdle ) {
        // Get move directions for each state
        const int32_t lastMoveDirection = oldPlayerState->animation.moveDirection;
        const int32_t currentMoveDirection = playerState->animation.moveDirection;

        // Append to the baseAnimStr the name of the directional animation.
        // Forward Left:
        if ( ( currentMoveDirection & PM_MOVEDIRECTION_FORWARD ) && ( currentMoveDirection & PM_MOVEDIRECTION_LEFT ) ) {
            #ifndef ONLY_FORWARD_AND_BACKWARD
            baseAnimStr += "_forward_left";
            #else
            baseAnimStr += "_forward";
            #endif
        // Forward Right:
        } 
        else if ( ( currentMoveDirection & PM_MOVEDIRECTION_FORWARD ) && ( currentMoveDirection & PM_MOVEDIRECTION_RIGHT ) ) {
            #ifndef ONLY_FORWARD_AND_BACKWARD
            baseAnimStr += "_forward_right";
            #else
            baseAnimStr += "_forward";
            #endif
        }
        // Forward:
        else if ( currentMoveDirection & PM_MOVEDIRECTION_FORWARD ) {
            baseAnimStr += "_forward";
        // Backward Left:
        }
        else if ( ( currentMoveDirection & PM_MOVEDIRECTION_BACKWARD ) && ( currentMoveDirection & PM_MOVEDIRECTION_LEFT ) ) {
            #ifndef ONLY_FORWARD_AND_BACKWARD
            baseAnimStr += "_backward_left";
            #else
            baseAnimStr += "_backward";
            #endif
        // Backward Right:
        }
        else if ( ( currentMoveDirection & PM_MOVEDIRECTION_BACKWARD ) && ( currentMoveDirection & PM_MOVEDIRECTION_RIGHT ) ) {
            #ifndef ONLY_FORWARD_AND_BACKWARD
            baseAnimStr += "_backward_right";
            #else
            baseAnimStr += "_backward";
            #endif
        // Backward:
        } else if ( currentMoveDirection & PM_MOVEDIRECTION_BACKWARD ) {
            baseAnimStr += "_backward";
        // In case isIdle isn't set YET.
        } else {
            // Left:
            if ( currentMoveDirection & PM_MOVEDIRECTION_LEFT && !( currentMoveDirection & PM_MOVEDIRECTION_FORWARD ) && !( currentMoveDirection & PM_MOVEDIRECTION_BACKWARD ) ) {
                baseAnimStr += "_strafe_left";
                // Right:
            } else if ( currentMoveDirection & PM_MOVEDIRECTION_RIGHT && !( currentMoveDirection & PM_MOVEDIRECTION_FORWARD ) && !( currentMoveDirection & PM_MOVEDIRECTION_BACKWARD ) ) {
                baseAnimStr += "_strafe_right";
            // For when isIdle is still set due to PM_ANIMATION_IDLE_EPSILON PMove case:
            } else {
                baseAnimStr = "idle";
                // Possible Crouched:
                if ( playerState->animation.isCrouched ) {
                    baseAnimStr += "_crouch";
                } else {
                    baseAnimStr += "_stand";
                }
            }
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