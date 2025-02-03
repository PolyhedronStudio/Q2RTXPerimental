/********************************************************************
*
*
*	ServerGame: Generic Client EntryPoints
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_chase.h"
#include "svgame/svg_utils.h"



/**
*
*
*   Client PMove Clip/Trace/PointContents:
*
*
**/
/**
*   @brief  Player Move specific 'Trace' wrapper implementation.
**/
static const trace_t q_gameabi SV_PM_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const void *passEntity, const contents_t contentMask ) {
    //if (pm_passent->health > 0)
    //    return gi.trace(start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
    //else
    //    return gi.trace(start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
    return gi.trace( start, mins, maxs, end, (edict_t *)passEntity, contentMask );
}
/**
*   @brief  Player Move specific 'Clip' wrapper implementation. Clips to world only.
**/
static const trace_t q_gameabi SV_PM_Clip( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const contents_t contentMask ) {
    return gi.clip( &g_edicts[ 0 ], start, mins, maxs, end, contentMask );
}
/**
*   @brief  Player Move specific 'PointContents' wrapper implementation.
**/
static const contents_t q_gameabi SV_PM_PointContents( const vec3_t point ) {
    return gi.pointcontents( point );
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
*   @brief  
**/
void SVG_Client_TraceForUseTarget( edict_t *ent, gclient_t *client, const bool processUserInput = false ) {
    // Get the (+targetuse) key state.
    const bool isTargetUseKeyHolding = ( client->userInput.heldButtons & BUTTON_USE_TARGET );
    const bool isTargetUseKeyPressed = ( client->userInput.pressedButtons & BUTTON_USE_TARGET );
    const bool isTargetUseKeyReleased = ( client->userInput.releasedButtons & BUTTON_USE_TARGET );

    // AngleVecs.
    Vector3 vForward, vRight;
    QM_AngleVectors( client->viewMove.viewAngles, &vForward, &vRight, NULL );

    // Determine the point that is the center of the crosshair, to use as the
    // start of the trace for finding the entity that is in-focus.
    Vector3 traceStart;
    Vector3 viewHeightOffset = { 0, 0, (float)ent->viewheight };
    SVG_Player_ProjectSource( ent, ent->s.origin, &viewHeightOffset.x, &vForward.x, &vRight.x, &traceStart.x );

    // Translate 48 units into the forward direction from the starting trace, to get our trace end position.
    constexpr float USE_TARGET_TRACE_DISTANCE = 48.f;
    Vector3 traceEnd = QM_Vector3MultiplyAdd( traceStart, USE_TARGET_TRACE_DISTANCE, vForward );
    // Now perform the trace.
    trace_t traceUseTarget = gi.trace( &traceStart.x, NULL, NULL, &traceEnd.x, ent, (contents_t)( MASK_PLAYERSOLID | MASK_MONSTERSOLID ) );

    // Get the current activate(in last frame) entity we were (+usetarget) using.
    edict_t *currentTargetEntity = ent->client->useTarget.currentEntity;

    // If it is a valid pointer and in use entity.
    if ( currentTargetEntity && currentTargetEntity->inuse ) {
        // And it differs from the one we found by tracing:
        if ( currentTargetEntity != traceUseTarget.ent ) {
            // AND it is a continuous usetarget supporting entity, with its state being continuously held:
            if ( SVG_UseTarget_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS )
                && SVG_UseTarget_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_CONTINUOUS ) ) {
                // Stop useTargetting the entity:
                if ( currentTargetEntity->use ) {
                    currentTargetEntity->use( currentTargetEntity, ent, ent, ENTITY_USETARGET_TYPE_SET, 0 );
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

    // Don't process user input?
    if ( !processUserInput ) {
        return;
    }

    // Are we continously holding +usetarget or did we single press it? If so, proceed.
    if ( currentTargetEntity && ( currentTargetEntity->inuse && currentTargetEntity->s.number != 0 ) ) {
        // The (+usetarget) key is neither pressed, nor held continuously, thus it was released this frame.
        if ( isTargetUseKeyReleased ) {
            // Stop with the continous entity usage:
            if ( SVG_UseTarget_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS )
                && SVG_UseTarget_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_CONTINUOUS ) ) {
                // Continous entity husage:
                if ( currentTargetEntity->use ) {
                    currentTargetEntity->use( currentTargetEntity, ent, ent, ENTITY_USETARGET_TYPE_SET, 0 );
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
                if ( SVG_UseTarget_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_DISABLED ) ) {
                    // Invalid, disabled, sound.
                    gi.sound( ent, CHAN_ITEM, gi.soundindex( "player/usetarget_invalid.wav" ), 0.8, ATTN_NORM, 0 );

                    // Stop with the continous entity usage:
                    if ( SVG_UseTarget_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS ) ) {
                        // Continous entity husage:
                        if ( currentTargetEntity->use ) {
                            currentTargetEntity->use( currentTargetEntity, ent, ent, ENTITY_USETARGET_TYPE_SET, 0 );
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
        
        if ( SVG_UseTarget_HasUseTargetFlags( currentTargetEntity, ( ENTITY_USETARGET_FLAG_PRESS | ENTITY_USETARGET_FLAG_TOGGLE | ENTITY_USETARGET_FLAG_CONTINUOUS ) )
            && ( isTargetUseKeyPressed /*|| isTargetUseKeyHolding*/ ) ) 
        {
            // Single press entity usage:
            if ( SVG_UseTarget_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_PRESS ) ) {
                if ( currentTargetEntity->use ) {
                    //// Trigger 'OFF' if it is toggled.
                    if ( SVG_UseTarget_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_ON ) ) {
                        currentTargetEntity->use( currentTargetEntity, ent, ent, ENTITY_USETARGET_TYPE_OFF, 0 );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state & ~ENTITY_USETARGET_STATE_ON );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state | ENTITY_USETARGET_STATE_OFF );
                        currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_OFF;
                        // Trigger 'ON' if it is untoggled.
                    } else {
                        currentTargetEntity->use( currentTargetEntity, ent, ent, ENTITY_USETARGET_TYPE_ON, 1 );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state & ~ENTITY_USETARGET_STATE_OFF );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state | ENTITY_USETARGET_STATE_ON );
                        currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_ON;
                    }
                }
            }
            // Toggle press entity usage:
            else if ( SVG_UseTarget_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_TOGGLE ) ) {
                if ( currentTargetEntity->use ) {
                    // Trigger 'TOGGLE OFF' if it is toggled.
                    if ( SVG_UseTarget_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_TOGGLED ) ) {
                        currentTargetEntity->use( currentTargetEntity, ent, ent, ENTITY_USETARGET_TYPE_TOGGLE, 0 );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state & ENTITY_USETARGET_STATE_TOGGLED );
                        currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_OFF;
                        // Trigger 'TOGGLE ON' if it is untoggled.
                    } else {
                        currentTargetEntity->use( currentTargetEntity, ent, ent, ENTITY_USETARGET_TYPE_TOGGLE, 1 );
                        //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state | ENTITY_USETARGET_STATE_TOGGLED );
                        currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_TOGGLED;
                    }
                }
            }

            // Toggle press entity usage:
            else if ( SVG_UseTarget_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS ) ) {
                if ( currentTargetEntity->use ) {
                    currentTargetEntity->use( currentTargetEntity, ent, ent, ENTITY_USETARGET_TYPE_SET, 1 );
                }
                currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_CONTINUOUS;
            }
        // Holding (+usetarget) key, if it is continuous and has its state set to it,
        // we CONTINUOUSLY USE the target.
        } else if ( isTargetUseKeyHolding
            && SVG_UseTarget_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS )
            && SVG_UseTarget_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_CONTINUOUS ) ) {
            // Continous entity husage:
            if ( currentTargetEntity->use ) {
                currentTargetEntity->use( currentTargetEntity, ent, ent, ENTITY_USETARGET_TYPE_SET, 1 );
            }
            // Apply continuous hold state.
            //currentTargetEntity->useTarget.state = ( currentTargetEntity->useTarget.state | ENTITY_USETARGET_STATE_CONTINUOUS );
            currentTargetEntity->useTarget.state = ENTITY_USETARGET_STATE_CONTINUOUS;
        // Holding (+usetarget) key, thus we continously USE the target entity.
        } //else if ( !isTargetUseKeyPressed && isTargetUseKeyHolding  
        //    && SVG_UseTarget_HasUseTargetFlags( currentTargetEntity, ENTITY_USETARGET_FLAG_CONTINUOUS )
        //    && SVG_UseTarget_HasUseTargetState( currentTargetEntity, ENTITY_USETARGET_STATE_CONTINUOUS ) ) 
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

    //if ( SVG_IsActiveEntity( currentTargetEntity ) ) {
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



