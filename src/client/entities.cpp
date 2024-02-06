/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
// cl_ents.c -- entity parsing and management

#include "cl_client.h"
#include "refresh/models.h"

extern qhandle_t cl_mod_powerscreen;
extern qhandle_t cl_mod_laser;
extern qhandle_t cl_mod_dmspot;
extern qhandle_t cl_sfx_footsteps[4];

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

// returns true if origin/angles update has been optimized out
static inline bool entity_is_optimized(const entity_state_t *state)
{
    //return cls.serverProtocol == PROTOCOL_VERSION_Q2PRO
    //    && state->number == cl.frame.clientNum + 1
    //    && cl.frame.ps.pmove.pm_type < PM_DEAD;
	// WID: net-protocol2: We don't want this anyway, should get rid of it?
    //if (cls.serverProtocol != PROTOCOL_VERSION_Q2PRO)
    if (state->number != cl.frame.clientNum + 1)
        return false;

    if (cl.frame.ps.pmove.pm_type >= PM_DEAD)
        return false;

    // WID: TODO: It does WORK with this, but do we want to do this?
    // using the entity origin for the player, wouldn't that mean
    // it is a slower update rate than the actual player is?
    if ( cls.serverProtocol == PROTOCOL_VERSION_Q2RTXPERIMENTAL ) {
        return false;
    }

    return true;
}

static inline void
entity_update_new(centity_t *ent, const entity_state_t *state, const vec_t *origin)
{
    static int entity_ctr;
    ent->id = ++entity_ctr;
    ent->trailcount = 1024;     // for diminishing rocket / grenade trails

    // duplicate the current state so lerping doesn't hurt anything
    ent->prev = *state;

// WID: 40hz
	ent->current_frame = ent->last_frame = state->frame;
	ent->frame_servertime = cl.servertime;
// WID: 40hz

    if (state->event == EV_PLAYER_TELEPORT ||
        state->event == EV_OTHER_TELEPORT ||
        (state->renderfx & RF_BEAM)) {
        // no lerping if teleported
        VectorCopy(origin, ent->lerp_origin);
        return;
    }

    // old_origin is valid for new entities,
    // so use it as starting point for interpolating between
    VectorCopy(state->old_origin, ent->prev.origin);
    VectorCopy(state->old_origin, ent->lerp_origin);
}

static inline void
entity_update_old(centity_t *ent, const entity_state_t *state, const vec_t *origin)
{
    int event = state->event;

    if (state->modelindex != ent->current.modelindex
        || state->modelindex2 != ent->current.modelindex2
        || state->modelindex3 != ent->current.modelindex3
        || state->modelindex4 != ent->current.modelindex4
        || event == EV_PLAYER_TELEPORT
        || event == EV_OTHER_TELEPORT
        || fabsf(origin[0] - ent->current.origin[0]) > 512
        || fabsf(origin[1] - ent->current.origin[1]) > 512
        || fabsf(origin[2] - ent->current.origin[2]) > 512
        || cl_nolerp->integer == 1) {
        // some data changes will force no lerping
        ent->trailcount = 1024;     // for diminishing rocket / grenade trails

        // duplicate the current state so lerping doesn't hurt anything
        ent->prev = *state;

		// WID: 40hz
		ent->last_frame = state->frame;
		// WID: 40hz

		// no lerping if teleported or morphed
        VectorCopy(origin, ent->lerp_origin);
        return;
    }

    // Handle proper lerping for animated entities by Hz.
	if ( ent->current_frame != state->frame ) {
		if ( state->renderfx & RF_OLD_FRAME_LERP ) {
			ent->last_frame = ent->current.old_frame;
		} else {
			ent->last_frame = ent->current.frame;
		}
		ent->current_frame = state->frame;
		ent->frame_servertime = cl.servertime;
	}

    // Set step height, and server time, if caught stair stepping.
    if ( state->renderfx & RF_STAIR_STEP ) {
        ent->step_height = state->origin[ 2 ] - ent->current.origin[ 2 ];
        ent->step_servertime = cl.servertime;

        Com_LPrintf( PRINT_DEVELOPER, "RF_STAIR_STEP for Monster(%d) step_height(%f), step_servertime(%zu)\n", state->number, ent->step_height, ent->step_servertime );
    }

    // shuffle the last state to previous
    ent->prev = ent->current;
}

static inline bool entity_is_new(const centity_t *ent) {
    // Last received frame was invalid.
    if ( !cl.oldframe.valid ) {
        return true;
    }
    // Wasn't in last received frame.
    if ( ent->serverframe != cl.oldframe.number ) {
        return true;
    }
    // Developer option, always new.
    if ( cl_nolerp->integer == 2 ) {
        return true;
    }
    //! Developer option, lerp from last received frame.
    if ( cl_nolerp->integer == 3 ) {
        return false;
    }
    //! Previous server frame was dropped.
    if ( cl.oldframe.number != cl.frame.number - 1 ) {
        return true;
    }

    return false;
}

