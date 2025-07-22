/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_combat.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_save.h"
#include "svgame/svg_utils.h"

#include "sharedgame/sg_tempentity_events.h"
#include "sharedgame/sg_usetarget_hints.h"

/**
*   @brief  Marks the edict as free
**/
DEFINE_GLOBAL_CALLBACK_THINK( SVG_FreeEdict )( svg_base_edict_t *ed ) -> void {
    g_edict_pool.FreeEdict( ed );
}

/**
*   @brief  Searches all active entities for the next one that holds
*           the matching string at fieldofs (use the FOFS_GENTITY() macro) in the structure.
*
*   @remark Searches beginning at the edict after from, or the beginning if NULL
*           NULL will be returned if the end of the list is reached.
**/
svg_base_edict_t *SVG_Entities_Find( svg_base_edict_t *from, const int32_t fieldofs, const char *match ) {
    char *s;

    // WID: Prevent nastyness when match is empty (Q_stricmp)
    if ( !match ) {
        return nullptr;
    }

    //if ( !from ) {
    //    from = g_edicts;
    //} else {
    //    from++;
    //}
    const int32_t startIndex = ( from ? from->s.number + 1 : 0 );

    //for ( ; from < &g_edicts[ globals.edictPool->num_edicts ]; from++ ) {
    //    if ( !from || !from->inuse ) {
	for ( int32_t i = startIndex; i < g_edict_pool.num_edicts; i++ ) {
        from = g_edict_pool.EdictForNumber( i );

        if ( !from || !from->inuse ) {
            continue;
        }
        s = *(char **)( (byte *)from + fieldofs );
        if ( !s ) {
            continue;
        }
        if ( !Q_stricmp( s, match ) ) {
            return from;
        }
    }

    return NULL;
}

/**
*   @brief  Similar to SVG_Entities_Find, but, returns entities that have origins within a spherical area.
**/
svg_base_edict_t *SVG_Entities_FindWithinRadius( svg_base_edict_t *from, const vec3_t org, const float rad ) {
    vec3_t  eorg;
    int     j;

    //if ( !from ) {
    //    from = g_edicts;
    //} else {
    //    from++;
    //}
    const int32_t startIndex = ( from ? from->s.number + 1 : 0 );

    //for ( ; from < &g_edicts[ globals.edictPool->num_edicts ]; from++ ) {
    //    if ( !from || !from->inuse ) {
    for ( int32_t i = startIndex; i < g_edict_pool.num_edicts; i++ ) {
        from = g_edict_pool.EdictForNumber( i );
        if ( !from || !from->inuse ) {
            continue;
        }
        if ( from->solid == SOLID_NOT ) {
            continue;
        }
        for ( j = 0; j < 3; j++ ) {
            eorg[ j ] = org[ j ] - ( from->s.origin[ j ] + ( from->mins[ j ] + from->maxs[ j ] ) * 0.5f );
        }
        if ( VectorLength( eorg ) > rad ) {
            continue;
        }
        return from;
    }

    return NULL;
}



/**
*
* 
*
*   Dead Body Queue:
*
*
* 
**/
/**
*   @brief
**/
void SVG_Entities_InitBodyQue( void ) {
    int     i;
    svg_base_edict_t *ent;

    level.body_que = 0;
    for ( i = 0; i < BODY_QUEUE_SIZE; i++ ) {
        ent = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>( "bodyque" );
    }
}
/**
*   @brief
**/
DECLARE_GLOBAL_CALLBACK_DIE( body_die );
DEFINE_GLOBAL_CALLBACK_DIE( body_die )( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point ) -> void {
    int n;

    if ( self->health < -40 ) {
        gi.sound( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), 1, ATTN_NORM, 0 );
        for ( n = 0; n < 4; n++ ) {
            SVG_Misc_ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_TYPE_ORGANIC );
        }
        self->s.origin[ 2 ] -= 48;
        SVG_Misc_ThrowClientHead( self, damage );
        self->takedamage = DAMAGE_NO;
    }
}

