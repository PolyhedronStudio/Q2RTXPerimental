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

#include "client.h"
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

static inline bool entity_is_optimized(const entity_state_t *state)
{
	// WID: net-protocol2: We don't want this anyway, should get rid of it?
    //if (cls.serverProtocol != PROTOCOL_VERSION_Q2PRO)
	if ( cls.serverProtocol == PROTOCOL_VERSION_Q2RTXPERIMENTAL )
        return false;

    if (state->number != cl.frame.clientNum + 1)
        return false;

    if (cl.frame.ps.pmove.pm_type >= PM_DEAD)
        return false;

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
        (state->renderfx & (RF_FRAMELERP | RF_BEAM))) {
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
// WID: 40hz
	if ( ent->current_frame != state->frame ) {
		if ( state->renderfx & RF_OLD_FRAME_LERP ) {
			ent->last_frame = ent->current.old_frame;
		} else {
			ent->last_frame = ent->current.frame;
		}
		ent->current_frame = state->frame;
		ent->frame_servertime = cl.servertime;
	}
// WID: 40hz

    // shuffle the last state to previous
    ent->prev = ent->current;
}

static inline bool entity_is_new(const centity_t *ent)
{
    if (!cl.oldframe.valid)
        return true;    // last received frame was invalid

    if (ent->serverframe != cl.oldframe.number)
        return true;    // wasn't in last received frame

    if (cl_nolerp->integer == 2)
        return true;    // developer option, always new

    if (cl_nolerp->integer == 3)
        return false;   // developer option, lerp from last received frame

    if (cl.oldframe.number != cl.frame.number - 1)
        return true;    // previous server frame was dropped

    return false;
}

static void parse_entity_update(const entity_state_t *state)
{
    centity_t *ent = &cl_entities[state->number];
    const vec_t *origin;
    vec3_t origin_v;

    // if entity is solid, decode mins/maxs and add to the list
    if (state->solid && state->number != cl.frame.clientNum + 1
        && cl.numSolidEntities < MAX_PACKET_ENTITIES) {
        cl.solidEntities[cl.numSolidEntities++] = ent;
        if (state->solid != PACKED_BSP) {
			// WID: upgr-solid: Q2RE Approach.
			MSG_UnpackSolidUint32( state->solid, ent->mins, ent->maxs );
            // encoded bbox
            //if (cl.esFlags & MSG_ES_LONGSOLID) {
            //    MSG_UnpackSolid32(state->solid, ent->mins, ent->maxs);
            //} else {
            //    MSG_UnpackSolid16(state->solid, ent->mins, ent->maxs);
            //}
        }
    }

    // work around Q2PRO server bandwidth optimization
    if (entity_is_optimized(state)) {
        //VectorScale(cl.frame.ps.pmove.origin, 0.125f, origin_v); // WID: float-movement
		VectorCopy(cl.frame.ps.pmove.origin, origin_v );
        origin = origin_v;
    } else {
        origin = state->origin;
    }

    if (entity_is_new(ent)) {
        // wasn't in last update, so initialize some things
        entity_update_new(ent, state, origin);
    } else {
        entity_update_old(ent, state, origin);
    }

    ent->serverframe = cl.frame.number;
    ent->current = *state;

    // work around Q2PRO server bandwidth optimization
    if (entity_is_optimized(state)) {
        Com_PlayerToEntityState(&cl.frame.ps, &ent->current);
    }
}

// an entity has just been parsed that has an event value
static void parse_entity_event(int number)
{
    centity_t *cent = &cl_entities[number];

    // EF_TELEPORTER acts like an event, but is not cleared each frame
    if ((cent->current.effects & EF_TELEPORTER)) {
        CL_TeleporterParticles(cent->current.origin);
    }

    switch (cent->current.event) {
    case EV_ITEM_RESPAWN:
        S_StartSound(NULL, number, CHAN_WEAPON, S_RegisterSound("items/respawn1.wav"), 1, ATTN_IDLE, 0);
        CL_ItemRespawnParticles(cent->current.origin);
        break;
    case EV_PLAYER_TELEPORT:
        S_StartSound(NULL, number, CHAN_WEAPON, S_RegisterSound("misc/tele1.wav"), 1, ATTN_IDLE, 0);
        CL_TeleportParticles(cent->current.origin);
        break;
    case EV_FOOTSTEP:
        if (cl_footsteps->integer)
            S_StartSound(NULL, number, CHAN_BODY, cl_sfx_footsteps[Q_rand() & 3], 1, ATTN_NORM, 0);
        break;
    case EV_FALLSHORT:
        S_StartSound(NULL, number, CHAN_AUTO, S_RegisterSound("player/land1.wav"), 1, ATTN_NORM, 0);
        break;
    case EV_FALL:
        S_StartSound(NULL, number, CHAN_AUTO, S_RegisterSound("*fall2.wav"), 1, ATTN_NORM, 0);
        break;
    case EV_FALLFAR:
        S_StartSound(NULL, number, CHAN_AUTO, S_RegisterSound("*fall1.wav"), 1, ATTN_NORM, 0);
        break;
    }
}

