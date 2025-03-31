/*********************************************************************
*
*
*	SVGame: Spectator Chase Camera:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_chase.h"



/**
*   @brief
**/
void SVG_ChaseCam_Update(svg_entity_t *ent)
{
    vec3_t o, ownerv, goal;
    svg_entity_t *targ;
    vec3_t forward, right;
    cm_trace_t trace;
    vec3_t angles;

    // is our chase target gone?
    if (!ent->client->chase_target->inuse
        || ent->client->chase_target->client->resp.spectator) {
        svg_entity_t *old = ent->client->chase_target;
        SVG_ChaseCam_Next(ent);
        if (ent->client->chase_target == old) {
            ent->client->chase_target = NULL;
			ent->client->ps.pmove.pm_flags &= ~( PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION ); // WID: pmove_new
            return;
        }
    }

    targ = ent->client->chase_target;

    VectorCopy(targ->s.origin, ownerv);

    ownerv[2] += targ->viewheight;

    VectorCopy(targ->client->viewMove.viewAngles, angles);
    if (angles[PITCH] > 56)
        angles[PITCH] = 56;
    AngleVectors(angles, forward, right, NULL);
    VectorNormalize(forward);
    VectorMA(ownerv, -30, forward, o);

    if (o[2] < targ->s.origin[2] + 20)
        o[2] = targ->s.origin[2] + 20;

    // jump animation lifts
    if ( !targ->groundInfo.entity ) {
        o[ 2 ] += 16;
    }

    trace = gi.trace(ownerv, vec3_origin, vec3_origin, o, targ, MASK_SOLID);

    VectorCopy(trace.endpos, goal);

    VectorMA(goal, 2, forward, goal);

    // pad for floors and ceilings
    VectorCopy(goal, o);
    o[2] += 6;
    trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
    if (trace.fraction < 1) {
        VectorCopy(trace.endpos, goal);
        goal[2] -= 6;
    }

    VectorCopy(goal, o);
    o[2] -= 6;
    trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
    if (trace.fraction < 1) {
        VectorCopy(trace.endpos, goal);
        goal[2] += 6;
    }

    if (targ->lifeStatus)
        ent->client->ps.pmove.pm_type = PM_DEAD;
    else
        ent->client->ps.pmove.pm_type = PM_FREEZE;

    VectorCopy(goal, ent->s.origin);
    ent->client->ps.pmove.delta_angles = /*ANGLE2SHORT*/QM_Vector3AngleMod( targ->client->viewMove.viewAngles - ent->client->resp.cmd_angles );

    if (targ->lifeStatus) {
        ent->client->ps.viewangles[ROLL] = 40;
        ent->client->ps.viewangles[PITCH] = -15;
        ent->client->ps.viewangles[YAW] = targ->client->killer_yaw; // targ->client->ps.stats[ STAT_KILLER_YAW ];
    } else {
        VectorCopy(targ->client->viewMove.viewAngles, ent->client->ps.viewangles);
        VectorCopy(targ->client->viewMove.viewAngles, ent->client->viewMove.viewAngles );
        QM_AngleVectors( ent->client->viewMove.viewAngles, &ent->client->viewMove.viewForward, nullptr, nullptr );
    }

    ent->viewheight = 0;
    ent->client->ps.pmove.pm_flags |= ( PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION ); // WID: pmove_new
    gi.linkentity(ent);
}



/**
*   @brief
**/
void SVG_ChaseCam_Next(svg_entity_t *ent)
{
    int i;
    svg_entity_t *e;

    if (!ent->client->chase_target)
        return;

    i = ent->client->chase_target - g_edicts;
    do {
        i++;
        if (i > maxclients->value)
            i = 1;
        e = g_edicts + i;
        if (!e->inuse)
            continue;
        if (!e->client->resp.spectator)
            break;
    } while (e != ent->client->chase_target);

    ent->client->chase_target = e;
    ent->client->update_chase = true;
}



/**
*   @brief
**/
void SVG_ChaseCam_Previous(svg_entity_t *ent)
{
    int i;
    svg_entity_t *e;

    if (!ent->client->chase_target)
        return;

    i = ent->client->chase_target - g_edicts;
    do {
        i--;
        if (i < 1)
            i = maxclients->value;
        e = g_edicts + i;
        if (!e->inuse)
            continue;
        if (!e->client->resp.spectator)
            break;
    } while (e != ent->client->chase_target);

    ent->client->chase_target = e;
    ent->client->update_chase = true;
}



/**
*   @brief
**/
void SVG_ChaseCam_GetTarget(svg_entity_t *ent)
{
    int i;
    svg_entity_t *other;

    for (i = 1; i <= maxclients->value; i++) {
        other = g_edicts + i;
        if (other->inuse && !other->client->resp.spectator) {
            ent->client->chase_target = other;
            ent->client->update_chase = true;
            SVG_ChaseCam_Update(ent);
            return;
        }
    }
    gi.centerprintf(ent, "No other players to chase.");
}

