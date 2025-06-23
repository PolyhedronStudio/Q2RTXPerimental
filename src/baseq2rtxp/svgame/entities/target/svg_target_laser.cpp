/********************************************************************
*
*
*	ServerGame: Misc Entity 'target_laser'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/target/svg_target_laser.h"

#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_tempentity_events.h"


#if 0
/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_target_laser_t )
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_laser_t, summedDistanceTraversed, SD_FIELD_TYPE_DOUBLE ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_laser_t, testVar, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_laser_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_target_laser_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_target_laser_t::Reset( const bool retainDictionary ) {
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
void svg_target_laser_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_target_laser_t::saveDescriptorFields, this );
}
#endif
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_target_laser_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    //ctx->read_fields( svg_target_laser_t::saveDescriptorFields, this );

    // Let everything else get spawned before we start firing.
    SetThinkCallback( &svg_target_laser_t::onThink );
	nextthink = level.time + FRAME_TIME_S;
    if ( this->spawnflags & svg_target_laser_t::SPAWNFLAG_START_ON ) {
        this->SetActive( true );
    } else {
        this->SetActive( false );
    }
}

#if 0
/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_target_laser_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    return Super::KeyValue( keyValuePair, errorStr );
}
#endif // #if 0

/**
*   @brief  Sets the laser state to either active or inactive(hides it.)
**/
void svg_target_laser_t::SetActive( const bool isActive ) {
    if ( isActive ) {
        if ( !activator ) {
            activator = this;
        }
        spawnflags |= svg_target_laser_t::SPAWNFLAG_START_ON;// 0x80000001;
        svflags &= ~SVF_NOCLIENT;
        DispatchThinkCallback( );
    } else {
        spawnflags &= ~svg_target_laser_t::SPAWNFLAG_START_ON;
        svflags |= SVF_NOCLIENT;
        nextthink = 0_ms;
    }
}


