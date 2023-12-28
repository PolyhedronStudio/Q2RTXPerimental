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
void CL_CheckPredictionError(void)
{
    int         frame;
    float       delta[3];
    uint64_t    cmdIndex;
    float       len;

    if (cls.demo.playback) {
        return;
    }

    if (sv_paused->integer) {
        VectorClear(cl.predictedState.error);
        return;
    }

    if (!cl_predict->integer || (cl.frame.ps.pmove.pm_flags & PMF_NO_POSITIONAL_PREDICTION))
        return;

    // calculate the last usercmd_t we sent that the server has processed
    frame = cls.netchan.incoming_acknowledged & CMD_MASK;
	cmdIndex = cl.history[frame].cmdNumber;

    // compare what the server returned with what we had predicted it to be
    VectorSubtract(cl.frame.ps.pmove.origin, cl.predictedStates[ cmdIndex & CMD_MASK ].origin, delta);

    // save the prediction error for interpolation
    len = fabs(delta[0]) + abs(delta[1]) + abs(delta[2]);
    //if (len < 1 || len > 640) {
	if (len < 1.0f || len > 80.0f) {
        // > 80 world units is a teleport or something
        VectorClear(cl.predictedState.error);
        return;
    }

    SHOWMISS("prediction miss on %i: %i (%f %f %f)\n",
             cl.frame.number, len, delta[0], delta[1], delta[2]);

    // don't predict steps against server returned data
	if ( cl.predictedState.step_frame <= cmdIndex ) {
		cl.predictedState.step_frame = cmdIndex + 1;
	}

    VectorCopy(cl.frame.ps.pmove.origin, cl.predictedStates[ cmdIndex & CMD_MASK ].origin);

    // save for error interpolation
	VectorCopy( delta, cl.predictedState.error );
}

/*
====================
CL_ClipMoveToEntities

====================
*/
static void CL_ClipMoveToEntities(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, trace_t *tr)
{
    int         i;
    trace_t     trace;
    mnode_t     *headnode;
    centity_t   *ent;
    mmodel_t    *cmodel;

    for (i = 0; i < cl.numSolidEntities; i++) {
        ent = cl.solidEntities[i];

        if (ent->current.solid == PACKED_BSP) {
            // special value for bmodel
            cmodel = cl.model_clip[ent->current.modelindex];
            if (!cmodel)
                continue;
            headnode = cmodel->headnode;
        } else {
            headnode = CM_HeadnodeForBox(ent->mins, ent->maxs);
        }

        if (tr->allsolid)
            return;

        CM_TransformedBoxTrace(&trace, start, end,
                               mins, maxs, headnode,  MASK_PLAYERSOLID,
                               ent->current.origin, ent->current.angles);

        CM_ClipEntity(tr, &trace, (struct edict_s *)ent);
    }
}


/*
================
CL_PMTrace
================
*/
static trace_t q_gameabi CL_Trace(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end)
{
    trace_t    t;

    // check against world
    CM_BoxTrace(&t, start, end, mins, maxs, cl.bsp->nodes, MASK_PLAYERSOLID);
    if (t.fraction < 1.0f)
        t.ent = (struct edict_s *)cl_entities;

    // check all other solid models
    CL_ClipMoveToEntities(start, mins, maxs, end, &t);

    return t;
}

static int CL_PointContents(const vec3_t point)
{
    int         i;
    centity_t   *ent;
    mmodel_t    *cmodel;
    int         contents;

    contents = CM_PointContents(point, cl.bsp->nodes);

    for (i = 0; i < cl.numSolidEntities; i++) {
        ent = cl.solidEntities[i];

        if (ent->current.solid != PACKED_BSP) // special value for bmodel
            continue;

        cmodel = cl.model_clip[ent->current.modelindex];
        if (!cmodel)
            continue;

        contents |= CM_TransformedPointContents(
                        point, cmodel->headnode,
                        ent->current.origin,
                        ent->current.angles);
    }

    return contents;
}

/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
void CL_PredictAngles(void)
{
	VectorAdd( cl.viewangles, cl.frame.ps.pmove.delta_angles, cl.predictedState.angles );
//cl.predictedState.angles[0] = cl.viewangles[0] + (cl.frame.ps.pmove.delta_angles[0]);
//cl.predictedState.angles[1] = cl.viewangles[1] + (cl.frame.ps.pmove.delta_angles[1]);
//cl.predictedState.angles[2] = cl.viewangles[2] + (cl.frame.ps.pmove.delta_angles[2]);
}

void CL_PredictMovement(void)
{
    if (cls.state != ca_active) {
        return;
    }

    if (cls.demo.playback) {
        return;
    }

    if (sv_paused->integer) {
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
    pm.trace = CL_Trace;
    pm.pointcontents = CL_PointContents;

    pm.s = cl.frame.ps.pmove;
#if USE_SMOOTH_DELTA_ANGLES
    VectorCopy( cl.delta_angles, pm.s.delta_angles );
#endif


    // Run previously stored and acknowledged frames
    while( ++acknowledgedCommandNumber <= currentCommandNumber ) {
        pm.cmd = cl.predictedStates[ acknowledgedCommandNumber & CMD_MASK ].cmd;
        clge->PlayerMove(&pm, &cl.pmp);

        // Save for debug checking
        VectorCopy(pm.s.origin, cl.predictedStates[ acknowledgedCommandNumber & CMD_MASK ].origin );
    }

    // Run pending cmd
	uint64_t frameNumber = currentCommandNumber;
    if (cl.predictedState.cmd.msec) {
        pm.cmd = cl.predictedState.cmd;
        pm.cmd.forwardmove = cl.localmove[0];
        pm.cmd.sidemove = cl.localmove[1];
        pm.cmd.upmove = cl.localmove[2];
		clge->PlayerMove(&pm, &cl.pmp);

        // Save for debug checking
        VectorCopy(pm.s.origin, cl.predictedStates[ (currentCommandNumber + 1) & CMD_MASK ].origin);
	// Use previous frame if no command is pending.
    } else {
		frameNumber = currentCommandNumber - 1;
    }

	// Stair Stepping:
    if (pm.s.pm_type != PM_SPECTATOR && (pm.s.pm_flags & PMF_ON_GROUND)) {
        const float oldz = cl.predictedStates[ cl.predictedState.step_frame & CMD_MASK ].origin[2];
		const float step = pm.s.origin[2] - oldz;
        //if (step > 63 && step < 160) {
		if ( step > 8 && step < 20 ) {
            cl.predictedState.predicted_step = step;// * 0.125f; // WID: float-movement
            cl.predictedState.step_time = cls.realtime;
            cl.predictedState.step_frame = frameNumber + 1;    // don't double step
        }
    }

    if ( cl.predictedState.step_frame < frameNumber ) {
        cl.predictedState.step_frame = frameNumber;
    }

    // Copy results out into the current predicted state.
    VectorCopy( pm.s.origin, cl.predictedState.origin );
    VectorCopy( pm.s.velocity, cl.predictedState.velocity );
    VectorCopy( pm.viewangles, cl.predictedState.angles );
}

