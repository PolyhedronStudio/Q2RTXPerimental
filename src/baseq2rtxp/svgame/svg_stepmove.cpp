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
// m_move.c -- monster movement

#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_utils.h"

#define STEPSIZE    18

/*
=============
M_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
int c_yes, c_no;

bool M_CheckBottom(svg_base_edict_t *ent)
{
    Vector3  mins, maxs, start, stop;
    svg_trace_t trace;
    int     x, y;
    float   mid, bottom;

    VectorAdd(ent->s.origin, ent->mins, mins);
    VectorAdd(ent->s.origin, ent->maxs, maxs);

// if all of the points under the corners are solid world, don't bother
// with the tougher checks
// the corners must be within 16 of the midpoint
    start[2] = mins[2] - 1;
    if ( ent->gravityVector[ 2 ] > 0 ) {
        // PGM
        //  FIXME - this will only handle 0,0,1 and 0,0,-1 gravity vectors
            start[ 2 ] = maxs[ 2 ] + 1;
        // PGM
    }
    for (x = 0 ; x <= 1 ; x++)
        for (y = 0 ; y <= 1 ; y++) {
            start[0] = x ? maxs[0] : mins[0];
            start[1] = y ? maxs[1] : mins[1];
            if (gi.pointcontents(&start) != CONTENTS_SOLID)
                goto realcheck;
        }

    c_yes++;
    return true;        // we got out easy

realcheck:
    c_no++;
//
// check it for real...
//
    start[2] = mins[2];

// the midpoint must be within 16 of the bottom
    start[0] = stop[0] = (mins[0] + maxs[0]) * 0.5f;
    start[1] = stop[1] = (mins[1] + maxs[1]) * 0.5f;
    //stop[2] = start[2] - 2 * STEPSIZE;
        // PGM
    if ( ent->gravityVector[ 2 ] > 0 ) {
        start[ 2 ] = ent->s.origin[2] + mins[ 2 ];
        stop[ 2 ] = start[ 2 ] - STEPSIZE * 2;
    } else {
        start[ 2 ] = ent->s.origin[2] + maxs[ 2 ];
        stop[ 2 ] = start[ 2 ] + STEPSIZE * 2;
    }
    // PGM
    trace = SVG_Trace(start, vec3_origin, vec3_origin, stop, ent, CM_CONTENTMASK_MONSTERSOLID);

    if (trace.fraction == 1.0f)
        return false;
    mid = bottom = trace.endpos[2];

// the corners must be within 16 of the midpoint
    for (x = 0 ; x <= 1 ; x++)
        for (y = 0 ; y <= 1 ; y++) {
            start[0] = stop[0] = x ? maxs[0] : mins[0];
            start[1] = stop[1] = y ? maxs[1] : mins[1];

            trace = SVG_Trace(start, vec3_origin, vec3_origin, stop, ent, CM_CONTENTMASK_MONSTERSOLID);

            if (trace.fraction != 1.0f && trace.endpos[2] > bottom)
                bottom = trace.endpos[2];
            if (trace.fraction == 1.0f || mid - trace.endpos[2] > STEPSIZE)
                return false;
        }

    c_yes++;
    return true;
}


/*
=============
SV_movestep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done, false is returned, and
pr_global_struct->trace_normal is set to the normal of the blocking wall
=============
*/
//FIXME since we need to test end position contents here, can we avoid doing
//it again later in catagorize position?
static const bool SV_movestep(svg_base_edict_t *ent, Vector3 move, bool relink)
{
    float       dz;
    vec3_t      oldorg, neworg, end;
    svg_trace_t     trace;
    int         i;
    float       stepsize;
    Vector3      test;
    int         contents;

// try the move
    VectorCopy(ent->s.origin, oldorg);
    VectorAdd(ent->s.origin, move, neworg);

// flying monsters don't step up
    if (ent->flags & (FL_SWIM | FL_FLY)) {
        // try one move with vertical motion, then one without
        for (i = 0 ; i < 2 ; i++) {
            VectorAdd(ent->s.origin, move, neworg);
            if (i == 0 && ent->enemy) {
                if (!ent->goalentity)
                    ent->goalentity = ent->enemy;
                dz = ent->s.origin[2] - ent->goalentity->s.origin[2];
                if (ent->goalentity->client) {
                    if (dz > 40)
                        neworg[2] -= 8;
                    if (!((ent->flags & FL_SWIM) && (ent->liquidInfo.level < 2)))
                        if (dz < 30)
                            neworg[2] += 8;
                } else {
                    if (dz > 8)
                        neworg[2] -= 8;
                    else if (dz > 0)
                        neworg[2] -= dz;
                    else if (dz < -8)
                        neworg[2] += 8;
                    else
                        neworg[2] += dz;
                }
            }
            trace = SVG_Trace(ent->s.origin, ent->mins, ent->maxs, neworg, ent, CM_CONTENTMASK_MONSTERSOLID);

            // fly monsters don't enter water voluntarily
            if (ent->flags & FL_FLY) {
                if (!ent->liquidInfo.level) {
                    test[0] = trace.endpos[0];
                    test[1] = trace.endpos[1];
                    test[2] = trace.endpos[2] + ent->mins[2] + 1;
                    contents = gi.pointcontents(&test);
                    if (contents & CM_CONTENTMASK_LIQUID)
                        return false;
                }
            }

            // swim monsters don't exit water voluntarily
            if (ent->flags & FL_SWIM) {
                if (ent->liquidInfo.level < 2) {
                    test[0] = trace.endpos[0];
                    test[1] = trace.endpos[1];
                    test[2] = trace.endpos[2] + ent->mins[2] + 1;
                    contents = gi.pointcontents(&test);
                    if (!(contents & CM_CONTENTMASK_LIQUID))
                        return false;
                }
            }

            if (trace.fraction == 1) {
                VectorCopy(trace.endpos, ent->s.origin);
                if (relink) {
                    gi.linkentity(ent);
                    SVG_Util_TouchTriggers(ent);
                }
                return true;
            }

            if (!ent->enemy)
                break;
        }

        return false;
    }

// push down from a step height above the wished position
    #if 0
    if (!(ent->monsterinfo.aiflags & AI_NOSTEP))
        stepsize = STEPSIZE;
    else
        stepsize = 1;
    #else
    	stepsize = STEPSIZE;
    #endif

    neworg[2] += stepsize;
    VectorCopy(neworg, end);
    end[2] -= stepsize * 2;

    trace = SVG_Trace(neworg, ent->mins, ent->maxs, end, ent, CM_CONTENTMASK_MONSTERSOLID);

    if (trace.allsolid)
        return false;

    if (trace.startsolid) {
        neworg[2] -= stepsize;
        trace = SVG_Trace(neworg, ent->mins, ent->maxs, end, ent, CM_CONTENTMASK_MONSTERSOLID);
        if (trace.allsolid || trace.startsolid)
            return false;
    }


    // don't go in to water
    if (ent->liquidInfo.level == 0) {
        test[0] = trace.endpos[0];
        test[1] = trace.endpos[1];
        test[2] = trace.endpos[2] + ent->mins[2] + 1;
        contents = gi.pointcontents(&test);

        if (contents & CM_CONTENTMASK_LIQUID)
            return false;
    }

    if (trace.fraction == 1) {
        // if monster had the ground pulled out, go ahead and fall
        if (ent->flags & FL_PARTIALGROUND) {
            VectorAdd(ent->s.origin, move, ent->s.origin);
            if (relink) {
                gi.linkentity(ent);
                SVG_Util_TouchTriggers(ent);
            }
            ent->groundInfo.entity = NULL;
            return true;
        }

        return false;       // walked off an edge
    }

	// Determine whether we stepped.
	bool stepped = false;
	if ( fabs( (float)ent->s.origin[ 2 ] - (float)(trace.endpos[ 2 ] )) > 8.f ) {
		stepped = true;
	}
    // check point traces down for dangling corners
    VectorCopy(trace.endpos, ent->s.origin);

    if (!M_CheckBottom(ent)) {
        if (ent->flags & FL_PARTIALGROUND) {
            // entity had floor mostly pulled out from underneath it
            // and is trying to correct
            if (relink) {
                gi.linkentity(ent);
                SVG_Util_TouchTriggers(ent);
            }
            return true;
        }
        VectorCopy(oldorg, ent->s.origin);
        return false;
    }

    if (ent->flags & FL_PARTIALGROUND) {
        ent->flags = static_cast<entity_flags_t>( ent->flags & ~FL_PARTIALGROUND );
    }
    ent->groundInfo.entity = trace.ent;
    ent->groundInfo.entityLinkCount = trace.ent->linkcount;

// the move is ok
    if (relink) {
        gi.linkentity(ent);
        SVG_Util_TouchTriggers(ent);
    }

	if ( stepped ) {
		ent->s.renderfx |= RF_STAIR_STEP;
	}
    return true;
}


