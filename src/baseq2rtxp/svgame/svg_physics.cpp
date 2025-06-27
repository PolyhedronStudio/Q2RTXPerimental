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
// g_phys.c

#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

/*

pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BOUNDS_BOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/

// [Paril-KEX] fetch the clipmask for this entity; certain modifiers
// affect the clipping behavior of objects.
const cm_contents_t SVG_GetClipMask( svg_base_edict_t *ent ) {
    // Get current clip mask.
    cm_contents_t mask = ent->clipmask;

    // If none, setup a default mask based on the svflags.
    if ( !mask ) {
        if ( ent->svflags & SVF_MONSTER ) {
            mask = ( CM_CONTENTMASK_MONSTERSOLID );
        } else if ( ent->svflags & SVF_PROJECTILE ) {
            mask = ( CM_CONTENTMASK_PROJECTILE );
        } else {
            mask = static_cast<cm_contents_t>( CM_CONTENTMASK_SHOT & ~CONTENTS_DEADMONSTER );
        }
    }

    // Non-Solid objects (items, etc) shouldn't try to clip
    // against players/monsters.
    if ( ent->solid == SOLID_NOT || ent->solid == SOLID_TRIGGER ) {
        mask = static_cast<cm_contents_t>( mask & ~( CONTENTS_MONSTER | CONTENTS_PLAYER ) );
    }

    // Monsters/Players that are also dead shouldn't clip
    // against players/monsters.
    if ( ( ent->svflags & ( SVF_MONSTER | SVF_PLAYER ) ) && ( ent->svflags & SVF_DEADMONSTER ) ) {
        mask = static_cast<cm_contents_t>( mask & ~( CONTENTS_MONSTER | CONTENTS_PLAYER ) );
    }

    return mask;
}

/*
============
SV_TestEntityPosition

============
*/
svg_base_edict_t *SV_TestEntityPosition(svg_base_edict_t *ent)
{
    svg_trace_t trace;
    //cm_contents_t mask;

    //if (ent->clipmask)
    //    mask = ent->clipmask;
    //else
    //    mask = CM_CONTENTMASK_SOLID;
    trace = SVG_Trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, SVG_GetClipMask( ent ) );

    if ( trace.startsolid ) {
        return g_edict_pool.EdictForNumber( 0 );
    }

    return nullptr;
}

/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity(svg_base_edict_t *ent)
{
//    int     i;

//
// bound velocity
//
    ent->velocity = QM_Vector3Clamp( ent->velocity,
        {
            -sv_maxvelocity->value,
            -sv_maxvelocity->value,
            -sv_maxvelocity->value,
        },
        {
            sv_maxvelocity->value,
            sv_maxvelocity->value,
            sv_maxvelocity->value,
        }
    );
    //for (i = 0; i < 3; i++)
    //    std::clamp(ent->velocity[i], -sv_maxvelocity->value, sv_maxvelocity->value);
}

/*
=============
SV_RunThink

Runs thinking code for this frame if necessary
=============
*/
bool SV_RunThink(svg_base_edict_t *ent)
{
    QMTime     thinktime;

    thinktime = ent->nextthink;
    if ( thinktime <= 0_ms ) {
        return false;
    }
    if ( thinktime > level.time ) {
        return true;
    }

    ent->nextthink = 0_ms;

    if ( !ent->HasThinkCallback() ) {
        // WID: Useful to output exact information about what entity we are dealing with here, that'll help us fix the problem :-).
        gi.error( "[ entityNumber(%d), inUse(%s), classname(%s), targetname(%s), luaName(%s), (nullptr) ent->think ]\n",
            ent->s.number, ( ent->inuse != false ? "true" : "false" ), (const char*)ent->classname, (const char *)ent->targetname, (const char *)ent->luaProperties.luaName);
        // Failed.
        return false;
    }

    ent->DispatchThinkCallback();

    return true;
}

/*
==================
SVG_Impact

Two entities have touched, so run their touch functions
==================
*/
void SVG_Impact(svg_base_edict_t *e1, svg_trace_t *trace)
{
    svg_base_edict_t     *e2;
//  cm_plane_t    backplane;

    e2 = trace->ent;

    if ( e1->HasTouchCallback() && e1->solid != SOLID_NOT ) {
        e1->DispatchTouchCallback( e2, &trace->plane, trace->surface );
    }

    if ( e2->HasTouchCallback() && e2->solid != SOLID_NOT ) {
        e2->DispatchTouchCallback( e1, NULL, NULL );
    }
}

/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
static constexpr double CLIPVELOCITY_STOP_EPSILON = 0.1;
//! Return bit set in case of having clipped to a floor plane.
static constexpr int32_t CLIPVELOCITY_CLIPPED_FLOOR = BIT( 0 );
//! Return bit set in case of having clipped to a step plane. (Straight up wall.)
static constexpr int32_t CLIPVELOCITY_CLIPPED_STEP  = BIT( 1 );
//! Return bit set in case of having resulting in a dead stop. ( Corner or such. )
static constexpr int32_t CLIPVELOCITY_CLIPPED_DEAD_STOP = BIT( 2 );
//! Return bit set in case of being trapped inside a solid.
static constexpr int32_t CLIPVELOCITY_CLIPPED_STUCK_SOLID = BIT( 3 );
//! Return bit set in case of num of clipped planes overflowing. ( Should never happen. )
static constexpr int32_t CLIPVELOCITY_CLIPPED_OVERFLOW = BIT( 4 );
//! Return bit set in case of stopping because if the crease not matching 2 planes.
static constexpr int32_t CLIPVELOCITY_CLIPPED_CREASE_STOP = BIT( 5 );