/***
*
*
*
*   Client
*
*
*
**/
/**
*   @brief  Updates the client's userinput states based on the usercommand.
**/
static void ClientUpdateUserInput( edict_t *ent, gclient_t *client, usercmd_t *userCommand ) {
    // [Paril-KEX] pass the usercommand buttons through even if we are in intermission or chasing.
    client->oldbuttons = client->buttons;
    client->buttons = userCommand->buttons;
    client->latched_buttons |= ( client->buttons & ~client->oldbuttons );

    // Store last userInput buttons.
    client->userInput.lastButtons = client->userInput.buttons; // client->buttons
    // Update with the new userCmd buttons.
    client->userInput.buttons = userCommand->buttons;
    // Determine which buttons changed state.
    const int32_t buttonsChanged = ( client->userInput.lastButtons ^ client->userInput.buttons );
    // The changed ones that are still down are 'pressed'.
    client->userInput.pressedButtons = ( buttonsChanged & client->userInput.buttons );
    // These are held down after being pressed:
    client->userInput.heldButtons = ( client->userInput.lastButtons & client->userInput.buttons );
    // The others are 'released'.
    client->userInput.releasedButtons = ( buttonsChanged & ( ~client->userInput.buttons ) );
}

/**
*   @return True if we're still in intermission mode.
**/
static const bool ClientCheckIntermission( edict_t *ent, gclient_t *client ) {
    if ( level.intermissionFrameNumber ) {
        client->ps.pmove.pm_type = PM_FREEZE;

        // can exit intermission after five seconds
        if ( ( level.frameNumber > level.intermissionFrameNumber + 5.0f * BASE_FRAMERATE ) && ( client->buttons & BUTTON_ANY ) ) {
            level.exitintermission = true;
        }

        // WID: Also seems set in p_hud.cpp -> SVG_HUD_MoveClientToIntermission
        client->ps.pmove.viewheight = ent->viewheight = PM_VIEWHEIGHT_STANDUP;

        // We're in intermission
        return true;
    }

    // Not in intermission.
    return false;
}

