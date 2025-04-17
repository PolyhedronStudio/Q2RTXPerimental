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
typedef struct {
    //! [maxclients] of client pointers.
    svg_client_t *clients;

    //! Can't store spawnpoint in level, because
    //! it would get overwritten by the savegame restore.
    //!
    //! Needed for coop respawns.
    char        spawnpoint[ 512 ];

    //! Store latched cvars here that we want to get at often.
    int32_t             maxclients;
    int32_t             maxentities;
    sg_gamemode_type_t  gamemode;

    //! Cross level triggers.
    crosslevel_target_flags_t     serverflags;
    //! Number of items.
    int32_t     num_items;
    //! Autosaved.
    bool        autosaved;

    /**
    *   Information for PUSH/STOP -movers.
    *
    *   We use numbers so we can fetch entities in-frame ensuring that we always have valid pointers.
    **/
    int32_t num_movewithEntityStates;
    struct game_locals_movewith_t {
        //! The child entity that has to move with its parent entity.
        int32_t childNumber;
        //! The parent entity that has to move its child entity.
        int32_t parentNumber;
    } *moveWithEntities;
} game_locals_t;