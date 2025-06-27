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
typedef enum engine_entity_event_e {
    // Engine Specific:
    EV_NONE = 0,
    EV_PLAYER_TELEPORT,
    EV_OTHER_TELEPORT,

    //! Game can start entities from this index.
    EV_ENGINE_MAX,
} engine_entity_events_t;
typedef uint32_t entity_event_t;