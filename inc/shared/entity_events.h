/********************************************************************
*
*
*   Entity Events:
*
*
********************************************************************/
#pragma once

/**
*	@description entity_state_t->event values
*				 ertity events are for effects that take place reletive
*				 to an existing entities origin.  Very network efficient.
*				 All muzzle flashes really should be converted to events...
**/
typedef enum {
    EV_NONE = 0,
    EV_ITEM_RESPAWN,
    EV_FOOTSTEP,
    EV_FALLSHORT,
    EV_FALL,
    EV_FALLFAR,
    EV_PLAYER_TELEPORT,
    EV_OTHER_TELEPORT,

    // WID: Ladder Steps.
    EV_FOOTSTEP_LADDER,
} entity_event_t;