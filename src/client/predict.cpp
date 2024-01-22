/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "client.h"

/*
===================
CL_CheckPredictionError
===================
*/
void CL_CheckPredictionError(void) {
	if ( cls.demo.playback ) {
		return;
	}

	if ( sv_paused->integer ) {
		VectorClear( cl.predictedState.error );
		return;
	}

	if ( !cl_predict->integer || ( cl.frame.ps.pmove.pm_flags & PMF_NO_POSITIONAL_PREDICTION ) ) {
		return;
	}

	// calculate the last usercmd_t we sent that the server has processed
	int64_t frame = cls.netchan.incoming_acknowledged & CMD_MASK;
	uint64_t cmdIndex = cl.history[ frame ].cmdNumber;

	// compare what the server returned with what we had predicted it to be
	vec3_t delta = { 0.f, 0.f, 0.f };
	VectorSubtract( cl.frame.ps.pmove.origin, cl.predictedStates[ cmdIndex & CMD_MASK ].origin, delta );

	// save the prediction error for interpolation
	const float len = fabs( delta[ 0 ] ) + abs( delta[ 1 ] ) + abs( delta[ 2 ] );
	//if (len < 1 || len > 640) {
	if ( len < 1.0f || len > 80.0f ) {
		// > 80 world units is a teleport or something
		VectorClear( cl.predictedState.error );
		return;
	}

	SHOWMISS( "prediction miss on %i: %i (%f %f %f)\n",
			 cl.frame.number, len, delta[ 0 ], delta[ 1 ], delta[ 2 ] );

	VectorCopy( cl.frame.ps.pmove.origin, cl.predictedStates[ cmdIndex & CMD_MASK ].origin );

	// save for error interpolation
	VectorCopy( delta, cl.predictedState.error );
}

/*
====================
CL_ClipMoveToEntities
====================
*/
static void CL_ClipMoveToEntities( trace_t *tr, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentmask ) {
    int         i;
    trace_t     trace;
    mnode_t *headnode;
    centity_t *ent;
    mmodel_t *cmodel;

    for ( i = 0; i < cl.numSolidEntities; i++ ) {
        ent = cl.solidEntities[ i ];

        if ( !( contentmask & CONTENTS_PLAYERCLIP ) ) { // if ( !( contentmask & CONTENTS_PLAYER ) ) {
            continue;
        }

        if ( ent->current.solid == PACKED_BSP ) {
            // special value for bmodel
            cmodel = cl.model_clip[ ent->current.modelindex ];
            if ( !cmodel ) {
                continue;
            }
            headnode = cmodel->headnode;
        } else {
            headnode = CM_HeadnodeForBox( ent->mins, ent->maxs );
        }

        if ( tr->allsolid ) {
            return;
        }

        CM_TransformedBoxTrace( &trace, start, end,
            mins, maxs, headnode, contentmask,
            ent->current.origin, ent->current.angles );

        CM_ClipEntity( tr, &trace, (struct edict_s *)ent );
    }
}

/*
================
CL_Trace
================
*/
void CL_Trace( trace_t *tr, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const struct edict_s *passEntity, int32_t contentmask ) {
    // check against world
    CM_BoxTrace( tr, start, end, mins, maxs, cl.bsp->nodes, contentmask );
    if ( tr->fraction < 1.0f ) {
        tr->ent = (struct edict_s *)cl_entities;
    }

    // check all other solid models
    CL_ClipMoveToEntities( tr, start, mins, maxs, end, contentmask );
}

/**
*   @brief  A wrapper to make it compatible with the pm->trace that desired a 'void' to be passed in.
**/
static trace_t q_gameabi CL_PM_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const void *passEntity, int32_t contentMask ) {
    trace_t t;
    CL_Trace( &t, start, mins, maxs, end, (const edict_s*)passEntity, contentMask );
    return t;
}

static trace_t q_gameabi CL_PM_Clip( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int32_t contentmask ) {
    trace_t     trace;

    if ( !mins ) {
        mins = vec3_origin;
    }
    if ( !maxs ) {
        maxs = vec3_origin;
    }

    CM_BoxTrace( &trace, start, end, mins, maxs, cl.bsp->nodes, contentmask );
    return trace;
}
static int CL_PM_PointContents(const vec3_t point) {
    int32_t contents = CM_PointContents( point, cl.bsp->nodes );

    for ( int32_t i = 0; i < cl.numSolidEntities; i++ ) {
        centity_t *ent = cl.solidEntities[ i ];

        if ( ent->current.solid != PACKED_BSP ) { // special value for bmodel
            continue;
        }

        mmodel_t *cmodel = cl.model_clip[ ent->current.modelindex ];
        if ( !cmodel ) {
            continue;
        }

        contents |= CM_TransformedPointContents( point, cmodel->headnode, ent->current.origin, ent->current.angles );
    }

    return contents;
}

/*
=================
CL_PredictAngles

Sets the predicted view angles.
=================
*/
void CL_PredictAngles(void) {
    if ( !cl_predict->integer || ( cl.frame.ps.pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) ) {
        return;
    }

	VectorAdd( cl.viewangles, cl.frame.ps.pmove.delta_angles, cl.predictedState.angles );
}

