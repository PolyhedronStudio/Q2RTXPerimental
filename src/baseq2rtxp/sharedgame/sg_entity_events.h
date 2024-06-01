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
    // Player/Entities:
    EV_FALL = EV_ENGINE_MAX,
    EV_FALLSHORT,
    EV_FALLFAR,
    EV_FOOTSTEP,
    EV_FOOTSTEP_LADDER,

    // Items:
    EV_ITEM_RESPAWN,

    EV_GAME_MAX
} sg_entity_events_t;