/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_breakable'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_breakable.h"



/**
*   Spawnflags:
**/
//! Spawn on trigger. (Initially invisible.)
static constexpr int32_t FUNC_BREAKABLE_SPAWNFLAG_SPAWN_ON_TRIGGER	= BIT( 0 );
//! Free entity on break/explode.
static constexpr int32_t FUNC_BREAKABLE_SPAWNFLAG_FREE_ON_BREAK_EXPLODE = BIT( 1 );
//! Brush has animations.
static constexpr int32_t FUNC_BREAKABLE_SPAWNFLAG_ANIMATE_ALL		= BIT( 2 );
//! Brush has animations.
static constexpr int32_t FUNC_BREAKABLE_SPAWNFLAG_ANIMATE_ALL_FAST	= BIT( 3 );



/**
*
*
*
*   Generic func_explosion Stuff:
*
*
*
**/
/**
*   @brief  
**/
void _become_( edict_t *self ) {
    // Do not free this entity, we might wanna be respawned.
    SVG_Misc_BecomeExplosion( self, 1, false );
}

/**
*   @brief
**/
void func_breakable_explode( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    // bmodel origins are (0 0 0), we need to adjust that here
    const Vector3 size = Vector3( self->size ) * 0.5f; // VectorScale( self->size, 0.5f, size );
    const Vector3 origin = Vector3( self->absmin ) + size; // VectorAdd( self->absmin, size, origin );
    VectorCopy( origin, self->s.origin );

    // Take damage no more.
    self->takedamage = DAMAGE_NO;

    // Perform  radius damage if set.
    if ( self->dmg ) {
        SVG_RadiusDamage( self, attacker, self->dmg, NULL, self->dmg + 40, MEANS_OF_DEATH_EXPLOSIVE );
    }

    // Do we want this?
    #if 0
    self->velocity = QM_Vector3Normalize( Vector3( self->s.origin ) - Vector3( inflictor->s.origin ) ); //VectorSubtract( self->s.origin, inflictor->s.origin, self->velocity ); //self->velocity = QM_Vector3Normalize( self->velocity );
    self->velocity *= 150;
    #endif

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

    self->other = inflictor;
    self->activator = attacker;

    // Fire target triggers.
    SVG_UseTargets( self, attacker );

    // Free, or become an explosion if we had radius damage set.
    if ( self->dmg ) {
        // Turn into explosion.
        _become_( self );
        // Pass along some arguments.
        svg_signal_argument_array_t signalArguments;
        svg_signal_argument_t signalArgument = {
            .type = SIGNAL_ARGUMENT_TYPE_NUMBER,
            .key = "damage",
            .value = {
                .number = (double)damage,
            },
        };
		signalArguments.push_back( signalArgument );
        // Signal that we just exploded..
        SVG_SignalOut( self, inflictor, attacker, "OnExplode", signalArguments );
    } else {
        // Pass along some arguments.
        svg_signal_argument_array_t signalArguments;
        svg_signal_argument_t signalArgument = {
            .type = SIGNAL_ARGUMENT_TYPE_NUMBER,
            .key = "damage",
            .value = {
                .number = (double)damage,
            },
        };
        signalArguments.push_back( signalArgument );
        // Signal that we just trigger spawned.
        SVG_SignalOut( self, inflictor, attacker, "OnBreak", signalArguments );
    }

    // Do NOT Free it, we wanna be able to respawn it.
	if ( SVG_HasSpawnFlags( self, FUNC_BREAKABLE_SPAWNFLAG_FREE_ON_BREAK_EXPLODE ) ) {
        self->nextthink = level.time + FRAME_TIME_MS;
        self->think = SVG_FreeEdict;
	}
}

/**
*   @brief  Triggers the breaking/exploding of the target when triggered by other entities.
**/
void func_breakable_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    func_breakable_explode( self, other, activator, self->health, self->s.origin );
}
/**
*   @brief  Pain handling.
**/
void func_breakable_pain( edict_t *self, edict_t *other, float kick, int damage ) {
	// Construct signal arguments for the 'OnPain' signal.
    const svg_signal_argument_array_t signalArguments = {
            {
                .type = SIGNAL_ARGUMENT_TYPE_NUMBER,
                .key = "kick",
                .value = {
                    .number = kick
                }
            },
            {
                .type = SIGNAL_ARGUMENT_TYPE_NUMBER, // TODO: WID: Was before sol, TYPE_INTEGER
                .key = "damage",
                .value = {
                    .integer = damage
                }
            }
    };
	// Signal that we just got hurt.
    self->activator = other;
    self->other = other;

    SVG_SignalOut( self, self, self->activator, "OnPain", signalArguments );
}

/**
*   @brief
**/
void func_breakable_spawn_on_trigger( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
	// Solid now that it's destroyable.
    self->solid = SOLID_BSP;
	// Enable entity to be send to clients.
    self->svflags &= ~SVF_NOCLIENT;
    // Unset the use callback.
    self->use = NULL;
    // Set pain.
    self->pain = func_breakable_pain;
    self->takedamage = DAMAGE_YES;


    gi.unlinkentity( self );
    // Apply brush model to self. (Will be of the *number type.)
    gi.setmodel( self, self->model );
    // KillBox any obstructing entities.
    SVG_Util_KillBox( self, true );
    // Link the entity.
    gi.linkentity( self );

    // Signal that we just trigger spawned.
    SVG_SignalOut( self, other, activator, "OnTriggerSpawned", {} );
}



