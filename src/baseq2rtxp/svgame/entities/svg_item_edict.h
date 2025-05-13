/********************************************************************
*
*
*	ServerGame: Base Item Edict
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


// Needed.
#include "svgame/entities/svg_base_edict.h"

/**
*
*
*   Game Pickup Item Entity Structure:
*
*
**/
struct svg_item_edict_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_item_edict_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_item_edict_t( const cm_entity_t *ed ) : Base( ed ) { };
    //! Destructor.
    virtual ~svg_item_edict_t() = default;



    /**
    *
    *	Define this as: "player" = svg_base_edict -> svg_item_edict_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:    classType:         superClassType:
        "svg_item_edict_t", svg_item_edict_t, svg_base_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_item_edict_t::onSpawn
    );



    /**
    *
    *   Save Descriptor Fields:
    *
    **/
    //! Declare the save descriptor field handling function implementations.
    //SVG_SAVE_DESCRIPTOR_FIELDS_DECLARE_IMPLEMENTATION();



    /**
    *
    *   Core:
    *
    **/
    /**
    *   Reconstructs the object, optionally retaining the entityDictionary.
    **/
    virtual void Reset( const bool retainDictionary = false ) override;

    /**
    *   @brief  Save the entity into a file using game_write_context.
    *   @note   Make sure to call the base parent class' Restore() function.
    **/
    virtual void Save( struct game_write_context_t *ctx ) override;
    /**
    *   @brief  Restore the entity from a loadgame read context.
    *   @note   Make sure to call the base parent class' Restore() function.
    **/
    virtual void Restore( struct game_read_context_t *ctx ) override;

    /**
    *   @brief  Called for each cm_entity_t key/value pair for this entity.
    *           If not handled, or unable to be handled by the derived entity type, it will return
    *           set errorStr and return false. True otherwise.
    **/
    virtual const bool KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) override;



    /**
    *
    * 
    *   Item
    *
    * 
    **/
    /**
    *   @brief  Spawn routine.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_item_edict_t, onSpawn );

    /**
    *   @brief  Drop the entity to floor properly and setup its
    *           needed properties.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_item_edict_t, onThink_DropToFloor );
    /**
    *   @brief  Drop the entity to floor properly and setup its
    *           needed properties.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_item_edict_t, onThink_Respawn );
    /**
    *   @brief  Will make the entity touchable again.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_item_edict_t, onTouch_DropMakeTouchable );

    /**
    *   @brief  Callback for when touched, will pickup(remove item from world) the
	*           item and give it to the player if all conditions are met.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_item_edict_t, onTouch );
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_item_edict_t, onTouch_DropTempTouch );

    /**
    *   @brief  Will use the actual item itself.
    **/
	DECLARE_MEMBER_CALLBACK_USE( svg_item_edict_t, onUse_UseItem );

    /**
    *
    *   Member Variables:
    *
    **/

};