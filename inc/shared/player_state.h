/********************************************************************
*
*
*   Player State:
*
*
********************************************************************/
#pragma once

// player_state_t->refdef flags
#define RDF_NONE            0       // No specific screen-space flags.
#define RDF_UNDERWATER      1       // Warp the client's screen as apropriate.
#define RDF_NOWORLDMODEL    2       // Used for rendering in the player configuration screen
//ROGUE
#define RDF_IRGOGGLES       4       //! Render IR Goggles.
#define RDF_UVGOGGLES       8       //! Render UV Goggles. ( Not implemented in VKPT. )

// player_state->stats[] static indices that are shared with the engine( required for reasons. )
#define STAT_HEALTH_ICON        0
#define STAT_HEALTH             1
#define STAT_AMMO_ICON          2
#define STAT_AMMO               3
#define STAT_ARMOR_ICON         4
#define STAT_ARMOR              5
#define STAT_SELECTED_ICON      6
#define STAT_PICKUP_ICON        7
#define STAT_PICKUP_STRING      8
#define STAT_TIMER_ICON         9
#define STAT_TIMER              10
#define STAT_HELPICON           11
#define STAT_SELECTED_ITEM      12
#define STAT_LAYOUTS            13
#define STAT_FRAGS              14
#define STAT_FLASHES            15      // cleared each frame, 1 = health, 2 = armor
#define STAT_CHASE              16
#define STAT_SPECTATOR          17
//#define STAT_SHOW_SCORES        19
//! Use this as the start offset for game specific player stats types.
#define STATS_GAME_OFFSET       19

//! Maximum number of stats we compile with.
#define MAX_STATS               64

// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 40(since we're running 40hz) player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates
typedef struct {
    // Communicate BIT precise.
    pmove_state_t   pmove;      // for prediction

    // These fields do not need to be communicated bit-precise
    vec3_t viewangles;		//! For fixed views.
    vec3_t viewoffset;		//! Add to pmovestate->origin
    vec3_t kick_angles;		//! Add to view direction to get render angles
                            //! set by weapon kicks, pain effects, etc

    //! Gun Angles on-screen.
    vec3_t gunangles;
    //! Gun offset on-screen.
    vec3_t gunoffset;
    //! Weapon(gun model) index.
    uint32_t gunindex;
    //! Current weapon model's frame.
    uint32_t gunframe;

    //! hz tickrate for gun logic which is coupled to animation frames.
    int32_t gunrate;

    //float damage_blend[ 4 ];      // rgba full screen damage blend effect
    //! RGBA full screen general blend effect
    float screen_blend[ 4 ];
    //! horizontal field of view
    float fov;

    //! Refdef flags.
    int32_t rdflags;

    //! Numerical player stats storage. ( fast status bar updates etc. )
    int32_t stats[ MAX_STATS ];
} player_state_t;