static void parse_entity_update(const entity_state_t *state)
{
    centity_t *ent = ENTITY_FOR_NUMBER( state->number );
    const vec_t *origin;
    vec3_t origin_v;

    // If entity is solid, and not our client entity, add it to the solid entity list.
    if ( state->solid && state->number != cl.frame.clientNum + 1 && cl.numSolidEntities < MAX_PACKET_ENTITIES) {
        // Add it to the solids entity list.
        cl.solidEntities[cl.numSolidEntities++] = ent;

        // If not a brush bsp entity, decode its mins and maxs.
        if (state->solid != PACKED_BSP) {
			// WID: upgr-solid: Q2RE Approach.
			MSG_UnpackSolidUint32( state->solid, ent->mins, ent->maxs );
        }
    }

    // Work around Q2PRO server bandwidth optimization.
    if (entity_is_optimized(state)) {
		VectorCopy(cl.frame.ps.pmove.origin, origin_v );
        origin = origin_v;
    } else {
        origin = state->origin;
    }

    if (entity_is_new(ent)) {
        // Wasn't in last update, so initialize some things.
        entity_update_new(ent, state, origin);
    } else {
        entity_update_old(ent, state, origin);
    }

    // Assign last received server frame.
    ent->serverframe = cl.frame.number;
    // Assign new state.
    ent->current = *state;

    // Work around Q2PRO server bandwidth optimization.
    if (entity_is_optimized(state)) {
        Com_PlayerToEntityState(&cl.frame.ps, &ent->current);
    }
}

// an entity has just been parsed that has an event value
static void parse_entity_event(const int32_t entityNumber ) {
    clge->ParseEntityEvent( entityNumber );
    //centity_t *cent = ENTITY_FOR_NUMBER( number );//centity_t *cent = &cl_entities[number];

    //// EF_TELEPORTER acts like an event, but is not cleared each frame
    //if ((cent->current.effects & EF_TELEPORTER)) {
    //    CL_TeleporterParticles(cent->current.origin);
    //}

    //switch (cent->current.event) {
    //case EV_ITEM_RESPAWN:
    //    S_StartSound(NULL, number, CHAN_WEAPON, S_RegisterSound("items/respawn1.wav"), 1, ATTN_IDLE, 0);
    //    CL_ItemRespawnParticles(cent->current.origin);
    //    break;
    //case EV_PLAYER_TELEPORT:
    //    S_StartSound(NULL, number, CHAN_WEAPON, S_RegisterSound("misc/tele1.wav"), 1, ATTN_IDLE, 0);
    //    CL_TeleportParticles(cent->current.origin);
    //    break;
    //case EV_FOOTSTEP:
    //    if (cl_footsteps->integer)
    //        S_StartSound(NULL, number, CHAN_BODY, cl_sfx_footsteps[Q_rand() & 3], 1, ATTN_NORM, 0);
    //    break;
    //case EV_FALLSHORT:
    //    S_StartSound(NULL, number, CHAN_AUTO, S_RegisterSound("player/land1.wav"), 1, ATTN_NORM, 0);
    //    break;
    //case EV_FALL:
    //    S_StartSound(NULL, number, CHAN_AUTO, S_RegisterSound("*fall2.wav"), 1, ATTN_NORM, 0);
    //    break;
    //case EV_FALLFAR:
    //    S_StartSound(NULL, number, CHAN_AUTO, S_RegisterSound("*fall1.wav"), 1, ATTN_NORM, 0);
    //    break;
    //}
}