/**
*   @brief  Get a que slot, leave an effect, and remove body into the queue.
**/
void SVG_Entities_BodyQueueAddForPlayer( svg_base_edict_t *ent ) {
    // Unlink entity.
    gi.unlinkentity( ent );

    // grab a body que and cycle to the next one
    svg_base_edict_t *body = g_edict_pool.EdictForNumber( game.maxclients + level.body_que + 1 );//&g_edicts[ game.maxclients + level.body_que + 1 ];
    level.body_que = ( level.body_que + 1 ) % BODY_QUEUE_SIZE;

    // send an effect on the removed body
    if ( body->s.modelindex ) {
        gi.WriteUint8( svc_temp_entity );
        gi.WriteUint8( TE_BLOOD );
        gi.WritePosition( body->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        gi.WriteDir8( vec3_origin );
        gi.multicast( body->s.origin, MULTICAST_PVS, false );
    }

    gi.unlinkentity( body );

    body->s.number = g_edict_pool.NumberForEdict( body );//body - g_edicts;
    VectorCopy( ent->s.origin, body->s.origin );
    VectorCopy( ent->s.origin, body->s.old_origin );
    VectorCopy( ent->s.angles, body->s.angles );
    body->s.modelindex = ent->s.modelindex;
    body->s.frame = ent->s.frame;
    body->s.skinnum = ent->s.skinnum;
    body->s.event = EV_OTHER_TELEPORT;

    body->svflags = ent->svflags;
    VectorCopy( ent->mins, body->mins );
    VectorCopy( ent->maxs, body->maxs );
    VectorCopy( ent->absmin, body->absmin );
    VectorCopy( ent->absmax, body->absmax );
    VectorCopy( ent->size, body->size );
    body->velocity = ent->velocity;
    body->avelocity = ent->avelocity;
    body->solid = ent->solid;
    body->clipmask = ent->clipmask;
    body->owner = ent->owner;
    body->movetype = ent->movetype;
    body->groundInfo.entity = ent->groundInfo.entity;

    body->s.entityType = ET_PLAYER_CORPSE;

    body->SetDieCallback( body_die );
    body->takedamage = DAMAGE_YES;

    gi.linkentity( body );
}



/**
*
*
*
*   Visibility Testing:
*
*
*
**/
/**
*   @brief  Tests for visibility even if the entity is visible to self, and also if not 'infront'.
*           The testing is done by a trace line (self->origin + self->viewheight) to (other->origin + other->viewheight),
*           which if hits nothing, means the entity is visible.
*   @return True if the entity 'other' is visible to 'self'.
**/
const bool SVG_Entity_IsVisible( svg_base_edict_t *self, svg_base_edict_t *other ) {
    vec3_t  spot1;
    vec3_t  spot2;
    svg_trace_t trace;

    VectorCopy( self->s.origin, spot1 );
    spot1[ 2 ] += self->viewheight;
    VectorCopy( other->s.origin, spot2 );
    spot2[ 2 ] += other->viewheight;
    trace = SVG_Trace( spot1, vec3_origin, vec3_origin, spot2, self, CM_CONTENTMASK_OPAQUE );

    if ( trace.fraction == 1.0f )
        return true;
    return false;
}

/**
*   @return True if the entity is in front (in sight) of self
**/
const bool SVG_Entity_IsInFrontOf( svg_base_edict_t *self, svg_base_edict_t *other, const float dotRangeArea ) {
    // If a client, use its forward vector.
    Vector3 forward = {};
    if ( SVG_Entity_IsClient( self ) ) {
        forward = self->client->viewMove.viewForward;
    // Calculate forward vector:
    } else {
        QM_AngleVectors( self->s.angles, &forward, nullptr, nullptr );
    }
    // Get direction.
    Vector3 direction = Vector3( other->s.origin ) - Vector3( self->s.origin );
    // Normalize direction.
    Vector3 normalizedDirection = QM_Vector3Normalize( direction );
    // Get dot product from normalized direction / forward.
    const float dot = QM_Vector3DotProduct( normalizedDirection, forward );
    // In 'front' of.
    if ( dot > dotRangeArea ) {
        return true;
    }
    // Not in 'front' of.
    return false;
}
/**
*   @return True if the testOrigin point is in front of entity 'self'.
**/
const bool SVG_Entity_IsInFrontOf( svg_base_edict_t *self, const Vector3 &testOrigin, const float dotRangeArea ) {
    // If a client, use its forward vector.
    Vector3 forward = {};
    if ( SVG_Entity_IsClient( self ) ) {
        forward = self->client->viewMove.viewForward;
        // Calculate forward vector:
    } else {
        QM_AngleVectors( self->s.angles, &forward, nullptr, nullptr );
    }
    // Get direction.
    Vector3 direction = testOrigin - self->s.origin;
    // Normalize direction.
    Vector3 normalizedDirection = QM_Vector3Normalize( direction );
    // Get dot product from normalized direction / forward.
    const float dot = QM_Vector3DotProduct( normalizedDirection, forward );
    // In 'front' of.
    if ( dot > dotRangeArea ) {
        return true;
    }
    // Not in 'front' of.
    return false;
}



/**
*
*
*
*   Entity UseTargetHint Functionality:
*
*
*
**/
/**
*   @brief  Find the matching information for the ID and assign it to the entity's useTarget.hintInfo.
**/
void SVG_Entity_SetUseTargetHintByID( svg_base_edict_t *ent, const int32_t id ) {
    // Get the useTargetHintID from the stats.
    const int32_t useTargetHintID = id;
    // Exit, since there is no useTargetHintID to display. (It is 0.)
    if ( !ent ) {
        return;
    }

    // If the ID > 0, then we'll fetch it from the UseTargetHint Data Array.
    const sg_usetarget_hint_t *useTargetHint = nullptr;
    if ( useTargetHintID > USETARGET_HINT_ID_NONE ) {
        useTargetHint = SG_UseTargetHint_GetByID( useTargetHintID );
    }

    // Ensure we got data for, and meant, to display.
    if ( !useTargetHint || useTargetHint->hintString == nullptr ) {
        ent->useTarget.hintInfo = nullptr;
        return;
    }

    // Apply to entity.
    ent->useTarget.hintInfo = useTargetHint;
}