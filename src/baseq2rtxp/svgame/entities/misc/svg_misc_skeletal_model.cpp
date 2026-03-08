/********************************************************************
*
*
*	ServerGame: Misc Entity 'misc_skeletal_model'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/entities/misc/svg_misc_skeletal_model.h"

#include "sharedgame/sg_means_of_death.h"


#if 1
/**
*   @brief  Save descriptor array definition for all the members of svg_misc_skeletal_model_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_misc_skeletal_model_t )
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_skeletal_model_t, modelAction, SD_FIELD_TYPE_LEVEL_QSTRING),
    //SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_skeletal_model_t, summedDistanceTraversed, SD_FIELD_TYPE_DOUBLE ),
    //SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_skeletal_model_t, testVar, SD_FIELD_TYPE_INT32 ),
    //SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_skeletal_model_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_misc_skeletal_model_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_misc_skeletal_model_t::Reset( const bool retainDictionary ) {
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
void svg_misc_skeletal_model_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_misc_skeletal_model_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_misc_skeletal_model_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_misc_skeletal_model_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_misc_skeletal_model_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
	// Ease of use.
	const std::string keyStr = keyValuePair->key;
	// Match: action
	if ( keyStr == "action" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
		// Assign the model action.
		modelAction = svg_level_qstring_t::from_char_str( keyValuePair->nullable_string );
		return true;
	// Match: angle_falloff
	} else {
		return Super::KeyValue( keyValuePair, errorStr );
	}
}
#endif // #if 0



/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_misc_skeletal_model_t, onTouch )( svg_misc_skeletal_model_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {

}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_misc_skeletal_model_t, thinkExplode )( svg_misc_skeletal_model_t *self ) -> void {

}


/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_DIE( svg_misc_skeletal_model_t, onDie )( svg_misc_skeletal_model_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) -> void {
    self->takedamage = DAMAGE_NO;
    self->nextthink = level.time + random_time( 150_ms );
    self->SetThinkCallback( &svg_misc_skeletal_model_t::thinkExplode );
    self->activator = attacker;
}

/**
*   @brief  
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_misc_skeletal_model_t, onSpawn) ( svg_misc_skeletal_model_t *self ) -> void {
    // Always spawn Super class.
    Super::onSpawn( self );

    self->solid = SOLID_BOUNDS_BOX;
    self->movetype = MOVETYPE_ROOTMOTION;

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

    self->SetDieCallback( &svg_misc_skeletal_model_t::onDie );
    self->takedamage = DAMAGE_YES;
    // Pain:
    self->SetPainCallback( &svg_misc_skeletal_model_t::onPain );
    //self->monsterinfo.aiflags = AI_NOSTEP;

    self->SetTouchCallback( &svg_misc_skeletal_model_t::onTouch );

    self->SetThinkCallback( M_droptofloor );
    self->nextthink = level.time + 20_hz;

    gi.linkentity( self );
}

/**
*   @brief  Death routine.
**/
DEFINE_MEMBER_CALLBACK_PAIN( svg_misc_skeletal_model_t, onPain )( svg_misc_skeletal_model_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void {

}