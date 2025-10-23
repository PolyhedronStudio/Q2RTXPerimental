/********************************************************************
*
*
*	ServerGame: Misc Entity 'target_lightramp'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/entities/light/svg_light_light.h"
#include "svgame/entities/target/svg_target_lightramp.h"

#include "sharedgame/sg_means_of_death.h"


#if 0
/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_target_lightramp_t )
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_lightramp_t, summedDistanceTraversed, SD_FIELD_TYPE_DOUBLE ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_lightramp_t, testVar, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_lightramp_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_target_lightramp_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_target_lightramp_t::Reset( const bool retainDictionary ) {
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
void svg_target_lightramp_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_target_lightramp_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_target_lightramp_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_target_lightramp_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_target_lightramp_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    return Super::KeyValue( keyValuePair, errorStr );
}
#endif // #if 0



/*QUAKED target_lightramp (0 .5 .8) (-8 -8 -8) (8 8 8) TOGGLE
speed		How many seconds the ramping will take
message		two letters; starting lightlevel and ending lightlevel
*/

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_target_lightramp_t, onSpawn ) ( svg_target_lightramp_t *self ) -> void {
    Super::onSpawn( self );

    if ( !self->message.ptr || strlen( self->message.ptr ) != 2 || self->message.ptr[ 0 ] < 'a' || self->message.ptr[ 0 ] > 'z' || self->message.ptr[ 1 ] < 'a' || self->message.ptr[ 1 ] > 'z' || self->message.ptr[ 0 ] == self->message.ptr[ 1 ] ) {
        gi.dprintf( "target_lightramp has bad ramp (%s) at %s\n", self->message.ptr, vtos( self->s.origin ) );
        SVG_FreeEdict( self );
        return;
    }

    #if 0 // <Q2RTXP>: WID: We want these in MP too.
    if ( deathmatch->value ) {
        SVG_FreeEdict( self );
        return;
    }
    #endif

    if ( !self->targetNames.target ) {
        gi.dprintf( "%s with no target at %s\n", (const char *)self->classname, vtos( self->s.origin ) );
        SVG_FreeEdict( self );
        return;
    }

    self->svflags |= SVF_NOCLIENT;
    self->SetUseCallback( &svg_target_lightramp_t::onUse );
    self->SetThinkCallback( &svg_target_lightramp_t::onThink );

    self->movedir[ 0 ] = self->message.ptr[ 0 ] - 'a';
    self->movedir[ 1 ] = self->message.ptr[ 1 ] - 'a';
    self->movedir[ 2 ] = ( self->movedir[ 1 ] - self->movedir[ 0 ] ) / ( self->speed / gi.frame_time_s );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_target_lightramp_t, onThink )( svg_target_lightramp_t *self ) -> void {
    char    style[ 2 ];

    style[ 0 ] = (char)( 'a' + self->movedir[ 0 ] + ( ( level.time - self->timestamp ) / gi.frame_time_s ).Seconds() * self->movedir[ 2 ] );
    style[ 1 ] = 0;
    gi.configstring( CS_LIGHTS + self->enemy->style, style );

    if ( ( level.time - self->timestamp ).Seconds() < self->speed ) {
        self->nextthink = level.time + FRAME_TIME_S;
    } else if ( self->spawnflags & svg_target_lightramp_t::SPAWNFLAG_TOGGLE ) {
        char    temp;

        temp = self->movedir[ 0 ];
        self->movedir[ 0 ] = self->movedir[ 1 ];
        self->movedir[ 1 ] = temp;
        self->movedir[ 2 ] *= -1;
    }
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_target_lightramp_t, onUse )( svg_target_lightramp_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    if ( !self->enemy ) {
        svg_base_edict_t *e;

        // check all the targets
        e = NULL;
        while ( 1 ) {
            e = SVG_Entities_Find( e, q_offsetof( svg_base_edict_t, targetname ), (const char *)self->targetNames.target );
            if ( !e ) {
                break;
            }
            if ( !e->GetTypeInfo()->IsSubClassType<svg_light_light_t>() ) {
                gi.dprintf( "%s: %s at %s ", __func__, (const char *)self->classname, vtos( self->s.origin ) );
                gi.dprintf( "%s: target %s (%s at %s) is not a light\n", __func__, (const char *)self->targetNames.target, (const char *)e->classname, vtos( e->s.origin ) );
            } else {
                self->enemy = e;
            }
        }

        if ( !self->enemy ) {
            gi.dprintf( "%s: %s target %s not found at %s\n", __func__, (const char *)self->classname, (const char *)self->targetNames.target, vtos( self->s.origin ) );
            SVG_FreeEdict( self );
            return;
        }
    }

    self->timestamp = level.time;
    self->DispatchThinkCallback();
}
/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_TOUCH( svg_target_lightramp_t, onTouch )( svg_target_lightramp_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void { }

/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_PAIN( svg_target_lightramp_t, onPain )( svg_target_lightramp_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void { }
/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_DIE( svg_target_lightramp_t, onDie )( svg_target_lightramp_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) -> void { }