/**
*   @brief  Prepares the client state for 'activation', also setting the predicted values
*           to those of the initially received first valid frame.
**/
static void CL_SetActiveState(void)
{
    cls.state = ca_active;

    cl.serverdelta = Q_align(cl.frame.number, 1);
    cl.time = cl.servertime = 0; // set time, needed for demos

    // initialize oldframe so lerping doesn't hurt anything
    cl.oldframe.valid = false;
    cl.oldframe.ps = cl.frame.ps;

    cl.frameflags = 0;
    cl.initialSeq = cls.netchan.outgoing_sequence;

    if (cls.demo.playback) {
        // init some demo things
        CL_FirstDemoFrame();
    } else {
        // Set the initial client predicted state values.
        VectorCopy(cl.frame.ps.pmove.origin, cl.predictedState.view.origin);//VectorScale(cl.frame.ps.pmove.origin, 0.125f, cl.predicted_origin); // WID: float-movement
        VectorCopy(cl.frame.ps.pmove.velocity, cl.predictedState.view.velocity);//VectorScale(cl.frame.ps.pmove.velocity, 0.125f, cl.predicted_velocity); // WID: float-movement
        if (cl.frame.ps.pmove.pm_type < PM_DEAD &&
            cls.serverProtocol >= PROTOCOL_VERSION_Q2RTXPERIMENTAL) {
            // enhanced servers don't send viewangles
            CL_PredictAngles();
        } else {
            // just use what server provided
            VectorCopy(cl.frame.ps.viewangles, cl.predictedState.view.angles);
        }

        // Copy predicted screen blend, renderflags and viewheight.
        cl.predictedState.view.screen_blend.x = cl.frame.ps.screen_blend[ 0 ];
        cl.predictedState.view.screen_blend.y = cl.frame.ps.screen_blend[ 1 ];
        cl.predictedState.view.screen_blend.z = cl.frame.ps.screen_blend[ 2 ];
        cl.predictedState.view.screen_blend.w = cl.frame.ps.screen_blend[ 3 ];
        // Copy predicted rdflags.
        cl.predictedState.view.rdflags = cl.frame.ps.rdflags;
        // Copy current viewheight into prev and current viewheights.
        cl.predictedState.view_height = cl.frame.ps.pmove.viewheight;
        // Reset local time of viewheight changes.
        cl.predictedState.view_height_time = cl.time;

        // Reset ground information.
        cl.predictedState.groundEntity = nullptr;
        cl.predictedState.groundPlane = { };
    }

    // Fire the ClientBegin callback of the client game module.
    clge->ClientBegin();

    //! Get rid of loading plaque.
    SCR_EndLoadingPlaque();     
    SCR_LagClear();
    //! Get rid of connection screen.
    Con_Close(false);           

    // Check for pause.
    CL_CheckForPause();

    // Update frame times.
    CL_UpdateFrameTimes();

    // Fire a local trigger in case it is not a demo playback.
    if (!cls.demo.playback) {
        EXEC_TRIGGER(cl_beginmapcmd);
        Cmd_ExecTrigger("#cl_enterlevel");
    }
}

/**
*   @brief  Duplicates old player state, into playerstate. Used as a utility function.
*   @param ps Pointer to the state that we want to copy data into.
*   @param ops Pointer to the state's data source.
**/
static void duplicate_player_state( player_state_t *ps, player_state_t *ops ) {
    *ops = *ps;
}

/**
*   Determine whether the player state has to lerp between the current and old frame,
*   or snap 'to'.
**/
static void CL_LerpOrSnapPlayerState( server_frame_t *oldframe, server_frame_t *frame, const int32_t framediv) {
    // Find player states to interpolate between
    player_state_t *ps = &frame->ps;
    player_state_t *ops = &oldframe->ps;

    // No lerping if previous frame was dropped or invalid.
    if ( !oldframe->valid ) {
        duplicate_player_state( ps, ops );
        return;
    }

    // Duplicate state in case the stored old frame number does not match to the
    // expected last frame number.
    const int32_t lastFrameNumber = frame->number - framediv;
    if ( oldframe->number != lastFrameNumber ) {
        duplicate_player_state( ps, ops );
        return;
    }

    // No lerping if player entity was teleported (origin check).
    if ( abs( ops->pmove.origin[ 0 ] - ps->pmove.origin[ 0 ] ) > 256 || // * 8 || // WID: float-movement
        abs( ops->pmove.origin[ 1 ] - ps->pmove.origin[ 1 ] ) > 256 || // * 8 || // WID: float-movement
        abs( ops->pmove.origin[ 2 ] - ps->pmove.origin[ 2 ] ) > 256 ) {// * 8) { // WID: float-movement
        duplicate_player_state( ps, ops );
        return;
    }

    // No lerping if player entity was teleported (event check).
    centity_t *clent = ENTITY_FOR_NUMBER( frame->clientNum + 1 );//ent = &cl_entities[frame->clientNum + 1];
    // If the player entity was within the range of lastFrameNumber and frame->number,
    // and had any teleport events going on, duplicate the player state into the old player state,
    // to prevent it from lerping afar distance.
    if ( clent->serverframe > lastFrameNumber && clent->serverframe <= frame->number &&
        ( clent->current.event == EV_PLAYER_TELEPORT || clent->current.event == EV_OTHER_TELEPORT ) ) {
        duplicate_player_state( ps, ops );
        return;
    }

    // No lerping if teleport bit was flipped.
    //if ((ops->pmove.pm_flags ^ ps->pmove.pm_flags) & PMF_TELEPORT_BIT){
    //    duplicate_player_state( ps, ops );
    //    return;
    //}
    //if ( ( ops->rdflags ^ ps->rdflags ) & RDF_TELEPORT_BIT ) {
    //    duplicate_player_state( ps, ops );
    //    return;
    //}

    // No lerping if POV number changed.
    if ( oldframe->clientNum != frame->clientNum ) {
        duplicate_player_state( ps, ops );
        return;
    }

    // No lerping in case of the enabled developer option.
    if ( cl_nolerp->integer == 1 ) {
        duplicate_player_state( ps, ops );
        return;
    }
}

