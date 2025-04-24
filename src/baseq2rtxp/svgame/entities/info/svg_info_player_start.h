/********************************************************************
*
*
*	ServerGame: Info Player Start Edicts
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


// Needed for save descriptor fields and functions.
#include "svgame/svg_save.h"

// Needed for inheritance.
#include "svgame/entities/svg_base_edict.h"

// Forward declare.
typedef struct cm_entity_s cm_entity_t;


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
    svg_info_player_base_start_t( const cm_entity_t *ed ) : Base( ed ) { };
    //! Destructor.
    virtual ~svg_info_player_base_start_t() = default;



    /**
    *
    *	Define this as an abstract base class for info_player_start types.
    *
    **/
    DefineAbstractClass( svg_info_player_base_start_t, svg_base_edict_t );



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
    // This entity has no special save descriptor fields to save, so we don't need this.
    #if 0
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
    *   info_player_start:
    *
    **/
    /**
    *   @brief  Spawn routine.
    **/
    static void info_player_start_base_spawn( svg_info_player_base_start_t *self ) { };



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
    svg_info_player_intermission_t( const cm_entity_t *ed ) : Base( ed ) { };
    //! Destructor.
    virtual ~svg_info_player_intermission_t() = default;

    /**
    *
    *	Define this as: "worldspawn" = svg_base_edict -> svg_worldspawn_edict_t
    *
    **/
    DefineWorldSpawnClass( "info_player_intermission", svg_info_player_intermission_t, svg_info_player_base_start_t, EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, svg_info_player_intermission_t::info_player_intermission_spawn );

    /**
    *
    *   info_player_start:
    *
    **/
    /**
    *   @brief  Spawn routine.
    **/
    static void info_player_intermission_spawn( svg_info_player_intermission_t *self );
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
    svg_info_player_start_t( const cm_entity_t *ed ) : Base( ed ) { };
    //! Destructor.
    virtual ~svg_info_player_start_t() = default;



    /**
    *
    *	WorldSpawn Class Definition:
    *
    **/
    DefineWorldSpawnClass(
        // classname:    classType:         superClassType:
        "info_player_start", svg_info_player_start_t, svg_info_player_base_start_t, 
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, 
        // spawnFunc:
        svg_info_player_start_t::info_player_start_spawn 
    );

    /**
    *
    *   info_player_start:
    *
    **/
    /**
    *   @brief  Spawn routine.
    **/
    static void info_player_start_spawn( svg_info_player_start_t *self );
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
    svg_info_player_coop_t( const cm_entity_t *ed ) : Base( ed ) { };
    //! Destructor.
    virtual ~svg_info_player_coop_t() = default;

    /**
    *
    *	Define this as: "worldspawn" = svg_base_edict -> svg_worldspawn_edict_t
    *
    **/
    DefineWorldSpawnClass( "info_player_coop", svg_info_player_coop_t, svg_info_player_base_start_t, EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, svg_info_player_coop_t::info_player_coop_spawn );

    /**
    *
    *   info_player_start:
    *
    **/
    /**
    *   @brief  Spawn routine.
    **/
    static void info_player_coop_spawn( svg_info_player_coop_t *self );
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
    svg_info_player_deathmatch_t( const cm_entity_t *ed ) : Base( ed ) { };
    //! Destructor.
    virtual ~svg_info_player_deathmatch_t() = default;

    /**
    *
    *	Define this as: "worldspawn" = svg_base_edict -> svg_worldspawn_edict_t
    *
    **/
    DefineWorldSpawnClass( "info_player_deathmatch", svg_info_player_deathmatch_t, svg_info_player_base_start_t, EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, svg_info_player_deathmatch_t::info_player_deathmatch_spawn );

    /**
    *
    *   info_player_start:
    *
    **/
    /**
    *   @brief  Spawn routine.
    **/
    static void info_player_deathmatch_spawn( svg_info_player_deathmatch_t *self );
};


