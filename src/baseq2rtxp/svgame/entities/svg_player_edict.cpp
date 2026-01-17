/********************************************************************
*
*
*	SVGame: Default Player Client Entity. Treated specially by the
*           game's core frame loop.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_misc.h"
#include "svgame/player/svg_player_obituary.h"
#include "svgame/svg_save.h"

#include "sharedgame/sg_entities.h"
#include "sharedgame/pmove/sg_pmove.h"
#include "svgame/entities/svg_item_edict.h"
#include "svgame/entities/svg_player_edict.h"


/**
*
* 
*
*   Save Descriptor Field Setup: svg_player_edict_t
*
* 
*
**/
/**
*   @brief  Save descriptor array definition for all the members of svg_player_edict_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_player_edict_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_player_edict_t, armor, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_player_edict_t, svg_base_edict_t );




/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_player_edict_t::Reset( const bool retainDictionary ) {
    // Now, reset derived-class state.
    IMPLEMENT_EDICT_RESET_BY_COPY_ASSIGNMENT( Super, SelfType, retainDictionary );
    #if 0
    // Call upon the base class.
    Super::Reset( retainDictionary );
    #endif
	// Reset the edict's save descriptor fields.
    armor = 0;
}


/**
*   @brief  Save the entity into a file using game_write_context.
*   @note   Make sure to call the base parent class' Save() function.
**/
void svg_player_edict_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_player_edict_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_player_edict_t::Restore( struct game_read_context_t *ctx ) {
	// Restore parent class fields.
    Super::Restore( ctx );
	// Restore all the members of this entity type.
    ctx->read_fields( svg_player_edict_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_player_edict_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
	return Super::KeyValue( keyValuePair, errorStr );
}



/**
*
*   Player
*
**/
/**
*   @brief
**/
static void LookAtKiller( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker ) {
    vec3_t dir = {};
    float killerYaw = 0.f;

    if ( attacker && attacker != world && attacker != self ) {
        VectorSubtract( attacker->s.origin, self->s.origin, dir );
    } else if ( inflictor && inflictor != world && inflictor != self ) {
        VectorSubtract( inflictor->s.origin, self->s.origin, dir );
    } else {
        self->client->killer_yaw = self->client->ps.stats[ STAT_KILLER_YAW ] = self->s.angles[ YAW ];
        return;
    }

    self->client->killer_yaw = self->client->ps.stats[ STAT_KILLER_YAW ] = QM_Vector3ToYaw( dir );

    //if ( dir[ 0 ] ) {
    //    self->client->killer_yaw = RAD2DEG( atan2( dir[ 1 ], dir[ 0 ] ) );
    //} else {
    //    self->client->killer_yaw = 0;
    //    if ( dir[ 1 ] > 0 ) {
    //        self->client->killer_yaw = 90;
    //    } else if ( dir[ 1 ] < 0 ) {
    //        self->client->killer_yaw = 270; // WID: pitch-fix.
    //    }
    //}
    //if ( self->client->killer_yaw < 0 ) {
    //    self->client->killer_yaw += 360;
    //}
}
/**
*   @brief
**/
static void TossClientWeapon( svg_base_edict_t *self ) {
    if ( !deathmatch->value )
        return;

    // Need to have actual ammo to toss.
    const gitem_t *item = self->client->pers.weapon;
	// Need to have actual ammo to toss.
    if ( !self->client->pers.inventory[ self->client->ammo_index ] ) {
        item = NULL;
    }

    // Can't toss away your fists.
    if ( item && ( strcmp( item->pickup_name, "Fists" ) == 0 ) ) {
        item = NULL;
    }

    //if (item && quad)
    //    spread = 22.5f;
    //else
    float spread = 0.0f;

    if ( item ) {
        self->client->viewMove.viewAngles[ YAW ] -= spread;
        svg_item_edict_t *drop = Drop_Item( self, item );
        self->client->viewMove.viewAngles[ YAW ] += spread;
        drop->spawnflags = DROPPED_PLAYER_ITEM;
    }
}


/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_player_edict_t, onSpawn )( svg_player_edict_t *self ) -> void {
    // Spawn Super class.
    Super::onSpawn( self );
}