/**
*   @brief  Called after finished parsing the frame data. All entity states and the
*           player state will be updated, and checked for 'snapping to' their new state,
*           or to smoothly lerp into it. It'll check for any prediction errors afterwards
*           also and calculate its correction value.
*           
*           Will switch the clientstatic state to 'ca_active' if it is the first
*           parsed valid frame and the client is done precaching all data.
**/
void CL_DeltaFrame(void)
{
    // Getting a valid frame message ends the connection process
    //if ( cls.state == ca_precached ) {
    if ( cl.frame.valid && cls.state == ca_precached ) {
        CL_SetActiveState();
    }

    // Determine the current delta frame's server time.
    int64_t framenum = cl.frame.number - cl.serverdelta;
    cl.servertime = framenum * CL_FRAMETIME;

    // rebuild the list of solid entities for this frame
    cl.numSolidEntities = 0;

    // Initialize position of the player's own entity from playerstate.
    // this is needed in situations when player entity is invisible, but
    // server sends an effect referencing it's origin (such as MZ_LOGIN, etc).
    centity_t *clent = ENTITY_FOR_NUMBER( cl.frame.clientNum + 1 );
    Com_PlayerToEntityState( &cl.frame.ps, &clent->current );

    // Iterate over the current frame entity states.
    for ( int32_t i = 0; i < cl.frame.numEntities; i++ ) {
        int32_t j = ( cl.frame.firstEntity + i ) & PARSE_ENTITIES_MASK;
        entity_state_t *state = &cl.entityStates[ j ];

        // Set the current and previous entity state.
        parse_entity_update( state );

        // Fire any needed entity events.
        parse_entity_event( state->number );
    }

    // If we're recording a demo, make sure to store this frame into the demo data.
    if ( cls.demo.recording && !cls.demo.paused && !cls.demo.seeking ) {
        CL_EmitDemoFrame();
    }

    if ( cls.demo.playback ) {
        // this delta has nothing to do with local viewangles,
        // clear it to avoid interfering with demo freelook hack
        VectorClear( cl.frame.ps.pmove.delta_angles );

        // TODO: Proper stair smoothing.

        // Record time of changing and adjusting viewheight if it differs from previous time.
        CL_AdjustViewHeight( cl.frame.ps.pmove.viewheight );
    }

    // Grab mouse in case of player move type changes between frames.
    // TODO: I have no clue why this was here to begin with, it wasn't me.
    if ( cl.oldframe.ps.pmove.pm_type != cl.frame.ps.pmove.pm_type ) {
        IN_Activate();
    }

    // Determine whether the player state has to lerp between the current and old frame,
    // or snap 'to'.
    CL_LerpOrSnapPlayerState( &cl.oldframe, &cl.frame, 1 );

    // See if we had any prediction errors.
    CL_CheckPredictionError();

    // Last but not least, set crosshair color, lol.
    SCR_SetCrosshairColor();
}

#if USE_DEBUG
// for debugging problems when out-of-date entity origin is referenced
void CL_CheckEntityPresent(int entnum, const char *what)
{
    centity_t *e = nullptr;

    if (entnum == cl.frame.clientNum + 1) {
        return; // player entity = current
    }

    e = ENTITY_FOR_NUMBER( entnum ); //e = &cl_entities[entnum];

    if (e && e->serverframe == cl.frame.number) {
        return; // current
    }

    if (e && e->serverframe) {
        Com_LPrintf(PRINT_DEVELOPER,
                    "SERVER BUG: %s on entity %d last seen %d frames ago\n",
                    what, entnum, cl.frame.number - e->serverframe);
    } else {
        Com_LPrintf(PRINT_DEVELOPER,
                    "SERVER BUG: %s on entity %d never seen before\n",
                    what, entnum);
    }
}
#endif




/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARMS

==========================================================================
*/

// Use a static entity ID on some things because the renderer relies on eid to match between meshes
// on the current and previous frames.
#define RESERVED_ENTITIY_GUN 1
#define RESERVED_ENTITIY_TESTMODEL 2
#define RESERVED_ENTITIY_COUNT 3



