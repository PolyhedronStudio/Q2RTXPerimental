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
    //! Used for registering the fields which need save and restoring 
    //! of the session's level locals.
    static svg_save_descriptor_field_t saveDescriptorFields[];

    uint64_t    frameNumber = 0;
    QMTime		time = 0_ms;

    float       gravity = 0.f;

    const cm_entity_t **cm_entities = nullptr;

    char        level_name[ MAX_QPATH ] = {};  // the descriptive name (Outer Base, etc)
    char        mapname[ MAX_QPATH ] = {};     // the server name (base1, etc)
    char        nextmap[ MAX_QPATH ] = {};     // go here when fraglimit is hit

    // intermission state
    int64_t         intermissionFrameNumber = 0;  // time the intermission was started

    // WID: C++20: Added const.
    const char *changemap = nullptr;
    int64_t     exitintermission = 0;
    Vector3     intermission_origin = QM_Vector3Zero();
    Vector3     intermission_angle = QM_Vector3Zero();

    svg_base_edict_t     *sight_client = nullptr;  // changed once each frame for coop games

    svg_base_edict_t     *sight_entity = nullptr;
    int64_t		sight_entity_framenum = 0;
    svg_base_edict_t     *sound_entity = nullptr;
    int64_t		sound_entity_framenum = 0;
    svg_base_edict_t     *sound2_entity = nullptr;
    int64_t		sound2_entity_framenum = 0;

    svg_base_edict_t *current_entity = nullptr;   // entity running from SVG_RunFrame
    int32_t     body_que = 0;               // dead bodies
};

//! Extern it.
extern svg_level_locals_t   level;