/********************************************************************
*
*
* 
*	ServerGame: Player UseTarget Functionality:
*
* 
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include "sharedgame/sg_usetarget_hints.h"
#include "svgame/player/svg_player_usetargets.h"

#include "svgame/entities/svg_player_edict.h"



/**
*
*
*
*   Client UseTargetHint Functionality.:
*
*
*
**/

/**
*   @brief  Determines the necessary UseTarget Hint information for the hovered entity(if any).
*   @return True if the entity has legitimate UseTarget Hint information. False if unset, or not found at all.
**/
static const bool UpdateUseTargetHint( svg_base_edict_t *ent, svg_client_t *client, svg_base_edict_t *useTargetEntity ) {
    // We got an entity.
    if ( useTargetEntity && useTargetEntity->useTarget.hintInfo ) {
        // Determine UseTarget Hint Info:
        const sg_usetarget_hint_t *useTargetHint = SG_UseTargetHint_GetByID( useTargetEntity->useTarget.hintInfo->index );
        // > 0 means it is a const char in our data array.
        if ( useTargetEntity->useTarget.hintInfo->index > USETARGET_HINT_ID_NONE ) {
            // Apply stats if valid data.
            if ( useTargetHint &&
                ( useTargetHint->index > USETARGET_HINT_ID_NONE && useTargetHint->index < USETARGET_HINT_ID_MAX ) ) {
                client->ps.stats[ STAT_USETARGET_HINT_ID ] = useTargetHint->index;
                client->ps.stats[ STAT_USETARGET_HINT_FLAGS ] |= ( SG_USETARGET_HINT_FLAGS_DISPLAY | useTargetHint->flags );
                // Clear out otherwise.
            } else {
                SVG_Player_ClearUseTargetHint( ent, client, useTargetEntity );
            }
            // < 0 means that it is passed to us by a usetarget hint info command.
        } else if ( useTargetEntity->useTarget.hintInfo->index < 0 ) {
            // TODO:
            SVG_Player_ClearUseTargetHint( ent, client, useTargetEntity );
            // Clear whichever it previously contained. we got nothing to display.
        } else {
            // Clear whichever it previously contained. we got nothing to display.
            SVG_Player_ClearUseTargetHint( ent, client, useTargetEntity );
        }
    } else {
        // Clear whichever it previously contained. we got nothing to display.
        SVG_Player_ClearUseTargetHint( ent, client, useTargetEntity );
    }

    // Succeeded at updating the UseTarget Hint if it is NOT zero.
    return ( client->ps.stats[ STAT_USETARGET_HINT_ID ] == 0 ? false : true );
}



/**
*
*   +usetarget/-usetarget:
*
*   Toggle Entities:
*   Will trigger an entity only once in case of the keypress(+usetarget).
*
*   Toggle Example:
*   When the keypress(+usetarget) is processed on a button, it will trigger it
*   only once. For the next keypress(+usetarget) it will trigger the button once
*   again, resulting in its state now untoggling.
*
*
*   Hold Entities:
*   In case of a 'hold' entity, it means we trigger on both the keypress(+usetarget),
*   and keyrelease(-usetarget), as well as when the client loses the entity out of its
*   crosshair's focus. Whether this can be perceived as truly holding depends on the
*   entity its own implementation. See the example below:
*
*   Hold Example:
*   We could have a box entity. When +usetarget triggered, it assigns the client as its
*   owner. In its 'use' callback, if its 'owner' pointer != (nullptr),
*   it can check if the activator is its actual owner. If it is, then it can reset its
*   'owner' pointer back to (nullptr). Theoretically this allows for concepts such as
*   the possibility of objects that one can pick up and move around.
*
**/

/**
*   @brief  Unsets the current client stats usetarget info.
**/
void SVG_Player_ClearUseTargetHint( svg_base_edict_t *ent, svg_client_t *client, svg_base_edict_t *useTargetEntity ) {
    // Nothing for display.
    client->ps.stats[ STAT_USETARGET_HINT_ID ] = 0;
    client->ps.stats[ STAT_USETARGET_HINT_FLAGS ] = 0;
}