static void set_active_state(void)
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
        // set initial cl.predicted_origin and cl.predicted_angles
        VectorCopy(cl.frame.ps.pmove.origin, cl.predictedState.origin);//VectorScale(cl.frame.ps.pmove.origin, 0.125f, cl.predicted_origin); // WID: float-movement
        VectorCopy(cl.frame.ps.pmove.velocity, cl.predictedState.velocity);//VectorScale(cl.frame.ps.pmove.velocity, 0.125f, cl.predicted_velocity); // WID: float-movement
        if (cl.frame.ps.pmove.pm_type < PM_DEAD &&
            cls.serverProtocol >= PROTOCOL_VERSION_Q2RTXPERIMENTAL) {
            // enhanced servers don't send viewangles
            CL_PredictAngles();
        } else {
            // just use what server provided
            VectorCopy(cl.frame.ps.viewangles, cl.predictedState.angles);
        }

        // Copy predicted screen blend, renderflags and viewheight.
        Vector4Copy( cl.frame.ps.screen_blend, cl.predictedState.screen_blend );
        // Copy predicted rdflags.
        cl.predictedState.rdflags = cl.frame.ps.rdflags;
        // Copy current viewheight into prev and current viewheights.
        cl.viewheight.current = cl.viewheight.previous = cl.frame.ps.pmove.viewheight;
    }

    // Reset local time of viewheight changes.
    cl.viewheight.change_time = 0;

    // Reset ground information.
    cl.lastGround.entity = nullptr;
    cl.lastGround.plane = { };

    SCR_EndLoadingPlaque();     // get rid of loading plaque
    SCR_LagClear();
    Con_Close(false);           // get rid of connection screen

    CL_CheckForPause();

    CL_UpdateFrameTimes();

    if (!cls.demo.playback) {
        EXEC_TRIGGER(cl_beginmapcmd);
        Cmd_ExecTrigger("#cl_enterlevel");
    }
}

static void
check_player_lerp(server_frame_t *oldframe, server_frame_t *frame, int framediv)
{
    player_state_t *ps, *ops;
    centity_t *ent;
    int oldnum;

    // find states to interpolate between
    ps = &frame->ps;
    ops = &oldframe->ps;

    // no lerping if previous frame was dropped or invalid
    if (!oldframe->valid)
        goto dup;

    oldnum = frame->number - framediv;
    if (oldframe->number != oldnum)
        goto dup;

    // no lerping if player entity was teleported (origin check)
    if (abs(ops->pmove.origin[0] - ps->pmove.origin[0]) > 256 || // * 8 || // WID: float-movement
        abs(ops->pmove.origin[1] - ps->pmove.origin[1]) > 256 || // * 8 || // WID: float-movement
        abs(ops->pmove.origin[2] - ps->pmove.origin[2]) > 256 ) {// * 8) { // WID: float-movement
        goto dup;
    }

    // no lerping if player entity was teleported (event check)
    ent = &cl_entities[frame->clientNum + 1];
    if (ent->serverframe > oldnum && ent->serverframe <= frame->number && 
		 (ent->current.event == EV_PLAYER_TELEPORT || ent->current.event == EV_OTHER_TELEPORT) ) {
        goto dup;
    }

    // no lerping if teleport bit was flipped
    //if ((ops->pmove.pm_flags ^ ps->pmove.pm_flags) & PMF_TELEPORT_BIT)
    //    goto dup;
    // no lerping if teleport bit was flipped
    //if ( !cl.csr.extended && ( ops->pmove.pm_flags ^ ps->pmove.pm_flags ) & PMF_TELEPORT_BIT )
    //    goto dup;
    //if ( cl.csr.extended && ( ops->rdflags ^ ps->rdflags ) & RDF_TELEPORT_BIT )
    //    goto dup;

    // no lerping if POV number changed
    if (oldframe->clientNum != frame->clientNum)
        goto dup;

    // developer option
    if (cl_nolerp->integer == 1)
        goto dup;

    return;

dup:
    // duplicate the current state so lerping doesn't hurt anything
    *ops = *ps;
}

