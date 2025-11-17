/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_relay'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"

#include "svgame/entities/trigger/svg_trigger_relay.h"


/***
*
*
*	trigger_relay
*
*
***/
#if 1
/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_trigger_relay_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_trigger_relay_t, useType, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_trigger_relay_t, useValue, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_trigger_relay_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_trigger_relay_t::Reset( const bool retainDictionary ) {
    // Now, reset derived-class state.
    IMPLEMENT_EDICT_RESET_BY_COPY_ASSIGNMENT( Super, SelfType, retainDictionary );

    #if 0
        // Call upon the base class.
        Super::Reset( retainDictionary );
    #endif
}


/**
*   @brief  Save the entity into a file using game_write_context.
*   @note   Make sure to call the base parent class' Save() function.
**/
void svg_trigger_relay_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_trigger_relay_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_trigger_relay_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_trigger_relay_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_trigger_relay_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    // Ease of use.
    const std::string keyStr = keyValuePair->key;
    // Match: style
    if ( keyStr == "useType" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        useType = static_cast<entity_usetarget_type_t>( keyValuePair->integer );
        return true;
    }
    // Match: count
    else if ( keyStr == "useValue" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        useValue = keyValuePair->integer;
        return true;
    } else {
        return Super::KeyValue( keyValuePair, errorStr );
    }
}
#endif // #if 0
/*QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
This fixed size trigger cannot be touched, it can only be fired by other events.
*/

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_trigger_relay_t, onUse )( svg_trigger_relay_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
	// Here we actually perform the UseTargets with the actual passed in trigger_relay
	// spawn useType/useValue.
	SVG_UseTargets( self, activator, useType, useValue );
}
/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_trigger_relay_t, onSpawn)( svg_trigger_relay_t *self ) -> void {
    // Always spawn Super class.
    Super::onSpawn( self );

    // Does not free itself. Instead construct a 'point bbox'.
    VectorCopy( self->s.origin, self->absMin );
    VectorCopy( self->s.origin, self->absMax );
    self->movetype = MOVETYPE_NONE;
    self->svFlags = SVF_NOCLIENT;

	// Assign a relayed use callback.
	self->SetUseCallback( &svg_trigger_relay_t::onUse );
}