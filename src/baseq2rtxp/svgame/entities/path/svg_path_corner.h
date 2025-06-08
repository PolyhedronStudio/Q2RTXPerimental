/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_train'.
*
*
********************************************************************/
#pragma once


/**
*	@brief	For trigger_elevator.
**/
void train_resume( svg_base_edict_t *self );

/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_train'.
*
*
********************************************************************/
#pragma once



// Needed.
#include "svgame/entities/svg_base_edict.h"
#include "svgame/entities/svg_pushmove_edict.h"



/**
*
*
*   Path Corner Entity Structure:
*
*
**/
struct svg_path_corner_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_path_corner_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_path_corner_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_path_corner_t() = default;


    /**
    *
    *	Define this as: "path_corner" = svg_base_edict_t -> svg_path_corner_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "path_corner", svg_path_corner_t, svg_base_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_path_corner_t::onSpawn
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
    //virtual const bool KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) override;
    #endif // #if 0


    /**
    *
    *   Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_path_corner_t, onSpawn );
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_path_corner_t, onPostSpawn );
    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_path_corner_t, onTouch );



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
    //! An index to the entity number of the platform
    int32_t platformEntityNumber = -1;
};