const int32_t SVG_Physics_ClipVelocity( Vector3 &in, vec3_t normal, Vector3 &out, const double overbounce ) {
    // Determine if clip got 'blocked'.
    int32_t blocked = 0;
    // Floor:
    if ( normal[ 2 ] > 0 ) {
        blocked |= CLIPVELOCITY_CLIPPED_FLOOR;
    }
    // Step/Wall:
    if ( !normal[ 2 ] ) {
        blocked |= CLIPVELOCITY_CLIPPED_STEP;
    }

    // Backoff factor.
    const double backOff = QM_Vector3DotProduct(in, normal) * overbounce;

    // Calculate and apply change.
    for ( int32_t i = 0; i < 3; i++ ) {
        // Calculate change for axis.
        const float change = normal[ i ] * backOff;
        // Apply velocity change.
        out[ i ] = in[ i ] - change;
        // Halt if we're past epsilon.
        if ( out[ i ] > -CLIPVELOCITY_STOP_EPSILON && out[ i ] < CLIPVELOCITY_STOP_EPSILON ) {
            out[ i ] = 0;
        }
    }

    return blocked;
}

/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
============
*/
#define MAX_CLIP_PLANES 5
int SV_FlyMove(svg_base_edict_t *ent, float time, const cm_contents_t mask)
{
    svg_base_edict_t     *hit;
    int         bumpcount, numbumps;
    vec3_t      dir;
    float       d;
    int         numplanes;
    Vector3     planes[MAX_CLIP_PLANES];
    Vector3      primal_velocity, original_velocity, new_velocity;
    int         i, j;
    svg_trace_t     trace;
    vec3_t      end;
    float       time_left;
    int         blocked;

    numbumps = 4;

    blocked = 0;
    VectorCopy(ent->velocity, original_velocity);
    VectorCopy(ent->velocity, primal_velocity);
    numplanes = 0;

    time_left = time;

    ent->groundInfo.entity = nullptr;
    for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
        for (i = 0; i < 3; i++)
            end[i] = ent->s.origin[i] + time_left * ent->velocity[i];

        trace = SVG_Trace(ent->s.origin, ent->mins, ent->maxs, end, ent, mask);

        if (trace.allsolid) {
            // entity is trapped in another solid
            VectorClear(ent->velocity);
            return CLIPVELOCITY_CLIPPED_STUCK_SOLID;
        }

        if (trace.fraction > 0) {
            // actually covered some distance
            VectorCopy(trace.endpos, ent->s.origin);
            VectorCopy(ent->velocity, original_velocity);
            numplanes = 0;
        }

        if (trace.fraction == 1)
            break;     // moved the entire distance

        hit = trace.ent;

        if (trace.plane.normal[2] > 0.7f) {
            blocked |= CLIPVELOCITY_CLIPPED_FLOOR;       // floor
            if (hit->solid == SOLID_BSP) {
                ent->groundInfo.entity = hit;
                ent->groundInfo.entityLinkCount = hit->linkcount;
            }
        }
        if (!trace.plane.normal[2]) {
            blocked |= CLIPVELOCITY_CLIPPED_STEP;       // step
        }

//
// run the impact function
//
        SVG_Impact(ent, &trace);
        if (!ent->inuse)
            break;      // removed by the impact function

        time_left -= time_left * trace.fraction;

        // cliped to another plane
        if (numplanes >= MAX_CLIP_PLANES) {
            // this shouldn't really happen
            VectorClear(ent->velocity);
            return CLIPVELOCITY_CLIPPED_OVERFLOW;
        }

        VectorCopy(trace.plane.normal, planes[numplanes]);
        numplanes++;

//
// modify original_velocity so it parallels all of the clip planes
//
        for (i = 0; i < numplanes; i++) {
            blocked |= SVG_Physics_ClipVelocity( original_velocity, &planes[i].x, new_velocity, 1 );

            for (j = 0; j < numplanes; j++)
                if ((j != i) && !VectorCompare(planes[i], planes[j])) {
                    if (DotProduct(new_velocity, planes[j]) < 0)
                        break;  // not ok
                }
            if (j == numplanes)
                break;
        }

        if (i != numplanes) {
            // go along this plane
            VectorCopy(new_velocity, ent->velocity);
        } else {
            // go along the crease
            if (numplanes != 2) {
//              gi.dprintf ("clip velocity, numplanes == %i\n",numplanes);
                VectorClear(ent->velocity);
                return CLIPVELOCITY_CLIPPED_CREASE_STOP;
            }
            CrossProduct(planes[0], planes[1], dir);
            d = DotProduct(dir, ent->velocity);
            VectorScale(dir, d, ent->velocity);
        }

//
// if original velocity is against the original velocity, stop dead
// to avoid tiny occilations in sloping corners
//
        if (DotProduct(ent->velocity, primal_velocity) <= 0) {
            VectorClear(ent->velocity);
            return blocked | CLIPVELOCITY_CLIPPED_DEAD_STOP;
        }
    }

    return blocked;
}

