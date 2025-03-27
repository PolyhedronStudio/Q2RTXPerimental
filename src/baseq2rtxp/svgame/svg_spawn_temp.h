/********************************************************************
*
*
*	ServerGame: SpawnTemp - TODO: Get rid of needing this.
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @brief  spawn_temp_t is only used to hold entity field values that
*           can be set from the editor, but aren't actualy present
*           in svg_entity_t during gameplay
**/
typedef struct {
    // world vars
    char *sky;
    float       skyrotate;
    int         skyautorotate;
    vec3_t      skyaxis;
    char        *nextmap;
    char        *musictrack;

    int         lip;
    int         distance;
    int         height;
    // WID: C++20: Added const.
    const char *noise;
    float       pausetime;
    char        *item;
    char        *gravity;

    float       minyaw;
    float       maxyaw;
    float       minpitch;
    float       maxpitch;
} spawn_temp_t;