/**
*
*
*
*   SignalIn:
*
*
*
**/
void SP_func_( edict_t *self );

/**
*   @brief  Signal Receiving:
**/
void func_breakable_onsignalin( edict_t *self, edict_t *other, edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) {
    /**
    *   Revive:
    **/
    if ( Q_strcasecmp( signalName, "Revive" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Get health.
		self->health = self->max_health = SVG_SignalArguments_GetValue( signalArguments, "health", self->max_health );

        // Remove it, since we're respawning.
        self->spawnflags &= FUNC_BREAKABLE_SPAWNFLAG_SPAWN_ON_TRIGGER;
  //      self->spawnflags &= ~FUNC_BREAKABLE_SPAWNFLAG_FREE_ON_BREAK_EXPLODE;

  //      // Unset use.
        self->use = nullptr;
		//self->pain = nullptr;
		//self->takedamage = DAMAGE_NO;
        self->svflags &= ~SVF_NOCLIENT;

        // Spawn as usual.
        // Set pain.
        //self->pain = func_breakable_pain;

        //// Default to 100 health.
        //if ( !self->health ) {
        //    // Set health.
        //    self->health = 100;
        //}
        //// Set max health to health, use for revival.
        //self->max_health = self->health;

        //// Die callback for destructing.
        //self->die = func_breakable_explode;
        //// Allow it to take damage.
        //self->takedamage = DAMAGE_YES;
        func_breakable_spawn_on_trigger( self, other, activator, ENTITY_USETARGET_TYPE_ON, 1 );
        // Turn it on, showing itself.
        //func_breakable_spawn_on_trigger( self, other, activator, ENTITY_USETARGET_TYPE_ON, 1 );
		//func_breakable_use( self, other, activator, ENTITY_USETARGET_TYPE_ON, 1 );
    }
    /**
    *   Break/Explode:
    **/
    // Break:
    if ( Q_strcasecmp( signalName, "Break" ) == 0 ) {
        func_breakable_explode( self, self, activator, self->health, self->s.origin );
    }
    // Explode:
    if ( Q_strcasecmp( signalName, "Explode" ) == 0 ) {
		// Get damage argument if any.
        const int64_t dmg = SVG_SignalArguments_GetValue( signalArguments, "damage", 100 );
        self->dmg = dmg;
        func_breakable_explode( self, self, activator, self->health, self->s.origin );
    }

    // WID: Useful for debugging.
    #if 0
    const int32_t otherNumber = ( other ? other->s.number : -1 );
    const int32_t activatorNumber = ( activator ? activator->s.number : -1 );
    gi.dprintf( "door_onsignalin[ self(#%d), \"%s\", other(#%d), activator(%d) ]\n", self->s.number, signalName, otherNumber, activatorNumber );
    #endif
}



/**
*
*
*
*   func_ Spawn:
*
*
*
**/
/**
*   @brief
**/
void SP_func_breakable( edict_t *self ) {
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
    
    // Signal Receiving:
    self->onsignalin = func_breakable_onsignalin;

    // Always clip to hull.
    self->svflags |= SVF_HULL;

    // Spawn after being triggered instead of immediately at level start:
    if ( SVG_HasSpawnFlags( self, FUNC_BREAKABLE_SPAWNFLAG_SPAWN_ON_TRIGGER ) ) {
        self->svflags |= SVF_NOCLIENT;
        self->solid = SOLID_NOT;
        self->use = func_breakable_spawn_on_trigger;
    // Spawned immediately:
    } else {
        // Solid now that it's destroyable.
        self->solid = SOLID_BSP;
        // If a targetname is set, setup its use callback instead of explosion death callback.
        if ( self->targetname ) {
            self->use = func_breakable_use;
        }
    }

    // Animation Effects SpawnFlags:
    if ( SVG_HasSpawnFlags( self, FUNC_BREAKABLE_SPAWNFLAG_ANIMATE_ALL ) ) {
        self->s.effects |= EF_ANIM_ALL;
    }
    if ( SVG_HasSpawnFlags( self, FUNC_BREAKABLE_SPAWNFLAG_ANIMATE_ALL_FAST ) ) {
        self->s.effects |= EF_ANIM_ALLFAST;
    }

    // No default use callback was set, so resort to actual default behavior of the object:
    // Explode after running out of health.
    if ( self->use != func_breakable_use ) {
        // Set pain.
        self->pain = func_breakable_pain;

        // Default to 100 health.
        if ( !self->health ) {
            // Set health.
            self->health = 100;
        }
        // Set max health to health, use for revival.
        self->max_health = self->health;

        // Die callback for destructing.
        self->die = func_breakable_explode;
        // Allow it to take damage.
        self->takedamage = DAMAGE_YES;
    }

    // Link it in for net and collision.
    gi.linkentity( self );
}