/********************************************************************
*
*
*	ServerGame: Info Player Start Edicts
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
*   Player Start Game Entity Structures:
*
*
**/
// This inherits from:
//
//struct svg_base_edict_t : public sv_shared_edict_t<svg_base_edict_t, svg_client_t>
struct svg_info_player_base_start_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_info_player_base_start_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_info_player_base_start_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_info_player_base_start_t() = default;



    /**
    *
    *	Define this as an abstract base class for info_player_start types.
    *
    **/
    DefineAbstractClass( svg_info_player_base_start_t, svg_base_edict_t );

    // This entity has no special save descriptor fields to save, so we don't need this.
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
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_info_player_base_start_t, onSpawn );



    /**
    *
    *
    *   Member Variables:
    *
    *
    **/
    // None.
};

/**
*   @brief  Camera position and angles for use when in 'intermission' mode.
*           (e.g. when the game is over, or when a player is in the 'intermission room'.)
**/
struct svg_info_player_intermission_t : public svg_info_player_base_start_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_info_player_intermission_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_info_player_intermission_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_info_player_intermission_t() = default;

    /**
    *
    *	Define this as: "info_player_intermission".
    *
    **/
    DefineWorldSpawnClass( 
        // classname:               classType:                      superClassType:
        "info_player_intermission", svg_info_player_intermission_t, svg_info_player_base_start_t, 
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, 
        // spawnFunc:
        svg_info_player_intermission_t::onSpawn
    );

    /**
    *
    *   Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_info_player_intermission_t, onSpawn );
};

/**
*   @brief  Generic player spawn point, used for Singleplayer games.
**/
struct svg_info_player_start_t : public svg_info_player_base_start_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_info_player_start_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_info_player_start_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_info_player_start_t() = default;



    /**
    *
    *	Define this as: "svg_info_player_start".
    *
    **/
    DefineWorldSpawnClass(
        // classname:    classType:         superClassType:
        "info_player_start", svg_info_player_start_t, svg_info_player_base_start_t, 
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, 
        // spawnFunc:
        svg_info_player_start_t::onSpawn 
    );

    /**
    *
    *   Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_info_player_start_t, onSpawn );
};

/**
*   @brief  Coop player spawn point, used for duh, Coop games.
**/
struct svg_info_player_coop_t : public svg_info_player_base_start_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_info_player_coop_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_info_player_coop_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_info_player_coop_t() = default;

    /**
    *
    *	Define this as: "svg_info_player_coop".
    *
    **/
    DefineWorldSpawnClass( 
        // classname:    classType:         superClassType:
        "info_player_coop", svg_info_player_coop_t, svg_info_player_base_start_t, 
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, 
        // spawnFunc:
        svg_info_player_coop_t::onSpawn );

    /**
    *
    *   Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_info_player_coop_t, onSpawn );
};

/**
*   @brief  DeathMatch player spawn point, used for duh, DeathMatchgames.
**/
struct svg_info_player_deathmatch_t : public svg_info_player_base_start_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_info_player_deathmatch_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_info_player_deathmatch_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_info_player_deathmatch_t() = default;

    /**
    *
    *	Define this as: "svg_info_player_deathmatch".
    *
    **/
    DefineWorldSpawnClass( 
        // classname:             classType:                    superClassType:
        "info_player_deathmatch", svg_info_player_deathmatch_t, svg_info_player_base_start_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, 
        // spawnFunc:
        svg_info_player_deathmatch_t::onSpawn
    );

    /**
    *
    *   Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_info_player_deathmatch_t, onSpawn );
};


