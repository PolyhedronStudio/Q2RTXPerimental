/********************************************************************
*
*
*   Temporar Entity Effects:
*
*
********************************************************************/
#pragma once


/**
*
*	Temp Entity Events:
*
*   Temp entity events are for things that happen at a location seperate from any existing entity.
*   Temporary entity messages are explicitly constructed and broadcast.
*
**/
// temp entity events
typedef enum temp_entity_event_e {
    TE_GUNSHOT,
    // TE_OTHER_TYPE_GUNSHOT

    TE_BLOOD,
    TE_MOREBLOOD,

    //TE_EXPLOSION1,
    //TE_EXPLOSION2,
    //TE_ROCKET_EXPLOSION,
    //TE_GRENADE_EXPLOSION,

    //TE_PISTOL_LASER,

    TE_BUBBLETRAIL,
    TE_BUBBLETRAIL2,
    //! A bullet hitting water.
    TE_SPLASH,

    TE_STEAM,
    TE_HEATBEAM_STEAM,

    TE_SPARKS,
    TE_HEATBEAM_SPARKS,
    TE_BULLET_SPARKS,
    TE_ELECTRIC_SPARKS,
    TE_LASER_SPARKS,
    TE_TUNNEL_SPARKS,
    TE_WELDING_SPARKS,

    TE_FLAME,

    TE_PLAIN_EXPLOSION,
    //TE_EXPLOSION1_BIG,
    //TE_EXPLOSION1_NP,
    //TE_ROCKET_EXPLOSION_WATER,
    //TE_GRENADE_EXPLOSION_WATER,
    //
    //TE_NUKEBLAST,

    TE_TELEPORT_EFFECT,

    //TE_GRAPPLE_CABLE,
    TE_FLASHLIGHT,
    //! 
    //TE_FORCEWALL,
	//! Renders a debug trail.
    TE_DEBUGTRAIL,

    TE_NUM_ENTITY_EVENTS
} temp_entity_event_t;
