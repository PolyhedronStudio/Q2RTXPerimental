/********************************************************************
*
*
*	ServerGame: Entity 'func_areaportal'.
* 
*   This is a non-visible object that divides the world into seperated 
*   when this portal is not activated. Usually enclosed in the middle 
*   of a door.
*
*
********************************************************************/
#pragma once



// Needed.
#include "svgame/entities/svg_base_edict.h"


/**
*
*
*   AreaPortal entity for.. AreaPortals.
*
*
**/
struct svg_func_areaportal_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_areaportal_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_areaportal_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_areaportal_t() = default;


    /**
    *
    *	Define this as: "func_areaportal" = svg_base_edict_t -> svg_pushmove_edict_t -> svg_func_door_t -> svg_func_areaportal_t.
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_areaportal", svg_func_areaportal_t, svg_base_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_areaportal_t::onSpawn
    );


    #if 0
    /**
    *
    *   Save Descriptor Fields:
    *
    **/
    //! Declare the save descriptor field handling function implementations.
    SVG_SAVE_DESCRIPTOR_FIELDS_DECLARE_IMPLEMENTATION();



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
    #endif // #if 0


    /**
    *
    *   AreaPortal Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_areaportal_t, onSpawn );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_func_areaportal_t, onUse );


    /**
    *
    *   Member Functions:
    *
    **/



    /**
    *
    *   Member Variables:
    *
    **/



    /**
    *   Spawnflags:
    **/

};