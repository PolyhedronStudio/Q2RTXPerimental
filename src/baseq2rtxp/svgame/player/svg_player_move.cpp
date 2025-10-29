/********************************************************************
*
*
*	ServerGame: Generic Client EntryPoints
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_chase.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "shared/player_state.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_entities.h"
#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_gamemode.h"
#include "sharedgame/sg_usetarget_hints.h"
#include "sharedgame/pmove/sg_pmove.h"

#include "svgame/entities/svg_player_edict.h"

#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_events.h"
#include "svgame/player/svg_player_move.h"
#include "svgame/player/svg_player_usetargets.h"

#include "svgame/svg_gamemode.h"
#include "svgame/gamemodes/svg_gm_basemode.h"

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
static const cm_trace_t q_gameabi SV_PM_Trace( const Vector3 *start, const Vector3 *mins, const Vector3 *maxs, const Vector3 *end, const void *passEntity, const cm_contents_t contentMask ) {
    //if (pm_passent->health > 0)
    //    return SVG_Trace(start, mins, maxs, end, pm_passent, CM_CONTENTMASK_PLAYERSOLID);
    //else
    //    return SVG_Trace(start, mins, maxs, end, pm_passent, CM_CONTENTMASK_DEADSOLID);
    return gi.trace( start, mins, maxs, end, (svg_base_edict_t *)passEntity, contentMask );
}
/**
*   @brief  Player Move specific 'Clip' wrapper implementation. Clips to world only.
**/
static const cm_trace_t q_gameabi SV_PM_Clip( const Vector3 *start, const Vector3 *mins, const Vector3 *maxs, const Vector3 *end, const cm_contents_t contentMask ) {
    return gi.clip( g_edict_pool.EdictForNumber( 0 ) /* worldspawn */, start, mins, maxs, end, contentMask);
}
/**
*   @brief  Player Move specific 'PointContents' wrapper implementation.
**/
static const cm_contents_t q_gameabi SV_PM_PointContents( const Vector3 *point ) {
    return gi.pointcontents( point );
}



