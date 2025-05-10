/********************************************************************
*
*
*	ServerGame: Level Locals Data
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @brief  This structure is cleared as each map is entered.
*           It is read/written to the level.sav file for savegames.
**/
struct svg_level_locals_t {
    static svg_save_descriptor_field_t saveDescriptorFields[];

    uint64_t    frameNumber;
    QMTime		time;

    const cm_entity_t **cm_entities;

    char        level_name[ MAX_QPATH ];  // the descriptive name (Outer Base, etc)
    char        mapname[ MAX_QPATH ];     // the server name (base1, etc)
    char        nextmap[ MAX_QPATH ];     // go here when fraglimit is hit

    // intermission state
    int64_t         intermissionFrameNumber;  // time the intermission was started

    // WID: C++20: Added const.
    const char *changemap;
    int64_t     exitintermission;
    vec3_t      intermission_origin;
    vec3_t      intermission_angle;

    svg_base_edict_t     *sight_client;  // changed once each frame for coop games

    svg_base_edict_t     *sight_entity;
    int64_t		sight_entity_framenum;
    svg_base_edict_t     *sound_entity;
    int64_t		sound_entity_framenum;
    svg_base_edict_t     *sound2_entity;
    int64_t		sound2_entity_framenum;

    svg_base_edict_t *current_entity;    // entity running from SVG_RunFrame
    int         body_que;           // dead bodies
};

//! Extern it.
extern svg_level_locals_t   level;