//============================================================================

/*
===============
M_ChangeYaw

===============
*/
void M_ChangeYaw(svg_base_edict_t *ent)
{
    // Get angle modded angles.
    const float current = QM_AngleMod(ent->s.angles[YAW]);
    // Get ideal desired for yaw angle.
    const float ideal = ent->ideal_yaw;

    // If we're already facing ideal yaw, escape.
    if ( current == ideal ) {
        return;
    }

    float move = ideal - current;
    const float speed = ent->yaw_speed;

    // Prevent the monster from rotating a full circle around the yaw.
    // Do so by keeping angles between -180/+180, depending on whether ideal yaw is higher or lower than current.
    move = QM_Wrap( move, -180.f, 180.f );
    //if (ideal > current) {
    //    if ( move >= 180 ) {
    //        move = move - 360;
    //    }
    //} else {
    //    if ( move <= -180 ) {
    //        move = move + 360;
    //    }
    //}
    // Clamp the yaw move speed.
    move = QM_Clamp( move, -speed, speed );
    //if (move > 0) {
    //    if ( move > speed ) {
    //        move = speed;
    //    }
    //} else {
    //    if ( move < -speed ) {
    //        move = -speed;
    //    }
    //}

    // AngleMod the final resulting angles.
    ent->s.angles[YAW] = QM_AngleMod( current + move );
}