/**
*
*
*
*   Intermission:
*
*
*
**/
/**
*   @return True if we're still in intermission mode.
**/
static const bool ClientCheckForIntermission( svg_player_edict_t *ent, svg_client_t *client ) {
    if ( level.intermissionFrameNumber ) {
        client->ps.pmove.pm_type = ( !game.mode->IsMultiplayer() ? PM_SPINTERMISSION : PM_INTERMISSION );

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
*
*
*
*   Movement Recoil:
*
*
*
**/
/**
*	@brief	Calculates the to be determined movement induced recoil factor.
**/
static void ClientCalculateMovementRecoil( svg_base_edict_t *ent ) {
    // Get playerstate.
    player_state_t *playerState = &ent->client->ps;

    // Base movement recoil factor.
    double baseMovementRecoil = 0.;

    // Is on a ladder?
    const bool isOnLadder = ( ( playerState->pmove.pm_flags & PMF_ON_LADDER ) ? true : false );
    // Determine if crouched(ducked).
    const bool isDucked = ( ( playerState->pmove.pm_flags & PMF_DUCKED ) ? true : false );
    // Determine if off-ground.
    //bool isOnGround = ( ( playerState->pmove.pm_flags & PMF_ON_GROUND ) ? true : false );
    const bool isOnGround = ( ent->groundInfo.entity != nullptr ? true : false );
    // Determine if in water.
    const bool isInWater = ( ent->liquidInfo.level > cm_liquid_level_t::LIQUID_NONE ? true : false );
    // Get liquid level.
    const cm_liquid_level_t liquidLevel = ent->liquidInfo.level;

    // Resulting move factor.
    double recoilMoveFactor = 0.;

    // First check if in water, so we can skip the other tests.
    if ( isInWater && ent->liquidInfo.level > cm_liquid_level_t::LIQUID_FEET ) {
        // Waist in water.
        recoilMoveFactor = 0.55;
        // Head under water.
        if ( ent->liquidInfo.level > cm_liquid_level_t::LIQUID_WAIST ) {
            recoilMoveFactor = 0.75;
        }
    } else {
        // If ducked, -0.3.
        if ( isDucked && isOnGround && !isOnLadder ) {
            recoilMoveFactor -= 0.3;
        } else if ( isOnLadder ) {
            recoilMoveFactor = 0.5;
        }
    }

    // Divide 1 by max speed, multiply by velocity length, ONLY if, speed > pm_stop_speed
    if ( playerState->xyzSpeed >= default_pmoveParams_t::pm_stop_speed / 2. ) {
        // Get multiply factor.
        const double multiplyFactor = ( 1.0 / default_pmoveParams_t::pm_max_speed );
        // Calculate actual scale factor.
        const double scaleFactor = multiplyFactor * playerState->xyzSpeed;
        // Assign.
        recoilMoveFactor += 0.5 * scaleFactor;
        //gi.dprintf( "playerState->xyzSpeed(%f), scaleFactor(%f)\n", playerState->xyzSpeed, multiplyFactor, scaleFactor );
    } else {
        //gi.dprintf( "playerState->xyzSpeed\n", playerState->xyzSpeed );
    }

    // ( We default to idle. ) Default to 0.
    ent->client->weaponState.recoil.moveFactor = recoilMoveFactor;
}



/***
*
*
*
*   User Input:
*
*
*
**/
/**
*   @brief  Updates the client's userinput states based on the usercommand.
**/
static void ClientUpdateUserInput( svg_player_edict_t *ent, svg_client_t *client, usercmd_t *userCommand ) {
	/**
    *   Regular button update as Q2 does:
    **/
    // Store old buttons.
    client->oldbuttons = client->buttons;
	// Update current buttons.
    client->buttons = userCommand->buttons;
	// Determine latched buttons.
    client->latched_buttons |= ( client->buttons & ~client->oldbuttons );
    //client->lastUserCommand = *userCommand;

	/**
    *   New button update for our extended userInput structure :
    *       - Makes it easier to deal with certain button states required for entity interactions.
    **/
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
*
*
*
*   PMove:
*
*
*
**/
/**
*   @brief  Copies in the necessary client data into the player move struct in order to perform a frame worth of
*           movement, copying back the resulting player move data into the client and entity fields.
**/
static void PMove_RunFrame( svg_player_edict_t *ent, svg_client_t *client, usercmd_t *userCommand, pmove_t *pm, pmoveParams_t *pmp ) {
    // Prepare the player move structure properties for simulation.
    pm->state = &client->ps;
    // Copy the current entity origin and velocity into our 'pmove movestate'.
    pm->state->pmove.origin = ent->s.origin;
    pm->state->pmove.velocity = ent->velocity;
    // Setup the gravity. PGM	trigger_gravity support
    pm->state->pmove.gravity = (short)( sv_gravity->value * ent->gravity );
    pm->state->pmove.speed = pmp->pm_max_speed;
    // Setup the specific Player Move controller type.
    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        if ( ent->client->resp.spectator ) {
            pm->state->pmove.pm_type = PM_SPECTATOR;
        } else {
            pm->state->pmove.pm_type = PM_NOCLIP;
        }
    // If our model index differs, we're gibbing out: (We are not assigned the 'player' model.)
    } else if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
        pm->state->pmove.pm_type = PM_GIB;
    // Dead:
    } else if ( ent->lifeStatus ) {
        pm->state->pmove.pm_type = PM_DEAD;
    // Otherwise, default, normal movement behavior:
    } else {
        pm->state->pmove.pm_type = PM_NORMAL;
    }
    // Determine if it has changed and we should 'resnap' to position.
    if ( memcmp( &client->old_pmove, &pm->state->pmove, sizeof( pm->state->pmove ) ) ) {
        pm->snapInitialPosition = true; // gi.dprintf ("pmove changed!\n");
    }
    // Setup 'User Command'
    pm->cmd = *userCommand;
    // Assign a pointer to the game module's matching player client entity.
    pm->playerEdict = reinterpret_cast<edict_ptr_t *>( ent );
    // Prepare PMove specific trace wrapper function pointers.
    pm->trace = SV_PM_Trace;
    pm->pointcontents = SV_PM_PointContents;
    pm->clip = SV_PM_Clip;
    // Let us not forget about the simulation time before the actual mode.
    pm->simulationTime = level.time;

    // Move!
    SG_PlayerMove( (pmove_s *)pm, (pmoveParams_s *)pmp );
    // Backup the pmove result as the 'old' previous client player move.
    client->old_pmove = pm->state->pmove;
    // Backup the command angles given from last command.
    client->resp.cmd_angles = userCommand->angles; // VectorCopy( userCommand->angles, client->resp.cmd_angles );

    // Ensure the entity has proper RF_STAIR_STEP applied to it when moving up/down those:
    if ( pm->ground.entity && ent->groundInfo.entity ) {
        const double stepsize = fabs( ent->s.origin[ 2 ] - pm->state->pmove.origin[ 2 ] );
        if ( stepsize > PM_STEP_MIN_SIZE && stepsize <= PM_STEP_MAX_SIZE ) {
            ent->s.renderfx |= RF_STAIR_STEP;
            ent->client->last_stair_step_frame = gi.GetServerFrameNumber() + 1;
        }
    }
}
/**
*   @brief  Copy in the remaining player move data into the entity and client structs, responding to possible changes.
**/
static const Vector3 PMove_PostFrame( svg_player_edict_t *ent, svg_client_t *client, pmove_t &pm ) {
    // [Paril-KEX] if we stepped onto/off of a ladder, reset the last ladder pos
    if ( ( pm.state->pmove.pm_flags & PMF_ON_LADDER ) != ( client->ps.pmove.pm_flags & PMF_ON_LADDER ) ) {
        client->last_ladder_pos = ent->s.origin;

        if ( pm.state->pmove.pm_flags & PMF_ON_LADDER ) {
            if ( !deathmatch->integer &&
                client->last_ladder_sound < level.time ) {
                //ent->s.event = EV_FOOTSTEP_LADDER;
                SVG_Util_AddEvent( ent, EV_FOOTSTEP_LADDER, 0 );
                client->last_ladder_sound = level.time + LADDER_SOUND_TIME;
            }
        }
    }

    // [Paril-KEX] save old position for PMove_ProcessTouchTraces
    const Vector3 oldOrigin = ent->s.origin;

    // Copy back into the entity, both the resulting origin and velocity.
    // <Q2RTXP>: WID: We do this before processing touches instead to prevent collision issues.
	// WID: Moved to already called SG_PlayerStateToEntityState earlier in SVG_Client_Think.
    //ent->s.origin = pm.state->pmove.origin;
    ent->velocity = pm.state->pmove.velocity;

    // Update entity bounding box.
    ent->mins = pm.mins; 
    ent->maxs = pm.maxs;
    // Update the entity's remaining viewheight, liquid and ground information:
    ent->viewheight = (int32_t)pm.state->pmove.viewheight;

    // Store all player move liquid info into the entity 'monster move' (fake name here) properties.
    ent->liquidInfo.level = pm.liquid.level;
    ent->liquidInfo.type = pm.liquid.type;
    
    // Do the same for ground info.
    ent->groundInfo.entity = static_cast<svg_base_edict_t *>( pm.ground.entity );
    ent->groundInfo.contents = pm.ground.contents;
    ent->groundInfo.material = pm.ground.material;
    ent->groundInfo.plane = pm.ground.plane;
    ent->groundInfo.surface = pm.ground.surface;
    // Update link count.
    if ( ent->groundInfo.entity ) {
        ent->groundInfo.entityLinkCount = ent->groundInfo.entity->linkCount;
    }

    // Apply a specific view angle if dead:
    // If dead, fix the angle and don't add any kicks
    if ( ent->lifeStatus > entity_lifestatus_t::LIFESTATUS_ALIVE ) {
        // Clear out weapon kick angles.
        client->ps.kick_angles = QM_Vector3Zero();
        client->ps.viewangles[ ROLL ] = 40;
        client->ps.viewangles[ PITCH ] = -15;
        client->ps.viewangles[ YAW ] = client->killer_yaw;
    // Otherwise, apply the player move state view angles:
    } else {
		// Copy over the viewangles from pmove state.
        client->ps.viewangles = pm.state->viewangles;
		// Update the viewMove structure as well.
        client->viewMove.viewAngles = client->ps.viewangles;
		// Also derive the forward/right/up vectors from it.
        QM_AngleVectors( client->viewMove.viewAngles, 
            &client->viewMove.viewForward, 
            &client->viewMove.viewRight, 
            &client->viewMove.viewUp 
        );
    }

    // Return the oldOrigin for later use.
    return oldOrigin;
}

/**
*   @brief  Will search for touching trigger and projectiles, dispatching their touch callback when touching.
**/
static void PMove_ProcessTouchTraces( svg_player_edict_t *ent, svg_client_t *client, pmove_t &pm, const Vector3 &oldOrigin ) {

    // If we're not 'No-Clipping', or 'Spectating', touch triggers and projectfiles.
    if ( ent->movetype != MOVETYPE_NOCLIP ) {
        SVG_Util_TouchTriggers( ent );
        SVG_Util_TouchProjectiles( ent, oldOrigin );
    }

    // <Q2RTXP>: WID: We do this here to prevent collision issues.
    // Copy back into the entity, both the resulting origin.
    ent->s.origin = pm.state->pmove.origin;

	// Relink the entity now that its position has been updated.
    gi.linkentity( ent );

    // Dispatch touch callbacks on all the remaining 'Solid' traced objects during our PMove.
    for ( int32_t i = 0; i < pm.touchTraces.count; i++ ) {
        const svg_trace_t &tr = svg_trace_t( pm.touchTraces.traces[ i ] );
        svg_base_edict_t *other = tr.ent;

        if ( other != nullptr && other->HasTouchCallback() ) {
            // TODO: Q2RE has these for last 2 args: const svg_trace_t &tr, bool other_touching_self
            // What the??
            other->DispatchTouchCallback( ent, &tr.plane, tr.surface );
        }
    }
}



/**
*
*
*
*   Spectator Think:
*
*
*
**/
/**
*   @brief  Spectator specific thinking logic.
**/
static void Client_SpectatorThink( svg_player_edict_t *ent, svg_client_t *client, usercmd_t *ucmd ) {
    /**
    *   Switch ChaseTarget:
    **/
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
    *   Chase-Cam specific behaviors:
    **/
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



/**
*
*
*
*   Player Think:
*
*
*
**/
/**
*   @brief  Player specific thinking logic.
**/
static void Client_PlayerThink( svg_player_edict_t *ent, svg_client_t *client, usercmd_t *ucmd ) {
    // Perform use Targets if we haven't already.
    SVG_Player_TraceForUseTarget( ent, client, true );

    // Check whether to engage switching to a new weapon.
    if ( client->newweapon ) {
        SVG_Player_Weapon_Change( ent );
    }
    // Process weapon thinking.
    //if ( player_ent->client->weapon_thunk == false ) {
    SVG_Player_Weapon_Think( ent, true );
	//}
    // Store that we thought for this frame.
    ent->client->weapon_thunk = true;
}



/**
*
*
*
*   Client Think: The core of a client's logic. (Mainly, user input response and thus movement!)
* 
* 
* 
**/
/**
*   @brief  This will be called once for each client frame, which will usually
*           be a couple times for each server frame.
**/
void SVG_Client_Think( svg_base_edict_t *ent, usercmd_t *ucmd ) {
    // Set the entity that is being processed.
    level.current_entity = ent;

    // Warn in case if it is not a client.
    if ( !ent ) {
        gi.bprintf( PRINT_WARNING, "%s: ent == nullptr\n", __func__ );
        return;
    }

    // Ensure we are dealing with a player entity here.
    if ( !ent->GetTypeInfo()->IsSubClassType<svg_player_edict_t>() ) {
        gi.dprintf( "%s: Not a player entity.\n", __func__ );
        return;
    }

    // In a function that is assigned to a function pointer, where we expect a 
    // svg_player_edict_t it'll already be the argument so we won't need casting :-)
    //
    // Just make sure to never assign it to an incorrect entity type.
    svg_player_edict_t *player_ent = static_cast<svg_player_edict_t *>( ent );

    // Acquire its client pointer.
    svg_client_t *client = player_ent->client;
    // Warn in case if it is not a client.
    if ( !client ) {
        gi.bprintf( PRINT_WARNING, "%s: ent->client == nullptr\n", __func__ );
        return;
    }

    /**
	*   Backup some soon to be old client data:
    **/
    // Backup old event sequence.
    int64_t oldEventSequence = client->ps.eventSequence;
    // Backup the old player state.
    client->ops = client->ps;

    /**
    *   Update client user input:
    **/
    ClientUpdateUserInput( player_ent, client, ucmd );

    /**
    *   Level Intermission Path:
    **/
    // Update intermission status.
    const bool isInIntermission = ClientCheckForIntermission( player_ent, client );
    // Exit.
    if ( isInIntermission ) {

        return;
    }

    /**
    *   Chase Target:
    **/
    if ( player_ent->client->chase_target ) {
		// Update the chase cam angles respectively.
        client->resp.cmd_angles = ucmd->angles;
    /**
    *   Player Move, and other 'Thinking' logic:
    **/
    } else {
        /**
        *   Configure player movement data and run a frame.
        **/
        pmove_t pm = {};
        pmoveParams_t pmp = {};

        // Backup old origin for touch processing.
        const Vector3 oldOrigin = client->ps.pmove.origin;
        
		// Configure default player move parameters.
        SG_ConfigurePlayerMoveParameters( &pmp );
        // Perform player movement.
        PMove_RunFrame( player_ent, client, ucmd, &pm, &pmp);
		// Save results of pmove/events before processing touch traces.
        if ( client->ps.eventSequence != oldEventSequence ) {
            player_ent->eventTime = level.time;
        }

        /**
		*   Player State to Entity State conversion:
        **/
        // Copy back into the entity, both the resulting origin.
        // We do this now so that the entity is in the correct position for touch traces
        // and events that may be triggered during the PMove.
        // Convert certain playerstate properties into entity state properties.
        SG_PlayerStateToEntityState( client->clientNum, &client->ps, &player_ent->s, false );
        
		// Send any remaining pending predictable events to all clients but "ourselves".
        SVG_Client_SendPendingPredictableEvents( player_ent, ent->client );

        //vec3_t old_origin = player_ent->s.origin;

        // Save old position for SVG_Util_TouchProjectiles
        // Updates bbox, groundinfo, etc.
        /*const Vector3 oldOrigin = */PMove_PostFrame( player_ent, client, pm );

        // Check for client playerstate its pmove generated events.
        SVG_PlayerState_CheckForEvents( player_ent, &client->ops, &client->ps, oldEventSequence );

        // PGM trigger_gravity support
        player_ent->gravity = 1.0;
        // PGM

        // Link the entity back in right after events.
        gi.linkentity( player_ent );

        // Calculate recoil based on movement factors.
        ClientCalculateMovementRecoil( ent );

        // Process touch callback dispatching for Triggers and Projectiles.
        PMove_ProcessTouchTraces( player_ent, client, pm, oldOrigin );
        // Save results of triggers and client events.
        if ( client->ps.eventSequence != oldEventSequence ) {
            player_ent->eventTime = level.time;
        }
    }

    /**
    *   Spectator Path:
    **/
    if ( client->resp.spectator ) {
		Client_SpectatorThink( player_ent, client, ucmd );
    /**
    *   Regular Player (And Weapon-)Path:
    **/
    } else {
		Client_PlayerThink( player_ent, client, ucmd );
    }

    /**
    *   Update chase cam if being followed.
    **/
    for ( int32_t i = 1; i <= maxclients->value; i++ ) {
        svg_base_edict_t *other = g_edict_pool.EdictForNumber( i );
        if ( other && other->inUse && other->client->chase_target == ent ) {
            SVG_ChaseCam_Update( other );
        }
    }
}