/*
==============
CL_AddViewWeapon
==============
*/
static int shell_effect_hack( void ) {
    centity_t *ent;
    int         flags = 0;

    ent = ENTITY_FOR_NUMBER( cl.frame.clientNum + 1 );//ent = &cl_entities[clgi.client->frame.clientNum + 1];
    if ( ent->serverframe != cl.frame.number )
        return 0;

    if ( !ent->current.modelindex )
        return 0;

    if ( ent->current.effects & EF_PENT )
        flags |= RF_SHELL_RED;
    if ( ent->current.effects & EF_QUAD )
        flags |= RF_SHELL_BLUE;
    if ( ent->current.effects & EF_DOUBLE )
        flags |= RF_SHELL_DOUBLE;
    if ( ent->current.effects & EF_HALF_DAMAGE )
        flags |= RF_SHELL_HALF_DAM;

    return flags;
}
static void CL_AddViewWeapon(void)
{
    player_state_t *ps, *ops;
    entity_t    gun;        // view model
    int         i, shell_flags;

    // allow the gun to be completely removed
    if (cl_player_model->integer == CL_PLAYER_MODEL_DISABLED) {
        return;
    }

    if (info_hand->integer == 2) {
        return;
    }

    // find states to interpolate between
    ps = &cl.frame.ps;
    ops = &cl.oldframe.ps;

    memset(&gun, 0, sizeof(gun));

    if (gun_model) {
        gun.model = gun_model;  // development tool
    } else {
        gun.model = cl.model_draw[ps->gunindex];
    }
    if (!gun.model) {
        return;
    }

	gun.id = RESERVED_ENTITIY_GUN;

    // set up gun position
    for (i = 0; i < 3; i++) {
        gun.origin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i] +
                        cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
        gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle(ops->gunangles[i],
                        ps->gunangles[i], cl.lerpfrac );
    }

    // Adjust the gun scale so that the gun doesn't intersect with walls.
    // The gun models are not exactly centered at the camera, so adjusting its scale makes them
    // shift on the screen a little when reasonable scale values are used. When extreme values are used,
    // such as 0.01, they move significantly - so we clamp the scale value to an expected range here.
    gun.scale = Cvar_ClampValue(cl_gunscale, 0.1f, 1.0f);

    VectorMA(gun.origin, cl_gun_y->value * gun.scale, cl.v_forward, gun.origin);
    VectorMA(gun.origin, cl_gun_x->value * gun.scale, cl.v_right, gun.origin);
    VectorMA(gun.origin, cl_gun_z->value * gun.scale, cl.v_up, gun.origin);

    VectorCopy(gun.origin, gun.oldorigin);      // don't lerp at all

    if (gun_frame) {
        gun.frame = gun_frame;  // development tool
        gun.oldframe = gun_frame;   // development tool
    } else {

// WID: 40hz - Does proper gun lerping.
// TODO: Add gunrate, and transfer it over the wire.
		if ( ops->gunindex != ps->gunindex ) { // just changed weapons, don't lerp from old
			cl.weapon.frame = cl.weapon.last_frame = ps->gunframe;
			cl.weapon.server_time = cl.servertime;
		} else if ( cl.weapon.frame == -1 || cl.weapon.frame != ps->gunframe ) {
			cl.weapon.frame = ps->gunframe;
			cl.weapon.last_frame = ops->gunframe;
			cl.weapon.server_time = cl.servertime;
		}

		//const float gun_ms = 1.f / ( !ps->gunrate ? 10 : ps->gunrate ) * 1000.f;
		const int32_t playerstate_gun_rate = ps->gunrate;
		const float gun_ms = 1.f / ( !playerstate_gun_rate ? 10 : playerstate_gun_rate ) * 1000.f;
		gun.backlerp = 1.f - ( ( cl.time - ( (float)cl.weapon.server_time - cl.sv_frametime ) ) / gun_ms );
		clamp( gun.backlerp, 0.0f, 1.f );
		gun.frame = cl.weapon.frame;
		gun.oldframe = cl.weapon.last_frame;
// WID: 40hz

		//gun.frame = ps->gunframe;
        //if (gun.frame == 0) {
        //    gun.oldframe = 0;   // just changed weapons, don't lerp from old
        //} else {
        //    gun.oldframe = ops->gunframe;
        //    gun.backlerp = 1.0f - cl.lerpfrac;
        //}
    }

    gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;

    if (cl_gunalpha->value != 1) {
        gun.alpha = Cvar_ClampValue(cl_gunalpha, 0.1f, 1.0f);
        gun.flags |= RF_TRANSLUCENT;
    }

	// add shell effect from player entity
	shell_flags = shell_effect_hack();

	// same entity in rtx mode
	if (cls.ref_type == REF_TYPE_VKPT) {
		gun.flags |= shell_flags;
	}

	model_t* model = MOD_ForHandle(gun.model);
	if (model && strstr(model->name, "v_flareg"))
		gun.scale *= 0.3f; // The flare gun is modeled too large, scale it down to match other weapons

    V_AddEntity(&gun);

	// separate entity in non-rtx mode
    if (shell_flags && cls.ref_type != REF_TYPE_VKPT) {
        gun.alpha = 0.30f * cl_gunalpha->value;
        gun.flags |= shell_flags | RF_TRANSLUCENT;
        V_AddEntity(&gun);
    }
}

