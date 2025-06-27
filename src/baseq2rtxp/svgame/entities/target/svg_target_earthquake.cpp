/********************************************************************
*
*
*	ServerGame: Misc Entity 'target_earthquake'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/entities/target/svg_target_earthquake.h"

#include "sharedgame/sg_means_of_death.h"


#if 0
/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_target_earthquake_t )
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_earthquake_t, summedDistanceTraversed, SD_FIELD_TYPE_DOUBLE ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_earthquake_t, testVar, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_earthquake_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_target_earthquake_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_target_earthquake_t::Reset( const bool retainDictionary ) {
    // Call upon the base class.
    Super::Reset( retainDictionary );
    // Reset the edict's save descriptor fields.
    //testVar = 1337;
    //testVar2 = {};
}


/**
*   @brief  Save the entity into a file using game_write_context.
*   @note   Make sure to call the base parent class' Save() function.
**/
void svg_target_earthquake_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_target_earthquake_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_target_earthquake_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_target_earthquake_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_target_earthquake_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    return Super::KeyValue( keyValuePair, errorStr );
}
#endif // #if 0



/*QUAKED target_earthquake (1 0 0) (-8 -8 -8) (8 8 8)
When triggered, this initiates a level-wide earthquake.
All players and monsters are affected.
"speed"     severity of the quake (default:200)
"count"     duration of the quake (default:5)
*/
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_target_earthquake_t, onSpawn ) ( svg_target_earthquake_t *self ) -> void {
    Super::onSpawn( self );

    if ( !self->targetname ) {
        gi.dprintf( "untargeted %s at %s\n", (const char *)self->classname, vtos( self->s.origin ) );
    }
    if ( !self->count ) {
        self->count = 5;
    }

    if ( !self->speed ) {
        self->speed = 200;
    }

    self->svflags |= SVF_NOCLIENT;
    self->SetThinkCallback( &svg_target_earthquake_t::onThink );
    self->SetUseCallback( &svg_target_earthquake_t::onUse );

    self->noise_index = gi.soundindex( "world/quake.wav" );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_target_earthquake_t, onThink )( svg_target_earthquake_t *self ) -> void { 
    // Positioned Sound.
    if ( self->last_move_time < level.time ) {
        gi.positioned_sound( self->s.origin, self, CHAN_AUTO, self->noise_index, 1.0f, ATTN_NONE, 0 );
        self->last_move_time = level.time + 6.5_sec;
    }

    int32_t i = 0;
    svg_base_edict_t *ed = g_edict_pool.EdictForNumber( i );
    for ( i = 1, ed = g_edict_pool.EdictForNumber( i ); i < globals.edictPool->num_edicts; i++, ed = g_edict_pool.EdictForNumber( i ) ) {
        if ( !ed || !ed->inuse ) {
            continue;
        }
        if ( !ed->client ) {
            //break;
            continue;
        }

        ed->client->viewMove.quakeTime = level.time + 1000_ms;
    }

    if ( level.time < self->timestamp ) {
        self->nextthink = level.time + 10_hz;
    }
}

/**
*   @brief  
**/
DEFINE_MEMBER_CALLBACK_USE( svg_target_earthquake_t, onUse )( svg_target_earthquake_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    //self->timestamp = level.time + QMTime::FromMilliseconds( self->count );
//self->nextthink = level.time + FRAME_TIME_S;
//self->last_move_time = 0_ms;
//   self->activator = activator;
    if ( self->spawnflags & 8 /*.has( SPAWNFLAGS_EARTHQUAKE_ONE_SHOT )*/ ) {
        uint32_t i = 1;
        svg_base_edict_t *ed = g_edict_pool.EdictForNumber( i );
        for ( i = 1, ed = g_edict_pool.EdictForNumber( i ); i < globals.edictPool->num_edicts; i++, ed = g_edict_pool.EdictForNumber( i ) ) {
            if ( !ed || !ed->inuse ) {
                continue;
            }
            if ( !ed->client ) {
                //break;
                continue;
            }

            ed->client->viewMove.damagePitch = -self->speed * 0.1f;
            ed->client->viewMove.damageTime = level.time + DAMAGE_TIME();
        }

        return;
    }

    self->timestamp = level.time + QMTime::FromSeconds( self->count );

    if ( self->spawnflags & 2 /*SPAWNFLAGS_EARTHQUAKE_TOGGLE*/ ) {
        if ( self->style )
            self->nextthink = 0_ms;
        else
            self->nextthink = level.time + FRAME_TIME_S;

        self->style = !self->style;
    } else {
        self->nextthink = level.time + FRAME_TIME_S;
        self->last_move_time = 0_ms;
    }

    self->activator = activator;
}
/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_TOUCH( svg_target_earthquake_t, onTouch )( svg_target_earthquake_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void { }

/**
*   @brief  
**/
//DEFINE_MEMBER_CALLBACK_PAIN( svg_target_earthquake_t, onPain )( svg_target_earthquake_t *self, svg_base_edict_t *other, float kick, int damage ) -> void { }
/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_DIE( svg_target_earthquake_t, onDie )( svg_target_earthquake_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point ) -> void { }