/**
*   @brief  Determine the impacting falling damage to take. Called directly by ClientThink after each PMove.
**/
void P_FallingDamage( edict_t *ent, const pmove_t &pm ) {
    int	   damage;
    vec3_t dir;

    // Dead stuff can't crater.
    if ( ent->health <= 0 || ent->lifeStatus ) {
        return;
    }

    if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
        return; // not in the player model
    }

    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        return;
    }

    // Never take falling damage if completely underwater.
    if ( pm.liquid.level == LIQUID_UNDER ) {
        return;
    }

    // ZOID
    //  never take damage if just release grapple or on grapple
    //if ( ent->client->ctf_grapplereleasetime >= level.time ||
    //    ( ent->client->ctf_grapple &&
    //        ent->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY ) )
    //    return;
    // ZOID
    
    // Get impact delta.
    float delta = pm.impact_delta;
    // Determine fall damage based on impact delta.
    delta = delta * delta * 0.0001f;
    // Soften damage if inside liquid by the waist.
    if ( pm.liquid.level == LIQUID_WAIST ) {
        delta *= 0.25f;
    }
    // Soften damage if inside liquid by the feet.
    if ( pm.liquid.level == LIQUID_FEET ) {
        delta *= 0.5f;
    }

    // Damage is too small to be taken into consideration.
    if ( delta < 1 ) {
        return;
    }

    // WID: restart footstep timer <- NO MORE!! Because doing so causes the weapon bob 
    // position to insta shift back to start.
    //ent->client->ps.bobCycle = 0;

    //if ( ent->client->landmark_free_fall ) {
    //    delta = min( 30.f, delta );
    //    ent->client->landmark_free_fall = false;
    //    ent->client->landmark_noise_time = level.time + 100_ms;
    //}

    if ( delta < 15 ) {
        if ( !( pm.playerState->pmove.pm_flags & PMF_ON_LADDER ) ) {
            ent->s.event = EV_FOOTSTEP;
            //gi.dprintf( "%s: delta < 15 footstep\n", __func__ );
        }
        return;
    }

    // Calculate the fall value for view adjustments.
    ent->client->viewMove.fallValue = delta * 0.5f;
    if ( ent->client->viewMove.fallValue > 40 ) {
        ent->client->viewMove.fallValue = 40;
    }
    ent->client->viewMove.fallTime = level.time + FALL_TIME();

    // Apply fall event based on delta.
    if ( delta > 30 ) {
        if ( delta >= 55 ) {
            ent->s.event = EV_FALLFAR;
        } else {
            ent->s.event = EV_FALL;
        }

        // WID: We DO want the VOICE channel to SHOUT in PAIN
        //ent->pain_debounce_time = level.time + FRAME_TIME_S; // No normal pain sound.

        damage = (int)( ( delta - 30 ) / 2 );
        if ( damage < 1 ) {
            damage = 1;
        }
        VectorSet( dir, 0.f, 0.f, 1.f );// dir = { 0, 0, 1 };

        if ( !deathmatch->integer ) {
            SVG_TriggerDamage( ent, world, world, dir, ent->s.origin, vec3_origin, damage, 0, DAMAGE_NONE, MEANS_OF_DEATH_FALLING );
        }
    } else {
        ent->s.event = EV_FALLSHORT;
    }

    // Falling damage noises alert monsters
    if ( ent->health >= 0 ) { // Was: if ( ent->health )
        SVG_Player_PlayerNoise( ent, &pm.playerState->pmove.origin[ 0 ], PNOISE_SELF );
    }
}