static void CL_SetupFirstPersonView(void) {
    const float lerp = cl.lerpfrac;

    // Add kick angles
    if (cl_kickangles->integer) {
        player_state_t *ps = &cl.frame.ps;
        player_state_t *ops = &cl.oldframe.ps;

        const Vector3 kickAngles = QM_Vector3LerpAngles( ops->kick_angles, ps->kick_angles, lerp );
        VectorAdd( cl.refdef.viewangles, kickAngles, cl.refdef.viewangles );
    }

    // Add the view weapon model.
    CL_AddViewWeapon();

    // Inform client state we're not in third-person view.
    cl.thirdPersonView = false;
}

/*
===============
CL_SetupThirdPersionView
===============
*/
static void CL_SetupThirdPersionView(void)
{
    vec3_t focus;
    float fscale, rscale;
    float dist, angle, range;
    trace_t trace;
    static const vec3_t mins = { -4, -4, -4 }, maxs = { 4, 4, 4 };

    // if dead, set a nice view angle
    if (cl.frame.ps.stats[STAT_HEALTH] <= 0) {
        cl.refdef.viewangles[ROLL] = 0;
        cl.refdef.viewangles[PITCH] = 10;
    }

    VectorMA(cl.refdef.vieworg, 512, cl.v_forward, focus);

    cl.refdef.vieworg[2] += 8;

    cl.refdef.viewangles[PITCH] *= 0.5f;
    AngleVectors(cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

    angle = DEG2RAD(cl_thirdperson_angle->value);
    range = cl_thirdperson_range->value;
    fscale = cos(angle);
    rscale = sin(angle);
    VectorMA(cl.refdef.vieworg, -range * fscale, cl.v_forward, cl.refdef.vieworg);
    VectorMA(cl.refdef.vieworg, -range * rscale, cl.v_right, cl.refdef.vieworg);

    CM_BoxTrace(&trace, cl.playerEntityOrigin, cl.refdef.vieworg,
                mins, maxs, cl.bsp->nodes, MASK_SOLID);
    if (trace.fraction != 1.0f) {
        VectorCopy(trace.endpos, cl.refdef.vieworg);
    }

    VectorSubtract(focus, cl.refdef.vieworg, focus);
    dist = sqrtf(focus[0] * focus[0] + focus[1] * focus[1]);

    cl.refdef.viewangles[PITCH] = -RAD2DEG(atan2(focus[2], dist));
    cl.refdef.viewangles[YAW] -= cl_thirdperson_angle->value;

    cl.thirdPersonView = true;
}

void CL_FinishViewValues(void)
{
    centity_t *ent;

    if (cl_player_model->integer != CL_PLAYER_MODEL_THIRD_PERSON)
        goto first;

    ent = ENTITY_FOR_NUMBER( cl.frame.clientNum + 1 );//ent = &cl_entities[cl.frame.clientNum + 1];
    if ( ent->serverframe != cl.frame.number ) {
        goto first;
    }

    // Need a model to display if we want to go third-person mode.
    if ( !ent->current.modelindex ) {
        goto first;
    }

    CL_SetupThirdPersionView();
    return;

first:
    CL_SetupFirstPersonView();
}

//#if USE_SMOOTH_DELTA_ANGLES
static inline float LerpShort(int a2, int a1, float frac)
{
    if (a1 - a2 > 32768)
        a1 &= 65536;
    if (a2 - a1 > 32768)
        a1 &= 65536;
    return a2 + frac * (a1 - a2);
}
//#endif

static inline float lerp_client_fov(float ofov, float nfov, float lerp)
{
    if (cls.demo.playback) {
        int fov = info_fov->integer;

        if (fov < 1)
            fov = 90;
        else if (fov > 160)
            fov = 160;

        if (info_uf->integer & UF_LOCALFOV)
            return fov;

        if (!(info_uf->integer & UF_PLAYERFOV)) {
            if (ofov >= 90)
                ofov = fov;
            if (nfov >= 90)
                nfov = fov;
        }
    }

    return ofov + lerp * (nfov - ofov);
}

/**
*   @brief  Sets cl.refdef view values and sound spatialization params.
*           Usually called from CL_PrepareViewEntities, but may be directly called from the main
*           loop if rendering is disabled but sound is running.
**/
void CL_CalcViewValues(void) {   
    vec3_t viewoffset;

    if ( !cl.frame.valid ) {
        return;
    }

    // Find states to interpolate between
    player_state_t *ps = &cl.frame.ps;
    player_state_t *ops = &cl.oldframe.ps;

    const float lerp = cl.lerpfrac;

    // TODO: In the future, when we got this moved into ClientGame, use PM_STEP_.. values from SharedGame.
    static constexpr int32_t STEP_TIME = 100;
    static constexpr float STEP_BASE_1_FRAMETIME = 0.01f;

    // calculate the origin
    //if (!cls.demo.playback && cl_predict->integer && !(ps->pmove.pm_flags & PMF_NO_POSITIONAL_PREDICTION) ) {
    if ( clge->UsePrediction() ) {
        // TODO: In the future, when we got this moved into ClientGame, use PM_STEP_.. values from SharedGame.
        // TODO: Is this accurate?
        // PM_MAX_STEP_HEIGHT = 18
        // PM_MIN_STEP_HEIGHT = 4
        //
        // However, the original code was 127 * 0.125 = 15.875, which is a 2.125 difference to PM_MAX_STEP_HEIGHT
        //static constexpr float STEP_HEIGHT = PM_MAX_STEP_HEIGHT - PM_MIN_STEP_HEIGHT // This seems like what would be more accurate?
        static constexpr float STEP_HEIGHT = 15.875;

        // use predicted values
        uint64_t delta = cls.realtime - cl.predictedState.step_time;
        float backlerp = lerp - 1.0f;

        VectorMA(cl.predictedState.view.origin, backlerp, cl.predictedState.error, cl.refdef.vieworg);

        // smooth out stair climbing
        if (fabs(cl.predictedState.step) < STEP_HEIGHT ) {
            delta <<= 1; // small steps
        }

		// WID: Prediction: Was based on old 10hz, 100ms.
        if ( delta < STEP_TIME ) {
            cl.refdef.vieworg[ 2 ] -= cl.predictedState.step * ( STEP_TIME - delta ) * ( 1.f / STEP_TIME );
        }
    } else {
        // just use interpolated values
        //for ( int32_t i = 0; i < 3; i++ ) {
        //    cl.refdef.vieworg[ i ] = ops->pmove.origin[ i ] +
        //        lerp * ( ps->pmove.origin[ i ] - ops->pmove.origin[ i ] );
        //}
        Vector3 viewOrg = QM_Vector3Lerp( ops->pmove.origin, ps->pmove.origin, lerp );
        VectorCopy( viewOrg, cl.refdef.vieworg );

    }

    // if not running a demo or on a locked frame, add the local angle movement
    if (cls.demo.playback) {
        LerpAngles(ops->viewangles, ps->viewangles, lerp, cl.refdef.viewangles);
    } else if (ps->pmove.pm_type < PM_DEAD) {
        // use predicted values
        VectorCopy(cl.predictedState.view.angles, cl.refdef.viewangles);
    } else if (ops->pmove.pm_type < PM_DEAD && !( ps->pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) ) {/*cls.serverProtocol > PROTOCOL_VERSION_Q2RTXPERIMENTAL ) {*/
        // lerp from predicted angles, since enhanced servers
        // do not send viewangles each frame
        LerpAngles(cl.predictedState.view.angles, ps->viewangles, lerp, cl.refdef.viewangles);
    } else {
		//if ( !( ps->pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) ) {
		// just use interpolated values
		//LerpAngles( ops->viewangles, ps->viewangles, lerp, cl.refdef.viewangles );
		//} else {
		VectorCopy( ps->viewangles, cl.refdef.viewangles );
		//}
    }

//#if USE_SMOOTH_DELTA_ANGLES
    cl.delta_angles[ 0 ] = AngleMod( LerpAngle( ops->pmove.delta_angles[ 0 ], ps->pmove.delta_angles[ 0 ], lerp ) );
    cl.delta_angles[ 1 ] = AngleMod( LerpAngle( ops->pmove.delta_angles[ 1 ], ps->pmove.delta_angles[ 1 ], lerp ) );
    cl.delta_angles[ 2 ] = AngleMod( LerpAngle( ops->pmove.delta_angles[ 2 ], ps->pmove.delta_angles[ 2 ], lerp ) );
//#endif

    // interpolate blend colors if the last frame wasn't clear
    float blendfrac = ops->screen_blend[ 3 ] ? cl.lerpfrac : 1;
    Vector4Lerp( ops->screen_blend, ps->screen_blend, blendfrac, cl.refdef.screen_blend );
    //float damageblendfrac = ops->damage_blend[ 3 ] ? cl.lerpfrac : 1;
    //Vector4Lerp( ops->damage_blend, ps->damage_blend, damageblendfrac, cl.refdef.damage_blend );

    // Mix in screen_blend from cgame pmove
    // FIXME: Should also be interpolated?...
    if ( cl.predictedState.view.screen_blend.z > 0 ) {
        float a2 = cl.refdef.screen_blend[ 3 ] + ( 1 - cl.refdef.screen_blend[ 3 ] ) * cl.predictedState.view.screen_blend.z; // new total alpha
        float a3 = cl.refdef.screen_blend[ 3 ] / a2; // fraction of color from old

        LerpVector( cl.predictedState.view.screen_blend, cl.refdef.screen_blend, a3, cl.refdef.screen_blend );
        cl.refdef.screen_blend[ 3 ] = a2;
    }


    // interpolate field of view
    cl.fov_x = lerp_client_fov(ops->fov, ps->fov, lerp);
    cl.fov_y = V_CalcFov(cl.fov_x, 4, 3);

    LerpVector(ops->viewoffset, ps->viewoffset, lerp, viewoffset);

    // Smooth out view height over 100ms
    //float viewheight_lerp = ( cl.time - cl.viewheight.change_time );
    //viewheight_lerp = STEP_TIME - min( viewheight_lerp, STEP_TIME );
    //float predicted_viewheight = cl.viewheight.current + (float)( cl.viewheight.previous - cl.viewheight.current ) * viewheight_lerp * STEP_BASE_1_FRAMETIME;
    //viewoffset[ 2 ] += predicted_viewheight;
    float viewheight_lerp = ( cl.time - cl.predictedState.view_height_time );
    viewheight_lerp = STEP_TIME - min( viewheight_lerp, STEP_TIME );
    float predicted_viewheight = cl.predictedState.view_height + (float)( cl.frame.ps.pmove.viewheight - cl.predictedState.view_height ) * viewheight_lerp * STEP_BASE_1_FRAMETIME;
    viewoffset[ 2 ] += predicted_viewheight;
    AngleVectors(cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

    VectorCopy(cl.refdef.vieworg, cl.playerEntityOrigin);
    VectorCopy(cl.refdef.viewangles, cl.playerEntityAngles);

    if (cl.playerEntityAngles[PITCH] > 180) {
        cl.playerEntityAngles[PITCH] -= 360;
    }

    cl.playerEntityAngles[PITCH] = cl.playerEntityAngles[PITCH] / 3;

    VectorAdd(cl.refdef.vieworg, viewoffset, cl.refdef.vieworg);

    VectorCopy(cl.refdef.vieworg, listener_origin);
    VectorCopy(cl.v_forward, listener_forward);
    VectorCopy(cl.v_right, listener_right);
    VectorCopy(cl.v_up, listener_up);
}

void CL_AddTestModel(void)
{
    static float frame = 0.f;
    static int prevtime = 0;

    if (cl_testmodel_handle != -1)
    {
        model_t* model = MOD_ForHandle(cl_testmodel_handle);

        if (model != NULL && model->meshes != NULL)
        {
            entity_t entity = { 0 };
            entity.model = cl_testmodel_handle;
            entity.id = RESERVED_ENTITIY_TESTMODEL;

        	VectorCopy(cl_testmodel_position, entity.origin);
            VectorCopy(cl_testmodel_position, entity.oldorigin);

            entity.alpha = cl_testalpha->value;
            clamp(entity.alpha, 0.f, 1.f);
            if (entity.alpha < 1.f)
                entity.flags |= RF_TRANSLUCENT;

            int numframes = model->numframes;
            if (model->iqmData)
                numframes = (int)model->iqmData->num_poses;

            if (numframes > 1 && prevtime != 0)
            {
                const float millisecond = 1e-3f;

                int timediff = cl.time - prevtime;
                frame += (float)timediff * millisecond * max(cl_testfps->value, 0.f);

                if (frame >= (float)numframes || frame < 0.f)
                    frame = 0.f;

                float frac = frame - floorf(frame);

                entity.oldframe = (int)frame;
                entity.frame = entity.oldframe + 1;
                entity.backlerp = 1.f - frac;
            }

            prevtime = cl.time;

            V_AddEntity(&entity);
        }
    }
}

/**
*   @brief  Prepares the current frame's view scene for the refdef by 
*           emitting all frame data(entities, particles, dynamic lights, lightstyles,
*           and temp entities) to the refresh definition.
**/
void CL_PrepareViewEntities(void)
{
    // WID: Moved to V_RenderView:
    //CL_CalcViewValues();
    //CL_FinishViewValues();
    clge->PrepareViewEntities();
    LOC_AddLocationsToScene();
}

/*
===============
CL_GetEntitySoundOrigin

Called to get the sound spatialization origin
===============
*/
void CL_GetEntitySoundOrigin(int entnum, vec3_t org)
{
    centity_t   *ent;
    mmodel_t    *cm;
    vec3_t      mid;

    if (entnum < 0 || entnum >= MAX_EDICTS) {
        Com_Error(ERR_DROP, "%s: bad entnum: %d", __func__, entnum);
    }

    if (!entnum || entnum == listener_entnum) {
        // should this ever happen?
        VectorCopy(listener_origin, org);
        return;
    }

    // interpolate origin
    // FIXME: what should be the sound origin point for RF_BEAM entities?
    ent = ENTITY_FOR_NUMBER( entnum );//ent = &cl_entities[entnum];
    LerpVector(ent->prev.origin, ent->current.origin, cl.lerpfrac, org);

    // offset the origin for BSP models
    if (ent->current.solid == PACKED_BSP) {
        cm = cl.model_clip[ent->current.modelindex];
        if (cm) {
            VectorAvg(cm->mins, cm->maxs, mid);
            VectorAdd(org, mid, org);
        }
    }
}

