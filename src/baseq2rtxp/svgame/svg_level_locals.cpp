/********************************************************************
*
*
*	SVGame: Local "Level" Data Structure Implementation(s).
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_game_locals.h"



/**
*   Save field descriptors for restoring/writing the svg_level_locals_t data to save games:
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_level_locals_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, frameNumber, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, time, SD_FIELD_TYPE_FRAMETIME ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, gravity, SD_FIELD_TYPE_DOUBLE ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_SIZE( svg_level_locals_t, level_name, SD_FIELD_TYPE_ZSTRING, MAX_QPATH ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_SIZE( svg_level_locals_t, mapname, SD_FIELD_TYPE_ZSTRING, MAX_QPATH ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_SIZE( svg_level_locals_t, nextmap, SD_FIELD_TYPE_ZSTRING, MAX_QPATH ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, intermissionFrameNumber, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, changemap, SD_FIELD_TYPE_LSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, exitintermission, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, intermission_origin, SD_FIELD_TYPE_VECTOR3 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, intermission_angle, SD_FIELD_TYPE_VECTOR3 ),
    // Set in-game during the frame, invalid at load/save time.
    //SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sight_client, SD_FIELD_TYPE_EDICT /*SD_FIELD_TYPE_CLIENT*/ ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sight_entity, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sight_entity_framenum, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sound_entity, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sound_entity_framenum, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sound2_entity, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sound2_entity_framenum, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, body_que, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_FIELDS_END();