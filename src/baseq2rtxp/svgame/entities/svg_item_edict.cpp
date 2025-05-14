/********************************************************************
*
*
*	SVGame: Default Player Client Entity. Treated specially by the
*           game's core frame loop.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_utils.h"
#include "svgame/svg_save.h"
#include "svgame/svg_game_items.h"
#include "svgame/svg_trigger.h"
#include "svgame/entities/svg_item_edict.h"
#include "sharedgame/sg_entity_effects.h"

/**
*
*
*
*   Save Descriptor Field Setup: svg_item_edict_t
*
*
*
**/
#if 0
/**
*   @brief  Save descriptor array definition for all the members of svg_item_edict_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_item_edict_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_item_edict_t, testVar, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_item_edict_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_item_edict_t, svg_base_edict_t );
#endif



/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_item_edict_t::Reset( const bool retainDictionary ) {
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
void svg_item_edict_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_item_edict_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_item_edict_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_item_edict_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_item_edict_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    return Super::KeyValue( keyValuePair, errorStr );
}



/**
*
* 
* 
*   Item
* 
* 
*
**/
/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_item_edict_t, onSpawn )( svg_item_edict_t *self ) -> void {
    // Precache the item data.
    SVG_PrecacheItem( self->item );

    // Setup the next think so the item spawns by dropping to the floor.
    // We don't do this right away since other entities might still spawn in its
    // place if we actually did.
    self->nextthink = level.time + 20_hz;    // items start after other solids
    self->SetThinkCallback( &svg_item_edict_t::onThink_DropToFloor );
    // Setup meta and visual properties.
    self->s.effects = self->item->world_model_flags;
    self->s.renderfx = RF_GLOW;
    self->s.entityType = ET_ITEM;
    if ( self->model ) {
        gi.modelindex( self->model );
    }
}
/**
*   @brief  Drop the entity to floor properly and setup its
*           needed properties.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_item_edict_t, onThink_DropToFloor )( svg_item_edict_t *self ) -> void {
    svg_trace_t     tr;
    vec3_t      dest;

    // First set model.
    if ( self->model ) {
        gi.setmodel( self, self->model );
    } else if ( self->item && self->item->world_model ) {
        gi.setmodel( self, self->item->world_model );
    }

    // Then the bbox.
    if ( self->item && self->item->tag >= ITEM_TAG_AMMO_BULLETS_PISTOL && self->item->tag <= ITEM_TAG_AMMO_SHELLS_SHOTGUN ) {
        VectorSet( self->mins, -8, -8, -8 );
        VectorSet( self->maxs, 8, 8, 8 );
    } else if ( self->item ) {
        VectorSet( self->mins, -16, -16, -16 );
        VectorSet( self->maxs, 16, 16, 16 );
    }

    self->solid = SOLID_TRIGGER;
    self->movetype = MOVETYPE_TOSS;
    self->SetTouchCallback( &svg_item_edict_t::onTouch );

    VectorCopy( self->s.origin, dest );
    dest[ 2 ] -= 128;

    tr = SVG_Trace( self->s.origin, self->mins, self->maxs, dest, self, CM_CONTENTMASK_SOLID );
    if ( tr.startsolid ) {
        gi.dprintf( "droptofloor: %s startsolid at %s\n", (const char *)self->classname, vtos( self->s.origin ) );
        SVG_FreeEdict( self );
        return;
    }

    VectorCopy( tr.endpos, self->s.origin );

    if ( self->targetNames.team ) {
        self->flags = static_cast<entity_flags_t>( self->flags & ~FL_TEAMSLAVE );
        self->chain = self->teamchain;
        self->teamchain = NULL;

        self->svflags |= SVF_NOCLIENT;
        self->solid = SOLID_NOT;
        if ( self == self->teammaster ) {
            self->nextthink = level.time + 10_hz;
            self->SetThinkCallback( &svg_item_edict_t::onThink_Respawn );
        }
    }

    if ( self->spawnflags & ITEM_NO_TOUCH ) {
        self->solid = SOLID_BOUNDS_BOX;
        self->SetTouchCallback( nullptr );
        self->s.effects &= ~EF_ROTATE;
        self->s.renderfx &= ~RF_GLOW;
    }

    if ( self->spawnflags & ITEM_TRIGGER_SPAWN ) {
        self->svflags |= SVF_NOCLIENT;
        self->solid = SOLID_NOT;
        self->SetUseCallback( &svg_item_edict_t::onUse_UseItem );
    }

    gi.linkentity( self );
}

/**
*   @brief  Drop the entity to floor properly and setup its
*           needed properties.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_item_edict_t, onThink_Respawn )( svg_item_edict_t *ent ) -> void {
    if ( ent->targetNames.team ) {
        svg_base_edict_t *master;
        int count;
        int choice;

        master = ent->teammaster;

        svg_base_edict_t *slave = master;

        for ( count = 0, slave = master; slave; slave = ( slave->chain), count++ )
            ;

        choice = Q_rand_uniform( count );

        for ( count = 0, slave = master; count < choice; slave = slave->chain, count++ )
            ;
    }

    ent->svflags &= ~SVF_NOCLIENT;
    ent->solid = SOLID_TRIGGER;
    ent->s.entityType = ET_ITEM;
    gi.linkentity( ent );

    // send an effect
    ent->s.event = EV_ITEM_RESPAWN;
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_item_edict_t, onTouch_DropTempTouch )( svg_item_edict_t *ent, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
	// Avoid touching the owner.
    if ( other == ent->owner ) {
        return;
    }

    // Dispatch touch callback.
    ent->DispatchTouchCallback( other, plane, surf );
    //Touch_Item( ent, other, plane, surf );
}
/**
*   @brief  Callback for when touched, will pickup(remove item from world) the
*           item and give it to the player if all conditions are met.
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_item_edict_t, onTouch )( svg_item_edict_t *ent, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
    bool    taken;

    if ( !other->client ) {
        return;
    }
    if ( other->health < 1 ) {
        return;     // dead people can't pickup
    }
    if ( !ent->item->pickup ) {
        return;     // not a grabbable item?
    }

    taken = ent->item->pickup( ent, other );

    if ( taken ) {
        // flash the screen
        other->client->bonus_alpha = 0.25f;

        // show icon and name on status bar
        other->client->ps.stats[ STAT_PICKUP_ICON ] = gi.imageindex( ent->item->icon );
        other->client->ps.stats[ STAT_PICKUP_STRING ] = CS_ITEMS + ITEM_INDEX( ent->item );
        other->client->pickup_msg_time = level.time + 3_sec;

        // change selected item
        if ( ent->item->use ) {
            other->client->pers.selected_item = other->client->ps.stats[ STAT_SELECTED_ITEM ] = ITEM_INDEX( ent->item );
        }

        #if 0
        if ( ent->item->pickup == Pickup_Health ) {
            if ( ent->count == 2 )
                gi.sound( other, CHAN_ITEM, gi.soundindex( "items/s_health.wav" ), 1, ATTN_NORM, 0 );
            else if ( ent->count == 10 )
                gi.sound( other, CHAN_ITEM, gi.soundindex( "items/n_health.wav" ), 1, ATTN_NORM, 0 );
            else if ( ent->count == 25 )
                gi.sound( other, CHAN_ITEM, gi.soundindex( "items/l_health.wav" ), 1, ATTN_NORM, 0 );
            else // (ent->count == 100)
                gi.sound( other, CHAN_ITEM, gi.soundindex( "items/m_health.wav" ), 1, ATTN_NORM, 0 );
        } else if ( ent->item->pickup_sound ) {
        
            gi.sound( other, CHAN_ITEM, gi.soundindex( ent->item->pickup_sound ), 1, ATTN_NORM, 0 );
        }
        #else
        gi.sound( other, CHAN_ITEM, gi.soundindex( ent->item->pickup_sound ), 1, ATTN_NORM, 0 );
        #endif // #if 0
    }

    if ( !( ent->spawnflags & ITEM_TARGETS_USED ) ) {
        SVG_UseTargets( ent, other );
        ent->spawnflags |= ITEM_TARGETS_USED;
    }

    if ( !taken ) {
        return;
    }

    if ( !( ( coop->value ) && ( ent->item->flags & ITEM_FLAG_STAY_COOP ) ) || ( ent->spawnflags & ( DROPPED_ITEM | DROPPED_PLAYER_ITEM ) ) ) {
        if ( ent->flags & FL_RESPAWN ) {
            ent->flags = static_cast<entity_flags_t>( ent->flags & ~FL_RESPAWN );
        } else {
            SVG_FreeEdict( ent );
        }
    }
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_item_edict_t, onTouch_DropMakeTouchable )( svg_item_edict_t *self ) -> void {
    self->SetTouchCallback( &svg_item_edict_t::onTouch );
    if ( deathmatch->value ) {
        self->nextthink = level.time + 29_sec;
        self->SetThinkCallback( SVG_FreeEdict );
    }
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_item_edict_t, onUse_UseItem ) ( svg_item_edict_t *ent, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    ent->svflags &= ~SVF_NOCLIENT;
    ent->SetUseCallback( nullptr );

    if ( ent->spawnflags & ITEM_NO_TOUCH ) {
        ent->solid = SOLID_BOUNDS_BOX;
        ent->SetTouchCallback( nullptr );
    } else {
        ent->solid = SOLID_TRIGGER;
        ent->SetTouchCallback( &svg_item_edict_t::onTouch );
    }

    gi.linkentity( ent );
}