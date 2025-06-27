/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door_rotating_rotating'.
*
*
********************************************************************/
#pragma once


/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door_rotating'.
*
*
********************************************************************/
#pragma once



// Needed.
#include "svgame/entities/svg_base_edict.h"
#include "svgame/entities/svg_pushmove_edict.h"
#include "svgame/entities/func/svg_func_door.h"


/**
*
*
*   Rotating Door Entity Structure:
*
*
**/
struct svg_func_door_rotating_t : public svg_func_door_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_door_rotating_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_door_rotating_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_door_rotating_t() = default;


    /**
    *
    *	Define this as: "func_door_rotating" = svg_base_edict_t -> svg_pushmove_edict_t -> svg_func_door_t -> svg_func_dor_rotating_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_door_rotating", svg_func_door_rotating_t, svg_func_door_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_door_rotating_t::onSpawn
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
    *   Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_door_rotating_t, onSpawn );
    #if 0
    /**
    *   @brief  Post-Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_func_door_rotating_t, onPostSpawn );
    /**
    *   @brief  Thinking.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_door_rotating_t, onThink );
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_door_rotating_t, onThink_OpenMove );
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_door_rotating_t, onThink_CloseMove );

    /**
    *   @brief  Blocked.
    **/
    DECLARE_MEMBER_CALLBACK_BLOCKED( svg_func_door_rotating_t, onBlocked );

    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_func_door_rotating_t, onTouch );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_func_door_rotating_t, onUse );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_USE( svg_func_door_rotating_t, onUse_AreaPortal );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_PAIN( svg_func_door_rotating_t, onPain );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_DIE( svg_func_door_rotating_t, onDie );
    /**
    *   @brief  Signal Receiving:
    **/
    DECLARE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_door_rotating_t, onSignalIn );

    /**
    *   PushMoveInfo EndMoves:
    **/
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_base_edict_t, onCloseEndMove );
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_base_edict_t, onOpenEndMove );
    #endif

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
};