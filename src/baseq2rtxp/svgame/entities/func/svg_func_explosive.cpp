/********************************************************************
*
*
*	ServerGame: Brush Entity 'func_explosive'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_explosive.h"



/*
QUAKED func_explosive (0 .5 .8) ? SpawnOnTrigger ANIMATED ANIMATED_FAST
Any brush that you want to explode or break apart.  If you want an
ex0plosion, set dmg and it will do a radius explosion of that amount
at the center of the bursh.

If targeted it will not be shootable.

health defaults to 100.

mass defaults to 75.  This determines how much debris is emitted when
it explodes.  You get one large chunk per 100 of mass (up to 8) and
one small chunk per 25 of mass (up to 16).  So 800 gives the most.
*/

void explosive_become_explosive( edict_t *self ) {
    gi.WriteUint8( svc_temp_entity );
    gi.WriteUint8( TE_EXPLOSION1 );
    gi.WritePosition( self->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.multicast( self->s.origin, MULTICAST_PVS, false );
}

/**
*   @brief
**/
void func_explosive_explode( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    // bmodel origins are (0 0 0), we need to adjust that here
    const Vector3 size = Vector3( self->size ) * 0.5f; // VectorScale( self->size, 0.5f, size );
    const Vector3 origin = Vector3( self->absmin ) + size; // VectorAdd( self->absmin, size, origin );
    VectorCopy( origin, self->s.origin );

    // Take damage no more.
    self->takedamage = DAMAGE_NO;

    // Perform explosive radius damage if set.
    if ( self->dmg ) {
        SVG_RadiusDamage( self, attacker, self->dmg, NULL, self->dmg + 40, MEANS_OF_DEATH_EXPLOSIVE );
    }
    self->velocity = QM_Vector3Normalize( Vector3( self->s.origin ) - Vector3( inflictor->s.origin ) ); //VectorSubtract( self->s.origin, inflictor->s.origin, self->velocity ); //self->velocity = QM_Vector3Normalize( self->velocity );
    self->velocity *= 150;

    // start chunks towards the center
    Vector3 center = size * 0.5f;

    // Apply default mass if needed.
    int32_t mass = self->mass;
    if ( !mass ) {
        mass = 75;
    }

    // Spawn any BIG chunks:
    if ( mass >= 100 ) {
        int32_t count = mass / 100;
        if ( count > 8 )
            count = 8;
        while ( count-- ) {
            const Vector3 chunkOrigin = QM_Vector3MultiplyAdd( origin, crandom(), center );
            SVG_Misc_ThrowDebris( self, "models/objects/debris1/tris.md2", 1, (vec_t*)&chunkOrigin.x);
        }
    }

    // Spawn any SM000AAAALL chunks:
    int32_t count = mass / 25;
    if ( count > 16 ) {
        count = 16;
    }
    while ( count-- ) {
        const Vector3 chunkOrigin = QM_Vector3MultiplyAdd( origin, crandom(), center );
        SVG_Misc_ThrowDebris( self, "models/objects/debris2/tris.md2", 2, (vec_t*)&chunkOrigin.x );
    }

    // Fire target triggers.
    SVG_UseTargets( self, attacker );

    // Free, or become an explosion if we had radius damage set.
    if ( self->dmg ) {
        // Turn into explosion.
        explosive_become_explosive( self );
        // Signal that we just exploded..
        SVG_SignalOut( self, inflictor, attacker, "OnExplode", {} );
    } else {
        // Signal that we just trigger spawned.
        SVG_SignalOut( self, inflictor, attacker, "OnBreak", {} );
    }

    // Free it.
    SVG_FreeEdict( self );
}

/**
*   @brief  Triggers the breaking/exploding of the target when triggered by other entities.
**/
void func_explosive_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    func_explosive_explode( self, self, activator, self->health, self->s.origin );
}

/**
*   @brief
**/
void func_explosive_spawn( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    self->solid = SOLID_BSP;
    self->svflags &= ~SVF_NOCLIENT;
    self->use = NULL;
    KillBox( self, false );
    gi.linkentity( self );

    // Signal that we just trigger spawned.
    SVG_SignalOut( self, other, activator, "OnTriggerSpawn", {} );
}

/**
*   @brief
**/
void SP_func_explosive( edict_t *self ) {
    // WID: Nah.
    #if 0
    if ( deathmatch->value ) {
        // auto-remove for deathmatch
        SVG_FreeEdict( self );
        return;
    }
    #endif
    // BSP Brush, always MoveType PUSH
    self->movetype = MOVETYPE_PUSH;
    // Ensure to treat it like one.
    self->s.entityType = ET_PUSHER;

    // Load debris models for spawned debris entities.
    gi.modelindex( "models/objects/debris1/tris.md2" );
    gi.modelindex( "models/objects/debris2/tris.md2" );

    // Apply brush model to self. (Will be of the *number type.)
    gi.setmodel( self, self->model );

    // Spawn after being triggered instead of immediately at level start:
    if ( SVG_HasSpawnFlags( self, FUNC_EXPLOSIVE_SPAWNFLAG_SPAWN_ON_TRIGGER ) ) {
        self->svflags |= SVF_NOCLIENT;
        self->solid = SOLID_NOT;
        self->use = func_explosive_spawn;
    // Spawned immediately:
    } else {
        // Solid now that it's destroyable.
        self->solid = SOLID_BSP;
        // If a targetname is set, setup its use callback instead of explosion death callback.
        if ( self->targetname ) {
            self->use = func_explosive_use;
        }
    }

    // Animation Effects SpawnFlags:
    if ( SVG_HasSpawnFlags( self, FUNC_EXPLOSIVE_SPAWNFLAG_ANIMATE_ALL ) ) {
        self->s.effects |= EF_ANIM_ALL;
    }
    if ( SVG_HasSpawnFlags( self, FUNC_EXPLOSIVE_SPAWNFLAG_ANIMATE_ALL_FAST ) ) {
        self->s.effects |= EF_ANIM_ALLFAST;
    }

    // No default use callback was set, so resort to actual default behavior of the object:
    // Explode after running out of health.
    if ( self->use != func_explosive_use ) {
        // Default to 100 health.
        if ( !self->health ) {
            self->health = 100;
        }
        // Die callback for destructing.
        self->die = func_explosive_explode;
        // Allow it to take damage.
        self->takedamage = DAMAGE_YES;
    }

    // Link it in for net and collision.
    gi.linkentity( self );
}