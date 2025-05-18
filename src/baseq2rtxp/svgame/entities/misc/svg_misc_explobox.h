/********************************************************************
*
*
*	ServerGame: Misc Entity 'misc_explobox'.
*
*
********************************************************************/
#pragma once

// Needed.
#include "svgame/entities/svg_base_edict.h"


/**
*
*
*   Explosive Barrel Entity Structure:
*
*
**/
struct svg_misc_explobox_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_misc_explobox_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_misc_explobox_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_misc_explobox_t() = default;


    /**
    *
    *	Define this as: "misc_explobox" = svg_base_edict -> svg_misc_explobox_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "misc_explobox", svg_misc_explobox_t, svg_base_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_misc_explobox_t::onSpawn
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
    *   TestDummy Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_misc_explobox_t, onSpawn );
    /**
    *   @brief  Post-Spawn.
    **/
    //DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_misc_explobox_t, onPostSpawn );
    /**
    *   @brief  Thinking.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_misc_explobox_t, onThink );
    DECLARE_MEMBER_CALLBACK_THINK( svg_misc_explobox_t, thinkExplode );

    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_misc_explobox_t, onTouch );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_USE( svg_misc_explobox_t, onUse );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_PAIN( svg_misc_explobox_t, onPain );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_DIE( svg_misc_explobox_t, onDie );



    /**
    *
    *   Member Variables:
    *
    **/

};