/*
============
SVG_AddGravity

============
*/
void SVG_AddGravity(svg_base_edict_t *ent)
{
    ent->velocity += ent->gravityVector * ( ent->gravity * level.gravity * gi.frame_time_s );
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
svg_trace_t SV_PushEntity(svg_base_edict_t *ent, vec3_t push)
{
    svg_trace_t trace;
    vec3_t  start;
    vec3_t  end;

    VectorCopy(ent->s.origin, start);
    VectorAdd(start, push, end);

retry:
    //if (ent->clipmask)
    //    mask = ent->clipmask;
    //else
    //    mask = CM_CONTENTMASK_SOLID;

    trace = SVG_Trace(start, ent->mins, ent->maxs, end, ent, SVG_GetClipMask( ent ) );

    VectorCopy(trace.endpos, ent->s.origin);
    gi.linkentity(ent);

    if (trace.fraction != 1.0f) {
        SVG_Impact(ent, &trace);

        // if the pushed entity went away and the pusher is still there
        if (!trace.ent->inuse && ent->inuse) {
            // move the pusher back and try again
            VectorCopy(start, ent->s.origin);
            gi.linkentity(ent);
            goto retry;
        }
    }

    // PGM
    // FIXME - is this needed?
    ent->gravity = 1.0;
    // PGM

    if (ent->inuse)
        SVG_Util_TouchTriggers(ent);

    return trace;
}

typedef struct {
    svg_base_edict_t *ent;
    vec3_t  origin;
    vec3_t  angles;
    //#if USE_SMOOTH_DELTA_ANGLES
	float deltayaw;
    //#endif
} pushed_t;
pushed_t    pushed[MAX_EDICTS], *pushed_p;

svg_base_edict_t *obstacle;

const float SnapToEights( const float x ) {
    // WID: Float-movement.
    //x *= 8.0f;
    //if (x > 0.0f)
    //    x += 0.5f;
    //else
    //    x -= 0.5f;
    //return 0.125f * (int)x;
    return x;
}

/*
============
SV_Push

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
============
*/
bool SV_Push(svg_base_edict_t *pusher, vec3_t move, vec3_t amove)
{
    int         i, e;
    svg_base_edict_t     *check, *block;
    vec3_t      mins, maxs;
    pushed_t    *p;
    vec3_t      org, org2, move2, forward, right, up;

    if ( !pusher ) {
        return false;
    }

    // clamp the move to 1/8 units, so the position will
    // be accurate for client side prediction
    for (i = 0; i < 3; i++)
        move[i] = SnapToEights(move[i]);

    // find the bounding box
    for (i = 0; i < 3; i++) {
        mins[i] = pusher->absmin[i] + move[i];
        maxs[i] = pusher->absmax[i] + move[i];
    }

// we need this for pushing things later
    VectorNegate(amove, org);
    AngleVectors(org, forward, right, up);

// save the pusher's original position
    pushed_p->ent = pusher;
    VectorCopy(pusher->s.origin, pushed_p->origin);
    VectorCopy(pusher->s.angles, pushed_p->angles);
    //#if USE_SMOOTH_DELTA_ANGLES
    if (pusher->client)
        pushed_p->deltayaw = pusher->client->ps.pmove.delta_angles[YAW];
    //#endif
    pushed_p++;

// move the pusher to it's final position
    VectorAdd(pusher->s.origin, move, pusher->s.origin);
    VectorAdd(pusher->s.angles, amove, pusher->s.angles);
    gi.linkentity(pusher);

// see if any solid entities are inside the final position
    check = g_edict_pool.EdictForNumber( 1 );
    for (e = 1; e < globals.edictPool->num_edicts; e++, check = g_edict_pool.EdictForNumber( e ) ) {
        if (!check->inuse)
            continue;
        if (check->movetype == MOVETYPE_PUSH
            || check->movetype == MOVETYPE_STOP
            || check->movetype == MOVETYPE_NONE
            || check->movetype == MOVETYPE_NOCLIP)
            continue;

        if (!check->area.prev)
            continue;       // not linked in anywhere

        // if the entity is standing on the pusher, it will definitely be moved
        if (check->groundInfo.entity != pusher) {
            // see if the ent needs to be tested
            if (check->absmin[0] >= maxs[0]
                || check->absmin[1] >= maxs[1]
                || check->absmin[2] >= maxs[2]
                || check->absmax[0] <= mins[0]
                || check->absmax[1] <= mins[1]
                || check->absmax[2] <= mins[2])
                continue;

            // see if the ent's bbox is inside the pusher's final position
            if (!SV_TestEntityPosition(check))
                continue;
        }

        if ((pusher->movetype == MOVETYPE_PUSH) || (check->groundInfo.entity == pusher)) {
            // move this entity
            pushed_p->ent = check;
            VectorCopy(check->s.origin, pushed_p->origin);
            VectorCopy(check->s.angles, pushed_p->angles);
            //#if USE_SMOOTH_DELTA_ANGLES
            if (check->client)
                pushed_p->deltayaw = check->client->ps.pmove.delta_angles[YAW];
            //#endif
            pushed_p++;

            // try moving the contacted entity
            VectorAdd(check->s.origin, move, check->s.origin);
            //#if USE_SMOOTH_DELTA_ANGLES
            if (check->client) {
                // FIXME: doesn't rotate monsters?
                // FIXME: skuller: needs client side interpolation
                //check->client->ps.pmove.delta_angles[YAW] += /*ANGLE2SHORT*/(amove[YAW]);
                check->client->ps.pmove.delta_angles[ YAW ] = QM_AngleMod( check->client->ps.pmove.delta_angles[ YAW ] + amove[ YAW ] );
            }
            //#endif

            // figure movement due to the pusher's amove
            VectorSubtract(check->s.origin, pusher->s.origin, org);
            org2[0] = DotProduct(org, forward);
            org2[1] = -DotProduct(org, right);
            org2[2] = DotProduct(org, up);
            VectorSubtract(org2, org, move2);
            VectorAdd(check->s.origin, move2, check->s.origin);

            // may have pushed them off an edge
            if (check->groundInfo.entity != pusher)
                check->groundInfo.entity = NULL;

            block = SV_TestEntityPosition(check);
            if (!block) {
                // pushed ok
                gi.linkentity(check);
                // impact?
                continue;
            }

            // if it is ok to leave in the old position, do it
            // this is only relevent for riding entities, not pushed
            // FIXME: this doesn't acount for rotation
            VectorSubtract(check->s.origin, move, check->s.origin);
            block = SV_TestEntityPosition(check);
            if (!block) {
                pushed_p--;
                continue;
            }
        }

        // save off the obstacle so we can call the block function
        obstacle = check;

        // move back any entities we already moved
        // go backwards, so if the same entity was pushed
        // twice, it goes back to the original position
        for (i = (pushed_p - pushed) - 1; i >= 0; i--) {
            p = &pushed[i];
            VectorCopy(p->origin, p->ent->s.origin);
            VectorCopy(p->angles, p->ent->s.angles);
            //#if USE_SMOOTH_DELTA_ANGLES
            if (p->ent->client) {
                p->ent->client->ps.pmove.delta_angles[YAW] = p->deltayaw;
            }
            //#endif
            gi.linkentity(p->ent);
        }
        return false;
    }

//FIXME: is there a better way to handle this?
    // see if anything we moved has touched a trigger
    for (i = (pushed_p - pushed) - 1; i >= 0; i--)
        SVG_Util_TouchTriggers(pushed[i].ent);

    return true;
}

/*
================
SV_Physics_Pusher

Bmodel objects don't interact with each other, but
push all box objects
================
*/
void SV_Physics_Pusher(svg_base_edict_t *ent)
{
    vec3_t      move, amove;
    svg_base_edict_t     *part, *mv;

    // if not a team captain, so movement will be handled elsewhere
    if (ent->flags & FL_TEAMSLAVE)
        return;

    // make sure all team slaves can move before commiting
    // any moves or calling any think functions
    // if the move is blocked, all moved objects will be backed out
#if 0
retry:
#endif
    pushed_p = pushed;
    for (part = ent; part; part = part->teamchain) {
        if (!VectorEmpty(part->velocity) || !VectorEmpty(part->avelocity)) {
            // object is moving
            VectorScale(part->velocity, gi.frame_time_s, move);
            VectorScale(part->avelocity, gi.frame_time_s, amove);

            if (!SV_Push(part, move, amove))
                break;  // move was blocked
        }
    }
    if (pushed_p > &pushed[MAX_EDICTS])
        gi.error("pushed_p > &pushed[MAX_EDICTS], memory corrupted");

    if (part) {
        // the move failed, bump all nextthink times and back out moves
        for (mv = ent ; mv ; mv = mv->teamchain) {
			if ( mv->nextthink > 0_ms )
				mv->nextthink += FRAME_TIME_S;
        }

        // if the pusher has a "blocked" function, call it
        // otherwise, just stay in place until the obstacle is gone
        if ( part->HasBlockedCallback() ) {
            part->DispatchBlockedCallback( obstacle );
        }
#if 0
        // if the pushed entity went away and the pusher is still there
        if (!obstacle->inuse && part->inuse)
            goto retry;
#endif
    } else {
        // the move succeeded, so call all think functions
        for (part = ent; part; part = part->teamchain) {
            SV_RunThink(part);
        }
    }
}

//==================================================================

/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
void SV_Physics_None(svg_base_edict_t *ent)
{
// regular thinking
    SV_RunThink(ent);
}

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip(svg_base_edict_t *ent)
{
// regular thinking
    if (!SV_RunThink(ent))
        return;
    if (!ent->inuse)
        return;

    VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);
    VectorMA(ent->s.origin, FRAMETIME, ent->velocity, ent->s.origin);

    gi.linkentity(ent);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss(svg_base_edict_t *ent)
{
    svg_trace_t     trace;
    vec3_t      move;
    float       backoff;
    svg_base_edict_t     *slave;
    int         wasinwater;
    int         isinwater;
    vec3_t      old_origin;

// regular thinking
    SV_RunThink(ent);
    if ( !ent->inuse ) {
        return;
    }

    // if not a team captain, so movement will be handled elsewhere
    if ( ent->flags & FL_TEAMSLAVE ) {
        return;
    }

    if ( ent->velocity[ 2 ] > 0 ) {
        ent->groundInfo.entity = nullptr;
    }

// check for the groundentity going away
    if ( ent->groundInfo.entity ) {
        if ( !ent->groundInfo.entity->inuse ) {
            ent->groundInfo.entity = nullptr;
        }
    }

// if onground, return without moving
    if ( ent->groundInfo.entity && ent->gravity > 0.0f ) {  // PGM - gravity hack
        if ( ent->svflags & SVF_MONSTER ) {
            M_CatagorizePosition( ent, ent->s.origin, ent->liquidInfo.level, ent->liquidInfo.type );
            M_WorldEffects( ent );
        }

        return;
    }

    VectorCopy(ent->s.origin, old_origin);

    SV_CheckVelocity(ent);

// add gravity
    if (ent->movetype != MOVETYPE_FLY && ent->movetype != MOVETYPE_FLYMISSILE)
        SVG_AddGravity(ent);

// move angles
    VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);

// move origin
    VectorScale(ent->velocity, FRAMETIME, move);
    trace = SV_PushEntity(ent, move);
    if (!ent->inuse)
        return;

    if (trace.fraction < 1) {
        if (ent->movetype == MOVETYPE_BOUNCE)
            backoff = 1.5f;
        else
            backoff = 1;

        SVG_Physics_ClipVelocity(ent->velocity, trace.plane.normal, ent->velocity, backoff);

        // stop if on ground
        if (trace.plane.normal[2] > 0.7f) {
            if (ent->velocity[2] < 60 || ent->movetype != MOVETYPE_BOUNCE) {
                ent->groundInfo.entity = trace.ent;
                ent->groundInfo.entityLinkCount = trace.ent->linkcount;
                VectorClear(ent->velocity);
                VectorClear(ent->avelocity);
            }
        }

//      if (ent->touch)
//          ent->touch (ent, trace.ent, &trace.plane, trace.surface);
    }

// check for water transition
    wasinwater = (ent->liquidInfo.type & CM_CONTENTMASK_WATER);
    ent->liquidInfo.type = gi.pointcontents(ent->s.origin);
    isinwater = ent->liquidInfo.type & CM_CONTENTMASK_WATER;

    if (isinwater)
        ent->liquidInfo.level = cm_liquid_level_t::LIQUID_FEET;
    else
        ent->liquidInfo.level = cm_liquid_level_t::LIQUID_NONE;

    const qhandle_t water_sfx_index = gi.soundindex( SG_RandomResourcePath( "world/water_land_splash", ".wav", 0, 8 ).c_str() );
    if ( !wasinwater && isinwater ) {
        gi.positioned_sound( old_origin, g_edict_pool.EdictForNumber( 0 ), CHAN_AUTO, water_sfx_index, 1, 1, 0);
    } else if ( wasinwater && !isinwater ) {
        gi.positioned_sound( ent->s.origin, g_edict_pool.EdictForNumber( 0 ), CHAN_AUTO, water_sfx_index, 1, 1, 0);
    }

// move teamslaves
    for (slave = ent->teamchain; slave; slave = slave->teamchain) {
        VectorCopy(ent->s.origin, slave->s.origin);
        gi.linkentity(slave);
    }
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
FIXME: is this true?
=============
*/

//FIXME: hacked in for E3 demo
#define sv_stopspeed        100.f
#define sv_friction         6.f
#define sv_waterfriction    1.f

void SV_AddRotationalFriction(svg_base_edict_t *ent)
{
    int     n;
    float   adjustment;

    VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);
    adjustment = FRAMETIME * sv_stopspeed * sv_friction;
    for (n = 0; n < 3; n++) {
        if (ent->avelocity[n] > 0) {
            ent->avelocity[n] -= adjustment;
            if (ent->avelocity[n] < 0)
                ent->avelocity[n] = 0;
        } else {
            ent->avelocity[n] += adjustment;
            if (ent->avelocity[n] > 0)
                ent->avelocity[n] = 0;
        }
    }
}

