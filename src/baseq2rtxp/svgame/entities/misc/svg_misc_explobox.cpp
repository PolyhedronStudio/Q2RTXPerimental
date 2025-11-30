/********************************************************************
*
*
*	ServerGame: Misc Entity 'misc_explobox'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/entities/misc/svg_misc_explobox.h"

#include "sharedgame/sg_means_of_death.h"


#if 0
/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_misc_explobox_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_explobox_t, summedDistanceTraversed, SD_FIELD_TYPE_DOUBLE ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_explobox_t, testVar, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_explobox_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_misc_explobox_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_misc_explobox_t::Reset( const bool retainDictionary ) {
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
void svg_misc_explobox_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_misc_explobox_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_misc_explobox_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_misc_explobox_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_misc_explobox_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    return Super::KeyValue( keyValuePair, errorStr );
}
#endif // #if 0



/*QUAKED misc_explobox (0 .5 .8) (-16 -16 0) (16 16 40)
Large exploding box.  You can override its mass (100),
health (80), and dmg (150).
*/
bool M_walkmove( svg_base_edict_t *ent, float yaw, float dist );

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_misc_explobox_t, onTouch )( svg_misc_explobox_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {

    if ( !other || ( other->groundInfo.entityNumber != ENTITYNUM_NONE ) || ( other->groundInfo.entityNumber == self->s.number ) ) {
        return;
    }

    // Calculate direction.
    vec3_t v = { };
    VectorSubtract( self->s.origin, other->s.origin, v );

    // Move ratio(based on their masses).
    const double ratio = (double)other->mass / (double)self->mass;

    // Yaw direction angle.
    const float yawAngle = QM_Vector3ToYaw( v );
    const float direction = yawAngle;
    // Distance to travel.
    const double distance = 20 * ratio * FRAMETIME;

    // Debug output:
    #if 0
    if ( plane ) {
        gi.dprintf( "self->s.origin( %s ), other->s.origin( %s )\n", vtos( self->s.origin ), vtos( other->s.origin ) );
        gi.dprintf( "v( %s ), plane->normal( %s ), direction(%f), distance(%f)\n", vtos( v ), vtos( plane->normal ), direction, distance );
    } else {
        gi.dprintf( "self->s.origin( %s ), other->s.origin( %s )\n", vtos( self->s.origin ), vtos( other->s.origin ) );
        gi.dprintf( "v( %s ), direction(%f), distance(%f)\n", vtos( v ), direction, distance );
    }
    #endif

    // WID: TODO: Use new monster walkmove/slidebox implementation.
    // Perform move.
    M_walkmove( self, direction, distance );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_misc_explobox_t, thinkExplode )( svg_misc_explobox_t *self ) -> void {
    vec3_t  save = {};

    SVG_RadiusDamage( self, self->activator, self->dmg, NULL, self->dmg + 40, MEANS_OF_DEATH_EXPLODED_BARREL );

    VectorCopy( self->s.origin, save );
    VectorMA( self->absMin, 0.5f, self->size, self->s.origin );

    #if 0
    // a few big chunks
    spd = 1.5f * (float)self->dmg / 200.0f;
    VectorMA( self->s.origin, crandom(), self->size, org );
    SVG_Misc_ThrowDebris( self, "models/debris/debris1/tris.md2", spd, org );
    VectorMA( self->s.origin, crandom(), self->size, org );
    SVG_Misc_ThrowDebris( self, "models/debris/debris1/tris.md2", spd, org );

    // bottom corners
    spd = 1.75f * (float)self->dmg / 200.0f;
    VectorCopy( self->absMin, org );
    SVG_Misc_ThrowDebris( self, "models/debris/debris3/tris.md2", spd, org );
    VectorCopy( self->absMin, org );
    org[ 0 ] += self->size[ 0 ];
    SVG_Misc_ThrowDebris( self, "models/debris/debris3/tris.md2", spd, org );
    VectorCopy( self->absMin, org );
    org[ 1 ] += self->size[ 1 ];
    SVG_Misc_ThrowDebris( self, "models/debris/debris3/tris.md2", spd, org );
    VectorCopy( self->absMin, org );
    org[ 0 ] += self->size[ 0 ];
    org[ 1 ] += self->size[ 1 ];
    SVG_Misc_ThrowDebris( self, "models/debris/debris3/tris.md2", spd, org );

    // a bunch of little chunks
    spd = 2 * self->dmg / 200;
    for ( i = 0; i < 8; i++ ) {
        VectorMA( self->s.origin, crandom(), self->size, org );
        SVG_Misc_ThrowDebris( self, "models/debris/debris2/tris.md2", spd, org );
    }

    VectorCopy( save, self->s.origin );
    if ( self->groundInfo.entity ) {
        SVG_Misc_BecomeExplosion( self, 2 );
    } else {
        SVG_Misc_BecomeExplosion( self, 1 );
    }
    #endif // #if 0
    if ( self->groundInfo.entityNumber != ENTITYNUM_NONE ) {
        SVG_Misc_BecomeExplosion( self, 2, true );
    } else {
        SVG_Misc_BecomeExplosion( self, 1, true );
    }
    //SVG_Misc_BecomeExplosion( self, true );
}


/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_DIE( svg_misc_explobox_t, onDie )( svg_misc_explobox_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) -> void {
    self->takedamage = DAMAGE_NO;
    self->nextthink = level.time + random_time( 150_ms );
    self->SetThinkCallback( &svg_misc_explobox_t::thinkExplode );
    self->activator = attacker;
}

/**
*   @brief  
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_misc_explobox_t, onSpawn) ( svg_misc_explobox_t *self ) -> void {
    // Always spawn Super class.
    Super::onSpawn( self );


    if ( deathmatch->value ) {
        // auto-remove for deathmatch
        SVG_FreeEdict( self );
        return;
    }

    gi.modelindex( "models/debris/debris1/tris.md2" );
    gi.modelindex( "models/debris/debris2/tris.md2" );
    gi.modelindex( "models/debris/debris3/tris.md2" );

    self->solid = SOLID_BOUNDS_BOX;
    self->movetype = MOVETYPE_STEP;

    self->model = svg_level_qstring_t::from_char_str( "models/objects/barrels/tris.md2" );
    self->s.modelindex = gi.modelindex( self->model );
    VectorSet( self->mins, -16, -16, 0 );
    VectorSet( self->maxs, 16, 16, 40 );

    if ( !self->mass )
        self->mass = 400;
    if ( !self->health )
        self->health = 10;
    if ( !self->dmg )
        self->dmg = 150;

    self->SetDieCallback( &svg_misc_explobox_t::onDie );
    self->takedamage = DAMAGE_YES;
    // Pain:
    self->SetPainCallback( &svg_misc_explobox_t::onPain );
    //self->monsterinfo.aiflags = AI_NOSTEP;

    self->SetTouchCallback( &svg_misc_explobox_t::onTouch );

    self->SetThinkCallback( M_droptofloor );
    self->nextthink = level.time + 20_hz;

    gi.linkentity( self );
}

/**
*   @brief  Death routine.
**/
DEFINE_MEMBER_CALLBACK_PAIN( svg_misc_explobox_t, onPain )( svg_misc_explobox_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void {

}