/*QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON RED GREEN BLUE YELLOW ORANGE FAT
When triggered, fires a laser.  You can either set a target
or a direction.
*/
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_target_laser_t, onSpawn ) ( svg_target_laser_t *self ) -> void {
    Super::onSpawn( self );

    // Let everything else get spawned before we start firing.
    self->SetThinkCallback( &svg_target_laser_t::onThink_StartSpawnLaser );
    self->nextthink = level.time + 1_sec;
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_target_laser_t, onThink )( svg_target_laser_t *self ) -> void {
    svg_base_edict_t *ignore;
    vec3_t  start;
    vec3_t  end;
    svg_trace_t tr;
    vec3_t  point;
    vec3_t  last_movedir;
    int32_t count;

    if ( self->spawnflags & 0x80000000 ) {
        count = 8;
    } else {
        count = 4;
    }

    if ( self->enemy ) {
        VectorCopy( self->movedir, last_movedir );
        VectorMA( self->enemy->absmin, 0.5f, self->enemy->size, point );
        VectorSubtract( point, self->s.origin, self->movedir );
        self->movedir = QM_Vector3Normalize( self->movedir );
        if ( !VectorCompare( self->movedir, last_movedir ) )
            self->spawnflags |= 0x80000000;
    }

    ignore = self;
    VectorCopy( self->s.origin, start );
    VectorMA( start, CM_MAX_WORLD_SIZE, self->movedir, end );
    while ( 1 ) {
        tr = SVG_Trace( start, qm_vector3_null, qm_vector3_null, end, ignore, ( CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER ) );

        if ( !tr.ent )
            break;

        // hurt it if we can
        if ( ( tr.ent->takedamage ) && !( tr.ent->flags & FL_IMMUNE_LASER ) )
            SVG_TriggerDamage( tr.ent, self, self->activator, &self->movedir.x, tr.endpos, vec3_origin, self->dmg, 1, DAMAGE_ENERGY, MEANS_OF_DEATH_LASER );

        // if we hit something that's not a monster or player or is immune to lasers, we're done
        if ( !( tr.ent->svflags & SVF_MONSTER ) && ( !tr.ent->client ) ) {
            if ( self->spawnflags & 0x80000000 ) {
                self->spawnflags &= ~0x80000000;
                gi.WriteUint8( svc_temp_entity );
                gi.WriteUint8( TE_LASER_SPARKS );
                gi.WriteUint8( count );
                gi.WritePosition( tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
                gi.WriteDir8( tr.plane.normal );
                gi.WriteUint8( self->s.skinnum );
                gi.multicast( tr.endpos, MULTICAST_PVS, false );
            }
            break;
        }

        ignore = tr.ent;
        VectorCopy( tr.endpos, start );
    }

    VectorCopy( tr.endpos, self->s.old_origin );

    self->nextthink = level.time + FRAME_TIME_S;
}
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_target_laser_t, onThink_StartSpawnLaser )( svg_target_laser_t *self ) -> void {
    svg_base_edict_t *ent;

    self->movetype = MOVETYPE_NONE;
    self->solid = SOLID_NOT;
    self->s.entityType = ET_BEAM;
    self->s.renderfx |= RF_BEAM | RF_TRANSLUCENT;
    self->s.modelindex = 1;         // must be non-zero

    // set the beam diameter
    if ( self->spawnflags & svg_target_laser_t::SPAWNFLAG_THICK ) {
        self->s.frame = 16;
    } else {
        self->s.frame = 4;
    }

    // set the color
    if ( self->spawnflags & svg_target_laser_t::SPAWNFLAG_COLOR_RED ) {
        self->s.skinnum = 0xf2f2f0f0;
    } else if ( self->spawnflags & svg_target_laser_t::SPAWNFLAG_COLOR_GREEN ) {
        self->s.skinnum = 0xd0d1d2d3;
    } else if ( self->spawnflags & svg_target_laser_t::SPAWNFLAG_COLOR_BLUE ) {
        self->s.skinnum = 0xf3f3f1f1;
    } else if ( self->spawnflags & svg_target_laser_t::SPAWNFLAG_COLOR_YELLOW ) {
        self->s.skinnum = 0xdcdddedf;
    } else if ( self->spawnflags & svg_target_laser_t::SPAWNFLAG_COLOR_ORANGE ) {
        self->s.skinnum = 0xe0e1e2e3;
    }

    if ( !self->enemy ) {
        if ( self->targetNames.path ) {
            ent = SVG_Entities_Find( NULL, q_offsetof( svg_base_edict_t, targetname ), (const char *)self->targetNames.path );
            if ( !ent )
                gi.dprintf( "%s at %s: %s is a bad target\n", (const char *)self->classname, vtos( self->s.origin ), (const char *)self->targetNames.path );
            self->enemy = ent;
        }
    }
    SVG_Util_SetMoveDir( self->s.angles, self->movedir );

    self->SetUseCallback( &svg_target_laser_t::onUse );
    self->SetThinkCallback( &svg_target_laser_t::onThink );

    if ( !self->dmg ) {
        self->dmg = 1;
    }

    VectorSet( self->mins, -8, -8, -8 );
    VectorSet( self->maxs, 8, 8, 8 );
    gi.linkentity( self );

    if ( self->spawnflags & svg_target_laser_t::SPAWNFLAG_START_ON ) {
        self->SetActive( true );
    } else {
        self->SetActive( false );
    }
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_target_laser_t, onUse )( svg_target_laser_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    self->activator = activator;
    if ( self->spawnflags & svg_target_laser_t::SPAWNFLAG_START_ON ) {
        self->SetActive( false );
    } else {
        self->SetActive( true );
    }

    SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, self->spawnflags & svg_target_laser_t::SPAWNFLAG_START_ON );
}
/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_TOUCH( svg_target_laser_t, onTouch )( svg_target_laser_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void { }

/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_PAIN( svg_target_laser_t, onPain )( svg_target_laser_t *self, svg_base_edict_t *other, float kick, int damage ) -> void { }
/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_DIE( svg_target_laser_t, onDie )( svg_target_laser_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point ) -> void { }