/**
*   @brief  Copies in the necessary client data into the player move struct in order to perform a frame worth of
*           movement, copying back the resulting player move data into the client and entity fields.
**/
static void ClientRunPlayerMove( edict_t *ent, gclient_t *client, usercmd_t *userCommand, pmove_t *pm, pmoveParams_t *pmp ) {
    // NoClip/Spectator:
    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        if ( ent->client->resp.spectator ) {
            client->ps.pmove.pm_type = PM_SPECTATOR;
        } else {
            client->ps.pmove.pm_type = PM_NOCLIP;
        }
        // If our model index differs, we're gibbing out:
    } else if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
        client->ps.pmove.pm_type = PM_GIB;
        // Dead:
    } else if ( ent->lifeStatus ) {
        client->ps.pmove.pm_type = PM_DEAD;
        // Otherwise, default, normal movement behavior:
    } else {
        client->ps.pmove.pm_type = PM_NORMAL;
    }

    // PGM	trigger_gravity support
    client->ps.pmove.gravity = (short)( sv_gravity->value * ent->gravity );

    //pm.playerState = client->ps;
    pm->playerState = &client->ps;
    // Copy the current entity origin and velocity into our 'pmove movestate'.
    pm->playerState->pmove.origin = ent->s.origin;
    pm->playerState->pmove.velocity = ent->velocity;
    // Determine if it has changed and we should 'resnap' to position.
    if ( memcmp( &client->old_pmove, &pm->playerState->pmove, sizeof( pm->playerState->pmove ) ) ) {
        pm->snapinitial = true; // gi.dprintf ("pmove changed!\n");
    }
    // Setup 'User Command', 'Player Skip Entity' and Function Pointers.
    pm->cmd = *userCommand;
    pm->player = ent;
    pm->trace = SV_PM_Trace;
    pm->pointcontents = SV_PM_PointContents;
    pm->clip = SV_PM_Clip;
    //pm.viewoffset = ent->client->ps.viewoffset;
    //pm->simulationTime = level.time;
    // Perform a PMove.
    SG_PlayerMove( (pmove_s*)pm, (pmoveParams_s*)pmp );

    // Save into the client pointer, the resulting move states pmove
    client->ps = *pm->playerState;
    //client->ps = pm.playerState;
    client->old_pmove = pm->playerState->pmove;
    // Backup the command angles given from last command.
    VectorCopy( userCommand->angles, client->resp.cmd_angles );

    // Ensure the entity has proper RF_STAIR_STEP applied to it when moving up/down those:
    if ( pm->ground.entity && ent->groundInfo.entity ) {
        const float stepsize = fabs( ent->s.origin[ 2 ] - pm->playerState->pmove.origin[ 2 ] );
        if ( stepsize > PM_MIN_STEP_SIZE && stepsize < PM_MAX_STEP_SIZE ) {
            ent->s.renderfx |= RF_STAIR_STEP;
            ent->client->last_stair_step_frame = gi.GetServerFrameNumber() + 1;
        }
    }
}
/**
*   @brief  Copy in the remaining player move data into the entity and client structs, responding to possible changes.
**/
static const Vector3 ClientPostPlayerMove( edict_t *ent, gclient_t *client, pmove_t &pm ) {
    // [Paril-KEX] if we stepped onto/off of a ladder, reset the last ladder pos
    if ( ( pm.playerState->pmove.pm_flags & PMF_ON_LADDER ) != ( client->ps.pmove.pm_flags & PMF_ON_LADDER ) ) {
        VectorCopy( ent->s.origin, client->last_ladder_pos );

        if ( pm.playerState->pmove.pm_flags & PMF_ON_LADDER ) {
            if ( !deathmatch->integer &&
                client->last_ladder_sound < level.time ) {
                ent->s.event = EV_FOOTSTEP_LADDER;
                client->last_ladder_sound = level.time + LADDER_SOUND_TIME;
            }
        }
    }

    // [Paril-KEX] save old position for ClientProcessTouches
    const Vector3 oldOrigin = ent->s.origin;

    // Copy back into the entity, both the resulting origin and velocity.
    VectorCopy( pm.playerState->pmove.origin, ent->s.origin );
    VectorCopy( pm.playerState->pmove.velocity, ent->velocity );
    // Copy back in bounding box results. (Player might've crouched for example.)
    VectorCopy( pm.mins, ent->mins );
    VectorCopy( pm.maxs, ent->maxs );

    // Play 'Jump' sound if pmove inquired so.
    if ( pm.jump_sound && !( pm.playerState->pmove.pm_flags & PMF_ON_LADDER ) ) { //if (~client->ps.pmove.pm_flags & pm.s.pm_flags & PMF_JUMP_HELD && pm.liquid.level == 0) {
        // Jump sound to play.
        const int32_t sndIndex = irandom( 2 ) + 1;
        std::string pathJumpSnd = "player/jump0"; pathJumpSnd += std::to_string( sndIndex ); pathJumpSnd += ".wav";
        gi.sound( ent, CHAN_VOICE, gi.soundindex( pathJumpSnd.c_str() ), 1, ATTN_NORM, 0 );
        // Jump sound to play.
        //gi.sound( ent, CHAN_VOICE, gi.soundindex( "player/jump01.wav" ), 1, ATTN_NORM, 0 );

        // Paril: removed to make ambushes more effective and to
        // not have monsters around corners come to jumps
        // SVG_PlayerNoise(ent, ent->s.origin, PNOISE_SELF);
    }

    // Update the entity's remaining viewheight, liquid and ground information:
    ent->viewheight = (int32_t)pm.playerState->pmove.viewheight;
    ent->liquidInfo.level = pm.liquid.level;
    ent->liquidInfo.type = pm.liquid.type;
    ent->groundInfo.entity = pm.ground.entity;
    if ( pm.ground.entity ) {
        ent->groundInfo.entityLinkCount = pm.ground.entity->linkcount;
    }

    // Apply a specific view angle if dead:
    if ( ent->lifeStatus ) {
        client->ps.viewangles[ ROLL ] = 40;
        client->ps.viewangles[ PITCH ] = -15;
        client->ps.viewangles[ YAW ] = client->killer_yaw;
        // Otherwise, apply the player move state view angles:
    } else {
        client->ps.viewangles = pm.playerState->viewangles; // VectorCopy( pm.playerState->viewangles, client->ps.viewangles );
        client->viewMove.viewAngles = client->ps.viewangles; // VectorCopy( client->ps.viewangles, client->viewMove.viewAngles );
        QM_AngleVectors( client->viewMove.viewAngles, &client->viewMove.viewForward, &client->viewMove.viewRight, &client->viewMove.viewUp );
    }

    // Return the oldOrigin for later use.
    return oldOrigin;
}
/**
*   @brief  Will search for touching trigger and projectiles, dispatching their touch callback when touching.
**/
static void ClientProcessTouches( edict_t *ent, gclient_t *client, pmove_t &pm, const Vector3 &oldOrigin ) {
    // If we're not 'No-Clipping', or 'Spectating', touch triggers and projectfiles.
    if ( ent->movetype != MOVETYPE_NOCLIP ) {
        SVG_Util_TouchTriggers( ent );
        SVG_Util_TouchProjectiles( ent, oldOrigin );
    }

    // Dispatch touch callbacks on all the remaining 'Solid' traced objects during our PMove.
    for ( int32_t i = 0; i < pm.touchTraces.numberOfTraces; i++ ) {
        trace_t &tr = pm.touchTraces.traces[ i ];
        edict_t *other = tr.ent;

        if ( other != nullptr && other->touch ) {
            // TODO: Q2RE has these for last 2 args: const trace_t &tr, bool other_touching_self
            // What the??
            other->touch( other, ent, &tr.plane, tr.surface );
        }
    }
}