/*
======================
SV_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.

======================
*/
bool SV_StepDirection( svg_base_edict_t *ent, float yaw, float dist ) {
    ent->ideal_yaw = yaw;
    M_ChangeYaw( ent );

    yaw = DEG2RAD(yaw);
    const Vector3 move = {
        cos( yaw ) * dist,
        sin( yaw ) * dist,
        0.f
    };

    const Vector3 oldorigin = ent->s.origin;
    if ( SV_movestep( ent, move, false ) ) {
        const float delta = ent->s.angles[ YAW ] - ent->ideal_yaw;
        if ( delta > 45 && delta < 315 ) {
            // not turned far enough, so don't take the step
            VectorCopy( oldorigin, ent->s.origin );
        }
        gi.linkentity( ent );
        SVG_Util_TouchTriggers( ent );
        SVG_Util_TouchProjectiles( ent, oldorigin );
        return true;
    }
    gi.linkentity( ent );
    SVG_Util_TouchTriggers( ent );
    return false;
}

/*
======================
SV_FixCheckBottom

======================
*/
void SV_FixCheckBottom(svg_base_edict_t *ent)
{
    ent->flags = static_cast<entity_flags_t>( ent->flags | FL_PARTIALGROUND );
}

/*
===============
M_walkmove
===============
*/
bool M_walkmove(svg_base_edict_t *ent, float yaw, float dist)
{
    vec3_t  move;

    if (!ent->groundInfo.entity && !(ent->flags & (FL_FLY | FL_SWIM)))
        return false;

    yaw = DEG2RAD(yaw);
    move[0] = cos(yaw) * dist;
    move[1] = sin(yaw) * dist;
    move[2] = 0;

    return SV_movestep(ent, move, true);
}