/**
*   @brief  Traces for the entity that is currently being targeted by the player's crosshair.
*           Will also process user input for (+usetarget) key activity if specified.
*           
*           Depending on the entity's useTarget flags and state, it will trigger the entity's
*           usetarget callback appropriately.
**/
void SVG_Player_TraceForUseTarget( svg_base_edict_t *ent, svg_client_t *client, const bool processUserInput /*= false */ ) {
    // Get the (+targetuse) key state.
    const bool isTargetUseKeyHolding = ( client->userInput.heldButtons & BUTTON_USE_TARGET );
    const bool isTargetUseKeyPressed = ( client->userInput.pressedButtons & BUTTON_USE_TARGET );
    const bool isTargetUseKeyReleased = ( client->userInput.releasedButtons & BUTTON_USE_TARGET );

    // AngleVecs.
    Vector3 vForward, vRight;
    QM_AngleVectors( client->viewMove.viewAngles, &vForward, &vRight, NULL );

    // Determine the point that is the center of the crosshair, to use as the
    // start of the trace for finding the entity that is in-focus.
    Vector3 viewHeightOffset = { 0., 0., (double)ent->viewheight };
    Vector3 traceStart = SVG_Player_ProjectSource( ent, ent->s.origin, viewHeightOffset, vForward, vRight );

    // Translate 48 units into the forward direction from the starting trace, to get our trace end position.
    constexpr float USE_TARGET_TRACE_DISTANCE = 48.f;
    Vector3 traceEnd = QM_Vector3MultiplyAdd( traceStart, USE_TARGET_TRACE_DISTANCE, vForward );
    // Now perform the trace.
    svg_trace_t traceUseTarget = SVG_Trace( traceStart, qm_vector3_null, qm_vector3_null, traceEnd, ent, (cm_contents_t)( CM_CONTENTMASK_PLAYERSOLID | CM_CONTENTMASK_MONSTERSOLID ) );

    // Get the current activate(in last frame) entity we were (+usetarget) using.
    svg_base_edict_t *currentTargetEntity = ent->client->useTarget.currentEntity;

    // If it is a valid pointer and in use entity.
    if ( currentTargetEntity && currentTargetEntity->inuse ) {
        // And it differs from the one we found by tracing:
        if ( currentTargetEntity != traceUseTarget.ent ) {
            // AND it is a continuous usetarget supporting entity, with its state being continuously held:
            if ( SVG_Entity_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS )
                && SVG_Entity_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_CONTINUOUS ) ) {
                // Stop useTargetting the entity:
                if ( currentTargetEntity->HasUseCallback() ) {
                    currentTargetEntity->DispatchUseCallback( ent, ent, ENTITY_USETARGET_TYPE_SET, 0 );
                }
                // Remove continuous state flag.
                currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state & ~ENTITY_USETARGET_STATE_CONTINUOUS );
            }

            // Store it as the previous usetarget entity that we had.
            client->useTarget.previousEntity = client->useTarget.currentEntity;

            // Remove the glow from specified entity.
            //if ( client->useTarget.previousEntity != nullptr ) {
            //    client->useTarget.previousEntity->s.renderfx &= ~RF_SHELL_DOUBLE;
            //}
        } else {
            //if ( traceUseTarget.ent ) {
            //    traceUseTarget.ent->s.renderfx |= RF_SHELL_DOUBLE;
            //}
        }
    }

    // Update the current target entity to that of the new found trace.
    currentTargetEntity = client->useTarget.currentEntity = traceUseTarget.ent;

    // Update the UseTarget Hinting since the entity has changed.
    const bool useTargetHintUpdated = UpdateUseTargetHint( ent, client, currentTargetEntity );

    // Don't process user input?
    if ( !processUserInput ) {
        return;
    }

    // Are we continously holding +usetarget or did we single press it? If so, proceed.
    if ( currentTargetEntity && ( currentTargetEntity->inuse && currentTargetEntity->s.number != 0 ) ) {
        // The (+usetarget) key is neither pressed, nor held continuously, thus it was released this frame.
        if ( isTargetUseKeyReleased ) {
            // Stop with the continous entity usage:
            if ( SVG_Entity_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS )
                && SVG_Entity_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_CONTINUOUS ) ) {
                // Continous entity husage:
                if ( currentTargetEntity->HasUseCallback() ) {
                    currentTargetEntity->DispatchUseCallback( ent, ent, ENTITY_USETARGET_TYPE_SET, 0 );
                }
                // Remove continuous state flag.
                currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_OFF;
            }
        }
    }

    // Don't continue if there is no (+usetarget) key activity present.
    if ( ( ( client->userInput.buttons | client->userInput.pressedButtons | client->userInput.releasedButtons ) & BUTTON_USE_TARGET ) == 0 ) {
        return;
    }

    // Are we continously holding +usetarget or did we single press it? If so, proceed.
    if ( currentTargetEntity && ( currentTargetEntity->inuse && currentTargetEntity->s.number != 0 ) ) {
        // Play audio sound for when pressing onto a valid entity.
        if ( isTargetUseKeyPressed ) {
            if ( currentTargetEntity->useTarget.flags != ENTITY_USETARGET_FLAG_NONE ) {
                if ( SVG_Entity_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_DISABLED ) ) {
                    // Invalid, disabled, sound.
                    gi.sound( ent, CHAN_ITEM, gi.soundindex( "player/usetarget_invalid.wav" ), 0.8, ATTN_NORM, 0 );

                    // Stop with the continous entity usage:
                    if ( SVG_Entity_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS ) ) {
                        // Continous entity husage:
                        if ( currentTargetEntity->HasUseCallback() ) {
                            currentTargetEntity->DispatchUseCallback( ent, ent, ENTITY_USETARGET_TYPE_SET, 0 );
                        }
                        // Remove continuous state flag.
                        currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_OFF;
                    }
                    return;
                } else {
                    // Use sound.
                    gi.sound( ent, CHAN_ITEM, gi.soundindex( "player/usetarget_use.wav" ), 0.25, ATTN_NORM, 0 );
                }
            }
        }

        if ( SVG_Entity_HasUseTargetFlags( currentTargetEntity, ( ENTITY_USETARGET_FLAG_PRESS | ENTITY_USETARGET_FLAG_TOGGLE | ENTITY_USETARGET_FLAG_CONTINUOUS ) )
            && ( isTargetUseKeyPressed /*|| isTargetUseKeyHolding*/ ) ) {
            // Single press entity usage:
            if ( SVG_Entity_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_PRESS ) ) {
                if ( currentTargetEntity->HasUseCallback() ) {
                    //// Trigger 'OFF' if it is toggled.
                    if ( SVG_Entity_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_ON ) ) {
                        currentTargetEntity->DispatchUseCallback( ent, ent, ENTITY_USETARGET_TYPE_OFF, 0 );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state & ~ENTITY_USETARGET_STATE_ON );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state | ENTITY_USETARGET_STATE_OFF );
                        currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_OFF;
                        // Trigger 'ON' if it is untoggled.
                    } else {
                        currentTargetEntity->DispatchUseCallback( ent, ent, ENTITY_USETARGET_TYPE_ON, 1 );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state & ~ENTITY_USETARGET_STATE_OFF );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state | ENTITY_USETARGET_STATE_ON );
                        currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_ON;
                    }
                }
            }
            // Toggle press entity usage:
            else if ( SVG_Entity_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_TOGGLE ) ) {
                if ( currentTargetEntity->HasUseCallback() ) {
                    // Trigger 'TOGGLE OFF' if it is toggled.
                    if ( SVG_Entity_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_TOGGLED ) ) {
                        currentTargetEntity->DispatchUseCallback( ent, ent, ENTITY_USETARGET_TYPE_TOGGLE, 0 );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state & ENTITY_USETARGET_STATE_TOGGLED );
                        currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_OFF;
                        // Trigger 'TOGGLE ON' if it is untoggled.
                    } else {
                        currentTargetEntity->DispatchUseCallback( ent, ent, ENTITY_USETARGET_TYPE_TOGGLE, 1 );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state | ENTITY_USETARGET_STATE_TOGGLED );
                        currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_TOGGLED;
                    }
                }
            }

            // Toggle press entity usage:
            else if ( SVG_Entity_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS ) ) {
                if ( currentTargetEntity->HasUseCallback() ) {
                    currentTargetEntity->DispatchUseCallback( ent, ent, ENTITY_USETARGET_TYPE_SET, 1 );
                }
                currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_CONTINUOUS;
            }
            // Holding (+usetarget) key, if it is continuous and has its state set to it,
            // we CONTINUOUSLY USE the target.
        } else if ( isTargetUseKeyHolding
            && SVG_Entity_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS )
            && SVG_Entity_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_CONTINUOUS ) ) {
            // Continous entity husage:
            if ( currentTargetEntity->HasUseCallback() ) {
                currentTargetEntity->DispatchUseCallback( ent, ent, ENTITY_USETARGET_TYPE_SET, 1 );
            }
            // Apply continuous hold state.
            //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state | ENTITY_USETARGET_STATE_CONTINUOUS );
            currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_CONTINUOUS;
            // Holding (+usetarget) key, thus we continously USE the target entity.
        } //else if ( !isTargetUseKeyPressed && isTargetUseKeyHolding  
        //    && SVG_Entity_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS )
        //    && SVG_Entity_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_CONTINUOUS ) ) 
        //{
        //    // Continous entity husage:
        //    if ( currentTargetEntity->use ) {
        //        currentTargetEntity->use( currentTargetEntity, ent, ent, ENTITY_USETARGET_TYPE_SET, 1 );
        //    }
        //    currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_CONTINUOUS;
        //} 

    } else {
        // Play audio sound for when pressing onto an invalid (+usetarget) entity.
        if ( isTargetUseKeyPressed ) {
            gi.sound( ent, CHAN_ITEM, gi.soundindex( "player/usetarget_invalid.wav" ), 0.8, ATTN_NORM, 0 );
        }
    }

    //if ( SVG_Entity_IsActive( currentTargetEntity ) ) {
    //    gi.dprintf( "targetname(\"%s\", %d)target(\"%s\", %d), isLocked(%s)\n", 
    //        currentTargetEntity->targetname.ptr, 
    //        currentTargetEntity->targetname.count,
    //        currentTargetEntity->targetNames.target.ptr, 
    //        currentTargetEntity->targetNames.target.count,
    //        currentTargetEntity->pushMoveInfo.lockState.isLocked ? "true" : "false"
    //    );
    //}
    //// <Debug>
    //std::string keys = "BUTTONS [ ";
    //keys += "isTargetUseKeyHolding(";
    //keys += isTargetUseKeyHolding ? "true" : "false";
    //keys += "), isTargetUseKeyPressed(";
    //keys += isTargetUseKeyPressed ? "true" : "false";
    //keys += "), isTargetUseKeyReleased(";
    //keys += isTargetUseKeyReleased ? "true" : "false";
    //keys += ") ]";
    //Q_DevPrint( keys.c_str() );
    //// </Debug>
}