/**
*   @brief  This will be called once for each client frame, which will usually
*           be a couple times for each server frame.
**/
void SVG_Client_Think( edict_t *ent, usercmd_t *ucmd ) {
    // Set the entity that is being processed.
    level.current_entity = ent;

    // Warn in case if it is not a client.
    if ( !ent ) {
        gi.bprintf( PRINT_WARNING, "%s: ent == nullptr\n", __func__ );
        return;
    }

    // Acquire its client pointer.
    gclient_t *client = ent->client;
    // Warn in case if it is not a client.
    if ( !client ) {
        gi.bprintf( PRINT_WARNING, "%s: ent->client == nullptr\n", __func__ );
        return;
    }

    // Backup the old player state.
    client->ops = client->ps;

    // Configure player movement data.
    pmove_t pm = {};
    pmoveParams_t pmp = {};
    SG_ConfigurePlayerMoveParameters( &pmp );

    // Update client user input.
    ClientUpdateUserInput( ent, client, ucmd );

    /**
    *   Level Intermission Path:
    **/
    // Update intermission status.
    const bool intermissionResult = ClientCheckIntermission( ent, client );
    // Exit.
    if ( intermissionResult ) {
        return;
    }

    /**
    *   Chase Target:
    **/
    if ( ent->client->chase_target ) {
        VectorCopy( ucmd->angles, client->resp.cmd_angles );
        /**
        *   Player Move, and other 'Thinking' logic:
        **/
    } else {
        // Perform player movement.
        ClientRunPlayerMove( ent, client, ucmd, &pm, &pmp );

        // Check for client playerstate its pmove generated events.
        //ClientCheckPlayerstateEvents( ent, &client->ops, &client->ps );

        // Apply falling damage directly.
        P_FallingDamage( ent, pm );

        //if ( ent->client->landmark_free_fall && pm.groundentity ) {
        //    ent->client->landmark_free_fall = false;
        //    ent->client->landmark_noise_time = level.time + 100_ms;
        //}
        // [Paril-KEX] save old position for SVG_Util_TouchProjectiles
        //vec3_t old_origin = ent->s.origin;

        // Updates the entity origin, angles, bbox, groundinfo, etc.
        const Vector3 oldOrigin = ClientPostPlayerMove( ent, client, pm );

        // Finally link the entity back in.
        gi.linkentity( ent );

        // PGM trigger_gravity support
        ent->gravity = 1.0;
        // PGM

        // Process touch callback dispatching for Triggers and Projectiles.
        ClientProcessTouches( ent, client, pm, oldOrigin );
    }

    /**
    *   Spectator Path:
    **/
    if ( client->resp.spectator ) {
        if ( client->latched_buttons & BUTTON_PRIMARY_FIRE ) {
            // Zero out latched buttons.
            client->latched_buttons = BUTTON_NONE;

            // Switch from chase target to no chase target:
            if ( client->chase_target ) {
                client->chase_target = nullptr;
                client->ps.pmove.pm_flags &= ~( PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION );
                // Otherwise, get an active chase target:
            } else {
                SVG_ChaseCam_GetTarget( ent );
            }
        }
        /**
        *   Regular Player (And Weapon-)Path:
        **/
    } else {
        // Perform use Targets if we haven't already.
        SVG_Client_TraceForUseTarget( ent, client, true );

        // Check whether to engage switching to a new weapon.
        if ( client->newweapon ) {
            SVG_Player_Weapon_Change( ent );
        }
        // Process weapon thinking.
        //if ( ent->client->weapon_thunk == false ) {
        SVG_Player_Weapon_Think( ent, true );
        // Store that we thought for this frame.
        ent->client->weapon_thunk = true;
        //}
        // Determine any possibly necessary player state events to go along.
    }

    /**
    *   Spectator/Chase-Cam specific behaviors:
    **/
    if ( client->resp.spectator ) {
        // Switch to next chase target (or the first in-line if not chasing any).
        if ( ucmd->upmove >= 10 ) {
            if ( !( client->ps.pmove.pm_flags & PMF_JUMP_HELD ) ) {
                client->ps.pmove.pm_flags |= PMF_JUMP_HELD;

                if ( client->chase_target ) {
                    SVG_ChaseCam_Next( ent );
                } else {
                    SVG_ChaseCam_GetTarget( ent );
                }
            }
            // Untoggle playerstate jump_held.
        } else {
            client->ps.pmove.pm_flags &= ~PMF_JUMP_HELD;
        }
    }

    // Update chase cam if being followed.
    for ( int32_t i = 1; i <= maxclients->value; i++ ) {
        edict_t *other = g_edicts + i;
        if ( other->inuse && other->client->chase_target == ent ) {
            SVG_ChaseCam_Update( other );
        }
    }
}