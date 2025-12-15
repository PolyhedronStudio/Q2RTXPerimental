/********************************************************************
*
*
*	SVGame: Local "Game" Data Structure Implementation(s).
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_game_locals.h"



/**
*   Save field descriptors for restoring/writing the svg_level_locals_t data to save games:
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_game_locals_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_game_locals_t, maxclients, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_game_locals_t, maxentities, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_game_locals_t, modeType, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_game_locals_t, serverflags, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_game_locals_t, num_items, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_game_locals_t, autosaved, SD_FIELD_TYPE_BOOL ),
SAVE_DESCRIPTOR_FIELDS_END();