void SV_Physics_Step(svg_base_edict_t *ent)
{
    bool	   wasonground;
    bool	   hitsound = false;
    float *vel;
    float	   speed, newspeed, control;
    float	   friction;
    svg_base_edict_t *groundentity;
    cm_contents_t mask = SVG_GetClipMask( ent );

    // airborne monsters should always check for ground
    if ( !ent->groundInfo.entity ) {
        M_CheckGround( ent, mask );
    }

    groundentity = ent->groundInfo.entity;

    SV_CheckVelocity( ent );

    if ( groundentity ) {
        wasonground = true;
    } else {
        wasonground = false;
    }

    if ( ent->avelocity[ 0 ] || ent->avelocity[ 1 ] || ent->avelocity[ 2 ] ) {
        SV_AddRotationalFriction( ent );
    }

    // FIXME: figure out how or why this is happening
    //if ( std::isnan( ent->velocity[ 0 ] ) || std::isnan( ent->velocity[ 1 ] ) || std::isnan( ent->velocity[ 2 ] ) )
    //if ( std::isnan( ent->velocity[ 0 ] ) || std::isnan( ent->velocity[ 1 ] ) || std::isnan( ent->velocity[ 2 ] ) )
    //    ent->velocity = {};

    // add gravity except:
    //   flying monsters
    //   swimming monsters who are in the water
    if ( !wasonground )
        if ( !( ent->flags & FL_FLY ) )
            if ( !( ( ent->flags & FL_SWIM ) && ( ent->liquidInfo.level > LIQUID_WAIST ) ) ) {
                //if ( ent->velocity[ 2 ] < level.gravity * -0.1f )
                if ( ent->velocity[ 2 ] < sv_gravity->value * -0.1f ) {
                    hitsound = true;
                }
                if ( ent->liquidInfo.level != LIQUID_UNDER ) {
                    SVG_AddGravity( ent );
                }
            }

    // friction for flying monsters that have been given vertical velocity
    if ( ( ent->flags & FL_FLY ) && ( ent->velocity[ 2 ] != 0 ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
        speed = fabsf( ent->velocity[ 2 ] );
        //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
        control = speed < sv_stopspeed ? sv_stopspeed : speed;
        friction = sv_friction / 3;
        newspeed = speed - ( gi.frame_time_s * control * friction );
        if ( newspeed < 0 ) {
            newspeed = 0;
        }
        newspeed /= speed;
        ent->velocity[ 2 ] *= newspeed;
    }

    // friction for swimming monsters that have been given vertical velocity
    if ( ( ent->flags & FL_SWIM ) && ( ent->velocity[ 2 ] != 0 ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
        speed = fabsf( ent->velocity[ 2 ] );
        //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
        control = speed < sv_stopspeed ? sv_stopspeed : speed;
        newspeed = speed - ( gi.frame_time_s * control * sv_waterfriction * (float)ent->liquidInfo.level );
        if ( newspeed < 0 )
            newspeed = 0;
        newspeed /= speed;
        ent->velocity[ 2 ] *= newspeed;
    }

    if ( ent->velocity[ 2 ] || ent->velocity[ 1 ] || ent->velocity[ 0 ] ) {
        // apply friction
        if ( ( wasonground || ( ent->flags & ( FL_SWIM | FL_FLY ) ) ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
            vel = &ent->velocity[0];
            speed = sqrtf( vel[ 0 ] * vel[ 0 ] + vel[ 1 ] * vel[ 1 ] );
            if ( speed ) {
                friction = sv_friction;

                // Paril: lower friction for dead monsters
                if ( ent->lifeStatus )
                    friction *= 0.5f;

                //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
                control = speed < sv_stopspeed ? sv_stopspeed : speed;
                newspeed = speed - gi.frame_time_s * control * friction;

                if ( newspeed < 0 )
                    newspeed = 0;
                newspeed /= speed;

                vel[ 0 ] *= newspeed;
                vel[ 1 ] *= newspeed;
            }
        }

        Vector3 old_origin = ent->s.origin;

        SV_FlyMove( ent, gi.frame_time_s, mask );

        SVG_Util_TouchProjectiles( ent, old_origin );

        M_CheckGround( ent, mask );

        gi.linkentity( ent );

        // ========
        // PGM - reset this every time they move.
        //       SVG_touchtriggers will set it back if appropriate
        ent->gravity = 1.0;
        // ========

        // [Paril-KEX] this is something N64 does to avoid doors opening
        // at the start of a level, which triggers some monsters to spawn.
        if ( /*!level.is_n64 || */level.time > FRAME_TIME_S ) {
            SVG_Util_TouchTriggers( ent );
        }

        if ( !ent->inuse ) {
            return;
        }

        if ( ent->groundInfo.entity ) {
            if ( !wasonground ) {
                if ( hitsound ) {
                    ent->s.event = EV_FOOTSTEP;
                }
            }
        }
    }

    if ( !ent->inuse ) { // PGM g_touchtrigger free problem
        return;
    }

    if ( ent->svflags & SVF_MONSTER ) {
        M_CatagorizePosition( ent, Vector3( ent->s.origin ), ent->liquidInfo.level, ent->liquidInfo.type );
        M_WorldEffects( ent );

        // [Paril-KEX] last minute hack to fix Stalker upside down gravity
        //if ( wasonground != !!ent->groundentity ) {
        //    if ( ent->monsterinfo.physics_change )
        //        ent->monsterinfo.physics_change( ent );
        //}
    }

    // regular thinking
    SV_RunThink( ent );

//    bool        wasonground = false;
//    bool        hitsound = false;
//    float       *vel = nullptr;
//    float       speed = 0.f, newspeed = 0.f, control = 0.f;
//    float       friction = 0.f;
//    svg_base_edict_t     *groundentity = nullptr;
//    cm_contents_t  mask = SVG_GetClipMask( ent );
//
//    // airborn monsters should always check for ground
//    if ( !ent->groundentity ) {
//        M_CheckGround( ent, mask );
//    }
//
//    groundentity = ent->groundentity;
//
//    SV_CheckVelocity(ent);
//
//    if (groundentity)
//        wasonground = true;
//    else
//        wasonground = false;
//
//    if (!VectorEmpty(ent->avelocity))
//        SV_AddRotationalFriction(ent);
//
//    // add gravity except:
//    //   flying monsters
//    //   swimming monsters who are in the water
//    if (! wasonground)
//        if (!(ent->flags & FL_FLY))
//            if (!((ent->flags & FL_SWIM) && (ent->liquidInfo.level > 2))) {
//                if (ent->velocity[2] < sv_gravity->value * -0.1f)
//                    hitsound = true;
//                if (ent->liquidInfo.level == 0)
//                    SVG_AddGravity(ent);
//            }
//
//    // friction for flying monsters that have been given vertical velocity
//    if ((ent->flags & FL_FLY) && (ent->velocity[2] != 0)) {
//        speed = fabsf(ent->velocity[2]);
//        control = speed < sv_stopspeed ? sv_stopspeed : speed;
//        friction = sv_friction / 3;
//        newspeed = speed - (FRAMETIME * control * friction);
//        if (newspeed < 0)
//            newspeed = 0;
//        newspeed /= speed;
//        ent->velocity[2] *= newspeed;
//    }
//
//    // friction for flying monsters that have been given vertical velocity
//    if ((ent->flags & FL_SWIM) && (ent->velocity[2] != 0)) {
//        speed = fabsf(ent->velocity[2]);
//        control = speed < sv_stopspeed ? sv_stopspeed : speed;
//        newspeed = speed - (FRAMETIME * control * sv_waterfriction * (float)ent->liquidInfo.level);
//        if (newspeed < 0)
//            newspeed = 0;
//        newspeed /= speed;
//        ent->velocity[2] *= newspeed;
//    }
//
//    if (ent->velocity[2] || ent->velocity[1] || ent->velocity[0]) {
//        // apply friction
//        // let dead monsters who aren't completely onground slide
//        if ((wasonground) || (ent->flags & (FL_SWIM | FL_FLY)))
//            if (!(ent->health <= 0.0f && !M_CheckBottom(ent))) {
//                vel = ent->velocity;
//                speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
//                if (speed) {
//                    friction = sv_friction;
//
//                    control = speed < sv_stopspeed ? sv_stopspeed : speed;
//                    newspeed = speed - FRAMETIME * control * friction;
//
//                    if (newspeed < 0)
//                        newspeed = 0;
//                    newspeed /= speed;
//
//                    vel[0] *= newspeed;
//                    vel[1] *= newspeed;
//                }
//            }
//
//        if (ent->svflags & SVF_MONSTER)
//            mask = CM_CONTENTMASK_MONSTERSOLID;
//        else
//            mask = CM_CONTENTMASK_SOLID;
//
//        const Vector3 oldOrigin = ent->s.origin;
//        SV_FlyMove(ent, FRAMETIME, mask);
//        SVG_Util_TouchProjectiles( ent, oldOrigin );
//
//        gi.linkentity(ent);
//        SVG_Util_TouchTriggers(ent);
//        if (!ent->inuse)
//            return;
//
//        if (ent->groundentity)
//            if (!wasonground)
//                if (hitsound)
//                    gi.sound(ent, 0, gi.soundindex("world/land01.wav"), 1, 1, 0);
//    }
//
//// regular thinking
//    SV_RunThink(ent);
}

/**
*   @brief  For RootMotion entities.
**/
void SV_Physics_RootMotion( svg_base_edict_t *ent ) {
    bool	   wasonground;
    bool	   hitsound = false;
    float *vel;
    float	   speed, newspeed, control;
    float	   friction;
    svg_base_edict_t *groundentity;
    cm_contents_t mask = SVG_GetClipMask( ent );

    // airborne monsters should always check for ground
    if ( !ent->groundInfo.entity ) {
        M_CheckGround( ent, mask );
    }

    groundentity = ent->groundInfo.entity;

    SV_CheckVelocity( ent );

    if ( groundentity ) {
        wasonground = true;
    } else {
        wasonground = false;
    }

    if ( ent->avelocity[ 0 ] || ent->avelocity[ 1 ] || ent->avelocity[ 2 ] ) {
        SV_AddRotationalFriction( ent );
    }

    // FIXME: figure out how or why this is happening
    //if ( std::isnan( ent->velocity[ 0 ] ) || std::isnan( ent->velocity[ 1 ] ) || std::isnan( ent->velocity[ 2 ] ) )
    //if ( std::isnan( ent->velocity[ 0 ] ) || std::isnan( ent->velocity[ 1 ] ) || std::isnan( ent->velocity[ 2 ] ) )
    //    ent->velocity = {};

    // add gravity except:
    //   flying monsters
    //   swimming monsters who are in the water
    if ( !wasonground )
        if ( !( ent->flags & FL_FLY ) )
            if ( !( ( ent->flags & FL_SWIM ) && ( ent->liquidInfo.level > LIQUID_WAIST ) ) ) {
                //if ( ent->velocity[ 2 ] < level.gravity * -0.1f )
                if ( ent->velocity[ 2 ] < sv_gravity->value * -0.1f )
                    hitsound = true;
                if ( ent->liquidInfo.level != LIQUID_UNDER )
                    SVG_AddGravity( ent );
            }

    // friction for flying monsters that have been given vertical velocity
    if ( ( ent->flags & FL_FLY ) && ( ent->velocity[ 2 ] != 0 ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
        speed = fabsf( ent->velocity[ 2 ] );
        //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
        control = speed < sv_stopspeed ? sv_stopspeed : speed;
        friction = sv_friction / 3;
        newspeed = speed - ( gi.frame_time_s * control * friction );
        if ( newspeed < 0 )
            newspeed = 0;
        newspeed /= speed;
        ent->velocity[ 2 ] *= newspeed;
    }

    // friction for swimming monsters that have been given vertical velocity
    if ( ( ent->flags & FL_SWIM ) && ( ent->velocity[ 2 ] != 0 ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
        speed = fabsf( ent->velocity[ 2 ] );
        //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
        control = speed < sv_stopspeed ? sv_stopspeed : speed;
        newspeed = speed - ( gi.frame_time_s * control * sv_waterfriction * (float)ent->liquidInfo.level );
        if ( newspeed < 0 )
            newspeed = 0;
        newspeed /= speed;
        ent->velocity[ 2 ] *= newspeed;
    }

    if ( ent->velocity[ 2 ] || ent->velocity[ 1 ] || ent->velocity[ 0 ] ) {
        // apply friction
        if ( ( wasonground || ( ent->flags & ( FL_SWIM | FL_FLY ) ) ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
            vel = &ent->velocity[ 0 ];
            speed = sqrtf( vel[ 0 ] * vel[ 0 ] + vel[ 1 ] * vel[ 1 ] );
            if ( speed ) {
                friction = sv_friction;

                // Paril: lower friction for dead monsters
                if ( ent->lifeStatus )
                    friction *= 0.5f;

                //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
                control = speed < sv_stopspeed ? sv_stopspeed : speed;
                newspeed = speed - gi.frame_time_s * control * friction;

                if ( newspeed < 0 )
                    newspeed = 0;
                newspeed /= speed;

                vel[ 0 ] *= newspeed;
                vel[ 1 ] *= newspeed;
            }
        }

        Vector3 old_origin = ent->s.origin;

        SV_FlyMove( ent, gi.frame_time_s, mask );

        SVG_Util_TouchProjectiles( ent, old_origin );

        M_CheckGround( ent, mask );

        gi.linkentity( ent );

        // ========
        // PGM - reset this every time they move.
        //       SVG_touchtriggers will set it back if appropriate
        ent->gravity = 1.0;
        // ========

        // [Paril-KEX] this is something N64 does to avoid doors opening
        // at the start of a level, which triggers some monsters to spawn.
        if ( /*!level.is_n64 || */level.time > FRAME_TIME_S ) {
            SVG_Util_TouchTriggers( ent );
        }

        if ( !ent->inuse ) {
            return;
        }

        if ( ent->groundInfo.entity ) {
            if ( !wasonground ) {
                if ( hitsound ) {
                    ent->s.event = EV_FOOTSTEP;
                }
            }
        }
    }

    if ( !ent->inuse ) { // PGM g_touchtrigger free problem
        return;
    }

    if ( ent->svflags & SVF_MONSTER ) {
        M_CatagorizePosition( ent, Vector3( ent->s.origin ), ent->liquidInfo.level, ent->liquidInfo.type );
        M_WorldEffects( ent );
    }

    // regular thinking
    SV_RunThink( ent );
}

//============================================================================
/*
================
SVG_RunEntity

================
*/
void SVG_RunEntity(svg_base_edict_t *ent)
{
    Vector3	previousOrigin;
    bool	isMoveStepper = false;

    if ( ent->movetype == MOVETYPE_STEP ) {
        previousOrigin = ent->s.origin;
        isMoveStepper = true;
    }

    if ( ent->HasPreThinkCallback() ) {
        ent->DispatchPreThinkCallback( );
    }

    switch ( ent->movetype ) {
    case MOVETYPE_PUSH:
    case MOVETYPE_STOP:
        SV_Physics_Pusher( ent );
        break;
    case MOVETYPE_NONE:
        SV_Physics_None( ent );
        break;
    case MOVETYPE_NOCLIP:
        SV_Physics_Noclip( ent );
        break;
    case MOVETYPE_STEP:
        SV_Physics_Step( ent );
        break;
    case MOVETYPE_ROOTMOTION:
        SV_Physics_RootMotion( ent );
        break;
    case MOVETYPE_TOSS:
    case MOVETYPE_BOUNCE:
    case MOVETYPE_FLY:
    case MOVETYPE_FLYMISSILE:
        SV_Physics_Toss( ent );
        break;
    default:
        gi.error( "SV_Physics: bad movetype %i", ent->movetype );
    }

    if ( isMoveStepper && ent->movetype == MOVETYPE_STEP ) {
        // if we moved, check and fix origin if needed
        if ( !VectorCompare( ent->s.origin, previousOrigin ) ) {
            svg_trace_t trace = SVG_Trace( ent->s.origin, ent->mins, ent->maxs, &previousOrigin.x, ent, SVG_GetClipMask( ent ) );
            if ( trace.allsolid || trace.startsolid )
                VectorCopy( previousOrigin, ent->s.origin ); // = previous_origin;
        }
    }

    if ( ent->HasPostThinkCallback() ) {
        ent->DispatchPostThinkCallback();
    }
}