/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
void CL_PredictMovement(void) {
    static constexpr float STEP_MIN_HEIGHT = 4.f;
    static constexpr float STEP_MAX_HEIGHT = 18.0f;

    static constexpr int32_t STEP_TIME = 100;
    static constexpr int32_t MAX_STEP_CHANGE = 32;

    if ( cls.state != ca_active ) {
        return;
    }

    if ( cls.demo.playback ) {
        return;
    }

    if ( sv_paused->integer ) {
        return;
    }

    if ( !cl_predict->integer || ( cl.frame.ps.pmove.pm_flags & PMF_NO_POSITIONAL_PREDICTION ) ) {
        // just set angles
        CL_PredictAngles();
        return;
    }

	uint64_t acknowledgedCommandNumber = cl.history[ cls.netchan.incoming_acknowledged & CMD_MASK ].cmdNumber;
	const uint64_t currentCommandNumber = cl.cmdNumber;

    // if we are too far out of date, just freeze
    if ( currentCommandNumber - acknowledgedCommandNumber > CMD_BACKUP - 1 ) {
        SHOWMISS("%i: exceeded CMD_BACKUP\n", cl.frame.number);
        return;
    }

    if ( !cl.predictedState.cmd.msec && currentCommandNumber == acknowledgedCommandNumber ) {
        SHOWMISS("%i: not moved\n", cl.frame.number);
        return;
    }

    // Copy over the current client state data into pmove.
	pmove_t pm = {};
    pm.trace = CL_PM_Trace;
    pm.pointcontents = CL_PM_PointContents;
    pm.clip = CL_PM_Clip;
    pm.s = cl.frame.ps.pmove;
    //#if USE_SMOOTH_DELTA_ANGLES
    VectorCopy( cl.delta_angles, pm.s.delta_angles );
    VectorCopy( cl.frame.ps.viewoffset, pm.viewoffset );
    //#endif

    // Run previously stored and acknowledged frames
    while ( ++acknowledgedCommandNumber <= currentCommandNumber ) {
        pm.cmd = cl.predictedStates[ acknowledgedCommandNumber & CMD_MASK ].cmd;
        clge->PlayerMove( &pm, &cl.pmp );

        // Save for debug checking
        VectorCopy( pm.s.origin, cl.predictedStates[ acknowledgedCommandNumber & CMD_MASK ].origin );
    }

    // Run pending cmd
    uint64_t frameNumber = currentCommandNumber; //! Default to current frame, expected behavior for if we got msec in predicedState.cmd
    if ( cl.predictedState.cmd.msec ) {
        pm.cmd = cl.predictedState.cmd;
        pm.cmd.forwardmove = cl.localmove[ 0 ];
        pm.cmd.sidemove = cl.localmove[ 1 ];
        pm.cmd.upmove = cl.localmove[ 2 ];
		clge->PlayerMove( &pm, &cl.pmp );

        // Save for debug checking
        VectorCopy( pm.s.origin, cl.predictedStates[ (currentCommandNumber + 1) & CMD_MASK ].origin );
	// Use previous frame if no command is pending.
    } else {
		frameNumber = currentCommandNumber - 1;
    }

	// Stair Stepping:
    const float oldZ = cl.predictedStates[ frameNumber & CMD_MASK ].origin[ 2 ];
    const float step = pm.s.origin[ 2 ] - oldZ;
    const float fabsStep = fabsf( step );
    
    // Consider a Z change being "stepping" if...
    const bool step_detected = ( fabsStep > STEP_MIN_HEIGHT && fabsStep < STEP_MAX_HEIGHT ) // Absolute change is in this limited range
                && ( ( cl.frame.ps.pmove.pm_flags & PMF_ON_GROUND ) || pm.step_clip ) // And we started off on the ground
                && ( ( pm.s.pm_flags & PMF_ON_GROUND ) && pm.s.pm_type <= PM_GRAPPLE ) // And are still predicted to be on the ground
                && ( memcmp( &cl.lastGround.plane, &pm.groundplane, sizeof( cplane_t ) ) != 0 // Plane memory isn't identical, or
                || cl.lastGround.entity != (centity_t*)pm.groundentity ); // we stand on another plane or entity

    // Code below adapted from Q3A.
    if ( step_detected ) {
        // check for stepping up before a previous step is completed
        const float delta = cls.realtime - cl.predictedState.step_time;
        float old_step = 0.f;
        if ( delta < STEP_TIME ) {
            old_step = cl.predictedState.step * ( STEP_TIME - delta ) / STEP_TIME;
        } else {
            old_step = 0;
        }

        // add this amount
        cl.predictedState.step = constclamp( old_step + step, -MAX_STEP_CHANGE, MAX_STEP_CHANGE );
        cl.predictedState.step_time = cls.realtime;
    }

    // Copy results out into the current predicted state.
    VectorCopy( pm.s.origin, cl.predictedState.origin );
    VectorCopy( pm.s.velocity, cl.predictedState.velocity );
    VectorCopy( pm.viewangles, cl.predictedState.angles );
    // To be merged with server screen blend.
    Vector4Copy( pm.screen_blend, cl.predictedState.screen_blend );
    // To be merged with server rdflags.
    cl.predictedState.rdflags = pm.rdflags;

    // Record viewheight changes.
    if ( cl.viewheight.current != pm.s.viewheight ) {
        // Backup the old 'current' viewheight.
        cl.viewheight.previous = cl.viewheight.current;
        // Apply new viewheight.
        cl.viewheight.current = pm.s.viewheight;
        // Register client's time of viewheight change.
        cl.viewheight.change_time = cl.time;
    }

    // Store resulting ground data.
    cl.lastGround.plane = pm.groundplane;
    cl.lastGround.entity = (centity_t *)pm.groundentity;
}