/*
==================
player_die
==================
*/
DEFINE_MEMBER_CALLBACK_DIE( svg_player_edict_t, onDie) ( svg_player_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) -> void {
    int     n;

    VectorClear( self->avelocity );

    self->takedamage = DAMAGE_YES;
    self->movetype = MOVETYPE_TOSS;

    // Unset weapon model.
    self->s.modelindex2 = 0;    // remove linked weapon model

    // Clear X and Z angles.
    self->s.angles[ 0 ] = 0;
    self->s.angles[ 2 ] = 0;

    // Stop playing any sounds.
    self->s.sound = 0;
    self->client->weaponState.activeSound = 0;
    
    // Set bbox maxs to PM_BBOX_DUCKED_MAXS.
    self->maxs[ 2 ] = PM_BBOX_DUCKED_MAXS.z;

    //  self->solid = SOLID_NOT;
        // Flag as to be treated as 'deadmonster' collision.
    self->svFlags |= SVF_DEADENTITY;

    if ( !self->lifeStatus ) {
        // Determine respawn time.
        self->client->respawn_time = ( level.time + 1_sec );
        // Make sure the playerstate its pmove knows we're dead.
        self->client->ps.pmove.pm_type = PM_DEAD;
        // Set the look at killer yaw.
        LookAtKiller( self, inflictor, attacker );
        // Notify the obituary.
        SVG_Player_Obituary( self, inflictor, attacker );
        // Toss away weaponry.
        TossClientWeapon( self );

        // clear inventory
        // this is kind of ugly, but it's how we want to handle keys in coop
        //for (n = 0; n < game.num_items; n++) {
        //    if (coop->value && itemlist[n].flags & IT_KEY)
        //        self->client->resp.pers_respawn.inventory[n] = self->client->pers.inventory[n];
        //    self->client->pers.inventory[n] = 0;
        //}
    }

    // Gib Death:
    if ( self->health < GIB_DEATH_HEALTH ) {
        // Play gib sound.
        //gi.sound( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), 1, ATTN_NORM, 0 );
		SVG_EntityEvent_GeneralSoundEx( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), ATTN_NORM );

        //! Throw 4 small meat gibs around.
        for ( n = 0; n < 4; n++ ) {
            SVG_Misc_ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_TYPE_ORGANIC );
        }
        // Turn ourself into the thrown head entity.
        SVG_Misc_ThrowClientHead( self, damage );

        // Gibs don't take damage, but fade away as time passes.
        self->takedamage = DAMAGE_NO;
        // Normal death:
    } else {
        if ( !self->lifeStatus ) {
            static int i;

            gi.dprintf( "%s: WID: TODO: Implement a player death player animation here\n", __func__ );

            //i = (i + 1) % 3;
            //// start a death animation
            //self->client->anim_priority = ANIM_DEATH;
            //if (self->client->ps.pmove.pm_flags & PMF_DUCKED) {
            //    self->s.frame = FRAME_crdeath1 - 1;
            //    self->client->anim_end = FRAME_crdeath5;
            //} else switch (i) {
            //    case 0:
            //        self->s.frame = FRAME_death101 - 1;
            //        self->client->anim_end = FRAME_death106;
            //        break;
            //    case 1:
            //        self->s.frame = FRAME_death201 - 1;
            //        self->client->anim_end = FRAME_death206;
            //        break;
            //    case 2:
            //        self->s.frame = FRAME_death301 - 1;
            //        self->client->anim_end = FRAME_death308;
            //        break;
            //    }
            //gi.sound(self, CHAN_VOICE, gi.soundindex(va("*death%i.wav", (Q_rand() % 4) + 1)), 1, ATTN_NORM, 0);
            //gi.sound( self, CHAN_VOICE, gi.soundindex( va( "player/death0%i.wav", ( irandom( 0, 4 ) ) + 1 ) ), 1, ATTN_NORM, 0 );
			SVG_EntityEvent_GeneralSoundEx( self, CHAN_VOICE, gi.soundindex( va( "player/death0%i.wav", ( irandom( 0, 4 ) ) + 1 ) ), ATTN_NORM );
        }
    }

    self->lifeStatus = LIFESTATUS_DEAD;

    gi.linkentity( self );
}
/**
*   @brief  Player pain is handled at the end of the frame in P_DamageFeedback.
**/
DEFINE_MEMBER_CALLBACK_PAIN( svg_player_edict_t, onPain)( svg_player_edict_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void {
    // Player pain is handled at the end of the frame in P_DamageFeedback.
}