/*
==================
CL_DeltaFrame

A valid frame has been parsed.
==================
*/
void CL_DeltaFrame(void)
{
    centity_t           *ent;
    entity_state_t      *state;
    int                 i, j;
    int                 framenum;
    int                 prevstate = cls.state;

    // getting a valid frame message ends the connection process
    if (cls.state == ca_precached)
        set_active_state();

    // set server time
    framenum = cl.frame.number - cl.serverdelta;
    cl.servertime = framenum * CL_FRAMETIME;

    // rebuild the list of solid entities for this frame
    cl.numSolidEntities = 0;

    // initialize position of the player's own entity from playerstate.
    // this is needed in situations when player entity is invisible, but
    // server sends an effect referencing it's origin (such as MZ_LOGIN, etc)
    ent = &cl_entities[cl.frame.clientNum + 1];
    Com_PlayerToEntityState(&cl.frame.ps, &ent->current);

    for (i = 0; i < cl.frame.numEntities; i++) {
        j = (cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK;
        state = &cl.entityStates[j];

        // set current and prev
        parse_entity_update(state);

        // fire events
        parse_entity_event(state->number);
    }

    if (cls.demo.recording && !cls.demo.paused && !cls.demo.seeking) {
        CL_EmitDemoFrame();
    }

    if (cls.demo.playback) {
        // this delta has nothing to do with local viewangles,
        // clear it to avoid interfering with demo freelook hack
        VectorClear(cl.frame.ps.pmove.delta_angles);
    }

    if (cl.oldframe.ps.pmove.pm_type != cl.frame.ps.pmove.pm_type) {
        IN_Activate();
    }

    check_player_lerp(&cl.oldframe, &cl.frame, 1);

    CL_CheckPredictionError();

    SCR_SetCrosshairColor();
}

#if USE_DEBUG
// for debugging problems when out-of-date entity origin is referenced
void CL_CheckEntityPresent(int entnum, const char *what)
{
    centity_t *e;

    if (entnum == cl.frame.clientNum + 1) {
        return; // player entity = current
    }

    e = &cl_entities[entnum];
    if (e->serverframe == cl.frame.number) {
        return; // current
    }

    if (e->serverframe) {
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

DYNLIGHT ENTITY SUPPORT:

==========================================================================
*/
void CL_PacketEntity_AddSpotlight( centity_t *cent, entity_t *ent, entity_state_t *s1 ) {
	// Calculate RGB vector.
	vec3_t rgb = { 1.f, 1.f, 1.f };
	rgb[ 0 ] = ( 1.0f / 255.f ) * s1->rgb[ 0 ];
	rgb[ 1 ] = ( 1.0f / 255.f ) * s1->rgb[ 1 ];
	rgb[ 2 ] = ( 1.0f / 255.f ) * s1->rgb[ 2 ];

	// Extract light intensity from "frame".
	float lightIntensity = s1->intensity;

	// Calculate the spotlight's view direction based on set euler angles.
	vec3_t view_dir, right_dir, up_dir;
	AngleVectors( ent->angles, view_dir, right_dir, up_dir );

	// Add the spotlight. (x = 90, y = 0, z = 0) should give us one pointing right down to the floor. (width 90, falloff 0)
	// Use the image based texture profile in case one is set.
	#if 0
	if ( s1->image_profile ) {
		qhandle_t spotlightPicHandle = R_RegisterImage( "flashlight_profile_01", IT_PIC, static_cast<imageflags_t>( IF_PERMANENT | IF_BILERP ) );
		V_AddSpotLightTexEmission( ent->origin, view_dir, lightIntensity,
					// TODO: Multiply the RGB?
					rgb[ 0 ] * 2, rgb[ 1 ] * 2, rgb[ 2 ] * 2,
					s1->angle_width, spotlightPicHandle );
	} else
	#endif
	{
		V_AddSpotLight( ent->origin, view_dir, lightIntensity,
						// TODO: Multiply the RGB?
						rgb[ 0 ] * 2, rgb[ 1 ] * 2, rgb[ 2 ] * 2,
						s1->angle_width, s1->angle_falloff );
	}

	// Add spotlight. (x = 90, y = 0, z = 0) should give us one pointing right down to the floor. (width 90, falloff 0)
	//V_AddSpotLight( ent->origin, view_dir, 225.0, 1.f, 0.1f, 0.1f, 45, 0 );
	//V_AddSphereLight( ent.origin, 500.f, 1.6f, 1.0f, 0.2f, 10.f );
	//V_AddSpotLightTexEmission( light_pos, view_dir, cl_flashlight_intensity->value, 1.f, 1.f, 1.f, 90.0f, flashlight_profile_tex );
}

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

static int adjust_shell_fx(int renderfx)
{
	// PMM - at this point, all of the shells have been handled
	// if we're in the rogue pack, set up the custom mixing, otherwise just
	// keep going
	if (!strcmp(fs_game->string, "rogue")) {
		// all of the solo colors are fine.  we need to catch any of the combinations that look bad
		// (double & half) and turn them into the appropriate color, and make double/quad something special
		if (renderfx & RF_SHELL_HALF_DAM) {
			// ditch the half damage shell if any of red, blue, or double are on
			if (renderfx & (RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
				renderfx &= ~RF_SHELL_HALF_DAM;
		}

		if (renderfx & RF_SHELL_DOUBLE) {
			// lose the yellow shell if we have a red, blue, or green shell
			if (renderfx & (RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_GREEN))
				renderfx &= ~RF_SHELL_DOUBLE;
			// if we have a red shell, turn it to purple by adding blue
			if (renderfx & RF_SHELL_RED)
				renderfx |= RF_SHELL_BLUE;
			// if we have a blue shell (and not a red shell), turn it to cyan by adding green
			else if (renderfx & RF_SHELL_BLUE) {
				// go to green if it's on already, otherwise do cyan (flash green)
				if (renderfx & RF_SHELL_GREEN)
					renderfx &= ~RF_SHELL_BLUE;
				else
					renderfx |= RF_SHELL_GREEN;
			}
		}
	}

	return renderfx;
}

/*
===============
CL_AddPacketEntities

===============
*/
static void CL_AddPacketEntities(void)
{
    entity_t            ent;
    entity_state_t      *s1;
    float               autorotate;
    int                 i;
    int                 pnum;
    centity_t           *cent;
    int                 autoanim;
    clientinfo_t        *ci;
    unsigned int        effects, renderfx;
	int base_entity_flags = 0;

    // bonus items rotate at a fixed rate
	autorotate = AngleMod( cl.time * BASE_FRAMETIME_1000 );//AngleMod(cl.time * 0.1f); // WID: 40hz: Adjusted.

    // brush models can auto animate their frames
    autoanim = 2 * cl.time / 1000;

    memset(&ent, 0, sizeof(ent));

    for (pnum = 0; pnum < cl.frame.numEntities; pnum++) {
        i = (cl.frame.firstEntity + pnum) & PARSE_ENTITIES_MASK;
        s1 = &cl.entityStates[i];

        cent = &cl_entities[s1->number];
        ent.id = cent->id + RESERVED_ENTITIY_COUNT;

        effects = s1->effects;
        renderfx = s1->renderfx;

        // set frame
        if (effects & EF_ANIM01)
            ent.frame = autoanim & 1;
        else if (effects & EF_ANIM23)
            ent.frame = 2 + (autoanim & 1);
        else if (effects & EF_ANIM_ALL)
            ent.frame = autoanim;
        else if (effects & EF_ANIM_ALLFAST)
            ent.frame = cl.time / BASE_FRAMETIME; // WID: 40hz: Adjusted. cl.time / 100;
        else
            ent.frame = s1->frame;

        // quad and pent can do different things on client
        if (effects & EF_PENT) {
            effects &= ~EF_PENT;
            effects |= EF_COLOR_SHELL;
            renderfx |= RF_SHELL_RED;
        }

        if (effects & EF_QUAD) {
            effects &= ~EF_QUAD;
            effects |= EF_COLOR_SHELL;
            renderfx |= RF_SHELL_BLUE;
        }

        if (effects & EF_DOUBLE) {
            effects &= ~EF_DOUBLE;
            effects |= EF_COLOR_SHELL;
            renderfx |= RF_SHELL_DOUBLE;
        }

        if (effects & EF_HALF_DAMAGE) {
            effects &= ~EF_HALF_DAMAGE;
            effects |= EF_COLOR_SHELL;
            renderfx |= RF_SHELL_HALF_DAM;
        }

        // optionally remove the glowing effect
        if (cl_noglow->integer)
            renderfx &= ~RF_GLOW;

        ent.oldframe = cent->prev.frame;
        ent.backlerp = 1.0f - cl.lerpfrac;

// WID: 40hz - Does proper frame lerping for 10hz models.
		// TODO: Add a specific render flag for this perhaps? 
		// TODO: must only do this on alias models
		// Don't do this for 'world' model?
		if ( ent.model != 0 && cent->last_frame != cent->current_frame ) {
			// Calculate back lerpfraction. (10hz.)
			ent.backlerp = 1.0f - ( ( cl.time - ( (float)cent->frame_servertime - cl.sv_frametime ) ) / 100.f );
			clamp( ent.backlerp, 0.0f, 1.0f );
			ent.frame = cent->current_frame;
			ent.oldframe = cent->last_frame;
		}
// WID: 40hz

        if (renderfx & RF_FRAMELERP) {
            // step origin discretely, because the frames
            // do the animation properly
            VectorCopy(cent->current.origin, ent.origin);
            VectorCopy(cent->current.old_origin, ent.oldorigin);  // FIXME

			// WID: Stair stepping interpolation for monster entities.
			if ( renderfx & RF_STAIR_STEP ) {
				// Calculate lerpfraction. (10hz.)
				float backlerpFrac = 1.0f - ( ( cl.time - ( (float)cent->frame_servertime - cl.sv_frametime ) ) / 100.f );
				clamp( backlerpFrac, 0.0f, 1.0f );

				// interpolate the z-origin
				const float zlerpedOrigin =
					( cent->prev.origin )[ 2 ] + ( backlerpFrac ) * ( ( cent->current.origin )[ 2 ] - ( cent->prev.origin )[ 2 ] );

				// Settle with the lerped Z for the RF_FRAMELERPED origins. 
				ent.origin[ 2 ] = zlerpedOrigin;
				ent.oldorigin[ 2 ] = zlerpedOrigin;
			}
		} else if ( renderfx & RF_BEAM ) {
			// interpolate start and end points for beams
			LerpVector( cent->prev.origin, cent->current.origin,
					   cl.lerpfrac, ent.origin );
			LerpVector( cent->prev.old_origin, cent->current.old_origin,
					   cl.lerpfrac, ent.oldorigin );
		} else {
            if (s1->number == cl.frame.clientNum + 1) {
                // use predicted origin
                VectorCopy(cl.playerEntityOrigin, ent.origin);
                VectorCopy(cl.playerEntityOrigin, ent.oldorigin);
            } else {
                // interpolate origin
                LerpVector(cent->prev.origin, cent->current.origin,
                           cl.lerpfrac, ent.origin);
                VectorCopy(ent.origin, ent.oldorigin);
            }
//#if USE_FPS
//            // run alias model animation
//            if (cent->prev_frame != s1->frame) {
//                int delta = cl.time - cent->anim_start;
//                float frac;
//
//                if (delta > BASE_FRAMETIME) {
//                    cent->prev_frame = s1->frame;
//                    frac = 1;
//                } else if (delta > 0) {
//                    frac = delta * BASE_1_FRAMETIME;
//                } else {
//                    frac = 0;
//                }
//
//                ent.oldframe = cent->prev_frame;
//                ent.backlerp = 1.0f - frac;
//            }
//#endif
        }

        if ((effects & EF_GIB) && !cl_gibs->integer) {
            goto skip;
        }

        // create a new entity

        // tweak the color of beams
        if (renderfx & RF_BEAM) {
            // the four beam colors are encoded in 32 bits of skinnum (hack)
            ent.alpha = 0.30f;
            ent.skinnum = (s1->skinnum >> ((Q_rand() % 4) * 8)) & 0xff;
            ent.model = 0;
        } else {
            // set skin
            if (s1->modelindex == MODELINDEX_PLAYER ) {
                // use custom player skin
                ent.skinnum = 0;
                ci = &cl.clientinfo[s1->skinnum & 0xff];
                ent.skin = ci->skin;
                ent.model = ci->model;
                if (!ent.skin || !ent.model) {
                    ent.skin = cl.baseclientinfo.skin;
                    ent.model = cl.baseclientinfo.model;
                    ci = &cl.baseclientinfo;
                }
                if (renderfx & RF_USE_DISGUISE) {
                    char buffer[MAX_QPATH];

                    Q_concat(buffer, sizeof(buffer), "players/", ci->model_name, "/disguise.pcx");
                    ent.skin = R_RegisterSkin(buffer);
                }
            } else {
                ent.skinnum = s1->skinnum;
                ent.skin = 0;
                ent.model = cl.model_draw[s1->modelindex];
                if (ent.model == cl_mod_laser || ent.model == cl_mod_dmspot)
                    renderfx |= RF_NOSHADOW;
            }
        }

        // only used for black hole model right now, FIXME: do better
        if ((renderfx & RF_TRANSLUCENT) && !(renderfx & RF_BEAM))
            ent.alpha = 0.70f;

        // render effects (fullbright, translucent, etc)
        if ((effects & EF_COLOR_SHELL))
            ent.flags = renderfx & RF_FRAMELERP;    // renderfx go on color shell entity
        else
            ent.flags = renderfx;

        // calculate angles
        if (effects & EF_ROTATE) {  // some bonus items auto-rotate
            ent.angles[0] = 0;
            ent.angles[1] = autorotate;
            ent.angles[2] = 0;
        } else if (effects & EF_SPINNINGLIGHTS) {
            vec3_t forward;
            vec3_t start;

            ent.angles[0] = 0;
            ent.angles[1] = AngleMod(cl.time / 2) + s1->angles[1];
            ent.angles[2] = 180;

            AngleVectors(ent.angles, forward, NULL, NULL);
            VectorMA(ent.origin, 64, forward, start);
            V_AddLight(start, 100, 1, 0, 0);
        } else if (s1->number == cl.frame.clientNum + 1) {
            VectorCopy(cl.playerEntityAngles, ent.angles);      // use predicted angles
        } else { // interpolate angles
            LerpAngles(cent->prev.angles, cent->current.angles,
                       cl.lerpfrac, ent.angles);

            // mimic original ref_gl "leaning" bug (uuugly!)
            if (s1->modelindex == MODELINDEX_PLAYER && cl_rollhack->integer) {
                ent.angles[ROLL] = -ent.angles[ROLL];
            }
        }

        //int base_entity_flags = 0; // WID: C++20: Moved to the top of function.
		base_entity_flags = 0; // WID: C++20: Make sure to however, reset it to 0.

        if (s1->number == cl.frame.clientNum + 1) {
            if (effects & EF_FLAG1)
                V_AddLight(ent.origin, 225, 1.0f, 0.1f, 0.1f);
            else if (effects & EF_FLAG2)
                V_AddLight(ent.origin, 225, 0.1f, 0.1f, 1.0f);
            else if (effects & EF_TAGTRAIL)
                V_AddLight(ent.origin, 225, 1.0f, 1.0f, 0.0f);
            else if (effects & EF_TRACKERTRAIL)
                V_AddLight(ent.origin, 225, -1.0f, -1.0f, -1.0f);

			if (!cl.thirdPersonView)
			{
				if(cls.ref_type == REF_TYPE_VKPT)
					base_entity_flags |= RF_VIEWERMODEL;    // only draw from mirrors
				else
                goto skip;
            }

			// don't tilt the model - looks weird
			ent.angles[0] = 0.f;

			// offset the model back a bit to make the view point located in front of the head
			vec3_t angles = { 0.f, ent.angles[1], 0.f };
			vec3_t forward;
			AngleVectors(angles, forward, NULL, NULL);

			float offset = -15.f;
			VectorMA(ent.origin, offset, forward, ent.origin);
			VectorMA(ent.oldorigin, offset, forward, ent.oldorigin);
        }

		// Spotlight
		//if ( s1->entityType == ET_SPOTLIGHT ) {
		if ( s1->effects & EF_SPOTLIGHT ) {
			CL_PacketEntity_AddSpotlight( cent, &ent, s1 );
			//return;
		}

        // if set to invisible, skip
        if (!s1->modelindex) {
            goto skip;
        }

        if (effects & EF_BFG) {
            ent.flags |= RF_TRANSLUCENT;
            ent.alpha = 0.30f;
        }

        if (effects & EF_PLASMA) {
            ent.flags |= RF_TRANSLUCENT;
            ent.alpha = 0.6f;
        }

        //if (effects & EF_SPHERETRANS) {
        //    ent.flags |= RF_TRANSLUCENT;
        //    if (effects & EF_TRACKERTRAIL)
        //        ent.alpha = 0.6f;
        //    else
        //        ent.alpha = 0.3f;
        //}

        ent.flags |= base_entity_flags;

		// in rtx mode, the base entity has the renderfx for shells
		if ((effects & EF_COLOR_SHELL) && cls.ref_type == REF_TYPE_VKPT) {
			renderfx = adjust_shell_fx(renderfx);
			ent.flags |= renderfx;
		}

        // add to refresh list
        V_AddEntity(&ent);

		// add dlights for flares
		model_t* model;
		if (ent.model && !(ent.model & 0x80000000) &&
			(model = MOD_ForHandle(ent.model)))
		{
			if (model->model_class == MCLASS_FLARE)
			{
				float phase = (float)cl.time * 0.03f + (float)ent.id;
				float anim = sinf(phase);

				float offset = anim * 1.5f + 5.f;
				float brightness = anim * 0.2f + 0.8f;

				vec3_t origin;
				VectorCopy(ent.origin, origin);
				origin[2] += offset;

				V_AddSphereLight(origin, 500.f, 1.6f * brightness, 1.0f * brightness, 0.2f * brightness, 5.f);
            }
        }

        // color shells generate a separate entity for the main model
        if ((effects & EF_COLOR_SHELL) && cls.ref_type != REF_TYPE_VKPT) {
			renderfx = adjust_shell_fx(renderfx);
            ent.flags = renderfx | RF_TRANSLUCENT | base_entity_flags;
            ent.alpha = 0.30f;
            V_AddEntity(&ent);
        }

        ent.skin = 0;       // never use a custom skin on others
        ent.skinnum = 0;
        ent.flags = base_entity_flags;
        ent.alpha = 0;

        // duplicate for linked models
        if (s1->modelindex2) {
            if (s1->modelindex2 == MODELINDEX_PLAYER ) {
                // custom weapon
                ci = &cl.clientinfo[s1->skinnum & 0xff];
                i = (s1->skinnum >> 8); // 0 is default weapon model
                if (i < 0 || i > cl.numWeaponModels - 1)
                    i = 0;
                ent.model = ci->weaponmodel[i];
                if (!ent.model) {
                    if (i != 0)
                        ent.model = ci->weaponmodel[0];
                    if (!ent.model)
                        ent.model = cl.baseclientinfo.weaponmodel[0];
                }
            } else
                ent.model = cl.model_draw[s1->modelindex2];

            // PMM - check for the defender sphere shell .. make it translucent
            if (!Q_strcasecmp(cl.configstrings[CS_MODELS + (s1->modelindex2)], "models/items/shell/tris.md2")) {
                ent.alpha = 0.32f;
                ent.flags = RF_TRANSLUCENT;
            }

			if ((effects & EF_COLOR_SHELL) && cls.ref_type == REF_TYPE_VKPT) {
				ent.flags |= renderfx;
			}

            V_AddEntity(&ent);

            //PGM - make sure these get reset.
            ent.flags = base_entity_flags;
            ent.alpha = 0;
        }

        if (s1->modelindex3) {
            ent.model = cl.model_draw[s1->modelindex3];
            V_AddEntity(&ent);
        }

        if (s1->modelindex4) {
            ent.model = cl.model_draw[s1->modelindex4];
            V_AddEntity(&ent);
        }

        if (effects & EF_POWERSCREEN) {
            ent.model = cl_mod_powerscreen;
            ent.oldframe = 0;
            ent.frame = 0;
            ent.flags |= (RF_TRANSLUCENT | RF_SHELL_GREEN);
            ent.alpha = 0.30f;
            V_AddEntity(&ent);
        }

        // add automatic particle trails
        if (effects & ~EF_ROTATE) {
            if (effects & EF_ROCKET) {
                if (!(cl_disable_particles->integer & NOPART_ROCKET_TRAIL)) {
                    CL_RocketTrail(cent->lerp_origin, ent.origin, cent);
                }
                if (cl_dlight_hacks->integer & DLHACK_ROCKET_COLOR)
                    V_AddLight(ent.origin, 200, 1, 0.23f, 0);
                else
                    V_AddLight(ent.origin, 200, 0.6f, 0.4f, 0.12f);
            } else if (effects & EF_BLASTER) {
                if (effects & EF_TRACKER) {
                    CL_BlasterTrail2(cent->lerp_origin, ent.origin);
                    V_AddLight(ent.origin, 200, 0.1f, 0.4f, 0.12f);
                } else {
                    CL_BlasterTrail(cent->lerp_origin, ent.origin);
                    V_AddLight(ent.origin, 200, 0.6f, 0.4f, 0.12f);
                }
            } else if (effects & EF_HYPERBLASTER) {
                if (effects & EF_TRACKER)
                    V_AddLight(ent.origin, 200, 0.1f, 0.4f, 0.12f);
                else
                    V_AddLight(ent.origin, 200, 0.6f, 0.4f, 0.12f);
            } else if (effects & EF_GIB) {
                CL_DiminishingTrail(cent->lerp_origin, ent.origin, cent, effects);
            } else if (effects & EF_GRENADE) {
                if (!(cl_disable_particles->integer & NOPART_GRENADE_TRAIL)) {
                    CL_DiminishingTrail(cent->lerp_origin, ent.origin, cent, effects);
                }
            } else if (effects & EF_FLIES) {
                CL_FlyEffect(cent, ent.origin);
            } else if (effects & EF_BFG) {
                if (effects & EF_ANIM_ALLFAST) {
                    CL_BfgParticles(&ent);
                    i = 100;
                } else {
                    static const int bfg_lightramp[6] = {300, 400, 600, 300, 150, 75};
                    i = s1->frame;
                    clamp(i, 0, 5);
                    i = bfg_lightramp[i];
                }
				const vec3_t nvgreen = { 0.2716f, 0.5795f, 0.04615f };
				V_AddSphereLight(ent.origin, i, nvgreen[0], nvgreen[1], nvgreen[2], 20.f);
            } else if (effects & EF_TRAP) {
                ent.origin[2] += 32;
                CL_TrapParticles(cent, ent.origin);
                i = (Q_rand() % 100) + 100;
                V_AddLight(ent.origin, i, 1, 0.8f, 0.1f);
            } else if (effects & EF_FLAG1) {
                CL_FlagTrail(cent->lerp_origin, ent.origin, 242);
                V_AddLight(ent.origin, 225, 1, 0.1f, 0.1f);
            } else if (effects & EF_FLAG2) {
                CL_FlagTrail(cent->lerp_origin, ent.origin, 115);
                V_AddLight(ent.origin, 225, 0.1f, 0.1f, 1);
            } else if (effects & EF_TAGTRAIL) {
                CL_TagTrail(cent->lerp_origin, ent.origin, 220);
                V_AddLight(ent.origin, 225, 1.0f, 1.0f, 0.0f);
            } else if (effects & EF_TRACKERTRAIL) {
                if (effects & EF_TRACKER) {
                    float intensity = 50 + (500 * (sin(cl.time / 500.0f) + 1.0f));
                    V_AddLight(ent.origin, intensity, -1.0f, -1.0f, -1.0f);
                } else {
                    CL_Tracker_Shell(cent->lerp_origin);
                    V_AddLight(ent.origin, 155, -1.0f, -1.0f, -1.0f);
                }
            } else if (effects & EF_TRACKER) {
                CL_TrackerTrail(cent->lerp_origin, ent.origin, 0);
                V_AddLight(ent.origin, 200, -1, -1, -1);
            } else if (effects & EF_GREENGIB) {
                CL_DiminishingTrail(cent->lerp_origin, ent.origin, cent, effects);
            } else if (effects & EF_IONRIPPER) {
                CL_IonripperTrail(cent->lerp_origin, ent.origin);
                V_AddLight(ent.origin, 100, 1, 0.5f, 0.5f);
            } else if (effects & EF_BLUEHYPERBLASTER) {
                V_AddLight(ent.origin, 200, 0, 0, 1);
            } else if (effects & EF_PLASMA) {
                if (effects & EF_ANIM_ALLFAST) {
                    CL_BlasterTrail(cent->lerp_origin, ent.origin);
                }
                V_AddLight(ent.origin, 130, 1, 0.5f, 0.5f);
            }
        }

skip:
        VectorCopy(ent.origin, cent->lerp_origin);
    }
}

static int shell_effect_hack(void)
{
    centity_t   *ent;
    int         flags = 0;

    ent = &cl_entities[cl.frame.clientNum + 1];
    if (ent->serverframe != cl.frame.number)
        return 0;

    if (!ent->current.modelindex)
        return 0;

    if (ent->current.effects & EF_PENT)
        flags |= RF_SHELL_RED;
    if (ent->current.effects & EF_QUAD)
        flags |= RF_SHELL_BLUE;
    if (ent->current.effects & EF_DOUBLE)
        flags |= RF_SHELL_DOUBLE;
    if (ent->current.effects & EF_HALF_DAMAGE)
        flags |= RF_SHELL_HALF_DAM;

    return flags;
}

/*
==============
CL_AddViewWeapon
==============
*/
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

static void CL_SetupFirstPersonView(void)
{
    player_state_t *ps, *ops;
    vec3_t kickangles;
    float lerp;

    // add kick angles
    if (cl_kickangles->integer) {
        ps = &cl.frame.ps;
        ops = &cl.oldframe.ps;

        lerp = cl.lerpfrac;

        LerpAngles(ops->kick_angles, ps->kick_angles, lerp, kickangles);
        VectorAdd(cl.refdef.viewangles, kickangles, cl.refdef.viewangles);
    }

    // add the weapon
    CL_AddViewWeapon();

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

static void CL_FinishViewValues(void)
{
    centity_t *ent;

    if (cl_player_model->integer != CL_PLAYER_MODEL_THIRD_PERSON)
        goto first;

    ent = &cl_entities[cl.frame.clientNum + 1];
    if (ent->serverframe != cl.frame.number)
        goto first;

    if (!ent->current.modelindex)
        goto first;

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

/*
===============
CL_CalcViewValues

Sets cl.refdef view values and sound spatialization params.
Usually called from CL_AddEntities, but may be directly called from the main
loop if rendering is disabled but sound is running.
===============
*/
void CL_CalcViewValues(void)
{
    // TODO: Move elsewhere
    static constexpr int32_t STEP_TIME = 100;
    static constexpr float STEP_BASE_1_FRAMETIME = 0.01f;

    player_state_t *ps, *ops;
    vec3_t viewoffset;
    float lerp;

    if ( !cl.frame.valid ) {
        return;
    }

    // find states to interpolate between
    ps = &cl.frame.ps;
    ops = &cl.oldframe.ps;

    lerp = cl.lerpfrac;

    // calculate the origin
    if (!cls.demo.playback && cl_predict->integer && !(ps->pmove.pm_flags & PMF_NO_POSITIONAL_PREDICTION) ) {
        // use predicted values
        unsigned delta = cls.realtime - cl.predictedState.step_time;
        float backlerp = lerp - 1.0f;

        VectorMA(cl.predictedState.origin, backlerp, cl.predictedState.error, cl.refdef.vieworg);

        // smooth out stair climbing
        if (cl.predictedState.step < 15.875) {//127 ) {// * 0.125f) { // WID: float-movement
            delta <<= 1; // small steps
        }

		// WID: Prediction: Was based on old 10hz, 100ms.
        //if (delta < 100) {
        //    cl.refdef.vieworg[2] -= cl.predicted_step * (100 - delta) * 0.01f;
        //}
		// WID: Prediction: Now should be dependant on specific framerate.
		//if ( delta < BASE_FRAMETIME ) {
		//	cl.refdef.vieworg[ 2 ] -= cl.predictedState.step * ( BASE_FRAMETIME - delta ) * BASE_1_FRAMETIME;
		//}
        if ( delta < STEP_TIME ) {
            cl.refdef.vieworg[ 2 ] -= cl.predictedState.step * ( STEP_TIME - delta ) * ( 1.f / STEP_TIME );
        }
    } else {
        int i;

        // just use interpolated values
        for (i = 0; i < 3; i++) {
			cl.refdef.vieworg[ i ] = ops->pmove.origin[ i ] +
				lerp * ( ps->pmove.origin[ i ] - ops->pmove.origin[ i ] );
        }
    }

    // if not running a demo or on a locked frame, add the local angle movement
    if (cls.demo.playback) {
        LerpAngles(ops->viewangles, ps->viewangles, lerp, cl.refdef.viewangles);
    } else if (ps->pmove.pm_type < PM_DEAD) {
        // use predicted values
        VectorCopy(cl.predictedState.angles, cl.refdef.viewangles);
    } else if (ops->pmove.pm_type < PM_DEAD && !( ps->pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) ) {/*cls.serverProtocol > PROTOCOL_VERSION_Q2RTXPERIMENTAL ) {*/
        // lerp from predicted angles, since enhanced servers
        // do not send viewangles each frame
        LerpAngles(cl.predictedState.angles, ps->viewangles, lerp, cl.refdef.viewangles);
    } else {
		if ( !( ps->pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) ) {
			// just use interpolated values
			LerpAngles( ops->viewangles, ps->viewangles, lerp, cl.refdef.viewangles );
		} else {
			VectorCopy( ps->viewangles, cl.refdef.viewangles );
		}
    }

//#if USE_SMOOTH_DELTA_ANGLES
    cl.delta_angles[0] = LerpAngle(ops->pmove.delta_angles[0], ps->pmove.delta_angles[0], lerp);
    cl.delta_angles[1] = LerpAngle(ops->pmove.delta_angles[1], ps->pmove.delta_angles[1], lerp);
    cl.delta_angles[2] = LerpAngle(ops->pmove.delta_angles[2], ps->pmove.delta_angles[2], lerp);
//#endif

    //// interpolate blend colors if the last frame wasn't clear
    float blendfrac = ops->screen_blend[ 3 ] ? cl.lerpfrac : 1;
    //float damageblendfrac = ops->damage_blend[ 3 ] ? cl.lerpfrac : 1;

    Vector4Lerp( ops->screen_blend, ps->screen_blend, blendfrac, cl.refdef.screen_blend );
    //Vector4Lerp( ops->damage_blend, ps->damage_blend, damageblendfrac, cl.refdef.damage_blend );

    // don't interpolate blend color
    //Vector4Copy(ps->blend, cl.refdef.screen_blend);

    // Mix in screen_blend from cgame pmove
    // FIXME: Should also be interpolated?...
    if ( cl.predictedState.screen_blend[ 3 ] > 0 ) {
        float a2 = cl.refdef.screen_blend[ 3 ] + ( 1 - cl.refdef.screen_blend[ 3 ] ) * cl.predictedState.screen_blend[ 3 ]; // new total alpha
        float a3 = cl.refdef.screen_blend[ 3 ] / a2; // fraction of color from old

        LerpVector( cl.predictedState.screen_blend, cl.refdef.screen_blend, a3, cl.refdef.screen_blend );
        cl.refdef.screen_blend[ 3 ] = a2;
    }


    // interpolate field of view
    cl.fov_x = lerp_client_fov(ops->fov, ps->fov, lerp);
    cl.fov_y = V_CalcFov(cl.fov_x, 4, 3);

    LerpVector(ops->viewoffset, ps->viewoffset, lerp, viewoffset);

    // Smooth out view height over 100ms
    float viewheight_lerp = ( cl.time - cl.viewheight.change_time );
    viewheight_lerp = STEP_TIME - min( viewheight_lerp, STEP_TIME );
    float predicted_viewheight = cl.viewheight.current + (float)( cl.viewheight.previous - cl.viewheight.current ) * viewheight_lerp * STEP_BASE_1_FRAMETIME;
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

/*
===============
CL_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CL_AddEntities(void)
{
    CL_CalcViewValues();
    CL_FinishViewValues();
    CL_AddPacketEntities();
    CL_AddTEnts();
    CL_AddParticles();
    CL_AddDLights();
    CL_AddLightStyles();
	CL_AddTestModel();
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
    ent = &cl_entities[entnum];
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

