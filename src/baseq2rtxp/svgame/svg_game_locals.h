/********************************************************************
*
*
*	ServerGame: Game Locals Data
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



// game.serverflags values
typedef enum crosslevel_target_flags_e {
    SFL_CROSS_TRIGGER_NONE = 0,

    SFL_CROSS_TRIGGER_1 = BIT( 0 ),
    SFL_CROSS_TRIGGER_2 = BIT( 1 ),
    SFL_CROSS_TRIGGER_3 = BIT( 2 ),
    SFL_CROSS_TRIGGER_4 = BIT( 3 ),
    SFL_CROSS_TRIGGER_5 = BIT( 4 ),
    SFL_CROSS_TRIGGER_6 = BIT( 5 ),
    SFL_CROSS_TRIGGER_7 = BIT( 6 ),
    SFL_CROSS_TRIGGER_8 = BIT( 7 ),
    //! Mask
    SFL_CROSS_TRIGGER_MASK = 255
} crosslevel_target_flags_t;
// Bit Operators:
QENUM_BIT_FLAGS( crosslevel_target_flags_t );


/**
*   @brief  This structure is left intact through an entire game
*           it should be initialized at dll load time, and read/written to
*           the server.ssv file for savegames
**/
struct svg_game_locals_t {
    /**
    *   Used for registering the fields which need save and restoring of the session's level locals.
    **/
    static svg_save_descriptor_field_t saveDescriptorFields[];

    //! [maxclients] sized array of client pointers.
    svg_client_t *clients = nullptr;
    //! Can't store spawnpoint in level, because it would get overwritten by the savegame restore which is needed for coop respawns.
    char        spawnpoint[ 512 ] = {};

    //! Store latched cvars here that we want to get at often.
    int32_t             maxclients = 0;
    int32_t             maxentities = 0;

    //! The gamemode type.
    sg_gamemode_type_t  modeType = ( sg_gamemode_type_t )0;
    //! The gamemode type matching object.
    svg_gamemode_t *mode = nullptr;

    //! Cross level triggers.
    crosslevel_target_flags_t serverflags = SFL_CROSS_TRIGGER_NONE;

    //! Number of items.
    int32_t     num_items = 0;
    //! Autosaved.
    bool        autosaved = false;

	//! The current view serverframe player edict.
    svg_player_edict_t *currentViewPlayer = nullptr;
	//! And the current view serverframe client.
    svg_client_t *currentViewClient = nullptr;

    /**
    *   Information for PUSH/STOP -movers.
    *
    *   We use numbers so we can fetch entities in-frame ensuring that we always have valid pointers.
    **/
    int32_t num_movewithEntityStates = 0;
    struct game_locals_movewith_t {
        //! The child entity that has to move with its parent entity.
        int32_t childNumber = 0;
        //! The parent entity that has to move its child entity.
        int32_t parentNumber = 0;
    } *moveWithEntities = {};
};

//! Extern, access all over game dll code.
extern svg_game_locals_t    game;